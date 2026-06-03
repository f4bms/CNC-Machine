#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>

#define PORT 8080
#define BUFFER_SIZE 4096

int main(int argc, char *argv[])
{
    if(argc != 3)
    {
        printf("Uso:\n");
        printf("./cliente <ip_servidor> <archivo>\n");
        return 1;
    }

    const char *server_ip = argv[1];
    const char *filename = argv[2];

    FILE *fp = fopen(filename, "rb");

    if(fp == NULL)
    {
        perror("fopen");
        return 1;
    }

    fseek(fp, 0, SEEK_END);
    uint64_t file_size = ftell(fp);
    rewind(fp);

    int sockfd = socket(AF_INET, SOCK_STREAM, 0);

    if(sockfd < 0)
    {
        perror("socket");
        return 1;
    }

    struct sockaddr_in server_addr;

    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(PORT);

    inet_pton(AF_INET,
              server_ip,
              &server_addr.sin_addr);

    if(connect(sockfd,
               (struct sockaddr*)&server_addr,
               sizeof(server_addr)) < 0)
    {
        perror("connect");
        return 1;
    }

    send(sockfd,
         &file_size,
         sizeof(file_size),
         0);

    char buffer[BUFFER_SIZE];

    size_t nread;

    while((nread = fread(buffer,
                         1,
                         BUFFER_SIZE,
                         fp)) > 0)
    {
        send(sockfd,
             buffer,
             nread,
             0);
    }

    fclose(fp);

    close(sockfd);

    printf("Archivo enviado\n");

    return 0;
}