#include <netinet/in.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/select.h>
#include <sys/socket.h>
#include <unistd.h>

#include "log.h"
#include "utils.h"
#include "client.h"

static char term_buf[MAX_SIZE_OF_BUF] = {0};

int main(int argc, const char *argv[]) {
    char log[MAX_SIZE_OF_LOG];
    int len;
    disable_echo();
    if (argc < 2) {
        len = sprintf(log, "Usage: ./bin/client.out <port>\n");
        print_error(log, len); 
        close_endpoint(EXIT_FAILURE);
    }
    int port = atoi(argv[1]);
    client_t client = {0};
    client.udp_fd = -1;
    client.tcp_fd = setup_client();
    connect_to_server(client.tcp_fd, port);
    fd_set working_fd_set, temp_fd_set;
    fd_set_init(&temp_fd_set, client.tcp_fd);
    int max_fd = client.tcp_fd;
    echo_stdin("", 0);
    while (1) {
        working_fd_set = temp_fd_set;
        select(max_fd + 1, &working_fd_set, NULL, NULL, NULL);
        monitor_fds(&max_fd, &working_fd_set, &temp_fd_set, &client);
    }
    FD_CLR(client.tcp_fd, &temp_fd_set);
    close(client.tcp_fd);
    close_udp_socket(&client, &temp_fd_set);
    close_endpoint(EXIT_SUCCESS);
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

void monitor_fds(int *max_fd, fd_set *working_fd_set, fd_set *temp_fd_set,
                 client_t *client) {
    for (int i = 0; i <= *max_fd; ++i) {
        if (FD_ISSET(i, working_fd_set)) {
            process_ready_fds(i, max_fd, temp_fd_set, client);
        }
    }
}

void process_ready_fds(int fd, int *max_fd, fd_set *temp_fd_set,
                       client_t *client) {
    if (fd == STDIN_FILENO) {
        process_stdin_fd(term_buf);
    } else {
        process_server_fd(fd, max_fd, temp_fd_set, client);
    }
    int len = strlen(term_buf);
    echo_stdin(term_buf, len);
    process_stdin(term_buf, len, client, temp_fd_set);
}

void process_server_fd(int fd, int *max_fd, fd_set *temp_fd_set,
                       client_t *client) {
    char rbuf[MAX_SIZE_OF_BUF];
    int rlen;
    if (client->udp_fd == fd) {
        rlen = receive_udp_buf(rbuf, fd);
    } else {
        rlen = receive_buf(rbuf, fd);
        if (rlen >= 0) {
            process_msg(rbuf, rlen, fd, max_fd, temp_fd_set, client);
        }
    }
}

void process_msg(const char *rbuf, int rlen, int server_fd,
                 int *max_fd, fd_set *temp_fd_set, client_t *client) {
    if (rlen == 0) {
        remove_server(server_fd, temp_fd_set, client);
    } else if (strncmp(rbuf, CONN_STAB, CONN_STAB_LEN) == 0) {
        process_connection(&rbuf[CONN_STAB_LEN], server_fd, max_fd,
                           temp_fd_set, client);
    } else {

    }
}

void remove_server(int server_fd, fd_set *temp_fd_set, client_t *client) {
    char log[MAX_SIZE_OF_LOG];
    int len = sprintf(log, "Server connection was closed! fd: %d\n",
                      server_fd);
    print_info(log, len);
    FD_CLR(client->tcp_fd, temp_fd_set);
    close(client->tcp_fd);
    close_udp_socket(client, temp_fd_set);
}

void process_connection(const char *buf, int server_fd, int *max_fd,
                        fd_set *temp_fd_set, client_t *client) {
    int port = 0;
    int port_len = stoi(buf, &port);
    setup_udp_connection(client, port);
    if (client->udp_fd >= 0) {
        FD_SET(client->udp_fd, temp_fd_set);
        if (client->udp_fd > *max_fd) {
            *max_fd = client->udp_fd;
        }
        client->question_num = atoi(&buf[port_len + QN_NUMBER_LEN]);
        char log[MAX_SIZE_OF_LOG];
        int len = sprintf(log, "Connection created successfully!\n");
        print_success(log, len);
    } else {
        char tbuf[MAX_SIZE_OF_BUF];
        int tlen = sprintf(tbuf, "Connection cannot be stablished!");
        send_buf(tbuf, tlen, server_fd);
    }
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

void process_stdin(char *buf, int rlen, client_t *client,
                   fd_set *temp_fd_set) {
    int need_send = 0;
    if (rlen > 1 && buf[rlen - 1] == '\n') {
        if (strcmp(buf, "exit\n") == 0) {
            FD_CLR(client->tcp_fd, temp_fd_set);
            close(client->tcp_fd);
            close_udp_socket(client, temp_fd_set);
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
        } else if (strncmp(buf, GET_SESH_LS_CMD, GET_SESH_LS_CMD_LEN) == 0) {
            need_send = 1;
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
    len = sprintf(log, GET_SESH_LS_CMD);
    len += sprintf(&log[GET_SESH_LS_CMD_LEN], "\n");
    print_log(log, len);
    len = sprintf(log, ATT_SESH_CMD);
    len += sprintf(&log[ATT_SESH_CMD_LEN], "<id>\n");
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
        send_udp_buf(buf, rlen - 1, client);
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
    if (qn_number == client->question_num) {
        ret_val = check_question_answer(&buf[qn_number_len],
                                        rlen - qn_number_len);
    } else {
        char log[MAX_SIZE_OF_LOG];
        int len;
        len = sprintf(log, "This question is not selected!\n");
        print_error(log, len);
    }
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
