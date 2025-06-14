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

typedef struct client_t {
    struct client_t *next;
    int fd;
    int retries;
    char role;
} client_t;

typedef struct question_t {
    char *q_str;
    char *a_str;
    int question_num;
    int asked_by;
    int answered_by;
    question_status status;
} question_t;

int setup_server(int port);
int create_socket();
void bind_port(int server_fd, int port);
void listen_port(int server_fd);
void monitor_fds(client_t **client_list, int *max_fd, fd_set *working_fd_set,
                 fd_set *temp_fd_set, int server_fd, int questions_fd);
void process_ready_fds(client_t **client_list, int fd, int *max_fd,
                       fd_set *temp_fd_set, int server_fd, int questions_fd);
void process_server_fd(client_t **client_list, int *max_fd,
                       fd_set *temp_fd_set, int client_fd);
int accept_client(client_t **client_list, int server_fd);
void add_new_client(client_t **client_list, int client_fd);
void process_client_fd(client_t **client_list, int fd,
                       fd_set *temp_fd_set, int questions_fd);
void process_msg(client_t **client_list, const char *rbuf, int rlen,
                 int client_fd, fd_set *temp_fd_set, int questions_fd);
client_t *find_client_by_fd(client_t *client_list, int client_fd);
void remove_client(client_t **client_list, int fd, fd_set *temp_fd_set);
void remove_node(client_t **list, int fd);
void set_role(const char *rbuf, int rlen, client_t *client);
void assign_role(char role, client_t *client);
void ask_question(const char *rbuf, client_t *client, int questions_fd);
void free_mem(client_t *list);
void process_stdin(char *buf, int len, client_t *client_list,
                   int questions_fd);

#endif /* __SERVER_H */
