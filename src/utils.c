#include <sys/socket.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/select.h>
#include <termios.h>
#include <unistd.h>

#include "utils.h"
#include "log.h"

const char *ROLE_STR[] = {"Student", "TA"};

void enable_echo() {
    struct termios termios_s;
    tcgetattr(STDIN_FILENO, &termios_s);
    termios_s.c_lflag |= ICANON;
    termios_s.c_lflag |= ECHO;
    tcsetattr(STDIN_FILENO, TCSANOW, &termios_s);
}

void disable_echo() {
    struct termios termios_s;
    tcgetattr(STDIN_FILENO, &termios_s);
    termios_s.c_lflag &= ~ICANON;
    termios_s.c_lflag &= ~ECHO;
    tcsetattr(STDIN_FILENO, TCSANOW, &termios_s);
}

void fd_set_init(fd_set *temp_fd_set, int self_fd) {
    FD_ZERO(temp_fd_set);
    FD_SET(STDIN_FILENO, temp_fd_set);
    FD_SET(self_fd, temp_fd_set);
}

void process_stdin_fd(char *buf) {
    char ch;
    static int rlen = 0;
    if (read(STDIN_FILENO, &ch, 1) > 0) {
        if ((rlen == 0 && (ch == ' ' || ch == '\t' || ch == '\n')) ||
            (rlen > 0 && buf[rlen - 1] == ' ' && ch == ' ') ||
            ch == '\t') {
            /* Do nothing */
        } else if (ch == '\e') {
            read(STDIN_FILENO, &ch, 1);
            read(STDIN_FILENO, &ch, 1);
        } else if (ch == '\b' || ch == DEL_CHAR) {
            rlen = (rlen > 0) ? rlen - 1 : 0;
            buf[rlen] = '\0';
        } else if (ch == '\n') {
            if (buf[rlen - 1] == ' ') {
                --rlen;
            }
            buf[rlen++] = ch;
            buf[rlen] = '\0';
            rlen = 0;
        } else {
            buf[rlen++] = ch;
            buf[rlen] = '\0';
        }
    }
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
    return tlen;
}

void echo_stdin(const char *buf, int len) {
    clear_line();
    write(STDOUT_FILENO, "<< ", 3);
    write(STDOUT_FILENO, buf, len);
}

void clear_line() {
    write(STDOUT_FILENO, "\r\e[K", 4);
}

void close_endpoint(int status) {
    enable_echo();
    exit(status);
}
