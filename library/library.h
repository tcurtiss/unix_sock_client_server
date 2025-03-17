// library.h : Header file for your target.

#pragma once

#ifdef __cplusplus
extern "C" {
#endif

  typedef int (*sig_handler_t)(int signum);
  typedef int (*msg_handler_t)(int client_fd, void *msg, int size);


  int dispatch_loop(int max_clients, sig_handler_t sig_handler,
                    msg_handler_t msg_handler, int max_msg_size);


  int connect_to_server();
  int send_recv_msg(int dest_fd, const void *send_msg, int send_size, void *rcv_msg, int max_recv_size);

#ifdef __cplusplus
}
#endif