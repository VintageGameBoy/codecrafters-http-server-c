#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <pthread.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netinet/ip.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>
#include <zlib.h>

#define RESPNESE_SIZE 4096

const char *response_Ok = "HTTP/1.1 200 OK\r\n\r\n";
const char *response_NotFound = "HTTP/1.1 404 Not Found\r\n\r\n";

char *fileDir = NULL;

void *handle_connection(void *pclient_socket);

//https://stackoverflow.com/a/57699371/7292958 从这里复制过来的
int compressToGzip(const char* input, int inputSize, char* output, int outputSize);

int main(int argc, char *argv[]) {
    if (argc >= 2 && strcmp(argv[1], "--directory") == 0) {
        fileDir = argv[2];
    }

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

    int connection_backlog = 10;
    if (listen(server_fd, connection_backlog) != 0) {
        printf("Listen failed: %s \n", strerror(errno));
        return 1;
    }

    printf("Waiting for a client to connect...\n");
    client_addr_len = sizeof(client_addr);

    while (true) {
        int client_fd = accept(server_fd, (struct sockaddr *) &client_addr, &client_addr_len);
        int *client_fd_ptr = (int *) malloc(sizeof(int));
        *client_fd_ptr = client_fd;
        printf("Client: %ld connected\n", client_fd);
        pthread_t new_process;
        pthread_create(&new_process, NULL, handle_connection, client_fd_ptr);
    }

    close(server_fd);

    return 0;
}

void *handle_connection(void *pclient_fd) {
    int client_fd = *((int *) pclient_fd);
    //const int MAX_REQUEST_SIZE = 10 * 1024 * 1024; // 10MB

    // 接收HTTP请求
    int bufferSize = 1024;
    char *httpRequest = (char *) malloc(bufferSize);
    int bytesRead = recv(client_fd, httpRequest, bufferSize, 0);

    printf("current httpRequest:%s -------httpRequestEnd\n bytesRead:%d\n", httpRequest, bytesRead);
    char *defaultUserAgent = "default ua";
    char method[10];
    char path[100];
    char UserAgent[100];
    char ContentType[100];
    int AcceptEncodingCount = 0; // 客户端编码请求数量
    char *AcceptEncoding[100]; // 客户端编码请求
    printf("create init char\n");
    sscanf(httpRequest, "%s", method);
    sscanf(strchr(httpRequest, ' ') + 1, "%s", path);
    char *userAgentStart = strstr(httpRequest, "User-Agent: ");
    if (userAgentStart != NULL) {
        sscanf(userAgentStart + 11, "%s", UserAgent);
    } else {
        strncpy(UserAgent, defaultUserAgent, sizeof(UserAgent));
        UserAgent[sizeof(UserAgent) - 1] = '\0';
    }
    char *ContentTypeStart = strstr(httpRequest, "Content-Type: ");
    if (ContentTypeStart != NULL) {
        sscanf(ContentTypeStart + 13, "%s", ContentType);
    }
    char *AcceptEncodingStart = strstr(httpRequest, "Accept-Encoding: ");
    if (AcceptEncodingStart != NULL) {
        AcceptEncodingStart += strlen("Accept-Encoding: ");
        // 忽略 \r\n\r\n 标记
        char *endOfHeader = strstr(AcceptEncodingStart, "\r\n\r\n");
        if (endOfHeader != NULL) {
            *endOfHeader = '\0';
        }
        char *encoding = strtok(AcceptEncodingStart, ",");
        while (encoding != NULL && AcceptEncodingCount < 100) {
            // 剔除空格
            while (*encoding == ' ') {
                encoding++;
            }
            AcceptEncoding[AcceptEncodingCount++] = encoding;
            encoding = strtok(NULL, ",");
        }
    }

    printf("current Method:%s Path:%s UserAgent:%s\n", method, path, UserAgent);

    if (strcmp(method, "GET") == 0 || strcmp(method, "POST") == 0) {
        if (strcmp(path, "/") == 0 || strcmp(path, "/index.html") == 0) {
            int sendResult = send(client_fd, response_Ok, strlen(response_Ok), 0);
            printf("path: %s  now response: %s \nsendResult: %d", path, response_Ok, sendResult);
        } else if (strncmp(path, "/echo/", 6) == 0) {
            char *str = path + 6;
            char *response_Echo = (char *) malloc(1024);
            bool sendState=false;
            sprintf(response_Echo, "HTTP/1.1 200 OK\r\nContent-Type: text/plain\r\nContent-Length: %ld\r\n\r\n%s",
                    strlen(str), str);
            for (int i = 0; i < AcceptEncodingCount; ++i) {
                if (strncmp(AcceptEncoding[i], "gzip", 4) == 0) {
                    // 创建一个缓冲区用于存储压缩后的数据
                    char compressedData[1024];
                    int compressedSize = compressToGzip(str, strlen(str), compressedData, sizeof(compressedData));

//                    // 检查压缩后的数据是否比原始数据更小
//                    if (compressedSize < strlen(str)) {
                        // 更新响应头，指明响应内容经过gzip压缩
                        sprintf(response_Echo,
                                "HTTP/1.1 200 OK\r\nContent-Encoding: gzip\r\nContent-Type: text/plain\r\nContent-Length: %d\r\n\r\n",
                                compressedSize);
                        // 发送响应头
                        send(client_fd, response_Echo, strlen(response_Echo), 0);
                        // 发送压缩后的数据
                        send(client_fd, compressedData, compressedSize, 0);
//                    } else {
//                        // 压缩后的数据比原始数据更大，不进行压缩
//                        sprintf(response_Echo,
//                                "HTTP/1.1 200 OK\r\nContent-Type: text/plain\r\nContent-Length: %ld\r\n\r\n%s",
//                                strlen(str), str);
//                        send(client_fd, response_Echo, strlen(response_Echo), 0);
//                    }
                    sendState=true;
                    break;
                }

            }
            if (!sendState){
                int sendResult = send(client_fd, response_Echo, strlen(response_Echo), 0);
                printf("path: %s  now response: %s \nsendResult: %d", path, response_Echo, sendResult);
            }
            free(response_Echo);
        } else if (strncmp(path, "/user-agent", 11) == 0) {
            char *response_UserAgent = (char *) malloc(1024);
            sprintf(response_UserAgent, "HTTP/1.1 200 OK\r\nContent-Type: text/plain\r\nContent-Length: %ld\r\n\r\n%s",
                    strlen(UserAgent), UserAgent);
            int sendResult = send(client_fd, response_UserAgent, strlen(response_UserAgent), 0);
            printf("path: %s  now response: %s \nsendResult: %d", path, response_UserAgent, sendResult);
            free(response_UserAgent);
        } else if (strncmp(path, "/files/", 7) == 0 && strcmp(method, "GET") == 0) {
            char *fileName = path + 7;
            char *response_files = (char *) malloc(1024);
            //char *filePath=(char *) malloc(strlen(fileDir)+ strlen(fileName)+1);
            char *filePath = (char *) malloc(128);
            strcpy(filePath, fileDir);
            strcat(filePath, fileName);
            printf("filePath:%s", filePath);
            if (access(filePath, F_OK) != 0) {
                int sendResult = send(client_fd, response_NotFound, strlen(response_NotFound), 0);
                printf("path: %s  now response: %s \nsendResult: %d", path, response_NotFound, sendResult);
            } else {
                FILE *fptr = fopen(filePath, "r");
                if (!fptr)
                    printf("%s : %s", fileDir, filePath);
                fseek(fptr, 0, SEEK_END);
                int size = ftell(fptr);
                fseek(fptr, 0, SEEK_SET);
                printf("skdnakjn %d", size);
                char *contents = (char *) malloc(size);
                fread(contents, size, 1, fptr);
                sprintf(response_files,
                        "HTTP/1.1 200 OK\r\nContent-Type: "
                        "application/octet-stream\r\nContent-Length: "
                        "%d\r\n\r\n%s\r\n\r\n",
                        size, contents);
                int sendResult = send(client_fd, response_files, strlen(response_files), 0);
                printf("path: %s  now response: %s \nsendResult: %d", path, response_files, sendResult);
                fclose(fptr);
                free(contents);
            }
            free(response_files);
            free(filePath);
        } else if (strncmp(path, "/files/", 7) == 0 && strcmp(method, "POST") == 0 &&
                   strcmp(ContentType, "application/octet-stream") == 0) {
            char *fileName = path + 7;
            char *filePath = (char *) malloc(128);
            strcpy(filePath, fileDir);
            strcat(filePath, fileName);
            char *dataStart = strstr(httpRequest, "\r\n\r\n");
            if (access(filePath, F_OK) == 0 || dataStart == NULL) { //已经存在或者没数据
                int sendResult = send(client_fd, response_NotFound, strlen(response_NotFound), 0);
                printf("path: %s  now response: %s \nsendResult: %d", path, response_NotFound, sendResult);
            } else {
                char *response = "HTTP/1.1 201 Created\r\n\r\n";
                FILE *fptr = fopen(filePath, "w");
                if (!fptr)
                    printf("%s : %s", fileDir, filePath);
                fprintf(fptr, dataStart + 4);
                int sendResult = send(client_fd, response, strlen(response), 0);
                printf("path: %s  now response: %s \nsendResult: %d", path, response, sendResult);
                fclose(fptr);
                free(response);
            }


            free(filePath);
        } else {
            int sendResult = send(client_fd, response_NotFound, strlen(response_NotFound), 0);
            printf("path: %s  now response: %s \nsendResult: %d", path, response_NotFound, sendResult);
        }


    } else {//这里要放一个不支持这个协议的返回，我不知道些什么先空着

    }
//    for (int i = 0; i < AcceptEncodingCount; i++) {
//        free(AcceptEncoding[i]);
//    }
    free(pclient_fd);
    free(httpRequest);
    close(client_fd);
}

int compressToGzip(const char* input, int inputSize, char* output, int outputSize)
{
    z_stream zs;
    zs.zalloc = Z_NULL;
    zs.zfree = Z_NULL;
    zs.opaque = Z_NULL;
    zs.avail_in = (uInt)inputSize;
    zs.next_in = (Bytef *)input;
    zs.avail_out = (uInt)outputSize;
    zs.next_out = (Bytef *)output;

    // hard to believe they don't have a macro for gzip encoding, "Add 16" is the best thing zlib can do:
    // "Add 16 to windowBits to write a simple gzip header and trailer around the compressed data instead of a zlib wrapper"
    deflateInit2(&zs, Z_DEFAULT_COMPRESSION, Z_DEFLATED, 15 | 16, 8, Z_DEFAULT_STRATEGY);
    deflate(&zs, Z_FINISH);
    deflateEnd(&zs);
    return zs.total_out;
}