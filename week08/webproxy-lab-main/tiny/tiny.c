/* $begin tinymain */
/*
 * tiny.c - A simple, iterative HTTP/1.0 Web server that uses the
 *     GET method to serve static and dynamic content.
 *
 * Updated 11/2019 droh
 *   - Fixed sprintf() aliasing issue in serve_static(), and clienterror().
 */
#include "csapp.h"

void doit(int fd);                                         // HTTP 요청 처리 메인 로직
void read_requesthdrs(rio_t *rp);                          // HTTP 요청 헤더 읽기
int parse_uri(char *uri, char *filename, char *cgiargs);   // URI 파싱 (정적/동적 구분)
void serve_static(int fd, char *filename, int filesize);   // 정적 파일(HTML, 이미지) 전송
void get_filetype(char *filename, char *filetype);         // 파일 확장자로 MIME 타입 결정
void serve_dynamic(int fd, char *filename, char *cgiargs); // CGI 프로그램 실행 (동적 콘텐츠)
void clienterror(int fd, char *cause, char *errnum, char *shortmsg,
                 char *longmsg); // HTTP 에러 응답 전송

/* 구현 순서 추천
 * clienterror -> read_requesthdrs -> parse_uri -> serve_static -> get_filetype -> serve_dynamic -> doit
 */

void clienterror(int fd, char *cause, char *errnum, char *shortmsg, char *longmsg)
{
  char buf[MAXLINE], body[MAXBUF];

  /* Build the HTTP response body */
  sprintf(body, "<html><title>Tiny Error</title>");
  sprintf(body, "%s<body bgcolor=ffffff>\r\n",
          body);
  sprintf(body, "%s%s: %s\r\n", body, errnum, shortmsg);
  sprintf(body, "%s<p>%s: %s\r\n", body, longmsg, cause);
  sprintf(body, "%s<hr><em>The Tiny Web server</em>\r\n", body);

  /* Print the HTTP response */
  sprintf(buf, "HTTP/1.0 %s %s\r\n", errnum, shortmsg);
  Rio_writen(fd, buf, strlen(buf));
  sprintf(buf, "Content-type: text/html\r\n");
  Rio_writen(fd, buf, strlen(buf));
  sprintf(buf, "Content-length: %d\r\n\r\n", (int)strlen(body));
  Rio_writen(fd, buf, strlen(buf));
  Rio_writen(fd, body, strlen(body));
}

void read_requesthdrs(rio_t *rp)
{
  char buf[MAXLINE];

  // GET /index.html HTTP/1.1 은 전 단게에서 읽혀졌다고 가정

  Rio_readlineb(rp, buf, MAXLINE); // Host: localhost
  while (strcmp(buf, "\r\n"))      // 빈 줄을 만날 때까지
  {
    Rio_readlineb(rp, buf, MAXLINE); // User-Agent: Mozilla/5.0 단순 소모하고 확인하는 용도
    printf("%s", buf);
  }
  return;
}

int parse_uri(char *uri, char *filename, char *cgiargs)
{
  char *ptr;

  if (!strstr(uri, "cgi-bin")) /* Static content (http://localhost/ or http://localhost/img/logo.png) */
  {
    strcpy(cgiargs, "");             // cgiargs = ""
    strcpy(filename, ".");           // filename = "."
    strcat(filename, uri);           // filename = "./"
    if (uri[strlen(uri) - 1] == '/') // uri = "/"
      strcat(filename, "home.html"); // filename ="./home.html"
    return 1;
  }

  else /* Dynamic content (http://localhost/cgi-bin/adder?123&456) */
  {
    ptr = index(uri, '?');
    if (ptr)
    {
      strcpy(cgiargs, ptr + 1); // 123&456
      *ptr = '\0';              // http://localhost/cgi-bin/adder
    }
    else
      strcpy(cgiargs, ""); // cgiargs = ""
    strcpy(filename, "."); // filename = "."
    strcat(filename, uri); // filename = "./cgi-bin/adder"
    return 0;
  }
}

void serve_static(int fd, char *filename, int filesize)
{
  int srcfd;
  char *srcp, filetype[MAXLINE], buf[MAXBUF];

  /* Send response headers to client */
  get_filetype(filename, filetype);                   // 파일 확장자를 보고 브라우저에게 fileType을 알려줌
  sprintf(buf, "HTTP/1.0 200 OK\r\n");                // buf에 첫 줄 작성, 200 OK
  sprintf(buf, "%sServer: Tiny Web Server\r\n", buf); // buf에 누적 작성
  sprintf(buf, "%sConnection: close\r\n", buf);
  sprintf(buf, "%sContent-length: %d\r\n", buf, filesize);   // 보낼 파일 크기를 알려줌. 브라우저는 값을 보고 언제까지 데이터를 받아야 할지 알게 됨
  sprintf(buf, "%sContent-type: %s\r\n\r\n", buf, filetype); // 파일 종류의 헤더를 넣음. \r\n\r\n은 이제 헤더 끝임을 알림
  Rio_writen(fd, buf, strlen(buf));                          // 헤더(buf)를 클라이언트(fd)에게 실제로 전송
  printf("Reponse headers:\n");
  printf("%s", buf); // 콘솔창에 헤더 출력

  /* Send response body to client */
  srcfd = Open(filename, O_RDONLY, 0);                        // 전송할 파일을 '읽기 전용'으로 연다.
  srcp = Mmap(0, filesize, PROT_READ, MAP_PRIVATE, srcfd, 0); // Mmap(가상 메모리 기법), 파일을 메모리로 그대로 매핑
  Close(srcfd);                                               // 메모리 매핑했으니 파일 식별자는 닫는다.
  Rio_writen(fd, srcp, filesize);                             // 메모리 주소(srcp)에 있는 파일 내용을 클라이언트(fd)에게 통째로 쏜다.
  Munmap(srcp, filesize);                                     // 사용이 끝난 메모리 매핑 해제 (메모리 반환)
}

/*
 * get_filetype - Derive file type from filename
 */
void get_filetype(char *filename, char *filetype)
{
  if (strstr(filename, ".html"))
    strcpy(filetype, "text/html");
  else if (strstr(filename, ".gif"))
    strcpy(filetype, "image/gif");
  else if (strstr(filename, ".png"))
    strcpy(filetype, "image/png");
  else if (strstr(filename, ".jpg"))
    strcpy(filetype, "image/jpeg");
  else
    strcpy(filetype, "text/plain");
}

void serve_dynamic(int fd, char *filename, char *cgiargs)
{
  char buf[MAXLINE], *emptylist[] = {NULL};

  /* Return first part of HTTP response */
  sprintf(buf, "HTTP/1.0 200 OK\r\n");
  Rio_writen(fd, buf, strlen(buf));
  sprintf(buf, "Server: Tiny Web Server\r\n");
  Rio_writen(fd, buf, strlen(buf)); // 응답의 시작을 알림

  if (Fork() == 0) /* Child: 현재 실행중인 서버 프로세스를 똑같이 하나 더 복제합니다. '== 0' 복제된 자식 프로세스만 코드 실행 */
  {
    /* Real server would set all CGI vars here */
    setenv("QUERY_STRING", cgiargs, 1);   // QUERY_STRING이라는 이름의 환경 변수에 cgiargs 저장
    Dup2(fd, STDOUT_FILENO);              /* Redirect stdout to client */
    Execve(filename, emptylist, environ); /* Run CGI program */
  }
  Wait(NULL); /* Parent waits for and reaps child */
}

/*
 * 웹 서버의 심장과 같은 역할
 * 클라이언트 요청이 들어왔을 때, 그 요청이 무엇인지 분석하고(Parse), 실제로 존재하는지 확인한 뒤, 적절한 응답(정적 혹은 동적)을 보내는 전체 과정을 제어
 */
void doit(int fd)
{
  int is_static;
  struct stat sbuf;
  char buf[MAXLINE], method[MAXLINE], uri[MAXLINE], version[MAXLINE];
  char filename[MAXLINE], cgiargs[MAXLINE];
  rio_t rio;

  /* Read request line and headers */
  Rio_readinitb(&rio, fd);
  Rio_readlineb(&rio, buf, MAXLINE); // 클라이언트가 보낸 첫 줄을 읽음 "GET /index.html HTTP/1.1"
  printf("Request headers:\n");
  printf("%s", buf);
  sscanf(buf, "%s %s %s", method, uri, version); // GET /index.html HTTP/1.1
  if (strcasecmp(method, "GET"))                 // GET 이외의 요청이 들어오면 501 에러
  {
    clienterror(fd, method, "501", "Not implemented", "Tiny does not implement this method");
    return;
  }
  read_requesthdrs(&rio); // 부가적인 헤더를 읽어서 소모

  /* Parse URI from GET request */
  is_static = parse_uri(uri, filename, cgiargs); // URI를 분석해서 정적/동적 판단하고 filename 추출
  if (stat(filename, &sbuf) < 0)                 // 파일의 정보를 가져옴 없으면 404
  {
    clienterror(fd, filename, "404", "Not found", "Tiny couldn't find this file");
    return;
  }

  if (is_static) /* Serve statuc content */
  {
    if (!(S_ISREG(sbuf.st_mode)) || !(S_IRUSR & sbuf.st_mode)) // 파일이 맞는지, 읽기 권한이 있는지 확인
    {
      clienterror(fd, filename, "403", "Fornidden", "Tiny couldn't read the file");
      return;
    }
    serve_static(fd, filename, sbuf.st_size); // 파일 읽어서 전송
  }
  else /* Serve dynamic content */
  {
    if (!(S_ISREG(sbuf.st_mode)) || !(S_IXUSR & sbuf.st_mode)) // 파일이 맞는지, 실행할 수 있는 파일인지 확인
    {
      clienterror(fd, filename, "403", "Forbidden", "Tiny couldn't run the CGI program");
      return;
    }
    serve_dynamic(fd, filename, cgiargs); // 자식 프로세스를 만들어 프로그램을 실행하고 그 결과를 전송
  }
}

int main(int argc, char **argv)
{
  int listenfd, connfd;
  char hostname[MAXLINE], port[MAXLINE];
  socklen_t clientlen;
  struct sockaddr_storage clientaddr;

  /* Check command line args */
  if (argc != 2)
  {
    fprintf(stderr, "usage: %s <port>\n", argv[0]);
    exit(1);
  }

  listenfd = Open_listenfd(argv[1]);
  while (1)
  {
    clientlen = sizeof(clientaddr);
    connfd = Accept(listenfd, (SA *)&clientaddr,
                    &clientlen); // line:netp:tiny:accept
    Getnameinfo((SA *)&clientaddr, clientlen, hostname, MAXLINE, port, MAXLINE,
                0);
    printf("Accepted connection from (%s, %s)\n", hostname, port);
    doit(connfd);  // line:netp:tiny:doit
    Close(connfd); // line:netp:tiny:close
  }
}
