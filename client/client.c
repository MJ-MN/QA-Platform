#include <netinet/in.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/select.h>
#include <sys/socket.h>
#include <unistd.h>

#include "log.h"
#include "client.h"
#include "utils.h"

static char term_buf[MAX_SIZE_OF_BUF] = {0};

int main(int argc, const char *argv[]) {
    char log[MAX_SIZE_OF_LOG];
    int len;
    disable_echo();
    if (argc < 2) {
        len = sprintf(log, "Invalid port!\n");
        print_error(log, len); 
        close_endpoint(EXIT_FAILURE);
    }
    int port = atoi(argv[1]);
    int server_fd = setup_client();
    connect_to_server(server_fd, port);
    fd_set working_fd_set, temp_fd_set;
    fd_set_init(&temp_fd_set, server_fd);
    int max_fd = server_fd;
    echo_stdin("", 0);
    while (1) {
        working_fd_set = temp_fd_set;
        select(max_fd + 1, &working_fd_set, NULL, NULL, NULL);
        monitor_fds(&max_fd, &working_fd_set, &temp_fd_set, server_fd);
    }
    return EXIT_SUCCESS;
}

int setup_client() {
    int client_fd = socket(AF_INET, SOCK_STREAM, PF_UNSPEC);
    return client_fd;
}

void connect_to_server(int server_fd, int port) {
    char log[MAX_SIZE_OF_LOG];
    int len = sprintf(log, "Connecting to server...\n");
    print_log(log, len);
    struct sockaddr_in socket_addr;
    socket_addr.sin_family = AF_INET;
    socket_addr.sin_addr.s_addr = INADDR_ANY;
    socket_addr.sin_port = htons(port);
    if (connect(server_fd, (struct sockaddr *)&socket_addr,
                sizeof(socket_addr)) < 0) {
        len = sprintf(log, "Connecting to server failed!\n");
        print_error(log, len);
        close_endpoint(EXIT_FAILURE);
    }
    len = sprintf(log, "Connected to server successfully!\n");
    print_success(log, len);
}

void monitor_fds(int *max_fd, fd_set *working_fd_set,
                 fd_set *temp_fd_set, int server_fd) {
    for (int i = 0; i <= *max_fd; ++i) {
        if (FD_ISSET(i, working_fd_set)) {
            process_ready_fds(i, temp_fd_set, server_fd);
        }
    }
}

void process_ready_fds(int fd, fd_set *temp_fd_set, int server_fd) {
    if (fd == STDIN_FILENO) {
        process_stdin_fd(term_buf);
    } else {
        process_server_fd(fd, temp_fd_set);
    }
    int len = strlen(term_buf);
    echo_stdin(term_buf, len);
    process_stdin(term_buf, len, server_fd);
}

void process_server_fd(int fd, fd_set *temp_fd_set) {
    char rbuf[MAX_SIZE_OF_BUF];
    int rlen = receive_buf(rbuf, fd);
    if (rlen >= 0) {
        process_msg(rbuf, rlen, fd, temp_fd_set);
    }
}

void process_msg(const char *rbuf, int rlen,
                 int server_fd, fd_set *temp_fd_set) {
    if (rlen == 0) {
        remove_server(server_fd, temp_fd_set);
    } else {
        print_msg(rbuf, rlen, MESSAGE_IN);
    }
}

void remove_server(int server_fd, fd_set *temp_fd_set) {
    char log[MAX_SIZE_OF_LOG];
    int len = sprintf(log, "Server connection was closed! fd: %d\n",
                      server_fd);
    print_info(log, len);
    close(server_fd);
    FD_CLR(server_fd, temp_fd_set);
}

void process_stdin(char *buf, int rlen, int fd) {
    int need_send = 0;
    if (rlen > 1 && buf[rlen - 1] == '\n') {
        if (strcmp(buf, "exit\n") == 0) {
            close_endpoint(EXIT_SUCCESS);
        } else if (strcmp(buf, "help\n") == 0) {
            print_man();
        } else if (strncmp(buf, SET_ROLE_CMD, SET_ROLE_CMD_LEN) == 0) {
            need_send = set_role(&buf[SET_ROLE_CMD_LEN],
                                 rlen - SET_ROLE_CMD_LEN);
        } else if (strncmp(buf, ASK_QN_CMD, ASK_QN_CMD_LEN) == 0) {
            need_send = ask_question(rlen - ASK_QN_CMD_LEN);
        } else {
            char log[MAX_SIZE_OF_LOG];
            int len;
            len = sprintf(log, "Invalid command!\n");
            print_error(log, len);
        }
        if (need_send) {
            send_buf(buf, rlen - 1, fd);
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
    len = sprintf(log, SET_QN_STS_CMD);
    len += sprintf(&log[SET_QN_STS_CMD_LEN], "<status>\n");
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
