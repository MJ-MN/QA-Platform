#define _POSIX_C_SOURCE 200809L
#include <fcntl.h>
#include <netinet/in.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/select.h>
#include <sys/signalfd.h>
#include <sys/socket.h>
#include <time.h>
#include <unistd.h>

#include "log.h"
#include "utils.h"
#include "server.h"

static char term_buf[MAX_SIZE_OF_BUF] = {0};

int main(int argc, const char *argv[]) {
    char log[MAX_SIZE_OF_LOG];
    int len;
    disable_echo();
    if (argc < 2) {
        len = sprintf(log, "Usage: ./bin/server.out <port>\n");
        print_error(log, len); 
        close_endpoint(EXIT_FAILURE);
    }
    int port = atoi(argv[1]);
    int server_fd = setup_server(port);
    len = sprintf(log, "Server is running...\n");
    print_log(log, len);
    fd_set working_fd_set, temp_fd_set;
    fd_set_init(&temp_fd_set, server_fd);
    int signal_fd = config_signal_alarm(&temp_fd_set);
    int max_fd = signal_fd;
    client_t *client_list = NULL;
    question_t *question_list = NULL;
    echo_stdin("", 0);
    while (1) {
        working_fd_set = temp_fd_set;
        select(max_fd + 1, &working_fd_set, NULL, NULL, NULL);
        monitor_fds(&client_list, &question_list, &max_fd,
                    &working_fd_set, &temp_fd_set, server_fd, signal_fd);
    }
    free_mem(&client_list, &question_list, &temp_fd_set);
    close(server_fd);
    close_endpoint(EXIT_SUCCESS);
}

int setup_server(int port) {
    int server_fd = create_tcp_socket();
    bind_tcp_port(server_fd, port);
    listen_port(server_fd);
    char log[MAX_SIZE_OF_LOG];
    int len = sprintf(log, "Server created successfully!\n");
    print_success(log, len);
    return server_fd;
}

int create_tcp_socket() {
    char log[MAX_SIZE_OF_LOG];
    int len = sprintf(log, "Creating TCP socket...\n");
    print_log(log, len);
    int server_fd = socket(AF_INET, SOCK_STREAM, PF_UNSPEC);
    if (server_fd < 0) {
        len = sprintf(log, "Socket creation failed!\n");
        print_error(log, len);
        close_endpoint(EXIT_FAILURE);
    }
    int enable = 1;
    setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &enable, sizeof(int));
    return server_fd;
}

void bind_tcp_port(int server_fd, int port) {
    char log[MAX_SIZE_OF_LOG];
    int len = sprintf(log, "Binding to port %d...\n", port);
    print_log(log, len);
    struct sockaddr_in socket_addr = {0};
    socket_addr.sin_family = AF_INET;
    socket_addr.sin_addr.s_addr = INADDR_ANY;
    socket_addr.sin_port = htons(port);
    if (bind(server_fd, (struct sockaddr *)&socket_addr,
             sizeof(struct sockaddr)) < 0) {
        len = sprintf(log, "Port binding failed!\n");
        print_error(log, len);
        close(server_fd);
        close_endpoint(EXIT_FAILURE);
    }
}

void listen_port(int server_fd) {
    char log[MAX_SIZE_OF_LOG];
    int len = sprintf(log, "Preparing to listen to port...\n");
    print_log(log, len);
    if (listen(server_fd, MAX_LEN_OF_QUEUE) < 0) {
        len = sprintf(log, "Port listening failed!\n");
        print_error(log, len);
        close(server_fd);
        close_endpoint(EXIT_FAILURE);
    }
}

int config_signal_alarm(fd_set *temp_fd_set) {
    sigset_t sig_set;
    sigemptyset(&sig_set);
    sigaddset(&sig_set, SIGALRM);
    sigprocmask(SIG_BLOCK, &sig_set, NULL);
    int sig_fd = signalfd(-1, &sig_set, 0);
    FD_SET(sig_fd, temp_fd_set);
    return sig_fd;
}

void monitor_fds(client_t **c_list, question_t **q_list, int *max_fd,
                 fd_set *working_fd_set, fd_set *temp_fd_set,
                 int server_fd, int sig_fd) {
    for (int i = 0; i <= *max_fd; ++i) {
        if (FD_ISSET(i, working_fd_set)) {
            process_ready_fds(c_list, q_list, i, max_fd,
                              temp_fd_set, server_fd, sig_fd);
        }
    }
}

void process_ready_fds(client_t **c_list, question_t **q_list, int fd,
                       int *max_fd, fd_set *temp_fd_set,
                       int server_fd, int sig_fd) {
    client_t *client = find_client_by_fd(*c_list, fd);
    if (fd == STDIN_FILENO) {
        process_stdin_fd(term_buf);
    } else if (fd == server_fd) {
        process_server_fd(c_list, max_fd, temp_fd_set, server_fd);
    } else if (fd == sig_fd) {
        process_signal_fd(*q_list, sig_fd, temp_fd_set);
    } else if (client->tcp_fd == fd) {
        process_client_fd(c_list, q_list, client, max_fd, temp_fd_set);
    } else {
        process_broadcast_msg(client);
    }
    int len = strlen(term_buf);
    echo_stdin(term_buf, len);
    process_stdin(term_buf, len, server_fd, c_list, q_list, temp_fd_set);
}

void process_server_fd(client_t **c_list, int *max_fd,
                       fd_set *temp_fd_set, int server_fd) {
    int new_client_fd = accept_client(c_list, server_fd);
    FD_SET(new_client_fd, temp_fd_set);
    if (new_client_fd > *max_fd) {
        *max_fd = new_client_fd;
    }
    char tbuf[MAX_SIZE_OF_BUF];
    int tlen = sprintf(tbuf, "Which one are you? (S)tudent/(T)A");
    send_buf(tbuf, tlen, new_client_fd);
}

int accept_client(client_t **c_list, int server_fd) {
    char log[MAX_SIZE_OF_LOG];
    int len;
    struct sockaddr_in socket_addr;
    socklen_t socket_len = sizeof(struct sockaddr);
    int client_fd = accept(server_fd, (struct sockaddr *)&socket_addr,
                           &socket_len);
    if (client_fd < 0) {
        len = sprintf(log, "Accepting client failed!\n");
        print_error(log, len);
    }
    add_new_client(c_list, client_fd);
    len = sprintf(log, "New client was connected fd: %d!\n", client_fd);
    print_info(log, len);
    return client_fd;
}

void add_new_client(client_t **c_list, int client_fd) {
    client_t *new_client = malloc(sizeof(client_t));
    memset(new_client, 0, sizeof(client_t));
    new_client->tcp_fd = client_fd;
    new_client->udp_fd = -1;
    new_client->role = ROLE_NONE;
    new_client->question_num = -1;
    new_client->next = *c_list;
    *c_list = new_client;
}

void process_signal_fd(question_t *q_list, int sig_fd, fd_set *temp_fd_set) {
    struct signalfd_siginfo sig_info;
    read(sig_fd, &sig_info, sizeof(sig_info));
    if (sig_info.ssi_signo == SIGALRM) {
        client_t *client = (client_t *)sig_info.ssi_ptr;
        question_t *question = find_question_by_number(q_list,
                                                       client->question_num);
        question->status = PENDING;
        char tbuf[MAX_SIZE_OF_BUF];
        int tlen = sprintf(tbuf, CLS_UDP_SOCK);
        send_udp_buf(tbuf, tlen, client);
        close_udp_socket(client, temp_fd_set);
    } else {
        char log[MAX_SIZE_OF_LOG];
        int len = sprintf(log, "Unknown signal received!\n");
        print_log(log, len);
    }
}

void process_broadcast_msg(client_t *client) {
    char rbuf[MAX_SIZE_OF_BUF];
    int rlen = receive_udp_buf(rbuf, client->udp_fd);
    if (rlen >= 0) {
        rbuf[rlen] = '\0';
        send_udp_buf(rbuf, rlen, client);
    }
}

void process_client_fd(client_t **c_list, question_t **q_list,
                       client_t *client, int *max_fd, fd_set *temp_fd_set) {
    char rbuf[MAX_SIZE_OF_BUF];
    int rlen = receive_buf(rbuf, client->tcp_fd);
    if (rlen >= 0) {
        rbuf[rlen] = '\0';
        process_msg(c_list, q_list, client, rbuf,
                    rlen, max_fd, temp_fd_set);
    }
}

void process_msg(client_t **c_list, question_t **q_list, client_t *client,
                 const char *rbuf, int rlen, int *max_fd,
                 fd_set *temp_fd_set) {
    if (rlen == 0) {
        remove_client(c_list, client, temp_fd_set);
    } else if (strncmp(rbuf, SET_ROLE_CMD, SET_ROLE_CMD_LEN) == 0) {
        set_role(&rbuf[SET_ROLE_CMD_LEN], rlen - SET_ROLE_CMD_LEN, client);
    } else if (strncmp(rbuf, ASK_QN_CMD, ASK_QN_CMD_LEN) == 0) {
        ask_question(&rbuf[ASK_QN_CMD_LEN], rlen - ASK_QN_CMD_LEN,
                     client, q_list);
    } else if (strncmp(rbuf, GET_QN_LS_CMD, GET_QN_LS_CMD_LEN) == 0) {
        get_questions_list(client, *q_list);
    } else if (strncmp(rbuf, SELECT_QN_CMD, SELECT_QN_CMD_LEN) == 0) {
        select_question(&rbuf[SELECT_QN_CMD_LEN], client,
                        *q_list, max_fd, temp_fd_set);
    } else if (strncmp(rbuf, SET_QN_STS_CMD, SET_QN_STS_CMD_LEN) == 0) {
        set_question_status(&rbuf[SET_QN_STS_CMD_LEN],
                            rlen - SET_QN_STS_CMD_LEN, client, *q_list);
    } else if (strncmp(rbuf, GET_SESS_LS_CMD, GET_SESS_LS_CMD_LEN) == 0) {
        get_sessions_list(client, *q_list);
    } else if (strncmp(rbuf, ATT_SESS_CMD, ATT_SESS_CMD_LEN) == 0) {
        attend_session(&rbuf[ATT_SESS_CMD_LEN], *c_list, client, *q_list);
    } else {
        char tbuf[MAX_SIZE_OF_BUF];
        int tlen = sprintf(tbuf, "Invalid command!");
        send_buf(tbuf, tlen, client->tcp_fd);
    }
}

client_t *find_client_by_fd(client_t *c_list, int client_fd) {
    while (c_list != NULL) {
        if (c_list->tcp_fd == client_fd ||
            c_list->udp_fd == client_fd) {
            return c_list;
        }
        c_list = c_list->next;
    }
    return NULL;
}

void remove_client(client_t **c_list, client_t *client, fd_set *temp_fd_set) {
    close(client->tcp_fd);
    FD_CLR(client->tcp_fd, temp_fd_set);
    close_udp_socket(client, temp_fd_set);
    remove_node(c_list, client);
    char log[MAX_SIZE_OF_LOG];
    int len = sprintf(log, "Client connection was closed fd: %d!\n",
                      client->tcp_fd);
    print_info(log, len);
}

void remove_node(client_t **list, client_t *client) {
    client_t *curr_node = *list;
    if (curr_node == client) {
        *list = curr_node->next;
        free(curr_node);
        return;
    }
    client_t *perv = NULL;
    while (curr_node != NULL) {
        if (curr_node == client) {
            perv->next = curr_node->next;
            free(curr_node);
            return;
        }
        perv = curr_node;
        curr_node = curr_node->next;
    }
}

void set_role(const char *rbuf, int rlen, client_t *client) {
    char role = rbuf[0];
    if ((role == ROLE_STUDENT || role == ROLE_TA) && rlen == 1) {
        assign_role(role, client);
    } else {
        char tbuf[MAX_SIZE_OF_BUF];
        int tlen = sprintf(tbuf, "Invalid role! ");
        tlen += sprintf(&tbuf[tlen], "Which one are you? (S)tudent/(T)A");
        send_buf(tbuf, tlen, client->tcp_fd);
    }
}

void assign_role(char role, client_t *client) {
    char tbuf[MAX_SIZE_OF_BUF];
    int tlen;
    if (client->role != ROLE_NONE && IS_FIRST_ROLE_TRY(client->retries)) {
        SET_FIRST_ROLE_TRY(client->retries);
        tlen = sprintf(tbuf, "Are you sure to change the role?\n");
        tlen += sprintf(&tbuf[tlen], "Enter again to confirm.");
        send_buf(tbuf, tlen, client->tcp_fd);
    } else {
        CLR_FIRST_ROLE_TRY(client->retries);
        client->role = role;
        char log[MAX_SIZE_OF_LOG];
        int len = sprintf(log, "Role assigned to %s for fd: %d!\n",
                          ROLE_STR[(role == ROLE_TA) ? 1 : 0], client->tcp_fd);
        print_info(log, len);
        tlen = sprintf(tbuf, "Role assigned to %s!",
                       ROLE_STR[(role == ROLE_TA) ? 1 : 0]);
        send_buf(tbuf, tlen, client->tcp_fd);
    }
}

void ask_question(const char *rbuf, int rlen, client_t *client,
                  question_t **q_list) {
    char tbuf[MAX_SIZE_OF_BUF];
    int tlen;
    if (client->role == ROLE_NONE) {
        tlen = sprintf(tbuf, "First, set your role!\nUsage: set_role <role>");
    } else if (client->role == ROLE_TA) {
        tlen = sprintf(tbuf, "This command is for students!");
    } else {
        add_new_question(rbuf, rlen, client, q_list);
        tlen = sprintf(tbuf, "Question registered!");
    }
    send_buf(tbuf, tlen, client->tcp_fd);
}

void add_new_question(const char *rbuf, int rlen, client_t *client,
                      question_t **q_list) {
    static int question_num = 0;
    question_t *question = malloc(sizeof(question_t));
    question->question_num = question_num++;
    question->q_str = malloc(rlen);
    memcpy(question->q_str, rbuf, rlen);
    question->asked_by = client->tcp_fd;
    question->status = PENDING;
    question->a_str = NULL;
    question->answered_by = -1;
    question->next = *q_list;
    *q_list = question;
}

void get_questions_list(client_t *client, question_t *q_list) {
    char tbuf[MAX_SIZE_OF_BUF];
    int tlen;
    if (client->role == ROLE_NONE) {
        tlen = sprintf(tbuf, "First, set your role!\nUsage: set_role <role>");
    } else if (client->role == ROLE_STUDENT) {
        tlen = sprintf(tbuf, "This command is for TAs!");
    } else {
        tlen = sprintf(tbuf, "List of questions:\n");
        while (q_list != NULL) {
            send_question(client, q_list, tbuf, &tlen, PENDING);
            q_list = q_list->next;
        }
        --tlen;
    }
    send_buf(tbuf, tlen, client->tcp_fd);
}

void send_question(client_t *client, question_t *qn, char *tbuf,
                   int *tlen, question_status qn_status) {
    if (qn->status == qn_status) {
        if(*tlen + strlen(qn->q_str) < MAX_SIZE_OF_BUF) {
            *tlen += sprintf(&tbuf[*tlen], "Q%d: %s\n", qn->question_num,
                             qn->q_str);
        } else {
            --*tlen;
            send_buf(tbuf, *tlen, client->tcp_fd);
            *tlen = sprintf(tbuf, "Q%d: %s\n", qn->question_num,
                            qn->q_str);
        }
    }
}

void select_question(const char *rbuf, client_t *client, question_t *q_list,
                     int *max_fd, fd_set *temp_fd_set) {
    char tbuf[MAX_SIZE_OF_BUF];
    int tlen;
    if (client->role == ROLE_NONE) {
        tlen = sprintf(tbuf, "First, set your role!\nUsage: set_role <role>");
    } else if (client->role == ROLE_STUDENT) {
        tlen = sprintf(tbuf, "This command is for TAs!");
    } else {
        tlen = process_select_question(client, q_list, tbuf, atoi(rbuf),
                                       max_fd, temp_fd_set);
    }
    send_buf(tbuf, tlen, client->tcp_fd);
}

int process_select_question(client_t *client, question_t *q_list, char *tbuf,
                            int q_num, int *max_fd, fd_set *temp_fd_set) {
    int tlen;
    question_t *question = find_question_by_number(q_list, q_num);
    if (question != NULL) {
        if (question->status == PENDING) {
            tlen = process_connection(client, question, tbuf, max_fd,
                                      temp_fd_set);
        } else {
            tlen = sprintf(tbuf, "Question is not in pending state!");
        }
    } else {
        tlen = sprintf(tbuf, "Question not found!");
    }
    return tlen;
}

question_t *find_question_by_number(question_t *q_list, int question_num) {
    while (q_list != NULL) {
        if (q_list->question_num == question_num) {
            return q_list;
        }
        q_list = q_list->next;
    }
    return NULL;
}

int process_connection(client_t *client, question_t *question, char *tbuf,
                       int *max_fd, fd_set *temp_fd_set) {
    static int udp_port = STARTING_UDP_PORT;
    int tlen;
    setup_udp_connection(client, udp_port);
    if (client->udp_fd >= 0) {
        question->status = ANSWERING;
        question->answered_by = client->tcp_fd;
        FD_SET(client->udp_fd, temp_fd_set);
        if (client->udp_fd > *max_fd) {
            *max_fd = client->udp_fd;
        }
        tlen = sprintf(tbuf, "%s%d%s%d!", CONN_STAB, udp_port,
                       QN_NUMBER, question->question_num);
        send_buf(tbuf, tlen, question->asked_by);
        udp_port += 2;
        set_alarm(client);
    } else {
        tlen = sprintf(tbuf, "Connection cannot be stablished!");
    }
    return tlen;
}

void setup_udp_connection(client_t *client, int port) {
    int udp_fd = create_udp_socket();
    if (udp_fd < 0) {
        return;
    }
    if (bind_udp_port(udp_fd, port) != RET_OK) {
        close(udp_fd);
        return;
    }
    client->udp_sock_addr.sin_family = AF_INET;
    client->udp_sock_addr.sin_port = htons(port + 1);
    client->udp_sock_addr.sin_addr.s_addr = htonl(INADDR_BROADCAST);
    client->udp_fd = udp_fd;
}

int create_udp_socket() {
    char log[MAX_SIZE_OF_LOG];
    int len = sprintf(log, "Creating UDP socket...\n");
    print_log(log, len);
    int udp_fd = socket(AF_INET, SOCK_DGRAM, PF_UNSPEC);
    if (udp_fd < 0) {
        len = sprintf(log, "Socket creation failed!\n");
        print_error(log, len);
        return -1;
    }
    const int enable = 1;
    setsockopt(udp_fd, SOL_SOCKET, SO_BROADCAST, &enable, sizeof(int));
    setsockopt(udp_fd, SOL_SOCKET, SO_REUSEADDR, &enable, sizeof(int));
    return udp_fd;
}

int bind_udp_port(int fd, int port) {
    char log[MAX_SIZE_OF_LOG];
    int len = sprintf(log, "Binding to port %d...\n", port);
    print_log(log, len);
    struct sockaddr_in socket_addr = {0};
    socket_addr.sin_family = AF_INET;
    socket_addr.sin_port = htons(port);
    socket_addr.sin_addr.s_addr = htonl(INADDR_ANY);
    if (bind(fd, (struct sockaddr *)&socket_addr,
             sizeof(struct sockaddr_in)) < 0) {
        len = sprintf(log, "Port binding failed!\n");
        print_error(log, len);
        return RET_ERR;
    }
    return RET_OK;
}

void set_alarm(client_t *client) {
    timer_t timer;
    struct sigevent sig_event = {0};
    sig_event.sigev_notify = SIGEV_SIGNAL;
    sig_event.sigev_signo  = SIGALRM;
    sig_event.sigev_value.sival_ptr = client;
    timer_create(CLOCK_REALTIME, &sig_event, &timer);
    struct itimerspec timer_spec = { .it_value.tv_sec = 60 };
    timer_settime(timer, 0, &timer_spec, NULL);
}

void set_question_status(const char *rbuf, int rlen, client_t *client,
                         question_t *q_list) {
    char tbuf[MAX_SIZE_OF_BUF];
    int tlen;
    if (client->role == ROLE_NONE) {
        tlen = sprintf(tbuf, "First, set your role!\nUsage: set_role <role>");
    } else if (client->role == ROLE_STUDENT) {
        tlen = process_question_answer(rbuf, rlen, tbuf, client, q_list);
    } else {
        tlen = sprintf(tbuf, "This command is for students!");
    }
    send_buf(tbuf, tlen, client->tcp_fd);
}

int process_question_answer(const char *rbuf, int rlen, char *tbuf,
                            client_t *client, question_t *q_list) {
    int tlen;
    int q_num_len = stoi(rbuf, &client->question_num);
    question_t *question = find_question_by_number(q_list,
                                                   client->question_num);
    if (question != NULL) {
        if (strncmp(&rbuf[q_num_len], QN_ANSWERED, QN_ANSWERED_LEN) == 0) {
            tlen = sprintf(tbuf, "Question status was set to answered!");
            question->a_str = malloc(rlen - q_num_len - QN_ANSWERED_LEN);
            memcpy(question->a_str, &rbuf[q_num_len + QN_ANSWERED_LEN],
                   rlen - QN_ANSWERED_LEN);
            client->question_num = -1;
            question->status = ANSWERED;
        } else if (strncmp(&rbuf[q_num_len], QN_NOT_ANSWERED,
                           QN_NOT_ANSWERED_LEN) == 0) {
            tlen = sprintf(tbuf, "Question status was set to pending!");
            client->question_num = -1;
            question->status = PENDING;
            question->answered_by = -1;
        } else {
            tlen = sprintf(tbuf, "Invalid status!\n");
            tlen += sprintf(tbuf, "%s <question_number> <status> <answer>",
                            SET_QN_STS_CMD);
        }
    } else {
        tlen = sprintf(tbuf, "Question not found!");
    }
    return tlen;
}

void get_sessions_list(client_t *client, question_t *q_list) {
    char tbuf[MAX_SIZE_OF_BUF];
    int tlen;
    if (client->role == ROLE_NONE) {
        tlen = sprintf(tbuf, "First, set your role!\nUsage: set_role <role>");
    } else if (client->role == ROLE_STUDENT) {
        tlen = sprintf(tbuf, "List of sessions & questions in progress:\n");
        while (q_list != NULL) {
            send_question(client, q_list, tbuf, &tlen, ANSWERING);
            q_list = q_list->next;
        }
        --tlen;
    } else {
        tlen = sprintf(tbuf, "This command is for students!");
    }
    send_buf(tbuf, tlen, client->tcp_fd);
}

void attend_session(const char *rbuf, client_t *c_list,
                    client_t *client, question_t *q_list) {
    char tbuf[MAX_SIZE_OF_BUF];
    int tlen;
    if (client->role == ROLE_NONE) {
        tlen = sprintf(tbuf, "First, set your role!\nUsage: set_role <role>");
    } else if (client->role == ROLE_STUDENT) {
        question_t *question = find_question_by_number(q_list, atoi(rbuf));
        tlen = process_attend_session(c_list, question, tbuf);
    } else {
        tlen = sprintf(tbuf, "This command is for students!");
    }
    send_buf(tbuf, tlen, client->tcp_fd);
}

int process_attend_session(client_t *c_list, question_t *question,
                           char *tbuf) {
    int tlen;
    if (question != NULL && question->status == ANSWERING) {
        client_t *client = find_client_by_fd(c_list, question->answered_by);
        tlen = sprintf(tbuf, "%s%d", SESS_ON_PRG,
                       ntohs(client->udp_sock_addr.sin_port) - 1);
    } else {
        tlen = sprintf(tbuf, "Question not found!");
    }
    return tlen;
}

void free_mem(client_t **c_list, question_t **q_list, fd_set *temp_fd_set) {
    client_t *c_next = NULL;
    while (*c_list != NULL) {
        c_next = (*c_list)->next;
        FD_CLR((*c_list)->tcp_fd, temp_fd_set);
        close((*c_list)->tcp_fd);
        close_udp_socket(*c_list, temp_fd_set);
        free(*c_list);
        *c_list = c_next;
    }
    question_t *q_next = NULL;
    while (*q_list != NULL) {
        q_next = (*q_list)->next;
        if ((*q_list)->a_str != NULL) {
            free((*q_list)->a_str);
        }
        if ((*q_list)->q_str != NULL) {
            free((*q_list)->q_str);
        }
        free(*q_list);
        *q_list = q_next;
    }
}

void process_stdin(char *buf, int len, int fd, client_t **c_list,
                   question_t **q_list, fd_set *temp_fd_set) {
    if (strcmp(buf, "exit\n") == 0) {
        free_mem(c_list, q_list, temp_fd_set);
        FD_CLR(fd, temp_fd_set);
        close(fd);
        close_endpoint(EXIT_SUCCESS);
    } else if (len > 0 && buf[len - 1] == '\n') {
        write(STDOUT_FILENO, "<< ", 3);
        buf[0] = '\0';
    }
}
