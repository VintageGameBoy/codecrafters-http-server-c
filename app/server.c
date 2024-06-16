#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netinet/ip.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>

const char *response_Ok = "HTTP/1.1 200 OK\r\n\r\n";
const char *response_NotFound = "HTTP/1.1 404 Not Found\r\n\r\n";

bool isRequestTooLarge(const char *request, int maxSize) {
    int requestSize = strlen(request);
    if (requestSize > maxSize) {
        return true;
    }
    return false;
}

int main() {
    setbuf(stdout, NULL);
    setbuf(stderr, NULL);

    int server_fd;
    struct sockaddr_in client_addr;
    int client_addr_len = sizeof(client_addr);

    server_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (server_fd == -1) {
        printf("Socket creation failed: %s...\n", strerror(errno));
        return 1;
    }

    // Since the tester restarts your program quite often, setting SO_REUSEADDR
    // ensures that we don't run into 'Address already in use' errors
    int reuse = 1;
    if (setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &reuse, sizeof(reuse)) < 0) {
        printf("SO_REUSEADDR failed: %s \n", strerror(errno));
        return 1;
    }

    struct sockaddr_in serv_addr = {.sin_family = AF_INET,
            .sin_port = htons(4221),
            .sin_addr = {htonl(INADDR_ANY)},
    };

    if (bind(server_fd, (struct sockaddr *) &serv_addr, sizeof(serv_addr)) != 0) {
        printf("Bind failed: %s \n", strerror(errno));
        return 1;
    }

    int connection_backlog = 5;
    if (listen(server_fd, connection_backlog) != 0) {
        printf("Listen failed: %s \n", strerror(errno));
        return 1;
    }

    printf("Waiting for a client to connect...\n");
    client_addr_len = sizeof(client_addr);

    int client_fd = accept(server_fd, (struct sockaddr *) &client_addr, &client_addr_len);
    printf("Client connected\n");

    const int MAX_REQUEST_SIZE = 10 * 1024 * 1024; // 10MB

    // 接收HTTP请求
    int bufferSize = 4096;
    char *httpRequest = (char *) malloc(bufferSize);
    int bytesRead = recv(client_fd, httpRequest, bufferSize, 0);
    if (bytesRead != 0) {//说明还有没读完的 先不做操作先

    }

    char method[10];
    char path[100];

    // 解析请求方法
    sscanf(httpRequest, "%s", method);
    // 解析请求路径
    sscanf(strchr(httpRequest, ' ') + 1, "%s", path);

    if (strcmp(method, "GET") == 0 || strcmp(method, "POST") == 0) {

        if (strcmp(path, "/") == 0 || strcmp(path, "/index.html") == 0) {
            int sendResult = send(client_fd, response_Ok, strlen(response_Ok), 0);
        } else {
            int sendResult = send(client_fd, response_NotFound, strlen(response_NotFound), 0);
        }


    }


    close(client_fd);
    close(server_fd);

    return 0;
}
