#ifndef __CLINET_H
#define __CLIENT_H

void *process_stdin_fd(void *arg);
int setup_client();
void connect_to_server(int server_fd, int port);
void *process_udp_fd(void *arg);
void process_msg(const char *rbuf, client_t *client);
void remove_server(client_t *client);
int process_connection(const char *buf, client_t *client);
void setup_udp_connection(client_t *client, int port);
int create_udp_socket();
int bind_udp_port(int fd, int port);
void process_stdin(char *buf, int rlen, client_t *client);
void print_man();
int set_role(const char *buf, int rlen);
int ask_question(int rlen);
int select_question(const char *buf);
void send_to_broadcast(const char *buf, int rlen, client_t *client);
int set_question_status(const char *buf, int rlen, client_t *client);
int check_question_answer(const char *buf, int rlen);
int attend_session(const char *buf, int rlen);

#endif /* __CLIENT_H */
