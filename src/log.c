#include <string.h>
#include <unistd.h>

#include "log.h"
#include "utils.h"

void print_log(const char *msg, int len) {
    clear_line();
    write(STDOUT_FILENO, msg, len);
}

void print_success(const char *msg, int len) {
    clear_line();
    write(STDOUT_FILENO, ANSI_COLOR_GRN, ANSI_CODE_LEN);
    write(STDOUT_FILENO, msg, len);
    write(STDOUT_FILENO, ANSI_RESET_ALL, ANSI_CODE_LEN);
}

void print_error(const char *msg, int len) {
    clear_line();
    write(STDOUT_FILENO, ANSI_COLOR_RED, ANSI_CODE_LEN);
    write(STDOUT_FILENO, msg, len);
    write(STDOUT_FILENO, ANSI_RESET_ALL, ANSI_CODE_LEN);
}

void print_warning(const char *msg, int len) {
    clear_line();
    write(STDOUT_FILENO, ANSI_COLOR_YLW, ANSI_CODE_LEN);
    write(STDOUT_FILENO, msg, len);
    write(STDOUT_FILENO, ANSI_RESET_ALL, ANSI_CODE_LEN);
}

void print_info(const char *msg, int len) {
    clear_line();
    write(STDOUT_FILENO, ANSI_COLOR_BLU, ANSI_CODE_LEN);
    write(STDOUT_FILENO, msg, len);
    write(STDOUT_FILENO, ANSI_RESET_ALL, ANSI_CODE_LEN);
}

void print_msg(const char *buf, int len, int inout) {
    clear_line();
    if (inout == MESSAGE_IN) {
        write(STDOUT_FILENO, ">> ", 3);
        write(STDOUT_FILENO, ANSI_COLOR_MGN, ANSI_CODE_LEN);
    } else {
        write(STDOUT_FILENO, "<< ", 3);
    }
    write(STDOUT_FILENO, buf, len);
    write(STDOUT_FILENO, ANSI_RESET_ALL, ANSI_CODE_LEN);
    write(STDOUT_FILENO, "\n", 1);
}
