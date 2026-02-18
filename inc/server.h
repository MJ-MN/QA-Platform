#ifndef __SERVER_H
#define __SERVER_H

#define MAX_LEN_OF_QUEUE 5

#define ROLE_TRY_MASK 0x00000001

#define SET_FIRST_ROLE_TRY(x) (x |= ROLE_TRY_MASK)
#define CLR_FIRST_ROLE_TRY(x) (x &= ~ROLE_TRY_MASK)
#define IS_FIRST_ROLE_TRY(x) !(x & ROLE_TRY_MASK)

typedef enum question_status {
    PENDING = 0,
    ANSWERING,
    ANSWERED
} question_status;

typedef struct question_t {
    struct question_t *next;
    char *q_str;
    char *a_str;
    int question_num;
    int asked_by;
    int answered_by;
    question_status status;
} question_t;

typedef struct server_struct_t {
    struct client_t **client_list;
    struct client_t *client;
    struct question_t **question_list;
    int server_fd;
} server_struct_t;

void *process_stdin_fd(void *arg);
int setup_server(int port);
int create_tcp_socket();
void bind_tcp_port(int server_fd, int port);
void listen_port(int server_fd);
int config_timer(fd_set *wfd_set, int udp_fd);
void process_server_fd(server_struct_t *server_struct);
client_t *accept_client(server_struct_t *server_struct);
client_t *add_new_client(server_struct_t *server_struct, int client_fd);
void process_timer_fd(server_struct_t *server_c_struct);
void *process_broadcast_msg(void *arg);
void *process_client_fd(void *arg);
void process_msg(server_struct_t *server_c_struct, const char *rbuf, int rlen);
client_t *find_client_by_fd(client_t *c_list, int client_fd);
void remove_client(server_struct_t *server_c_struct);
void remove_node(server_struct_t *server_c_struct);
void set_role(const char *rbuf, int rlen, client_t *client);
void assign_role(char role, client_t *client);
void ask_question(const char *rbuf, int rlen,
                  server_struct_t *server_c_struct);
void add_new_question(const char *rbuf, int rlen,
                      server_struct_t *server_c_struct);
void get_questions_list(client_t *client, question_t *q_list);
void send_question(client_t *client, question_t *q_list, char *tbuf,
                   int *tlen, question_status qn_status);
void select_question(const char *rbuf, server_struct_t *server_c_struct);
int process_select_question(server_struct_t *server_c_struct,
                            char *tbuf, int q_num);
question_t *find_question_by_number(question_t *q_list, int question_num);
int process_connection(server_struct_t *server_c_struct, question_t *question,
                       char *tbuf);
void setup_udp_connection(client_t *client, int port);
int create_udp_socket();
int bind_udp_port(int fd, int port);
void set_question_status(const char *rbuf, int rlen, client_t *client,
                         question_t *q_list);
int process_question_answer(const char *rbuf, int rlen, char *tbuf,
                            client_t *client, question_t *q_list);
void get_sessions_list(client_t *client, question_t *q_list);
void attend_session(const char *rbuf, server_struct_t *server_c_struct);
int process_attend_session(client_t *c_list, question_t *question,
                           char *tbuf);
void free_mem(server_struct_t *server_struct);
void process_stdin(char *buf, int len, server_struct_t *server_struct);

#endif /* __SERVER_H */
