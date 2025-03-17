#include <stdio.h>
#include <string.h>
#include <stdlib.h> // Include the stdlib.h header for the exit function
#include <unistd.h> // Include the unistd.h header for the close function
#include "library.h"


int main()
{
  int counter = 0;
  int status = -1;

  char my_buffer[100];

  int server_fd = connect_to_server();
  if (server_fd == -1) {
    return status;
  }

  while (counter++ < 10) {
    sprintf(my_buffer, "Hello server (%d)!", counter);
    status = send_recv_msg(server_fd, my_buffer, strlen(my_buffer)+1, my_buffer, sizeof(my_buffer));
    if (status <= 0) {
      printf("Server went away: %d\n", status);
      break;
    }
    printf("Received from server: %s\n", my_buffer);
    sleep(1);
  }

  // Close the socket
  close(server_fd);
  return status;
}
