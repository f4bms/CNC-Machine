#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>

#include "../cifrado/AES.h"

#define PORT 8080
#define BUFFER_SIZE 4096
#define AES_KEY_FILE "../cifrado/aes_key.txt"

// Convierte un entero de 64 bits entre orden local y orden de red.
static uint64_t htonll_local(uint64_t value)
{
    uint32_t high = (uint32_t)(value >> 32);
    uint32_t low = (uint32_t)(value & 0xFFFFFFFFu);

    uint64_t high_net = (uint64_t)htonl(high);
    uint64_t low_net = (uint64_t)htonl(low);

    return (low_net << 32) | high_net;
}

// Envía exactamente len bytes aunque send() entregue fragmentos parciales.
static int send_all(int sockfd, const void *buffer, size_t len)
{
    size_t total = 0;
    const uint8_t *ptr = (const uint8_t*)buffer;

    while(total < len)
    {
        ssize_t sent = send(sockfd, ptr + total, len - total, 0);

        if(sent <= 0)
        {
            return -1;
        }

        total += (size_t)sent;
    }

    return 0;
}

int main(int argc, char *argv[])
{
    // Cliente: carga la imagen, la cifra y la manda al servidor en un frame binario.
    if(argc != 3)
    {
        printf("Uso:\n");
        printf("./cliente <ip_servidor> <archivo>\n");
        return 1;
    }

    const char *server_ip = argv[1];
    const char *filename = argv[2];
    // La llave se carga desde un archivo editable compartido con el servidor.
    uint8_t shared_key[AES_KEY_SIZE];

    if(aes_load_key_from_file(AES_KEY_FILE, shared_key) != 0)
    {
        fprintf(stderr,
                "No se pudo cargar la llave AES desde %s (minimo 16 caracteres)\n",
                AES_KEY_FILE);
        return 1;
    }

    // La imagen se carga completa porque el cifrado opera sobre el archivo entero.
    // Paso 1: leer imagen original completa desde disco.
    FILE *fp = fopen(filename, "rb");

    if(fp == NULL)
    {
        perror("fopen");
        return 1;
    }

    // Calcula tamano para reservar un buffer exacto.
    fseek(fp, 0, SEEK_END);
    uint64_t file_size = (uint64_t)ftell(fp);
    rewind(fp);

    uint8_t *plain_buffer = (uint8_t*)malloc((size_t)file_size);

    if(plain_buffer == NULL)
    {
        perror("malloc");
        fclose(fp);
        return 1;
    }

    if(fread(plain_buffer, 1, (size_t)file_size, fp) != (size_t)file_size)
    {
        perror("fread");
        free(plain_buffer);
        fclose(fp);
        return 1;
    }

    fclose(fp);

    // Paso 2: cifrar con AES-CBC antes de enviar.
    uint8_t iv[AES_IV_SIZE];
    uint8_t *cipher_buffer = NULL;
    size_t cipher_size = 0;

    // Se cifra el buffer antes de abrir la conexión para simplificar el flujo de error.
    if(aes_generate_iv(iv) != 0)
    {
        fprintf(stderr, "Error generando IV\n");
        free(plain_buffer);
        return 1;
    }

    if(aes_encrypt_cbc(plain_buffer,
                       (size_t)file_size,
                       shared_key,
                       iv,
                       &cipher_buffer,
                       &cipher_size) != 0)
    {
        fprintf(stderr, "Error cifrando archivo con AES\n");
        free(plain_buffer);
        return 1;
    }

    free(plain_buffer);

    // Se conecta al servidor para enviar tamaño original, tamaño cifrado, IV y ciphertext.
    // Paso 3: abrir socket TCP hacia el servidor.
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

    // Establece la sesion de transporte del payload cifrado.
    if(connect(sockfd,
               (struct sockaddr*)&server_addr,
               sizeof(server_addr)) < 0)
    {
        perror("connect");
        return 1;
    }

    // El frame viaja como: tamaño original, tamaño cifrado, IV y datos cifrados.
    uint64_t file_size_net = htonll_local(file_size);
    uint64_t cipher_size_net = htonll_local((uint64_t)cipher_size);

    // Paso 4: envia cabecera + IV + ciphertext en orden fijo.
    if(send_all(sockfd, &file_size_net, sizeof(file_size_net)) != 0 ||
       send_all(sockfd, &cipher_size_net, sizeof(cipher_size_net)) != 0 ||
       send_all(sockfd, iv, AES_IV_SIZE) != 0 ||
       send_all(sockfd, cipher_buffer, cipher_size) != 0)
    {
        fprintf(stderr, "Error enviando payload cifrado\n");
        aes_free_buffer(cipher_buffer);
        close(sockfd);
        return 1;
    }

    aes_free_buffer(cipher_buffer);

    close(sockfd);

    printf("Archivo cifrado y enviado\n");

    return 0;
}