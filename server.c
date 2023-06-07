#include <stdio.h>
#include <unistd.h>
#include <signal.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>
#include <sys/wait.h>
#include <arpa/inet.h>
#include <stdlib.h>
#include <string.h>
#include <sys/time.h>
#include <time.h>

enum Constants {
	BUF_SIZE = 1024,
	SECONDS = 30,
};

int create_listener(char *service) {
    struct addrinfo *res = NULL;
    int gai_err;
    struct addrinfo hint = {
        .ai_family = AF_UNSPEC,
        .ai_socktype = SOCK_STREAM,
        .ai_flags = AI_PASSIVE,
    };
    if ((gai_err = getaddrinfo(NULL, service, &hint, &res))) {
        return -1;
    }
    int sock = -1;
    for (struct addrinfo *ai = res; ai; ai = ai->ai_next) {
        sock = socket(ai->ai_family, ai->ai_socktype, 0);
        if (sock < 0) {
            continue;
        }
        int one = 1;
        setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, &one, sizeof(one));
        if (bind(sock, ai->ai_addr, ai->ai_addrlen) < 0) {
            close(sock);
            sock = -1;
            continue;
        }
        if (listen(sock, SOMAXCONN) < 0) {
            close(sock);
            sock = -1;
            continue;
        }

        break;
    }
    freeaddrinfo(res);
    return sock;
}

void free_data(char **data, int size) {
	for (int i = 0; i < size; ++i) {
		free(data[i]);
	}
	free(data);
}

int client_fl = 0;

void handler_alarm(int signo) {
	if (client_fl == 0) {
		_exit(0);
	}
	client_fl = 0;
}

int main(int argc, char *argv[]) {
	struct sigaction sa_alarm = {
        .sa_handler = handler_alarm,
        .sa_flags = SA_RESTART,
    };
    sigaction(SIGALRM, &sa_alarm, NULL);
	struct sigaction sa_chld = {
        .sa_handler = SIG_IGN,
        .sa_flags = SA_RESTART,
    };
    sigaction(SIGCHLD, &sa_chld, NULL);
	if (argc != 2) {
		perror("Wrong arguments\n");
		return 1;
	}
	if (daemon(1, 1) != 0) {
		perror("Can't create daemon process\n");
		return 1;
	}
	struct timeval tval = {
        .tv_sec = SECONDS,
        .tv_usec = 0,
    };
    struct itimerval itval = {
        .it_interval = tval,
        .it_value = tval,
    };
    if (setitimer(ITIMER_REAL, &itval, NULL) == -1) {
        return 1;
    }
	int sock = create_listener(argv[1]);
	if (sock < 0) {
		perror("Can't create listen server\n");
		return 1;
	}
	while (1) {
		int fd_sock = accept(sock, NULL, NULL);
		if (fd_sock == -1) {
			perror("Accept error\n");
			return 1;
		}
		client_fl = 1;
		int fd_in = dup(fd_sock);
		FILE *fread = fdopen(fd_in, "r");
		int c_argc;
		if (fscanf(fread, "%d", &c_argc) != 1) {
			close(fd_sock);
			fclose(fread);
			continue;
		}
		char **data = calloc(c_argc - 2, sizeof(*data));
		int arr_size = 0;
		char buf[BUF_SIZE];
		for (int i = 0; i < c_argc - 3; ++i) {
			if (fscanf(fread, "%s", buf) != 1) {
				free_data(data, arr_size);
				arr_size = -1;
				break;
			}
			data[arr_size] = calloc(strlen(buf), sizeof(**data));
			data[arr_size] = strcpy(data[arr_size], buf);
			arr_size++;
		}
		if (arr_size <= 0) {
			close(fd_sock);
			fclose(fread);
			continue;	
		}
		fclose(fread);
		data[arr_size] = NULL;
		pid_t pid1 = fork();
		if (pid1 == 0) {
			dup2(fd_sock, STDIN_FILENO);
			dup2(fd_sock, STDOUT_FILENO);
			close(fd_sock);
			execvp(data[0], data);
			return 0;
		}
		free_data(data, arr_size);
		close(fd_sock);
        while (waitpid(-1, NULL, WNOHANG) > 0) {
        }
	}
}