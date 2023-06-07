#include <arpa/inet.h>
#include <netdb.h>
#include <netinet/in.h>
#include <signal.h>
#include <stdio.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>

enum Constants {
    BUF_SIZE = 20,
};

int create_connection(char *node, char *service) {
    struct addrinfo *res = NULL;
    int gai_err;
    struct addrinfo hint = {
        .ai_family = AF_UNSPEC,
        .ai_socktype = SOCK_STREAM,
    };
    if ((gai_err = getaddrinfo(node, service, &hint, &res))) {
        fprintf(stderr, "gai error: %s\n", gai_strerror(gai_err));
        return -1;
    }
    int sock = -1;
    for (struct addrinfo *ai = res; ai; ai = ai->ai_next) {
        sock = socket(ai->ai_family, ai->ai_socktype, 0);
        if (sock < 0) {
            continue;
        }
        if (connect(sock, ai->ai_addr, ai->ai_addrlen) < 0) {
            close(sock);
            sock = -1;
            continue;
        }
        break;
    }
    freeaddrinfo(res);
    return sock;
}

int main(int argc, char *argv[]) {
    char addr[BUF_SIZE], port[BUF_SIZE];
    int pos = 0;
    for (; argv[1][pos] != ':'; ++pos) {
        addr[pos] = argv[1][pos];
    }
    pos++;
    for (int i = 0; pos < strlen(argv[1]); ++pos, ++i) {
        port[i] = argv[1][pos];
    }
    int fd_sock = create_connection(addr, port);
    if (fd_sock < 0) {
        return 0;
    }
    int fd_out = dup(fd_sock);
    FILE *fwrite = fdopen(fd_out, "r+");
    fprintf(fwrite, "%d\n", argc);
    for (int i = 3; i < argc; ++i) {
        fprintf(fwrite, "%s\n", argv[i]);
    }
    fclose(fwrite);
    dup2(fd_sock, STDIN_FILENO);
    char c;
    while (read(fd_sock, &c, 1) == 1) {
        write(STDOUT_FILENO, &c, 1);
    }
    close(fd_sock);
}