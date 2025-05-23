#include <netinet/in.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/select.h>
#include <sys/socket.h>
#include <unistd.h>

#include "log.h"
#include "server.h"
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
    int server_fd = setup_server(port);
    len = sprintf(log, "Server is running...\n");
    print_log(log, len);
    fd_set working_fd_set, temp_fd_set;
    fd_set_init(&temp_fd_set, server_fd);
    int max_fd = server_fd;
    client_t *client_list = NULL;
    echo_stdin("", 0);
    while (1) {
        working_fd_set = temp_fd_set;
        select(max_fd + 1, &working_fd_set, NULL, NULL, NULL);
        monitor_fds(&client_list, &max_fd, &working_fd_set,
                    &temp_fd_set, server_fd);
    }
    free_mem(client_list);
    close_endpoint(EXIT_SUCCESS);
}

int setup_server(int port) {
    int server_fd = create_socket();
    bind_port(server_fd, port);
    listen_port(server_fd);
    char log[MAX_SIZE_OF_LOG];
    int len = sprintf(log, "Server created successfully!\n");
    print_success(log, len);
    return server_fd;
}

int create_socket() {
    char log[MAX_SIZE_OF_LOG];
    int len = sprintf(log, "Creating socket...\n");
    print_log(log, len);
    int server_fd = socket(AF_INET, SOCK_STREAM, PF_UNSPEC);
    if (server_fd < 0) {
        len = sprintf(log, "Socket creation failed!\n");
        print_error(log, len);
        close_endpoint(EXIT_FAILURE);
    }
    int opt = 1;
    setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));
    return server_fd;
}

void bind_port(int server_fd, int port) {
    char log[MAX_SIZE_OF_LOG];
    int len = sprintf(log, "Binding to port %d...\n", port);
    print_log(log, len);
    struct sockaddr_in socket_addr;
    socket_addr.sin_family = AF_INET;
    socket_addr.sin_addr.s_addr = INADDR_ANY;
    socket_addr.sin_port = htons(port);
    if (bind(server_fd, (struct sockaddr *)&socket_addr,
             sizeof(socket_addr)) < 0) {
        len = sprintf(log, "Port binding failed!\n");
        print_error(log, len);
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
        close_endpoint(EXIT_FAILURE);
    }
}

void monitor_fds(client_t **client_list, int *max_fd,
                 fd_set *working_fd_set, fd_set *temp_fd_set, int server_fd) {
    for (int i = 0; i <= *max_fd; ++i) {
        if (FD_ISSET(i, working_fd_set)) {
            process_ready_fds(client_list, i, max_fd,
                              temp_fd_set, server_fd);
        }
    }
}

void process_ready_fds(client_t **client_list, int fd,
                       int *max_fd, fd_set *temp_fd_set, int server_fd) {
    if (fd == STDIN_FILENO) {
        process_stdin_fd(term_buf);
    } else if (fd == server_fd) {
        process_server_fd(client_list, max_fd, temp_fd_set, server_fd);
    } else {
        process_client_fd(client_list, fd, temp_fd_set);
    }
    int len = strlen(term_buf);
    echo_stdin(term_buf, len);
    process_stdin(term_buf, len, *client_list);
}

void process_server_fd(client_t **client_list, int *max_fd,
                       fd_set *temp_fd_set, int server_fd) {
    int new_client_fd = accept_client(client_list, server_fd);
    FD_SET(new_client_fd, temp_fd_set);
    if (new_client_fd > *max_fd) {
        *max_fd = new_client_fd;
    }
    char tbuf[MAX_SIZE_OF_BUF];
    int tlen;
    tlen = sprintf(tbuf, "Which one are you? (S)tudent/(T)A");
    send_buf(tbuf, tlen, new_client_fd);
    print_msg(tbuf, tlen, MESSAGE_OUT);
}

int accept_client(client_t **client_list, int server_fd) {
    char log[MAX_SIZE_OF_LOG];
    int len;
    struct sockaddr_in socket_addr;
    socklen_t socket_len = sizeof(socket_addr);
    int client_fd = accept(server_fd, (struct sockaddr *)&socket_addr,
                           &socket_len);
    if (client_fd < 0) {
        len = sprintf(log, "Accepting client failed!\n");
        print_error(log, len);
    }
    add_new_client(client_list, client_fd);
    len = sprintf(log, "New client was connected fd: %d!\n", client_fd);
    print_info(log, len);
    return client_fd;
}

void add_new_client(client_t **client_list, int client_fd) {
    client_t *new_client = malloc(sizeof(client_t));
    new_client->fd = client_fd;
    new_client->role = ROLE_NONE;
    new_client->retries = 0;
    new_client->next = *client_list;
    *client_list = new_client;
}

void process_client_fd(client_t **client_list, int fd, fd_set *temp_fd_set) {
    char rbuf[MAX_SIZE_OF_BUF];
    int rlen = receive_buf(rbuf, fd);
    if (rlen >= 0) {
        process_msg(client_list, rbuf, rlen, fd, temp_fd_set);
    }
}

void process_msg(client_t **client_list, const char *rbuf, int rlen,
                 int client_fd, fd_set *temp_fd_set) {
    print_msg(rbuf, rlen, MESSAGE_IN);
    client_t *client = find_client_by_fd(*client_list, client_fd);
    if (rlen == 0) {
        remove_client(client_list, client_fd, temp_fd_set);
    } else if (strncmp(rbuf, SET_ROLE_CMD, SET_ROLE_CMD_LEN) == 0) {
        set_role(&rbuf[SET_ROLE_CMD_LEN], rlen - SET_ROLE_CMD_LEN, client);
    } else {
        char tbuf[MAX_SIZE_OF_BUF];
        int tlen;
        tlen = sprintf(tbuf, "Invalid command!");
        send_buf(tbuf, tlen, client->fd);
        print_msg(tbuf, tlen, MESSAGE_OUT);
    }
}

client_t *find_client_by_fd(client_t *client_list, int client_fd) {
    client_t *node = client_list;
    while (node != NULL) {
        if (node->fd == client_fd) {
            return node;
        }
        node = node->next;
    }
    return NULL;
}

void remove_client(client_t **client_list, int fd,
                   fd_set *temp_fd_set) {
    close(fd);
    FD_CLR(fd, temp_fd_set);
    remove_node(client_list, fd);
    char log[MAX_SIZE_OF_LOG];
    int len = sprintf(log, "Client connection was closed fd: %d!\n", fd);
    print_info(log, len);
}

void remove_node(client_t **list, int fd) {
    client_t *curr_node = *list;
    if (curr_node->fd == fd) {
        *list = curr_node->next;
        free(curr_node);
        return;
    }
    client_t *perv = NULL;
    while (curr_node != NULL) {
        if (curr_node->fd == fd) {
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
        int tlen;
        tlen = sprintf(tbuf, "Invalid role! ");
        tlen += sprintf(&tbuf[tlen], "Which one are you? (S)tudent/(T)A");
        send_buf(tbuf, tlen, client->fd);
        print_msg(tbuf, tlen, MESSAGE_OUT);     
    }
}

void assign_role(char role, client_t *client) {
    char tbuf[MAX_SIZE_OF_BUF];
    int tlen;
    if (client->role != ROLE_NONE && IS_FIRST_ROLE_TRY(client->retries)) {
        SET_FIRST_ROLE_TRY(client->retries);
        tlen = sprintf(tbuf, "Are you sure to change the role?\n");
        tlen += sprintf(&tbuf[tlen], "Enter again to confirm.");
        send_buf(tbuf, tlen, client->fd);
        print_msg(tbuf, tlen, MESSAGE_OUT);
    } else {
        CLR_FIRST_ROLE_TRY(client->retries);
        client->role = role;
        char log[MAX_SIZE_OF_LOG];
        int len;
        len = sprintf(log, "Role assigned to %s for fd: %d!\n",
                      ROLE_STR[(role == ROLE_TA) ? 1 : 0], client->fd);
        print_info(log, len);
        tlen = sprintf(tbuf, "Role assigned to %s!",
                       ROLE_STR[(role == ROLE_TA) ? 1 : 0]);
        send_buf(tbuf, tlen, client->fd);
        print_msg(tbuf, tlen, MESSAGE_OUT);
    }
}

void free_mem(client_t *list) {
    client_t *node = list;
    client_t *next = NULL;
    while (node != NULL) {
        next = node->next;
        free(node);
        node = next;
    }
}

void process_stdin(char *buf, int len, client_t *client_list) {
    if (strcmp(buf, "exit\n") == 0) {
        free_mem(client_list);
        close_endpoint(EXIT_SUCCESS);
    } else if (len > 0 && buf[len - 1] == '\n') {
        write(STDOUT_FILENO, "<< ", 3);
        buf[0] = '\0';
    }
}
