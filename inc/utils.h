#ifndef __UTILS_H
#define __UTILS_H

#include <sys/select.h>

#define MAX_SIZE_OF_BUF 1024

#define RET_OK 1
#define RET_ERR 0

#define DEL_CHAR 127

#define SET_ROLE_CMD "set_role "
#define SET_ROLE_CMD_LEN 9
#define ASK_QN_CMD "ask_question "
#define ASK_QN_CMD_LEN 13
#define GET_QN_LS_CMD "get_questions_list "
#define GET_QN_LS_CMD_LEN 19
#define SET_QN_STS_CMD "set_question_status "
#define SET_QN_STS_CMD_LEN 20
#define GET_SESH_LS_CMD "get_sessions_list "
#define GET_SESH_LS_CMD_LEN 18
#define ATT_SESH_CMD "attend_session "
#define ATT_SESH_CMD_LEN 15

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

void enable_echo();
void disable_echo();
void fd_set_init(fd_set *temp_fd_set, int self_fd);
void process_stdin_fd(char *buf);
int receive_buf(char *rbuf, int fd);
int send_buf(const char *buf, int slen, int fd);
void echo_stdin(const char *buf, int len);
void clear_line();
void close_endpoint(int status);

#endif /* __UTILS_H */