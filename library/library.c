#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <sys/epoll.h>
#include <signal.h>
#include <sys/signalfd.h>

#include "library.h"

#define ABSTRACT_SOCKET_NAME "\0epoll_unix_socket"

int server_fd, epoll_fd, signal_fd;

int dispatch_loop(int max_clients, sig_handler_t sig_handler,
  msg_handler_t msg_handler, int max_msg_size)

{
  struct sockaddr_un addr;
  struct epoll_event ev, *events;
  sigset_t mask;
  bool loop_active = true;
  int status = EXIT_SUCCESS;

  events = (struct epoll_event *)malloc(max_clients * sizeof(events[0]));

  // Block signals so that they can be handled by signalfd
  sigemptyset(&mask);
  sigaddset(&mask, SIGINT);
  sigaddset(&mask, SIGTERM);
  if (sigprocmask(SIG_BLOCK, &mask, NULL) == -1) {
    perror("sigprocmask");
    exit(EXIT_FAILURE);
  }

  // Create signalfd
  signal_fd = signalfd(-1, &mask, 0);
  if (signal_fd == -1) {
    perror("signalfd");
    exit(EXIT_FAILURE);
  }

  // Create server socket
  if ((server_fd = socket(AF_UNIX, SOCK_SEQPACKET, 0)) == -1) {
    perror("socket");
    exit(EXIT_FAILURE);
  }

  // Bind socket to an abstract namespace
  memset(&addr, 0, sizeof(addr));
  addr.sun_family = AF_UNIX;
  strncpy(addr.sun_path, ABSTRACT_SOCKET_NAME, sizeof(addr.sun_path) - 1);
  if (bind(server_fd, (struct sockaddr *)&addr, sizeof(addr)) == -1) {
    perror("bind");
    close(server_fd);
    exit(EXIT_FAILURE);
  }

  // Listen for incoming connections
  if (listen(server_fd, max_clients) == -1) {
    perror("listen");
    close(server_fd);
    exit(EXIT_FAILURE);
  }

  // Create epoll instance
  if ((epoll_fd = epoll_create1(0)) == -1) {
    perror("epoll_create1");
    close(server_fd);
    exit(EXIT_FAILURE);
  }

  // Add server socket to epoll
  ev.events = EPOLLIN;
  ev.data.fd = server_fd;
  if (epoll_ctl(epoll_fd, EPOLL_CTL_ADD, server_fd, &ev) == -1) {
    perror("epoll_ctl: server_fd");
    close(server_fd);
    close(epoll_fd);
    exit(EXIT_FAILURE);
  }

  // Add signal fd to epoll
  ev.events = EPOLLIN;
  ev.data.fd = signal_fd;
  if (epoll_ctl(epoll_fd, EPOLL_CTL_ADD, signal_fd, &ev) == -1) {
    perror("epoll_ctl: signal_fd");
    close(server_fd);
    close(epoll_fd);
    close(signal_fd);
    exit(EXIT_FAILURE);
  }

  // Stay in the main dispatch loop until the signal or msg handler returns non-zero
  while (loop_active) {
    int nfds = epoll_wait(epoll_fd, events, max_clients, -1);
    if (nfds == -1) {
      perror("epoll_wait");
      close(server_fd);
      close(epoll_fd);
      close(signal_fd);
      exit(EXIT_FAILURE);
    }

    for (int n = 0; n < nfds; ++n) {
      if (events[n].data.fd == server_fd) {
        // Accept new client connection
        int client_fd = accept(server_fd, NULL, NULL);
        if (client_fd == -1) {
          perror("accept");
        } else {
          ev.events = EPOLLIN | EPOLLET;
          ev.data.fd = client_fd;
          if (epoll_ctl(epoll_fd, EPOLL_CTL_ADD, client_fd, &ev) == -1) {
            perror("epoll_ctl: client_fd");
            close(client_fd);
          }
        }
      } else if (events[n].data.fd == signal_fd) {
        // Handle signal
        struct signalfd_siginfo fdsi;
        ssize_t s = read(signal_fd, &fdsi, sizeof(fdsi));
        if (s != sizeof(fdsi)) {
          perror("read");
        }
        int sig_status = sig_handler(fdsi.ssi_signo);
        // Exit if handler returns non-zero
        if (sig_status != 0) {
          loop_active = false;
          // Exit status is failure if sig handler returned negative
          if (sig_status < 0) {
            status = EXIT_FAILURE;
          }
        }
      } else {
        // Handle client data
        printf("Accepted client on fd %d, server fd %d\n", events[n].data.fd, server_fd);

        struct iovec iov;
        struct msghdr msg;

        iov.iov_base = malloc(max_msg_size);
        iov.iov_len = max_msg_size;

        memset(&msg, 0, sizeof(msg));
        msg.msg_iov = &iov;
        msg.msg_iovlen = 1;

        ssize_t bytes_read = recvmsg(events[n].data.fd, &msg, 0);


        int msg_status = msg_handler(events[n].data.fd, iov.iov_base, (int)bytes_read);
        free(iov.iov_base);

        // Exit if handler returns non-zero
        if (msg_status != 0) {
          loop_active = false;
          // Exit status is failure if msg handler returned negative
          if (msg_status < 0) {
            status = EXIT_FAILURE;
          }
        }
      }
    }
  }

  // Shut down all sockets and signal listeners
  close(server_fd);
  close(epoll_fd);
  close(signal_fd);
  free(events);
  return status;
}

// Connect to server socket, returns file descriptor or -1 on error
int connect_to_server()
{
  int client_fd = -1;
  struct sockaddr_un addr;

  do {
    // Create client socket
    if ((client_fd = socket(AF_UNIX, SOCK_SEQPACKET, 0)) == -1) {
      perror("socket");
      break;
    }
    // Set up the address structure
    memset(&addr, 0, sizeof(addr));
    addr.sun_family = AF_UNIX;
    strncpy(addr.sun_path, ABSTRACT_SOCKET_NAME, sizeof(addr.sun_path) - 1);
    // Connect to the server
    if (connect(client_fd, (struct sockaddr *)&addr, sizeof(addr)) == -1) {
      perror("connect");
      close(client_fd);
      break;
    }
    printf("Connected to server: client fd %d\n", client_fd);
  } while (0);

  return client_fd;
}

int send_recv_msg(int dest_fd, const void *send_msg, int send_size, void *rcv_msg, int max_recv_size)
{
  // Send a message to the server
  struct iovec iov;
  struct msghdr msg;
  int status = -1;

  do {
    iov.iov_base = (void *)send_msg;
    iov.iov_len = send_size;
    memset(&msg, 0, sizeof(msg));
    msg.msg_iov = &iov;
    msg.msg_iovlen = 1;
    if (sendmsg(dest_fd, &msg, 0) == -1) {
      perror("sendmsg");
      break;
    }

    // If no receive msg needed, just return now
    if (rcv_msg == NULL) {
      status = 0;
      break;
    }


    // Await a response
    struct iovec iov_recv;
    struct msghdr msg_recv;
    iov_recv.iov_base = rcv_msg;
    iov_recv.iov_len = max_recv_size;
    memset(&msg_recv, 0, sizeof(msg_recv));
    msg_recv.msg_iov = &iov_recv;
    msg_recv.msg_iovlen = 1;
    ssize_t bytes_read = recvmsg(dest_fd, &msg_recv, 0);
    if (bytes_read >= 0) {
      status = (int)bytes_read;
      if (bytes_read == 0) {
        printf("Server closed the connection\n");
      }
    } else {
      perror("recvmsg");
    }
  } while (0);

  return status;
}
