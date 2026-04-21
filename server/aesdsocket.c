#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <syslog.h>
#include <signal.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <fcntl.h>
#include <errno.h>
#include <pthread.h>
#include <sys/queue.h>
#include <stdbool.h>
#include <time.h>

#define PORT 9000
#define DATA_FILE "/var/tmp/aesdsocketdata"
#define BUF_SIZE 20000 

struct thread_node {
    pthread_t thread_id;
    int client_fd;
    bool thread_complete;
    SLIST_ENTRY(thread_node) entries;
};

int server_fd = -1;
pthread_mutex_t file_mutex = PTHREAD_MUTEX_INITIALIZER;
volatile bool exit_requested = false;
SLIST_HEAD(thread_list_head, thread_node) head = SLIST_HEAD_INITIALIZER(head);

void handle_signal(int sig) {
    exit_requested = true;
    if (server_fd != -1) shutdown(server_fd, SHUT_RDWR);
}

void* timestamp_handler(void* arg) {
    while (!exit_requested) {
        for (int i = 0; i < 100 && !exit_requested; i++) usleep(100000);
        if (exit_requested) break;

        time_t rawtime;
        struct tm *info;
        char buffer[128];
        time(&rawtime);
        info = localtime(&rawtime);
        int len = strftime(buffer, sizeof(buffer), "timestamp:%a, %d %b %Y %T %z\n", info);

        pthread_mutex_lock(&file_mutex);
        int fd = open(DATA_FILE, O_WRONLY | O_CREAT | O_APPEND, 0666);
        if (fd != -1) {
            write(fd, buffer, len);
            close(fd);
        }
        pthread_mutex_unlock(&file_mutex);
    }
    return NULL;
}

void* client_handler(void* thread_param) {
    struct thread_node *params = (struct thread_node *)thread_param;
    char *rx_buffer = malloc(BUF_SIZE);
    if (!rx_buffer) { params->thread_complete = true; return NULL; }

    ssize_t total_recv = 0;
    while (!exit_requested) {
        ssize_t bytes = recv(params->client_fd, rx_buffer + total_recv, BUF_SIZE - total_recv - 1, 0);
        if (bytes <= 0) break;
        total_recv += bytes;
        if (memchr(rx_buffer, '\n', total_recv)) break;
    }

    if (total_recv > 0) {
        pthread_mutex_lock(&file_mutex);
        int fd = open(DATA_FILE, O_RDWR | O_CREAT | O_APPEND, 0666);
        if (fd != -1) {
            write(fd, rx_buffer, total_recv);
            lseek(fd, 0, SEEK_SET);
            ssize_t bytes_read;
            while ((bytes_read = read(fd, rx_buffer, BUF_SIZE)) > 0) {
                send(params->client_fd, rx_buffer, bytes_read, 0);
            }
            close(fd);
        }
        pthread_mutex_unlock(&file_mutex);
    }

    free(rx_buffer);
    close(params->client_fd);
    params->thread_complete = true;
    return NULL;
}

int main(int argc, char *argv[]) {
    openlog("aesdsocket", LOG_PID, LOG_USER);
    struct sigaction sa = {0};
    sa.sa_handler = handle_signal;
    sigaction(SIGINT, &sa, NULL); sigaction(SIGTERM, &sa, NULL);

    server_fd = socket(AF_INET, SOCK_STREAM, 0);
    int opt = 1;
    setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    struct sockaddr_in addr = {0};
    addr.sin_family = AF_INET;
    addr.sin_port = htons(PORT);
    addr.sin_addr.s_addr = INADDR_ANY;

    if (bind(server_fd, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
        close(server_fd);
        return -1;
    }

    if (argc > 1 && strcmp(argv[1], "-d") == 0) {
        pid_t pid = fork();
        if (pid < 0) return -1;
        if (pid > 0) exit(0);
        setsid(); chdir("/");
        int devnull = open("/dev/null", O_RDWR);
        dup2(devnull, 0); dup2(devnull, 1); dup2(devnull, 2);
        close(devnull);
    }

    if (listen(server_fd, 10) < 0) {
        close(server_fd);
        return -1;
    }

    SLIST_INIT(&head);
    pthread_t time_tid;
    pthread_create(&time_tid, NULL, timestamp_handler, NULL);

    while (!exit_requested) {
        struct sockaddr_in caddr;
        socklen_t clen = sizeof(caddr);
        int cfd = accept(server_fd, (struct sockaddr *)&caddr, &clen);
        if (exit_requested) break;
        if (cfd < 0) continue;

        struct thread_node *node = malloc(sizeof(struct thread_node));
        node->client_fd = cfd;
        node->thread_complete = false;
        pthread_create(&node->thread_id, NULL, client_handler, node);
        SLIST_INSERT_HEAD(&head, node, entries);

        struct thread_node *it, *prev = NULL, *next;
        it = SLIST_FIRST(&head);
        while (it != NULL) {
            next = SLIST_NEXT(it, entries);
            if (it->thread_complete) {
                pthread_join(it->thread_id, NULL);
                if (prev == NULL) head.slh_first = next;
                else SLIST_NEXT(prev, entries) = next;
                free(it);
            } else {
                prev = it;
            }
            it = next;
        }
    }

    pthread_join(time_tid, NULL);
    while (!SLIST_EMPTY(&head)) {
        struct thread_node *it = SLIST_FIRST(&head);
        pthread_join(it->thread_id, NULL);
        SLIST_REMOVE_HEAD(&head, entries);
        free(it);
    }
    
    pthread_mutex_destroy(&file_mutex);
    remove(DATA_FILE);
    close(server_fd);
    closelog();
    return 0;
}
