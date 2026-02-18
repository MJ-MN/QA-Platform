#define _POSIX_C_SOURCE 200809L
#include <fcntl.h>
#include <netinet/in.h>
#include <pthread.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/select.h>
#include <sys/signalfd.h>
#include <sys/socket.h>
#include <sys/timerfd.h>
#include <unistd.h>

#include "log.h"
#include "utils.h"
#include "server.h"

static char term_buf[MAX_SIZE_OF_BUF] = {0};
pthread_mutex_t client_udp_mutex = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t client_list_mutex = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t question_list_mutex = PTHREAD_MUTEX_INITIALIZER;

int main(int argc, const char *argv[]) {
    char log[MAX_SIZE_OF_LOG];
    int len;
    if (argc < 2) {
        len = sprintf(log, "Usage: ./bin/server.out <port>\n");
        print_error(log, len); 
        close_endpoint(EXIT_FAILURE);
    }
    int port = atoi(argv[1]);
    int server_fd = setup_server(port);
    client_t *client_list = NULL;
    question_t *question_list = NULL;
    server_struct_t server_struct = {
        .client_list = &client_list,
        .client = NULL,
        .question_list = &question_list,
        .server_fd = server_fd
    };
    pthread_t stdin_thread;
    pthread_create(&stdin_thread, NULL, process_stdin_fd, &server_struct);
    pthread_detach(stdin_thread);
    len = sprintf(log, "Server is running...\n");
    print_log(log, len);
    write(STDOUT_FILENO, "<< ", 3);
    while (1) {
        process_server_fd(&server_struct);
    }
    free_mem(&server_struct);
    close(server_fd);
    close_endpoint(EXIT_SUCCESS);
}

void *process_stdin_fd(void *arg) {
    server_struct_t *server_struct = (server_struct_t *)arg;
    char ch;
    int rlen = 0;
    while (1) {
        read(STDIN_FILENO, &ch, 1);
        if (ch == DEL_CHAR) {
            rlen = (rlen > 0) ? rlen - 1 : 0;
        } else if (ch == '\n') {
            rlen = (term_buf[rlen - 1] == ' ') ? rlen - 1 : rlen;
            term_buf[rlen++] = ch;
            process_stdin(term_buf, rlen, server_struct);
            rlen = 0;
        } else {
            term_buf[rlen++] = ch;
        }
    }
    return NULL;
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

int config_timer(fd_set *wfd_set, int udp_fd) {
    int timer_fd = timerfd_create(CLOCK_MONOTONIC, 0);
    struct itimerspec timer_spec = {0};
    timer_spec.it_value.tv_sec = 60;
    timerfd_settime(timer_fd, 0, &timer_spec, NULL);
    FD_ZERO(wfd_set);
    FD_SET(udp_fd, wfd_set);
    FD_SET(timer_fd, wfd_set);
    return timer_fd;
}

void process_server_fd(server_struct_t *server_struct) {
    client_t *new_client = accept_client(server_struct);
    server_struct_t *server_c_struct = malloc(sizeof(server_struct_t));
    server_c_struct->client_list = server_struct->client_list;
    server_c_struct->question_list = server_struct->question_list;
    server_c_struct->client = new_client;
    pthread_t tid;
    pthread_create(&tid, NULL, process_client_fd, server_c_struct);
    pthread_detach(tid);
}

client_t *accept_client(server_struct_t *server_struct) {
    char log[MAX_SIZE_OF_LOG];
    int len;
    struct sockaddr_in socket_addr;
    socklen_t socket_len = sizeof(struct sockaddr);
    int client_fd = accept(server_struct->server_fd,
                           (struct sockaddr *)&socket_addr, &socket_len);
    if (client_fd < 0) {
        len = sprintf(log, "Accepting client failed!\n");
        print_error(log, len);
    }
    len = sprintf(log, "New client was connected fd: %d!\n", client_fd);
    print_info(log, len);
    return add_new_client(server_struct, client_fd);
}

client_t *add_new_client(server_struct_t *server_struct, int client_fd) {
    client_t *new_client = malloc(sizeof(client_t));
    memset(new_client, 0, sizeof(client_t));
    new_client->tcp_fd = client_fd;
    new_client->udp_fd = -1;
    new_client->role = ROLE_NONE;
    new_client->question_num = -1;
    pthread_mutex_lock(&client_list_mutex);
    new_client->next = *server_struct->client_list;
    *server_struct->client_list = new_client;
    pthread_mutex_unlock(&client_list_mutex);
    return new_client;
}

void process_timer_fd(server_struct_t *server_c_struct) {
    question_t *question =
        find_question_by_number(*server_c_struct->question_list,
                                server_c_struct->client->question_num);
    question->status = PENDING;
    char tbuf[MAX_SIZE_OF_BUF];
    int tlen = sprintf(tbuf, CLS_UDP_SOCK);
    send_udp_buf(tbuf, tlen, server_c_struct->client);
    write(STDOUT_FILENO, "<< ", 3);
}

void *process_broadcast_msg(void *arg) {
    server_struct_t *server_c_struct = (server_struct_t *)arg;
    fd_set working_fd_set, temp_fd_set;
    int timer_fd = config_timer(&temp_fd_set, server_c_struct->client->udp_fd);
    int max_fd = timer_fd;
    while (1) {
        working_fd_set = temp_fd_set;
        select(max_fd + 1, &working_fd_set, NULL, NULL, NULL);
        if (FD_ISSET(timer_fd, &working_fd_set)) {
            process_timer_fd(server_c_struct);
            break;
        }
        if (FD_ISSET(server_c_struct->client->udp_fd, &working_fd_set)) {
            char rbuf[MAX_SIZE_OF_BUF];
            int rlen = receive_udp_buf(rbuf, server_c_struct->client->udp_fd);
            if (rlen >= 0) {
                rbuf[rlen] = '\0';
                send_udp_buf(rbuf, rlen, server_c_struct->client);
                write(STDOUT_FILENO, "<< ", 3);
            }
        }
    }
    close_udp_socket(server_c_struct->client);
    return NULL;
}

void *process_client_fd(void *arg) {
    server_struct_t *server_c_struct = (server_struct_t *)arg;
    char buf[MAX_SIZE_OF_BUF];
    int len = sprintf(buf, "Which one are you? (S)tudent/(T)A");
    send_buf(buf, len, server_c_struct->client->tcp_fd);
    write(STDOUT_FILENO, "<< ", 3);
    while (1) {
        len = receive_buf(buf, server_c_struct->client->tcp_fd);
        if (len == 0) {
            remove_client(server_c_struct);
            write(STDOUT_FILENO, "<< ", 3);
            break;
        } else if (len > 0) {
            buf[len] = '\0';
            process_msg(server_c_struct, buf, len);
            write(STDOUT_FILENO, "<< ", 3);
        }
    }
    free(server_c_struct);
    return NULL;
}

void process_msg(server_struct_t *server_c_struct, const char *rbuf, int rlen) {
    if (strncmp(rbuf, SET_ROLE_CMD, SET_ROLE_CMD_LEN) == 0) {
        set_role(&rbuf[SET_ROLE_CMD_LEN], rlen - SET_ROLE_CMD_LEN,
                 server_c_struct->client);
    } else if (strncmp(rbuf, ASK_QN_CMD, ASK_QN_CMD_LEN) == 0) {
        ask_question(&rbuf[ASK_QN_CMD_LEN], rlen - ASK_QN_CMD_LEN,
                     server_c_struct);
    } else if (strncmp(rbuf, GET_QN_LS_CMD, GET_QN_LS_CMD_LEN) == 0) {
        get_questions_list(server_c_struct->client,
                           *server_c_struct->question_list);
    } else if (strncmp(rbuf, SELECT_QN_CMD, SELECT_QN_CMD_LEN) == 0) {
        select_question(&rbuf[SELECT_QN_CMD_LEN], server_c_struct);
    } else if (strncmp(rbuf, SET_QN_STS_CMD, SET_QN_STS_CMD_LEN) == 0) {
        set_question_status(&rbuf[SET_QN_STS_CMD_LEN],
                            rlen - SET_QN_STS_CMD_LEN, server_c_struct->client,
                            *server_c_struct->question_list);
    } else if (strncmp(rbuf, GET_SESS_LS_CMD, GET_SESS_LS_CMD_LEN) == 0) {
        get_sessions_list(server_c_struct->client,
                          *server_c_struct->question_list);
    } else if (strncmp(rbuf, ATT_SESS_CMD, ATT_SESS_CMD_LEN) == 0) {
        attend_session(&rbuf[ATT_SESS_CMD_LEN], server_c_struct);
    } else {
        char tbuf[MAX_SIZE_OF_BUF];
        int tlen = sprintf(tbuf, "Invalid command!");
        send_buf(tbuf, tlen, server_c_struct->client->tcp_fd);
    }
}

client_t *find_client_by_fd(client_t *c_list, int client_fd) {
    pthread_mutex_lock(&client_list_mutex);
    while (c_list != NULL) {
        if (c_list->tcp_fd == client_fd ||
            c_list->udp_fd == client_fd) {
            break;
        }
        c_list = c_list->next;
    }
    pthread_mutex_unlock(&client_list_mutex);
    return c_list;
}

void remove_client(server_struct_t *server_c_struct) {
    close(server_c_struct->client->tcp_fd);
    close_udp_socket(server_c_struct->client);
    remove_node(server_c_struct);
    char log[MAX_SIZE_OF_LOG];
    int len = sprintf(log, "Client connection was closed fd: %d!\n",
                      server_c_struct->client->tcp_fd);
    print_info(log, len);
}

void remove_node(server_struct_t *server_c_struct) {
    pthread_mutex_lock(&client_list_mutex);
    client_t *curr_node = *server_c_struct->client_list;
    client_t *perv = NULL;
    while (curr_node != NULL) {
        if (curr_node == server_c_struct->client) {
            if (perv == NULL) {
                *server_c_struct->client_list = curr_node->next;
            } else {
                perv->next = curr_node->next;
            }
            free(curr_node);
            break;
        }
        perv = curr_node;
        curr_node = curr_node->next;
    }
    pthread_mutex_unlock(&client_list_mutex);
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

void ask_question(const char *rbuf, int rlen,
                  server_struct_t *server_c_struct) {
    char tbuf[MAX_SIZE_OF_BUF];
    int tlen;
    if (server_c_struct->client->role == ROLE_NONE) {
        tlen = sprintf(tbuf, "First, set your role!\nUsage: set_role <role>");
    } else if (server_c_struct->client->role == ROLE_TA) {
        tlen = sprintf(tbuf, "This command is for students!");
    } else {
        add_new_question(rbuf, rlen, server_c_struct);
        tlen = sprintf(tbuf, "Question registered!");
    }
    send_buf(tbuf, tlen, server_c_struct->client->tcp_fd);
}

void add_new_question(const char *rbuf, int rlen,
                      server_struct_t *server_c_struct) {
    static int question_num = 0;
    question_t *question = malloc(sizeof(question_t));
    question->question_num = question_num++;
    question->q_str = malloc(rlen);
    memcpy(question->q_str, rbuf, rlen);
    question->asked_by = server_c_struct->client->tcp_fd;
    question->status = PENDING;
    question->a_str = NULL;
    question->answered_by = -1;
    pthread_mutex_lock(&question_list_mutex);
    question->next = *server_c_struct->question_list;
    *server_c_struct->question_list = question;
    pthread_mutex_unlock(&question_list_mutex);
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
            pthread_mutex_lock(&question_list_mutex);
            q_list = q_list->next;
            pthread_mutex_unlock(&question_list_mutex);
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

void select_question(const char *rbuf, server_struct_t *server_c_struct) {
    char tbuf[MAX_SIZE_OF_BUF];
    int tlen;
    if (server_c_struct->client->role == ROLE_NONE) {
        tlen = sprintf(tbuf, "First, set your role!\nUsage: set_role <role>");
    } else if (server_c_struct->client->role == ROLE_STUDENT) {
        tlen = sprintf(tbuf, "This command is for TAs!");
    } else {
        tlen = process_select_question(server_c_struct, tbuf, atoi(rbuf));
    }
    send_buf(tbuf, tlen, server_c_struct->client->tcp_fd);
}

int process_select_question(server_struct_t *server_c_struct,
                            char *tbuf, int q_num) {
    int tlen;
    question_t *question =
        find_question_by_number(*server_c_struct->question_list, q_num);
    if (question != NULL) {
        if (question->status == PENDING) {
            tlen = process_connection(server_c_struct, question, tbuf);
        } else {
            tlen = sprintf(tbuf, "Question is not in pending state!");
        }
    } else {
        tlen = sprintf(tbuf, "Question not found!");
    }
    return tlen;
}

question_t *find_question_by_number(question_t *q_list, int question_num) {
    pthread_mutex_lock(&question_list_mutex);
    while (q_list != NULL) {
        if (q_list->question_num == question_num) {
            break;
        }
        q_list = q_list->next;
    }
    pthread_mutex_unlock(&question_list_mutex);
    return q_list;
}

int process_connection(server_struct_t *server_c_struct, question_t *question,
                       char *tbuf) {
    static int udp_port = STARTING_UDP_PORT;
    int tlen;
    setup_udp_connection(server_c_struct->client, udp_port);
    if (server_c_struct->client->udp_fd >= 0) {
        question->status = ANSWERING;
        question->answered_by = server_c_struct->client->tcp_fd;
        server_c_struct->client->question_num = question->question_num;
        pthread_t tid;
        pthread_create(&tid, NULL, process_broadcast_msg, server_c_struct);
        pthread_detach(tid);
        tlen = sprintf(tbuf, "%s%d%s%d!", CONN_STAB, udp_port,
                       QN_NUMBER, question->question_num);
        send_buf(tbuf, tlen, question->asked_by);
        udp_port += 2;
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
            pthread_mutex_lock(&question_list_mutex);
            q_list = q_list->next;
            pthread_mutex_unlock(&question_list_mutex);
        }
        --tlen;
    } else {
        tlen = sprintf(tbuf, "This command is for students!");
    }
    send_buf(tbuf, tlen, client->tcp_fd);
}

void attend_session(const char *rbuf, server_struct_t *server_c_struct) {
    char tbuf[MAX_SIZE_OF_BUF];
    int tlen;
    if (server_c_struct->client->role == ROLE_NONE) {
        tlen = sprintf(tbuf, "First, set your role!\nUsage: set_role <role>");
    } else if (server_c_struct->client->role == ROLE_STUDENT) {
        question_t *question =
            find_question_by_number(*server_c_struct->question_list,
                                    atoi(rbuf));
        tlen = process_attend_session(*server_c_struct->client_list, question, tbuf);
    } else {
        tlen = sprintf(tbuf, "This command is for students!");
    }
    send_buf(tbuf, tlen, server_c_struct->client->tcp_fd);
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

void free_mem(server_struct_t *server_struct) {
    client_t *c_next = NULL;
    pthread_mutex_lock(&client_list_mutex);
    while (*server_struct->client_list != NULL) {
        c_next = (*server_struct->client_list)->next;
        close((*server_struct->client_list)->tcp_fd);
        close_udp_socket(*server_struct->client_list);
        free(*server_struct->client_list);
        *server_struct->client_list = c_next;
    }
    pthread_mutex_unlock(&client_list_mutex);
    question_t *q_next = NULL;
    pthread_mutex_lock(&question_list_mutex);
    while (*server_struct->question_list != NULL) {
        q_next = (*server_struct->question_list)->next;
        if ((*server_struct->question_list)->a_str != NULL) {
            free((*server_struct->question_list)->a_str);
        }
        if ((*server_struct->question_list)->q_str != NULL) {
            free((*server_struct->question_list)->q_str);
        }
        free(*server_struct->question_list);
        *server_struct->question_list = q_next;
    }
    pthread_mutex_unlock(&question_list_mutex);
}

void process_stdin(char *buf, int len, server_struct_t *server_struct) {
    if (strcmp(buf, "exit\n") == 0) {
        free_mem(server_struct);
        close(server_struct->server_fd);
        close_endpoint(EXIT_SUCCESS);
    } else if (len > 0 && buf[len - 1] == '\n') {
        write(STDOUT_FILENO, "<< ", 3);
        buf[0] = '\0';
    }
}
