#ifndef __LOG_H
#define __LOG_H

#define MAX_SIZE_OF_LOG 64

#define MESSAGE_IN 0
#define MESSAGE_OUT 1

void print_log(const char *msg, int len);
void print_success(const char *msg, int len);
void print_error(const char *msg, int len);
void print_warning(const char *msg, int len);
void print_info(const char *msg, int len);
void print_msg(const char *buf, int len, int inout);

#endif /* __LOG_H */
