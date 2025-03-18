#include <stdio.h>
#include <string.h>
#include <stdlib.h> // Include the stdlib.h header for the exit function
#include <unistd.h> // Include the unistd.h header for the close function
#include "library.h"

int server_fd = -1;

void setup() {
    if (server_fd == -1) {
        server_fd = connect_to_server();
        if (server_fd == -1) {
            printf("Failed to connect to server\n");
            exit(1);
        }
        printf("Connected to server on fd %d\n", server_fd);
    }
}

void teardown() {
    if (server_fd != -1) {
        close(server_fd);
        server_fd = -1;
        printf("Closed connection to server\n");
    }
}

void testapi1(int counter) {
    char my_buffer[100];
    setup();
    sprintf(my_buffer, "Hello server (%d)!", counter);
    printf("Sending to server: %s\n", my_buffer);
    int status = send_recv_msg(server_fd, my_buffer, strlen(my_buffer) + 1, my_buffer, sizeof(my_buffer));
    if (status <= 0) {
        printf("Server died? status %d\n", status);
        exit(1);
    }
    printf("Received from server: %s\n", my_buffer);
}

void testapi2(int counter) {
    char my_buffer[100];
    setup();
    sprintf(my_buffer, "Goodbye server (%d)!", counter);
    printf("Sending to server: %s\n", my_buffer);
    int status = send_recv_msg(server_fd, my_buffer, strlen(my_buffer) + 1, my_buffer, sizeof(my_buffer));
    if (status <= 0) {
        printf("Server died? status %d\n", status);
        exit(1);
    }
    printf("Received from server: %s\n", my_buffer);
}

int main() {
    int counter = 0;
    int status = -1;

    while (counter++ < 10) {
        testapi1(counter);
        sleep(1);
        testapi2(counter);
        sleep(1);
    }

    // Close the socket
    teardown();
    return status;
}
