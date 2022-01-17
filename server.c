#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include <arpa/inet.h>
#include <err.h>

char response1[] = "HTTP/1.0 200 OK\r\n";
char response2[] = "Content-Type: text/plain\r\n"
		   "Content-Length: 6\r\n"
		   "Connection: close\r\n\r\n"
		   "answer";
int main()
{
  int port = 6666;
  int one = 1, client_fd;
  struct sockaddr_in svr_addr, cli_addr;
  socklen_t sin_len = sizeof(cli_addr);

  int sock = socket(AF_INET, SOCK_STREAM, 0);
  setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, &one, sizeof(int));

  svr_addr.sin_family = AF_INET;
  svr_addr.sin_addr.s_addr = INADDR_ANY;
  svr_addr.sin_port = htons(port);

  bind(sock, (struct sockaddr *) &svr_addr, sizeof(svr_addr));
  listen(sock, 5);

  while (1) {
    client_fd = accept(sock, (struct sockaddr *) &cli_addr, &sin_len);
    sleep(1);
    write(client_fd, response1, sizeof(response1) - 1);
    sleep(3);
    write(client_fd, response2, sizeof(response2) - 1);
    close(client_fd);
  }
}
