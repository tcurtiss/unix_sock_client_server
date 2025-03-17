#include <stdio.h>
#include <string.h>
#include "library.h"



int handle_msg(int client_fd, void *msg, int size)
{
  if (size == 0) {
    printf("Client %d disconnected\n", client_fd);
    return 0;
  }
  printf("Received message from client %d: %s\n", client_fd, (char *)msg);

  char reply[100] = "Hello, client! (%s)";
  sprintf(reply, "Hello, client! (%s)", (char *)msg);
  int status = send_recv_msg(client_fd, reply, strlen(reply), NULL, 0);
  printf("Replied message to client %d: %s (status=%d)\n", client_fd, reply, status);
  if (status <= 0) {
    return status;
  }

  return 0;
}

int handle_signal(int signum)
{
  printf("Caught signal %d\n", signum);
  return signum != 2 ? 0 : -1;
}

int main()
{
  int status = dispatch_loop(10, handle_signal, handle_msg, 1000);
  return status;
}
