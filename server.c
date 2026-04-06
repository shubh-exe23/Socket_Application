#define _GNU_SOURCE
#include <arpa/inet.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#define MAX_CLIENTS 10
#define MSG_QUERY_LOAD 1
#define MSG_RUN_CODE   2

int clients[MAX_CLIENTS];
int client_count = 0;

/* ── helpers ── */
int send_all(int fd, void *buf, int len) {
    int sent = 0;
    while (sent < len) {
        int n = send(fd, (char*)buf + sent, len - sent, 0);
        if (n <= 0) return -1;
        sent += n;
    }
    return 0;
}

int recv_all(int fd, void *buf, int len) {
    int rec = 0;
    while (rec < len) {
        int n = recv(fd, (char*)buf + rec, len - rec, 0);
        if (n <= 0) return -1;
        rec += n;
    }
    return 0;
}

void send_str(int fd, char *s) {
    uint32_t len = htonl(strlen(s));
    send_all(fd, &len, 4);
    send_all(fd, s, strlen(s));
}

char *read_file(char *name) {
    FILE *fp = fopen(name, "r");
    if (!fp) return NULL;

    char *buf = malloc(4096);
    buf[0] = '\0';

    char temp[256];
    while (fgets(temp, sizeof(temp), fp))
        strcat(buf, temp);

    fclose(fp);
    return buf;
}

/* ── main ── */
int main() {
    int listener = socket(AF_INET, SOCK_STREAM, 0);

    struct sockaddr_in addr;
    addr.sin_family = AF_INET;
    addr.sin_port = htons(9001);
    addr.sin_addr.s_addr = INADDR_ANY;

    bind(listener, (struct sockaddr*)&addr, sizeof(addr));
    listen(listener, 5);

    printf("[server] Waiting for clients...\n");

    /* Accept clients */
    while (client_count < MAX_CLIENTS) {
        int cfd = accept(listener, NULL, NULL);
        clients[client_count++] = cfd;
        printf("[server] Client connected (%d)\n", client_count);
    }

    /* Main loop */
    while (1) {
        char filename[100];
        printf("Enter C file: ");
        scanf("%s", filename);

        char *code = read_file(filename);
        if (!code) {
            printf("File error\n");
            continue;
        }

        /* Find least loaded client */
        int min_load = 1e9, best = -1;

        for (int i = 0; i < client_count; i++) {
            uint32_t msg = htonl(MSG_QUERY_LOAD);
            send_all(clients[i], &msg, 4);

            uint32_t load;
            recv_all(clients[i], &load, 4);
            load = ntohl(load);

            printf("Client %d load = %d\n", i, load);

            if (load < min_load) {
                min_load = load;
                best = i;
            }
        }

        printf("[server] Sending to client %d\n", best);

        uint32_t msg = htonl(MSG_RUN_CODE);
        send_all(clients[best], &msg, 4);
        send_str(clients[best], code);

        /* Receive output */
        uint32_t len;
        recv_all(clients[best], &len, 4);
        len = ntohl(len);

        char *output = malloc(len + 1);
        recv_all(clients[best], output, len);
        output[len] = '\0';

        printf("\nOUTPUT:\n%s\n", output);

        free(code);
        free(output);
    }
}