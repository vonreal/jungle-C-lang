#include "csapp.h"

/* 문서에 명시된 User-Agent 상수 문자열 */
static const char *user_agent_hdr =
    "User-Agent: Mozilla/5.0 (X11; Linux x86_64; rv:10.0.3) Gecko/20120305 "
    "Firefox/10.0.3\r\n";

/* 함수 선언 */
void doit(int fd);
void parse_uri(char *uri, char *hostname, char *path, char *port);
void build_http_header(char *http_header, char *hostname, char *path, rio_t *client_rio);
void clienterror(int fd, char *cause, char *errnum, char *shortmsg, char *longmsg);

int main(int argc, char **argv)
{
  int listenfd, connfd;
  char hostname[MAXLINE], port[MAXLINE];
  socklen_t clientlen;
  struct sockaddr_storage clientaddr;

  /* 명령줄 인수 확인: 포트 번호 필수 */
  if (argc != 2)
  {
    fprintf(stderr, "usage: %s <port>\n", argv[0]);
    exit(1);
  }

  /* 끊긴 소켓에 쓸 때 발생하는 SIGPIPE로 인한 프로세스 종료 방지 */
  Signal(SIGPIPE, SIG_IGN);

  /* 지정된 포트에서 수신 대기 소켓 오픈 */
  listenfd = Open_listenfd(argv[1]);

  /* 순차적 웹 프록시 기본 루프 (1단계 요구사항)*/
  while (1)
  {
    clientlen = sizeof(clientaddr);
    connfd = Accept(listenfd, (SA *)&clientaddr, &clientlen);
    Getnameinfo((SA *)&clientaddr, clientlen, hostname, MAXLINE, port, MAXLINE, 0);
    printf("Accepted connection from (%s, %s)\n", hostname, port);

    /* 클라이언트 요청 처리 */
    doit(connfd);

    /* 처리가 끝나면 연결 식별자 닫기*/
    Close(connfd);
  }
  return 0;
}

void doit(int fd)
{
  int serverfd;
  char buf[MAXLINE], method[MAXLINE], uri[MAXLINE], version[MAXLINE];
  char server_http_header[MAXLINE];
  char hostname[MAXLINE], path[MAXLINE], port[MAXLINE];
  rio_t rio, server_rio;

  /* 1. 클라이언트의 요청 라인 읽기 */
  Rio_readinitb(&rio, fd);
  if (!Rio_readlineb(&rio, buf, MAXLINE))
    return;
  printf("Request headers:\n%s", buf);

  /* 메서드, 전체 URI, 버전 파싱 */
  sscanf(buf, "%s %s %s", method, uri, version);

  /* GET 요청만 처리 */
  if (strcasecmp(method, "GET"))
  {
    clienterror(fd, method, "501", "Not implemented", "Proxy does not implement this method");
    return;
  }

  /* 2. 전체 URL 파싱하여 호스트, 경로, 포트 분리 */
  parse_uri(uri, hostname, path, port);

  /* 3. 목적지 서버로 보낼 조작된 HTTP/1.0 헤더 조립 */
  build_http_header(server_http_header, hostname, path, &rio);

  /* 4. 목적지 서버와 연결 (클라이언트 역할 수행) */
  serverfd = Open_clientfd(hostname, port);
  if (serverfd < 0)
  {
    printf("connection failed\n");
    return;
  }

  /* 5. 조립한 요청(헤더)을 대상 서버로 전송*/
  Rio_readinitb(&server_rio, serverfd);
  Rio_writen(serverfd, server_http_header, strlen(server_http_header));

  /* 6. 대상 서버의 응답을 끝까지 읽어 클라이언트에게 그대로 릴레이 (바이트 단위 처리) */
  size_t n;
  while ((n = Rio_readnb(&server_rio, buf, MAXLINE)) > 0)
    Rio_writen(fd, buf, n);

  /* 7. 서버 연결 종료 */
  Close(serverfd);
}

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

  strcpy(path, "/"); // 기본 경로 설정

  if (pos_port != NULL)
  {
    /* 포트가 명시된 경우 (예: :8080) */
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
    /* 포트가 명시되지 않은 경우 (기본 80포트) */
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

/*
 * 클라이언트가 보낸 헤더들을 순회하며 대상 서버로 보낼 HTTP 헤더 구성
 * HTTP/1.0 및 과제 필수 헤더 조건(Host, User-Agent, Connection, Proxy-Connection) 충족
 */
void build_http_header(char *http_header, char *hostname, char *path, rio_t *client_rio)
{
  char buf[MAXLINE], request_hdr[MAXLINE], other_hdr[MAXLINE], host_hdr[MAXLINE];

  /* 최신 브라우저가 보낸 요청을 HTTP/1.0 GET 요청 라인으로 변환 */
  sprintf(request_hdr, "GET %s HTTP/1.0\r\n", path);

  other_hdr[0] = '\0';
  host_hdr[0] = '\0';

  /* 클라이언트가 보낸 나머지 부가 헤더 읽기 루프 */
  while (Rio_readlineb(client_rio, buf, MAXLINE) > 0)
  {
    if (!strcmp(buf, "\r\n"))
      break; // 빈 줄을 만나면 헤더 끝

    /* 브라우저가 Host 헤더를 보냈다면 추출 */
    if (!strncasecmp(buf, "Host:", 5))
      strcpy(host_hdr, buf);
    /* 필수로 덮어씌워야 할 헤더들을 제외한 나머지 헤더 보존 */
    else if (strncasecmp(buf, "Connection:", 11) &&
             strncasecmp(buf, "Proxy-Connection:", 17) &&
             strncasecmp(buf, "User-Agent:", 11))
    {
      /* 버퍼 오버플로우 방지: 잔여 공간 확인 후 추가 */
      size_t remaining = MAXLINE - strlen(other_hdr) - 1;
      strncat(other_hdr, buf, remaining);
    }
  }

  /* 브라우저가 Host 헤더를 보내지 않았다면 우리가 파싱한 호스트 이름으로 강제 생성 */
  if (strlen(host_hdr) == 0)
    sprintf(host_hdr, "Host: %s\r\n", hostname);

  /* 필수 조건 헤더들을 모두 조합 */
  sprintf(http_header, "%s%s%sConnection: close\r\nProxy-Connection: close\r\n%s\r\n",
          request_hdr,
          host_hdr,
          user_agent_hdr,
          other_hdr);
}

/*
 * 클라이언트(브라우저)에게 HTTP 에러 응답을 전송
 * tiny.c에서 그대로 가져옴
 */
void clienterror(int fd, char *cause, char *errnum, char *shortmsg, char *longmsg)
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