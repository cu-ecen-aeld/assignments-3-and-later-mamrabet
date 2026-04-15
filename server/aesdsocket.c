#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <syslog.h>
#include <signal.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <fcntl.h>
#include <errno.h>

#define PORT "9000"
#define DATA_FILE "/var/tmp/aesdsocketdata"
#define BUFFER_SIZE 1024

int server_fd = -1;
int client_fd = -1;

void handle_signal(int sig) {
    if (sig == SIGINT || sig == SIGTERM) {
        syslog(LOG_INFO, "Caught signal, exiting");
        if (server_fd != -1) close(server_fd);
        if (client_fd != -1) close(client_fd);
        remove(DATA_FILE);
        closelog();
        exit(0);
    }
}

int main(int argc, char *argv[]) {
    int daemon_mode = 0;
    if (argc > 1 && strcmp(argv[1], "-d") == 0) {
        daemon_mode = 1;
    }

    openlog("aesdsocket", LOG_PID, LOG_USER);


    struct sigaction sa;
    sa.sa_handler = handle_signal;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = 0;
    sigaction(SIGINT, &sa, NULL);
    sigaction(SIGTERM, &sa, NULL);


    server_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (server_fd == -1) return -1;


    int opt = 1;
    setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));


    struct sockaddr_in server_addr;
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = INADDR_ANY;
    server_addr.sin_port = htons(9000);

    if (bind(server_fd, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0) {
        close(server_fd);
        return -1;
    }


    if (daemon_mode) {
        pid_t pid = fork();
        if (pid < 0) return -1;
        if (pid > 0) exit(0); 
        setsid();
        chdir("/");
        close(STDIN_FILENO);
        close(STDOUT_FILENO);
        close(STDERR_FILENO);
    }


    if (listen(server_fd, 10) < 0) {
        close(server_fd);
        return -1;
    }

    while (1) {
        struct sockaddr_in client_addr;
        socklen_t addr_len = sizeof(client_addr);
        
        client_fd = accept(server_fd, (struct sockaddr *)&client_addr, &addr_len);
        if (client_fd == -1) continue;

        char client_ip[INET_ADDRSTRLEN];
        inet_ntop(AF_INET, &client_addr.sin_addr, client_ip, sizeof(client_ip));
        syslog(LOG_INFO, "Accepted connection from %s", client_ip);


        int fd = open(DATA_FILE, O_RDWR | O_CREAT | O_APPEND, 0666);
        if (fd == -1) {
            syslog(LOG_ERR, "Could not open data file: %s", strerror(errno));
            close(client_fd);
            continue;
        }

        char *rx_buffer = malloc(BUFFER_SIZE);
        if (!rx_buffer) {
            syslog(LOG_ERR, "Malloc failed");
            close(fd);
            close(client_fd);
            continue;
        }

        ssize_t bytes_recv;
        while ((bytes_recv = recv(client_fd, rx_buffer, BUFFER_SIZE, 0)) > 0) {
            if (write(fd, rx_buffer, bytes_recv) != bytes_recv) {
                syslog(LOG_ERR, "Failed to write to file");
                break;
            }
            if (memchr(rx_buffer, '\n', bytes_recv)) {

                fsync(fd); 
                lseek(fd, 0, SEEK_SET);

                char tx_buffer[BUFFER_SIZE];
                ssize_t bytes_read;
                while ((bytes_read = read(fd, tx_buffer, sizeof(tx_buffer))) > 0) {
                    send(client_fd, tx_buffer, bytes_read, 0);
                }
                break; 
            }
        }

        free(rx_buffer);
        close(fd);
        close(client_fd);
        syslog(LOG_INFO, "Closed connection from %s", client_ip);
    }

    return 0;
}
