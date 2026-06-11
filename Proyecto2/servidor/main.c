#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>

#include "admin_tareas.h"
#include "../cifrado/AES.h"

#define PORT 8080
#define BUFFER_SIZE 4096
#define AES_KEY_FILE "../cifrado/aes_key.txt"

// Convierte un entero de 64 bits desde orden de red al orden local.
static uint64_t ntohll_local(uint64_t value)
{
    uint32_t high_net = (uint32_t)(value >> 32);
    uint32_t low_net = (uint32_t)(value & 0xFFFFFFFFu);

    uint32_t high = ntohl(high_net);
    uint32_t low = ntohl(low_net);

    return ((uint64_t)low << 32) | high;
}

// Recibe exactamente len bytes aunque recv() llegue fragmentado.
static int recv_all(int sockfd, void *buffer, size_t len)
{
    size_t total = 0;
    uint8_t *ptr = (uint8_t*)buffer;

    while(total < len)
    {
        ssize_t n = recv(sockfd, ptr + total, len - total, 0);

        if(n <= 0)
        {
            return -1;
        }

        total += (size_t)n;
    }

    return 0;
}

int main(void)
{
    // Servidor: recibe el frame cifrado, lo descifra y activa el procesamiento MPI.
    int server_fd;
    int client_fd;

    struct sockaddr_in server_addr;
    struct sockaddr_in client_addr;

    socklen_t client_len = sizeof(client_addr);
    // Cliente y servidor usan la misma llave editable desde archivo.
    uint8_t shared_key[AES_KEY_SIZE];

    if(aes_load_key_from_file(AES_KEY_FILE, shared_key) != 0)
    {
        fprintf(stderr,
                "No se pudo cargar la llave AES desde %s (minimo 16 caracteres)\n",
                AES_KEY_FILE);
        return 1;
    }

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

    // Se recibe el encabezado del frame antes del IV y el ciphertext.
    uint64_t file_size_net = 0;
    uint64_t cipher_size_net = 0;

    if(recv_all(client_fd, &file_size_net, sizeof(file_size_net)) != 0 ||
       recv_all(client_fd, &cipher_size_net, sizeof(cipher_size_net)) != 0)
    {
        fprintf(stderr, "Error recibiendo encabezado cifrado\n");
        close(client_fd);
        close(server_fd);
        return 1;
    }

    uint64_t file_size = ntohll_local(file_size_net);
    uint64_t cipher_size = ntohll_local(cipher_size_net);

    printf("Tamaño original esperado: %lu bytes\n", file_size);
    printf("Tamaño cifrado recibido: %lu bytes\n", cipher_size);

    // El payload se reconstruye en memoria y luego se descifra con AES.
    uint8_t iv[AES_IV_SIZE];

    if(recv_all(client_fd, iv, AES_IV_SIZE) != 0)
    {
        fprintf(stderr, "Error recibiendo IV\n");
        close(client_fd);
        close(server_fd);
        return 1;
    }

    uint8_t *cipher_buffer = (uint8_t*)malloc((size_t)cipher_size);

    if(cipher_buffer == NULL)
    {
        perror("malloc");
        close(client_fd);
        close(server_fd);
        return 1;
    }

    if(recv_all(client_fd, cipher_buffer, (size_t)cipher_size) != 0)
    {
        fprintf(stderr, "Error recibiendo payload cifrado\n");
        free(cipher_buffer);
        close(client_fd);
        close(server_fd);
        return 1;
    }

    uint8_t *plain_buffer = NULL;
    size_t plain_size = 0;

    if(aes_decrypt_cbc(cipher_buffer,
                       (size_t)cipher_size,
                       shared_key,
                       iv,
                       &plain_buffer,
                       &plain_size) != 0)
    {
        fprintf(stderr, "Error descifrando payload AES\n");
        free(cipher_buffer);
        close(client_fd);
        close(server_fd);
        return 1;
    }

    free(cipher_buffer);

    if(plain_size != (size_t)file_size)
    {
        fprintf(stderr,
                "Advertencia: tamano descifrado (%lu) no coincide con encabezado (%lu)\n",
                (unsigned long)plain_size,
                (unsigned long)file_size);
    }

    // El archivo se vuelve a escribir ya descifrado para que MPI lo procese igual que antes.
    FILE *fp = fopen("../img/received.bin", "wb");

    if(fp == NULL)
    {
        perror("fopen");
        return 1;
    }

    fwrite(plain_buffer, 1, plain_size, fp);

    fclose(fp);

    aes_free_buffer(plain_buffer);

    printf("Archivo descifrado y guardado\n");

    // El pipeline continúa igual: el servidor entrega el archivo limpio a MPI.
    printf("Iniciando procesamiento MPI...\n");

    admin_tareas_ejecutar("../img/received.bin");

    close(client_fd);
    close(server_fd);

    return 0;
}