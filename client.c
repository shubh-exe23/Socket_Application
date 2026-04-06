#define _GNU_SOURCE
#include <arpa/inet.h>
#include <netdb.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/wait.h>

#define MSG_QUERY_LOAD 1
#define MSG_RUN_CODE   2

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

int send_str(int fd, char *s) {
    uint32_t len = htonl(strlen(s));
    send_all(fd, &len, 4);
    return send_all(fd, s, strlen(s));
}

char *recv_str(int fd) {
    uint32_t len;
    if (recv_all(fd, &len, 4) < 0) return NULL;
    len = ntohl(len);

    char *buf = malloc(len + 1);
    recv_all(fd, buf, len);
    buf[len] = '\0';
    return buf;
}

/* ── load tracking ── */
int active_jobs = 0;

/* ── run code ── */
char *run_code(char *code) {
    FILE *fp = fopen("temp.c", "w");
    fputs(code, fp);
    fclose(fp);

    system("gcc temp.c -o temp.out 2> err.txt");
    system("./temp.out > out.txt 2>> err.txt");

    FILE *out = fopen("out.txt", "r");
    FILE *err = fopen("err.txt", "r");

    char *result = malloc(4096);
    result[0] = '\0';

    char buf[256];

    if (out) {
        while (fgets(buf, sizeof(buf), out))
            strcat(result, buf);
        fclose(out);
    }

    if (err) {
        while (fgets(buf, sizeof(buf), err))
            strcat(result, buf);
        fclose(err);
    }

    return result;
}

/* ── main ── */
int main() {
    char ip[100];
    int port;

    printf("Enter server IP: ");
    scanf("%s", ip);
    printf("Enter port: ");
    scanf("%d", &port);

    int fd = socket(AF_INET, SOCK_STREAM, 0);

    struct sockaddr_in addr;
    addr.sin_family = AF_INET;
    addr.sin_port = htons(port);
    inet_pton(AF_INET, ip, &addr.sin_addr);

    connect(fd, (struct sockaddr*)&addr, sizeof(addr));

    printf("[client] Connected\n");

    while (1) {
        uint32_t msg;
        if (recv_all(fd, &msg, 4) < 0) break;
        msg = ntohl(msg);

        if (msg == MSG_QUERY_LOAD) {
            uint32_t load = htonl(active_jobs);
            send_all(fd, &load, 4);
        }

        else if (msg == MSG_RUN_CODE) {
            char *code = recv_str(fd);

            active_jobs++;

            char *output = run_code(code);
            send_str(fd, output);

            active_jobs--;

            free(code);
            free(output);
        }
    }

    close(fd);
}