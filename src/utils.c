#include <netinet/in.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/select.h>
#include <sys/socket.h>
#include <termios.h>
#include <unistd.h>

#include "utils.h"
#include "log.h"

const char *ROLE_STR[] = {"Student", "TA"};

int stoi(const char *str, int *num) {
    *num = 0;
    int len = 0;
    while (*str <= ASCII_9 && *str >= ASCII_0) {
        *num = *num * 10 + (int)*str - ASCII_0;
        ++len;
        ++str;
    }
    return len;
}

int receive_buf(char *rbuf, int fd) {
    char log[MAX_SIZE_OF_LOG];
    int len;
    int rlen = recv(fd, rbuf, MAX_SIZE_OF_BUF, 0);
    if (rlen < 0) {
        len = sprintf(log, "Receiving message failed!\n");
        print_error(log, len);
        return rlen;
    }
    len = sprintf(log, "%d byte(s) was received from fd: %d!\n", rlen, fd);
    print_log(log, len);
    print_msg(rbuf, rlen, MESSAGE_IN);
    return rlen;
}

int receive_udp_buf(char *rbuf, int fd) {
    char log[MAX_SIZE_OF_LOG];
    int len;
    int rlen = recvfrom(fd, rbuf, MAX_SIZE_OF_BUF, 0, NULL, NULL);
    if (rlen < 0) {
        len = sprintf(log, "Receiving UDP message failed!\n");
        print_error(log, len);
        return rlen;
    }
    len = sprintf(log, "%d byte(s) was received from fd: %d over UDP!\n",
                  rlen, fd);
    print_log(log, len);
    print_msg(rbuf, rlen, MESSAGE_IN);
    return rlen;
}

int send_buf(const char *buf, int slen, int fd) {
    int tlen = send(fd, buf, slen, 0);
    char log[MAX_SIZE_OF_LOG];
    int len;
    if (tlen < 0) {
        len = sprintf(log, "Sending message failed!\n");
        print_error(log, len);
        return tlen;
    }
    len = sprintf(log, "%d byte(s) was sent to fd: %d!\n", tlen, fd);
    print_log(log, len);
    print_msg(buf, tlen, MESSAGE_OUT);
    return tlen;
}

int send_udp_buf(const char *buf, int slen, client_t *client) {
    int tlen = sendto(client->udp_fd, buf, slen, 0,
                      (struct sockaddr *)&client->udp_sock_addr,
                      sizeof(struct sockaddr));
    char log[MAX_SIZE_OF_LOG];
    int len;
    if (tlen < 0) {
        len = sprintf(log, "Sending UDP message failed!\n");
        print_error(log, len);
        return tlen;
    }
    len = sprintf(log, "%d byte(s) was sent to fd: %d over UDP!\n",
                  tlen, client->udp_fd);
    print_log(log, len);
    print_msg(buf, tlen, MESSAGE_OUT);
    return tlen;
}

void clear_line() {
    write(STDOUT_FILENO, "\r\e[K", 4);
}

void close_endpoint(int status) {
    exit(status);
}

void close_udp_socket(client_t *client) {
    if (client->udp_fd >= 0) {
        close(client->udp_fd);
        client->udp_fd = -1;
    }
}
