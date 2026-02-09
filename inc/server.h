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

int setup_server(int port);
int create_tcp_socket();
void bind_tcp_port(int server_fd, int port);
void listen_port(int server_fd);
void monitor_fds(client_t **c_list, question_t **q_list, int *max_fd,
                 fd_set *working_fd_set, fd_set *temp_fd_set, int server_fd);
void process_ready_fds(client_t **c_list, question_t **q_list, int fd,
                       int *max_fd, fd_set *temp_fd_set, int server_fd);
void process_server_fd(client_t **c_list, int *max_fd,
                       fd_set *temp_fd_set, int client_fd);
int accept_client(client_t **c_list, int server_fd);
void add_new_client(client_t **c_list, int client_fd);
void process_broadcast_msg(client_t *client);
void process_client_fd(client_t **c_list, question_t **q_list,
                       client_t *client, int *max_fd, fd_set *temp_fd_set);
void process_msg(client_t **c_list, question_t **q_list, client_t *client,
                 const char *rbuf, int rlen, int *max_fd, fd_set *temp_fd_set);
client_t *find_client_by_fd(client_t *c_list, int client_fd);
void remove_client(client_t **c_list, client_t *client, fd_set *temp_fd_set);
void remove_node(client_t **list, client_t *client);
void set_role(const char *rbuf, int rlen, client_t *client);
void assign_role(char role, client_t *client);
void ask_question(const char *rbuf, int rlen, client_t *client,
                  question_t **q_list);
void add_new_question(const char *rbuf, int rlen, client_t *client,
                      question_t **q_list);
void get_questions_list(client_t *client, question_t *q_list);
void send_question(client_t *client, question_t *q_list,
                   char *tbuf, int *tlen);
void select_question(const char *rbuf, client_t *client, question_t *q_list,
                     int *max_fd, fd_set *temp_fd_set);
int process_select_question(client_t *client, question_t *q_list, char *tbuf,
                            int q_num, int *max_fd, fd_set *temp_fd_set);
question_t *find_question_by_number(question_t *q_list, int question_num);
int process_connection(client_t *client, question_t *question, char *tbuf,
                       int *max_fd, fd_set *temp_fd_set);
void setup_udp_connection(client_t *client, int port);
int create_udp_socket();
int bind_udp_port(int fd, int port);
void set_question_status(const char *rbuf, int rlen, client_t *client,
                         question_t *q_list);
int process_question_answer(const char *rbuf, int rlen, char *tbuf,
                            client_t *client, question_t *q_list);
void free_mem(client_t **c_list, question_t **q_list, fd_set *temp_fd_set);
void process_stdin(char *buf, int len, int fd, client_t **c_list,
                   question_t **q_list, fd_set *temp_fd_set);

#endif /* __SERVER_H */
