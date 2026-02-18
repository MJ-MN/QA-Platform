#include <netinet/in.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>

#include "log.h"
#include "utils.h"
#include "client.h"

static char term_buf[MAX_SIZE_OF_BUF] = {0};
pthread_mutex_t client_udp_mutex = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t client_question_mutex = PTHREAD_MUTEX_INITIALIZER;

int main(int argc, const char *argv[]) {
    char log[MAX_SIZE_OF_LOG];
    int len;
    if (argc < 2) {
        len = sprintf(log, "Usage: ./bin/client.out <port>\n");
        print_error(log, len); 
        close_endpoint(EXIT_FAILURE);
    }
    int port = atoi(argv[1]);
    client_t client = {0};
    client.udp_fd = -1;
    client.question_num = -1;
    client.tcp_fd = setup_client();
    pthread_t stdin_thread;
    pthread_create(&stdin_thread, NULL, process_stdin_fd, &client);
    pthread_detach(stdin_thread);
    connect_to_server(client.tcp_fd, port);
    write(STDOUT_FILENO, "<< ", 3);
    char rbuf[MAX_SIZE_OF_BUF];
    int rlen = 0;
    while (1) {
        rlen = receive_buf(rbuf, client.tcp_fd);
        if (rlen == 0) {
            remove_server(&client);
            write(STDOUT_FILENO, "<< ", 3);
            break;
        } else if (rlen > 0) {
            process_msg(rbuf, &client);
            write(STDOUT_FILENO, "<< ", 3);
        }
    }
    close(client.tcp_fd);
    close_udp_socket(&client);
    close_endpoint(EXIT_SUCCESS);
}

void *process_stdin_fd(void *arg) {
    client_t *client = (client_t *)arg;
    char ch;
    int rlen = 0;
    while (1) {
        read(STDIN_FILENO, &ch, 1);
        if (ch == DEL_CHAR) {
            rlen = (rlen > 0) ? rlen - 1 : 0;
        } else if (ch == '\n') {
            term_buf[rlen++] = ch;
            term_buf[rlen] = '\0';
            process_stdin(term_buf, rlen, client);
            rlen = 0;
        } else {
            term_buf[rlen++] = ch;
        }
    }
    return NULL;
}

int setup_client() {
    int client_fd = socket(AF_INET, SOCK_STREAM, PF_UNSPEC);
    return client_fd;
}

void connect_to_server(int server_fd, int port) {
    char log[MAX_SIZE_OF_LOG];
    int len = sprintf(log, "Connecting to server...\n");
    print_log(log, len);
    struct sockaddr_in socket_addr = {0};
    socket_addr.sin_family = AF_INET;
    socket_addr.sin_addr.s_addr = INADDR_ANY;
    socket_addr.sin_port = htons(port);
    if (connect(server_fd, (struct sockaddr *)&socket_addr,
                sizeof(socket_addr)) < 0) {
        len = sprintf(log, "Connecting to server failed!\n");
        print_error(log, len);
        close(server_fd);
        close_endpoint(EXIT_FAILURE);
    }
    len = sprintf(log, "Connected to server successfully!\n");
    print_success(log, len);
}

void *process_udp_fd(void *arg) {
    client_t *client = (client_t *)arg;
    char rbuf[MAX_SIZE_OF_BUF];
    while (1) {
        receive_udp_buf(rbuf, client->udp_fd);
        write(STDOUT_FILENO, "<< ", 3);
        if (strncmp(rbuf, CLS_UDP_SOCK, CLS_UDP_SOCK_LEN) == 0) {
            break;
        }
    }
    pthread_mutex_lock(&client_udp_mutex);
    close_udp_socket(client);
    pthread_mutex_unlock(&client_udp_mutex);
    return NULL;
}

void process_msg(const char *rbuf, client_t *client) {
    if (strncmp(rbuf, CONN_STAB, CONN_STAB_LEN) == 0) {
        if (process_connection(&rbuf[CONN_STAB_LEN], client)) {
            pthread_mutex_lock(&client_question_mutex);
            client->question_num = atoi(&rbuf[UDP_PORT_LEN + QN_NUMBER_LEN]);
            pthread_mutex_unlock(&client_question_mutex);
        }
    } else if (strncmp(rbuf, SESS_ON_PRG, SESS_ON_PRG_LEN) == 0) {
        process_connection(&rbuf[SESS_ON_PRG_LEN], client);
    } else {
        /* Do nothing */
    }
}

void remove_server(client_t *client) {
    char log[MAX_SIZE_OF_LOG];
    int len = sprintf(log, "Server connection was closed! fd: %d\n",
                      client->tcp_fd);
    print_info(log, len);
    close(client->tcp_fd);
    pthread_mutex_lock(&client_udp_mutex);
    close_udp_socket(client);
    pthread_mutex_unlock(&client_udp_mutex);
}

int process_connection(const char *buf, client_t *client) {
    int ret_val = RET_OK;
    setup_udp_connection(client, atoi(buf));
    if (client->udp_fd >= 0) {
        char log[MAX_SIZE_OF_LOG];
        int len = sprintf(log, "Connection created successfully!\n");
        print_success(log, len);
    } else {
        char tbuf[MAX_SIZE_OF_BUF];
        int tlen = sprintf(tbuf, "Connection cannot be stablished!");
        send_buf(tbuf, tlen, client->tcp_fd);
        ret_val = RET_ERR;
    }
    return ret_val;
}

void setup_udp_connection(client_t *client, int port) {
    int udp_fd = create_udp_socket();
    if (udp_fd < 0) {
        return;
    }
    if (bind_udp_port(udp_fd, port + 1) != RET_OK) {
        close(udp_fd);
        return;
    }
    client->udp_sock_addr.sin_family = AF_INET;
    client->udp_sock_addr.sin_port = htons(port);
    client->udp_sock_addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    client->udp_fd = udp_fd;
    pthread_t udp_thread;
    pthread_create(&udp_thread, NULL, process_udp_fd, client);
    pthread_detach(udp_thread);
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
             sizeof(struct sockaddr)) < 0) {
        len = sprintf(log, "Port binding failed!\n");
        print_error(log, len);
        return RET_ERR;
    }
    return RET_OK;
}

void process_stdin(char *buf, int rlen, client_t *client) {
    int need_send = 0;
    if (rlen > 1) {
        if (strcmp(buf, "exit\n") == 0) {
            close(client->tcp_fd);
            close_udp_socket(client);
            close_endpoint(EXIT_SUCCESS);
        } else if (strcmp(buf, "help\n") == 0) {
            print_man();
        } else if (strncmp(buf, SET_ROLE_CMD, SET_ROLE_CMD_LEN) == 0) {
            need_send = set_role(&buf[SET_ROLE_CMD_LEN],
                                 rlen - SET_ROLE_CMD_LEN);
        } else if (strncmp(buf, ASK_QN_CMD, ASK_QN_CMD_LEN) == 0) {
            need_send = ask_question(rlen - ASK_QN_CMD_LEN);
        } else if (strncmp(buf, GET_QN_LS_CMD, GET_QN_LS_CMD_LEN) == 0) {
            need_send = 1;
        } else if (strncmp(buf, SELECT_QN_CMD, SELECT_QN_CMD_LEN) == 0) {
            need_send = select_question(&buf[SELECT_QN_CMD_LEN]);
        } else if (strncmp(buf, UDP_MODE, UDP_MODE_LEN) == 0) {
            send_to_broadcast(&buf[UDP_MODE_LEN], rlen - UDP_MODE_LEN, client);
        } else if (strncmp(buf, SET_QN_STS_CMD, SET_QN_STS_CMD_LEN) == 0) {
            need_send = set_question_status(&buf[SET_QN_STS_CMD_LEN],
                                            rlen - SET_QN_STS_CMD_LEN, client);
        } else if (strncmp(buf, GET_SESS_LS_CMD, GET_SESS_LS_CMD_LEN) == 0) {
            need_send = 1;
        } else if (strncmp(buf, ATT_SESS_CMD, ATT_SESS_CMD_LEN) == 0) {
            need_send = attend_session(&buf[ATT_SESS_CMD_LEN],
                                       rlen - ATT_SESS_CMD_LEN);
        } else {
            char log[MAX_SIZE_OF_LOG];
            int len;
            len = sprintf(log, "Invalid command!\n");
            print_error(log, len);
        }
        if (need_send) {
            send_buf(buf, rlen - 1, client->tcp_fd);
        }
        buf[0] = '\0';
        write(STDOUT_FILENO, "<< ", 3);
    }
}

void print_man() {
    char log[MAX_SIZE_OF_LOG];
    int len;
    len = sprintf(log, SET_ROLE_CMD);
    len += sprintf(&log[SET_ROLE_CMD_LEN], "<role>\n");
    print_log(log, len);
    len = sprintf(log, ASK_QN_CMD);
    len += sprintf(&log[ASK_QN_CMD_LEN], "<question>\n");
    print_log(log, len);
    len = sprintf(log, GET_QN_LS_CMD);
    len += sprintf(&log[GET_QN_LS_CMD_LEN], "\n");
    print_log(log, len);
    len = sprintf(log, SELECT_QN_CMD);
    len += sprintf(&log[SELECT_QN_CMD_LEN], "<question_number>\n");
    print_log(log, len);
    len = sprintf(log, UDP_MODE);
    len += sprintf(&log[UDP_MODE_LEN], "<message>\n");
    print_log(log, len);
    len = sprintf(log, SET_QN_STS_CMD);
    len += sprintf(&log[SET_QN_STS_CMD_LEN],
                   "<question_number> <status> <answer>\n");
    print_log(log, len);
    len = sprintf(log, GET_SESS_LS_CMD);
    len += sprintf(&log[GET_SESS_LS_CMD_LEN], "\n");
    print_log(log, len);
    len = sprintf(log, ATT_SESS_CMD);
    len += sprintf(&log[ATT_SESS_CMD_LEN], "<id>\n");
    print_log(log, len);
}

int set_role(const char *buf, int rlen) {
    int ret_val = RET_ERR;
    char role = buf[0];
    if ((role == ROLE_STUDENT || role == ROLE_TA) && rlen == 2) {
        ret_val = RET_OK;
    } else {
        char log[MAX_SIZE_OF_LOG];
        int len;
        len = sprintf(log, "Invalid role!\n");
        print_error(log, len);
    }
    return ret_val;
}

int ask_question(int rlen) {
    int ret_val = RET_ERR;
    if (rlen > MIN_SIZE_OF_QA) {
        ret_val = RET_OK;
    } else {
        char log[MAX_SIZE_OF_LOG];
        int len;
        len = sprintf(log, "Too short question!\n");
        print_error(log, len);
    }
    return ret_val;
}

int select_question(const char *buf) {
    int ret_val = RET_OK;
    int i = 0;
    while (buf[i] != '\n') {
        if (buf[i] < ASCII_0 || buf[i] > ASCII_9) {
            char log[MAX_SIZE_OF_LOG];
            int len;
            len = sprintf(log, "Invalid question number!\n");
            print_error(log, len);
            ret_val = RET_ERR;
            break;
        }
        ++i;
    }
    return ret_val;
}

void send_to_broadcast(const char *buf, int rlen, client_t *client) {
    if (client->udp_fd >= 0) {
        pthread_mutex_lock(&client_udp_mutex);
        send_udp_buf(buf, rlen - 1, client);
        write(STDOUT_FILENO, "<< ", 3);
        pthread_mutex_unlock(&client_udp_mutex);
    } else {
        char log[MAX_SIZE_OF_LOG];
        int len;
        len = sprintf(log, "No UDP port found!\n");
        print_error(log, len);
    }
}

int set_question_status(const char *buf, int rlen, client_t *client) {
    int ret_val = RET_ERR;
    int qn_number = 0;
    int qn_number_len = stoi(buf, &qn_number);
    pthread_mutex_lock(&client_question_mutex);
    if (qn_number == client->question_num) {
        ret_val = check_question_answer(&buf[qn_number_len],
                                        rlen - qn_number_len);
    } else {
        char log[MAX_SIZE_OF_LOG];
        int len;
        len = sprintf(log, "This question is not selected!\n");
        print_error(log, len);
    }
    pthread_mutex_unlock(&client_question_mutex);
    return ret_val;
}

int check_question_answer(const char *buf, int rlen) {
    int ret_val = RET_ERR;
    if (strncmp(buf, QN_NOT_ANSWERED, QN_NOT_ANSWERED_LEN) == 0) {
        ret_val = RET_OK;
    } else if (strncmp(buf, QN_ANSWERED, QN_ANSWERED_LEN) == 0) {
        if (rlen - QN_ANSWERED_LEN > MIN_SIZE_OF_QA) {
            ret_val = RET_OK;
        } else {
            char log[MAX_SIZE_OF_LOG];
            int len;
            len = sprintf(log, "Too short answer!\n");
            print_error(log, len);
        }
    } else {
        char log[MAX_SIZE_OF_LOG];
        int len;
        len = sprintf(log, "Invalid status!\n");
        print_error(log, len);
    }
    return ret_val;
}

int attend_session(const char *buf, int rlen) {
    int ret_val = RET_ERR;
    int session_num;
    int session_num_len = stoi(buf, &session_num);
    if (session_num_len != rlen - 1) {
        char log[MAX_SIZE_OF_LOG];
        int len;
        len = sprintf(log, "Invalid session number!\n");
        print_error(log, len);
    } else {
        ret_val = RET_OK;
    }
    return ret_val;
}
