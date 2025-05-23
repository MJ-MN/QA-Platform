#ifndef __CLINET_H
#define __CLIENT_H

int setup_client();
void connect_to_server(int server_fd, int port);
void monitor_fds(int *max_fd, fd_set *working_fd_set, fd_set *temp_fd_set,
                 int server_fd);
void process_ready_fds(int fd, fd_set *temp_fd_set,
                       int server_fd);
void process_server_fd(int fd, fd_set *temp_fd_set);
void process_msg(const char *rbuf, int rlen,
                 int server_fd, fd_set *temp_fd_set);
void remove_server(int server_fd, fd_set *temp_fd_set);
void process_stdin(char *buf, int rlen, int fd);
void print_man();
int set_role(char *buf, int len);

#endif /* __CLIENT_H */
