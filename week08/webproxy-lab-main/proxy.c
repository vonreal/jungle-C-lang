#include "csapp.h"

/* ── 캐시 크기 상수 ─────────────────────────────────────────────────── */
#define MAX_CACHE_SIZE  1048576   /* 전체 캐시 최대 크기: 1 MiB  */
#define MAX_OBJECT_SIZE 102400    /* 단일 객체 최대 크기: 100 KiB */

/* ── 문서에 명시된 User-Agent 상수 문자열 ─────────────────────────────── */
static const char *user_agent_hdr =
    "User-Agent: Mozilla/5.0 (X11; Linux x86_64; rv:10.0.3) Gecko/20120305 "
    "Firefox/10.0.3\r\n";

/* ── 캐시 자료구조 ──────────────────────────────────────────────────── */

/*
 * 이중 연결 리스트 노드: head = MRU(가장 최근), tail = LRU(가장 오래됨)
 * 캐시 히트/삽입 시 항상 head로 이동 → LRU 제거 시 tail에서 제거
 */
typedef struct cache_block
{
  char uri[MAXLINE];          /* 캐시 키 */
  char *data;                 /* 캐시된 응답 데이터 */
  size_t size;                /* 데이터 크기 (바이트) */
  struct cache_block *prev;
  struct cache_block *next;
} cache_block_t;

/*
 * 캐시 전체 구조체
 * 동기화: 고전적 first-readers-writers 패턴
 *   - mutex: readcnt 보호용 세마포어
 *   - w    : 쓰기 독점 잠금 (첫 번째 읽기 스레드가 획득, 마지막이 해제)
 * 여러 스레드가 동시에 읽기 가능, 쓰기는 단독 접근
 */
typedef struct
{
  cache_block_t *head;      /* MRU 쪽 끝 */
  cache_block_t *tail;      /* LRU 쪽 끝 */
  size_t total_size;        /* 현재 캐시된 총 데이터 크기 */
  int readcnt;              /* 현재 캐시를 읽고 있는 스레드 수 */
  sem_t mutex;              /* readcnt 접근 보호 */
  sem_t w;                  /* 쓰기 독점 잠금 */
} cache_t;

static cache_t cache;

/* ── 함수 선언 ──────────────────────────────────────────────────────── */
void  doit(int fd);
void  parse_uri(char *uri, char *hostname, char *path, char *port);
void  build_http_header(char *http_header, char *hostname, char *path,
                        rio_t *client_rio);
void  clienterror(int fd, char *cause, char *errnum, char *shortmsg,
                  char *longmsg);
void *thread(void *vargp);

void  cache_init(void);
int   cache_read(const char *uri, int fd);
void  cache_write(const char *uri, const char *data, size_t size);
static void move_to_head(cache_block_t *block);

/* ── main ───────────────────────────────────────────────────────────── */
int main(int argc, char **argv)
{
  int listenfd, *connfdp;
  char hostname[MAXLINE], port[MAXLINE];
  socklen_t clientlen;
  struct sockaddr_storage clientaddr;
  pthread_t tid;

  if (argc != 2)
  {
    fprintf(stderr, "usage: %s <port>\n", argv[0]);
    exit(1);
  }

  /* 끊긴 소켓에 쓸 때 발생하는 SIGPIPE로 인한 프로세스 종료 방지 */
  Signal(SIGPIPE, SIG_IGN);

  /* 캐시 초기화 (3단계) */
  cache_init();

  listenfd = Open_listenfd(argv[1]);

  while (1)
  {
    clientlen = sizeof(clientaddr);
    /* connfd를 힙에 할당: 스택 변수를 넘기면 루프가 덮어쓰기 전에 스레드가
     * 읽는다는 보장이 없어 레이스 컨디션 발생 */
    connfdp  = Malloc(sizeof(int));
    *connfdp = Accept(listenfd, (SA *)&clientaddr, &clientlen);
    Getnameinfo((SA *)&clientaddr, clientlen, hostname, MAXLINE, port, MAXLINE, 0);
    printf("Accepted connection from (%s, %s)\n", hostname, port);

    /* 요청마다 새 스레드 생성 후 즉시 detach — 메모리 누수 방지 */
    Pthread_create(&tid, NULL, thread, connfdp);
    Pthread_detach(tid);
  }
  return 0;
}

/* 각 스레드가 실행할 함수: doit 호출 후 소켓과 힙 메모리 해제 */
void *thread(void *vargp)
{
  int connfd = *((int *)vargp);
  Free(vargp);
  doit(connfd);
  Close(connfd);
  return NULL;
}

/* ── doit ───────────────────────────────────────────────────────────── */
void doit(int fd)
{
  int serverfd;
  char buf[MAXLINE], method[MAXLINE], uri[MAXLINE], version[MAXLINE];
  char server_http_header[MAXLINE];
  char hostname[MAXLINE], path[MAXLINE], port[MAXLINE];
  rio_t rio, server_rio;

  Rio_readinitb(&rio, fd);
  if (!Rio_readlineb(&rio, buf, MAXLINE))
    return;
  printf("Request headers:\n%s", buf);

  sscanf(buf, "%s %s %s", method, uri, version);

  if (strcasecmp(method, "GET"))
  {
    clienterror(fd, method, "501", "Not implemented",
                "Proxy does not implement this method");
    return;
  }

  /* ── 캐시 히트 확인: 있으면 캐시에서 즉시 응답하고 반환 ── */
  if (cache_read(uri, fd))
    return;

  parse_uri(uri, hostname, path, port);
  build_http_header(server_http_header, hostname, path, &rio);

  serverfd = Open_clientfd(hostname, port);
  if (serverfd < 0)
  {
    printf("connection failed\n");
    return;
  }

  Rio_readinitb(&server_rio, serverfd);
  Rio_writen(serverfd, server_http_header, strlen(server_http_header));

  /*
   * 서버 응답을 클라이언트로 릴레이하면서 동시에 캐시 버퍼에 누적.
   * MAX_OBJECT_SIZE를 초과하면 버퍼링만 중단하고 릴레이는 계속함.
   */
  char   obj_buf[MAX_OBJECT_SIZE];
  size_t obj_size  = 0;
  int    cacheable = 1;
  size_t n;

  while ((n = Rio_readnb(&server_rio, buf, MAXLINE)) > 0)
  {
    Rio_writen(fd, buf, n);
    if (cacheable)
    {
      if (obj_size + n <= MAX_OBJECT_SIZE)
      {
        memcpy(obj_buf + obj_size, buf, n);
        obj_size += n;
      }
      else
      {
        cacheable = 0;  /* 객체가 너무 크면 캐싱 포기 */
      }
    }
  }

  if (cacheable && obj_size > 0)
    cache_write(uri, obj_buf, obj_size);

  Close(serverfd);
}

/* ── 캐시 구현 ──────────────────────────────────────────────────────── */

void cache_init(void)
{
  cache.head       = NULL;
  cache.tail       = NULL;
  cache.total_size = 0;
  cache.readcnt    = 0;
  Sem_init(&cache.mutex, 0, 1);
  Sem_init(&cache.w,     0, 1);
}

/*
 * cache_read: URI로 캐시 검색. 히트 시 클라이언트(fd)로 전송 후 1 반환.
 *
 * 동기화 전략 (first-readers-writers):
 *   읽기 구간: readcnt 기반 readers lock → 여러 스레드 동시 읽기 허용
 *   LRU 갱신: write lock → move_to_head (URI로 재검색해 use-after-free 방지)
 *
 * 읽기 도중 Rio_writen 블로킹을 피하기 위해
 * read lock 내에서 로컬 버퍼로 복사 후 lock 해제 → 전송 순서로 처리.
 */
int cache_read(const char *uri, int fd)
{
  char  *local_data = NULL;
  size_t local_size = 0;

  /* ── reader lock in ── */
  P(&cache.mutex);
  if (++cache.readcnt == 1)
    P(&cache.w);
  V(&cache.mutex);

  for (cache_block_t *b = cache.head; b; b = b->next)
  {
    if (!strcmp(b->uri, uri))
    {
      /* lock 해제 후 전송을 위해 로컬 버퍼에 복사 */
      local_data = Malloc(b->size);
      memcpy(local_data, b->data, b->size);
      local_size = b->size;
      break;
    }
  }

  /* ── reader lock out ── */
  P(&cache.mutex);
  if (--cache.readcnt == 0)
    V(&cache.w);
  V(&cache.mutex);

  if (!local_data)
    return 0;   /* 캐시 미스 */

  /* lock 밖에서 클라이언트로 전송 (블로킹이 lock을 점유하지 않도록) */
  Rio_writen(fd, local_data, local_size);
  Free(local_data);

  /* LRU 갱신: write lock 획득 후 URI로 재검색 (포인터 재사용 금지) */
  P(&cache.w);
  for (cache_block_t *b = cache.head; b; b = b->next)
  {
    if (!strcmp(b->uri, uri))
    {
      move_to_head(b);
      break;
    }
  }
  V(&cache.w);

  return 1;   /* 캐시 히트 */
}

/*
 * cache_write: 새 객체를 캐시에 삽입. write lock 독점 사용.
 * 공간 부족 시 LRU(tail) 블록을 여유가 생길 때까지 제거.
 */
void cache_write(const char *uri, const char *data, size_t size)
{
  P(&cache.w);

  /* 공간이 생길 때까지 LRU(tail) 블록 제거 */
  while (cache.total_size + size > MAX_CACHE_SIZE && cache.tail)
  {
    cache_block_t *lru = cache.tail;
    if (lru->prev)
      lru->prev->next = NULL;
    else
      cache.head = NULL;
    cache.tail        = lru->prev;
    cache.total_size -= lru->size;
    free(lru->data);
    free(lru);
  }

  /* 새 블록 생성 후 head(MRU)에 삽입 */
  cache_block_t *block = Malloc(sizeof(cache_block_t));
  strncpy(block->uri, uri, MAXLINE - 1);
  block->uri[MAXLINE - 1] = '\0';
  block->data = Malloc(size);
  memcpy(block->data, data, size);
  block->size = size;

  block->prev = NULL;
  block->next = cache.head;
  if (cache.head)
    cache.head->prev = block;
  else
    cache.tail = block;
  cache.head        = block;
  cache.total_size += size;

  V(&cache.w);
}

/* 블록을 MRU 위치(head)로 이동. 호출자가 write lock을 보유해야 함. */
static void move_to_head(cache_block_t *block)
{
  if (block == cache.head)
    return;

  /* 현재 위치에서 분리 */
  if (block->prev)
    block->prev->next = block->next;
  if (block->next)
    block->next->prev = block->prev;
  else
    cache.tail = block->prev;  /* block이 tail이었던 경우 */

  /* head에 삽입 */
  block->prev       = NULL;
  block->next       = cache.head;
  cache.head->prev  = block;
  cache.head        = block;
}

/* ── parse_uri ──────────────────────────────────────────────────────── */
/*
 * URL 파싱 함수 (http://hostname:port/path 형태를 분리)
 * 포트가 생략된 경우 기본 HTTP 포트인 80번으로 설정
 */
void parse_uri(char *uri, char *hostname, char *path, char *port)
{
  char *pos = strstr(uri, "//");
  pos = pos != NULL ? pos + 2 : uri;

  char *pos_port = strstr(pos, ":");
  char *pos_path = strstr(pos, "/");

  strcpy(path, "/");

  if (pos_port != NULL)
  {
    *pos_port = '\0';
    sscanf(pos, "%s", hostname);

    if (pos_path != NULL)
    {
      *pos_path = '\0';
      sscanf(pos_port + 1, "%s", port);
      *pos_path = '/';
      sscanf(pos_path, "%s", path);
    }
    else
    {
      sscanf(pos_port + 1, "%s", port);
    }
  }
  else
  {
    strcpy(port, "80");
    if (pos_path != NULL)
    {
      *pos_path = '\0';
      sscanf(pos, "%s", hostname);
      *pos_path = '/';
      sscanf(pos_path, "%s", path);
    }
    else
    {
      sscanf(pos, "%s", hostname);
    }
  }
}

/* ── build_http_header ──────────────────────────────────────────────── */
/*
 * 클라이언트가 보낸 헤더들을 순회하며 대상 서버로 보낼 HTTP 헤더 구성
 * HTTP/1.0 및 과제 필수 헤더 조건(Host, User-Agent, Connection, Proxy-Connection) 충족
 */
void build_http_header(char *http_header, char *hostname, char *path,
                       rio_t *client_rio)
{
  char buf[MAXLINE], request_hdr[MAXLINE], other_hdr[MAXLINE], host_hdr[MAXLINE];

  sprintf(request_hdr, "GET %s HTTP/1.0\r\n", path);

  other_hdr[0] = '\0';
  host_hdr[0]  = '\0';

  while (Rio_readlineb(client_rio, buf, MAXLINE) > 0)
  {
    if (!strcmp(buf, "\r\n"))
      break;

    if (!strncasecmp(buf, "Host:", 5))
      strcpy(host_hdr, buf);
    else if (strncasecmp(buf, "Connection:", 11) &&
             strncasecmp(buf, "Proxy-Connection:", 17) &&
             strncasecmp(buf, "User-Agent:", 11))
    {
      /* 버퍼 오버플로우 방지: 잔여 공간 확인 후 추가 */
      size_t remaining = MAXLINE - strlen(other_hdr) - 1;
      strncat(other_hdr, buf, remaining);
    }
  }

  if (strlen(host_hdr) == 0)
    sprintf(host_hdr, "Host: %s\r\n", hostname);

  sprintf(http_header, "%s%s%sConnection: close\r\nProxy-Connection: close\r\n%s\r\n",
          request_hdr, host_hdr, user_agent_hdr, other_hdr);
}

/* ── clienterror ────────────────────────────────────────────────────── */
void clienterror(int fd, char *cause, char *errnum, char *shortmsg,
                 char *longmsg)
{
  char buf[MAXLINE], body[MAXBUF];

  sprintf(body, "<html><title>Proxy Error</title>");
  sprintf(body, "%s<body bgcolor=ffffff>\r\n", body);
  sprintf(body, "%s%s: %s\r\n", body, errnum, shortmsg);
  sprintf(body, "%s<p>%s: %s\r\n", body, longmsg, cause);
  sprintf(body, "%s<hr><em>The Proxy Web server</em>\r\n", body);

  sprintf(buf, "HTTP/1.0 %s %s\r\n", errnum, shortmsg);
  Rio_writen(fd, buf, strlen(buf));
  sprintf(buf, "Content-type: text/html\r\n");
  Rio_writen(fd, buf, strlen(buf));
  sprintf(buf, "Content-length: %d\r\n\r\n", (int)strlen(body));
  Rio_writen(fd, buf, strlen(buf));
  Rio_writen(fd, body, strlen(body));
}
