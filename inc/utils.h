#ifndef __UTILS_H
#define __UTILS_H

#define MAX_SIZE_OF_BUF 1024
#define MIN_SIZE_OF_QA 8
#define STARTING_UDP_PORT 50000
#define UDP_PORT_LEN 5

#define RET_OK 1
#define RET_ERR 0

#define DEL_CHAR 127
#define ASCII_0 48
#define ASCII_9 57

#define SET_ROLE_CMD "set_role "
#define SET_ROLE_CMD_LEN 9
#define ASK_QN_CMD "ask_question "
#define ASK_QN_CMD_LEN 13
#define GET_QN_LS_CMD "get_questions_list"
#define GET_QN_LS_CMD_LEN 18
#define SELECT_QN_CMD "select_question "
#define SELECT_QN_CMD_LEN 16
#define SET_QN_STS_CMD "set_question_status "
#define SET_QN_STS_CMD_LEN 20
#define GET_SESS_LS_CMD "get_sessions_list"
#define GET_SESS_LS_CMD_LEN 17
#define ATT_SESS_CMD "attend_session "
#define ATT_SESS_CMD_LEN 15

#define CONN_STAB "Connection was stablished on port "
#define CONN_STAB_LEN 34
#define QN_NUMBER " for question #"
#define QN_NUMBER_LEN 15
#define UDP_MODE "udp_mode: "
#define UDP_MODE_LEN 10
#define QN_NOT_ANSWERED " not_answered"
#define QN_NOT_ANSWERED_LEN 13
#define QN_ANSWERED " answered "
#define QN_ANSWERED_LEN 10

#define ROLE_NONE '0'
#define ROLE_STUDENT 'S'
#define ROLE_TA 'T'
extern const char *ROLE_STR[];

#define ANSI_RESET_ALL "\e[00m"
#define ANSI_COLOR_BLK "\e[30m"
#define ANSI_COLOR_RED "\e[31m"
#define ANSI_COLOR_GRN "\e[32m"
#define ANSI_COLOR_YLW "\e[33m"
#define ANSI_COLOR_BLU "\e[34m"
#define ANSI_COLOR_MGN "\e[35m"
#define ANSI_COLOR_CYN "\e[36m"
#define ANSI_COLOR_WHT "\e[37m"
#define ANSI_CODE_LEN 5

typedef struct client_t {
    struct sockaddr_in udp_sock_addr;
    struct client_t *next;
    int tcp_fd;
    int udp_fd;
    int retries;
    int question_num;
    char role;
} client_t;

void enable_echo();
void disable_echo();
int stoi(const char *str, int *num);
void fd_set_init(fd_set *temp_fd_set, int self_fd);
void process_stdin_fd(char *buf);
int receive_buf(char *rbuf, int fd);
int receive_udp_buf(char *rbuf, int fd);
int send_buf(const char *buf, int slen, int fd);
int send_udp_buf(const char *buf, int slen, client_t *client);
void echo_stdin(const char *buf, int len);
void clear_line();
void close_endpoint(int status);
void close_udp_socket(client_t *client, fd_set *temp_fd_set);

#endif /* __UTILS_H */