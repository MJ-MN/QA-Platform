#ifndef __CLINET_H
#define __CLIENT_H

int setup_client();
void connect_to_server(int server_fd, int port);
void monitor_fds(int *max_fd, fd_set *working_fd_set, fd_set *temp_fd_set,
                 client_t *client);
void process_ready_fds(int fd, int *max_fd, fd_set *temp_fd_set,
                       client_t *client);
void process_server_fd(int fd, int *max_fd, fd_set *temp_fd_set,
                       client_t *client);
void process_msg(const char *rbuf, int rlen, int server_fd,
                 int *max_fd, fd_set *temp_fd_set, client_t *client);
void remove_server(int server_fd, fd_set *temp_fd_set, client_t *client);
void process_connection(const char *buf, int server_fd,
                        int *max_fd, fd_set *temp_fd_set, client_t *client);
void setup_udp_connection(client_t *client, int port);
int create_udp_socket();
int bind_udp_port(int fd, int port);
void process_stdin(char *buf, int rlen, client_t *client);
void print_man();
int set_role(const char *buf, int rlen);
int ask_question(int rlen);
int select_question(const char *buf);
void send_to_broadcast(const char *buf, int rlen, client_t *client);

#endif /* __CLIENT_H */
