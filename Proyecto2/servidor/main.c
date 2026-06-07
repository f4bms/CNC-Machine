#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>

#include "admin_tareas.h"

#define PORT 8080
#define BUFFER_SIZE 4096

int main(void)
{
    int server_fd;
    int client_fd;

    struct sockaddr_in server_addr;
    struct sockaddr_in client_addr;

    socklen_t client_len = sizeof(client_addr);

    server_fd = socket(AF_INET, SOCK_STREAM, 0);

    if(server_fd < 0)
    {
        perror("socket");
        return 1;
    }

    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(PORT);
    server_addr.sin_addr.s_addr = INADDR_ANY;

    if(bind(server_fd,
            (struct sockaddr*)&server_addr,
            sizeof(server_addr)) < 0)
    {
        perror("bind");
        return 1;
    }

    if(listen(server_fd, 5) < 0)
    {
        perror("listen");
        return 1;
    }

    printf("Servidor escuchando en puerto %d\n", PORT);

    client_fd = accept(server_fd,
                       (struct sockaddr*)&client_addr,
                       &client_len);

    if(client_fd < 0)
    {
        perror("accept");
        return 1;
    }

    printf("Cliente conectado\n");

    uint64_t file_size;

    recv(client_fd,
         &file_size,
         sizeof(file_size),
         MSG_WAITALL);

    printf("Tamaño recibido: %lu bytes\n", file_size);

    FILE *fp = fopen("../img/received.bin", "wb");

    if(fp == NULL)
    {
        perror("fopen");
        return 1;
    }

    char buffer[BUFFER_SIZE];

    uint64_t total = 0;

    while(total < file_size)
    {
        ssize_t n = recv(client_fd,
                         buffer,
                         BUFFER_SIZE,
                         0);

        if(n <= 0)
            break;

        fwrite(buffer, 1, n, fp);

        total += n;
    }

    fclose(fp);

    printf("Archivo guardado\n");

    printf("Iniciando procesamiento MPI...\n");

    admin_tareas_ejecutar("../img/received.bin");

    close(client_fd);
    close(server_fd);

    return 0;
}