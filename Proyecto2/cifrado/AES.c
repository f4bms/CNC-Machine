// Implementación manual de la capa AES compartida por cliente y servidor.
#include "AES.h"

#include <openssl/evp.h>
#include <openssl/rand.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int aes_load_key_from_file(
    const char *path,
    uint8_t key[AES_KEY_SIZE]
)
{
    // La llave se toma de la primera línea del archivo y se recortan saltos de línea.
    FILE *fp = NULL;
    char line[256];
    size_t len;

    if(path == NULL || key == NULL)
    {
        return -1;
    }

    fp = fopen(path, "r");

    if(fp == NULL)
    {
        return -1;
    }

    if(fgets(line, sizeof(line), fp) == NULL)
    {
        fclose(fp);
        return -1;
    }

    fclose(fp);

    len = strlen(line);

    while(len > 0 && (line[len - 1] == '\n' || line[len - 1] == '\r'))
    {
        line[len - 1] = '\0';
        len--;
    }

    if(len < AES_KEY_SIZE)
    {
        return -1;
    }

    memcpy(key, line, AES_KEY_SIZE);
    return 0;
}

int aes_generate_iv(uint8_t iv[AES_IV_SIZE])
{
    // El IV debe ser impredecible para cada mensaje cifrado.
    if(iv == NULL)
    {
        return -1;
    }

    if(RAND_bytes(iv, AES_IV_SIZE) != 1)
    {
        return -1;
    }

    return 0;
}

int aes_encrypt_cbc(
    const uint8_t *plaintext,
    size_t plaintext_len,
    const uint8_t key[AES_KEY_SIZE],
    const uint8_t iv[AES_IV_SIZE],
    uint8_t **ciphertext,
    size_t *ciphertext_len
)
{
    // OpenSSL maneja el padding PKCS#7 y el bloque CBC internamente.
    EVP_CIPHER_CTX *ctx = NULL;
    uint8_t *out = NULL;
    int out_len1 = 0;
    int out_len2 = 0;

    if(plaintext == NULL || key == NULL || iv == NULL || ciphertext == NULL || ciphertext_len == NULL)
    {
        return -1;
    }

    *ciphertext = NULL;
    *ciphertext_len = 0;

    ctx = EVP_CIPHER_CTX_new();

    if(ctx == NULL)
    {
        return -1;
    }

    out = (uint8_t*)malloc(plaintext_len + AES_IV_SIZE);

    if(out == NULL)
    {
        EVP_CIPHER_CTX_free(ctx);
        return -1;
    }

    if(EVP_EncryptInit_ex(ctx, EVP_aes_128_cbc(), NULL, key, iv) != 1)
    {
        free(out);
        EVP_CIPHER_CTX_free(ctx);
        return -1;
    }

    if(EVP_EncryptUpdate(ctx, out, &out_len1, plaintext, (int)plaintext_len) != 1)
    {
        free(out);
        EVP_CIPHER_CTX_free(ctx);
        return -1;
    }

    if(EVP_EncryptFinal_ex(ctx, out + out_len1, &out_len2) != 1)
    {
        free(out);
        EVP_CIPHER_CTX_free(ctx);
        return -1;
    }

    *ciphertext = out;
    *ciphertext_len = (size_t)(out_len1 + out_len2);

    EVP_CIPHER_CTX_free(ctx);
    return 0;
}

int aes_decrypt_cbc(
    const uint8_t *ciphertext,
    size_t ciphertext_len,
    const uint8_t key[AES_KEY_SIZE],
    const uint8_t iv[AES_IV_SIZE],
    uint8_t **plaintext,
    size_t *plaintext_len
)
{
    // El descifrado usa el mismo par clave/IV que el cliente envió.
    EVP_CIPHER_CTX *ctx = NULL;
    uint8_t *out = NULL;
    int out_len1 = 0;
    int out_len2 = 0;

    if(ciphertext == NULL || key == NULL || iv == NULL || plaintext == NULL || plaintext_len == NULL)
    {
        return -1;
    }

    *plaintext = NULL;
    *plaintext_len = 0;

    ctx = EVP_CIPHER_CTX_new();

    if(ctx == NULL)
    {
        return -1;
    }

    out = (uint8_t*)malloc(ciphertext_len);

    if(out == NULL)
    {
        EVP_CIPHER_CTX_free(ctx);
        return -1;
    }

    if(EVP_DecryptInit_ex(ctx, EVP_aes_128_cbc(), NULL, key, iv) != 1)
    {
        free(out);
        EVP_CIPHER_CTX_free(ctx);
        return -1;
    }

    if(EVP_DecryptUpdate(ctx, out, &out_len1, ciphertext, (int)ciphertext_len) != 1)
    {
        free(out);
        EVP_CIPHER_CTX_free(ctx);
        return -1;
    }

    if(EVP_DecryptFinal_ex(ctx, out + out_len1, &out_len2) != 1)
    {
        free(out);
        EVP_CIPHER_CTX_free(ctx);
        return -1;
    }

    *plaintext = out;
    *plaintext_len = (size_t)(out_len1 + out_len2);

    EVP_CIPHER_CTX_free(ctx);
    return 0;
}

void aes_free_buffer(uint8_t *buffer)
{
    // Wrapper simple para liberar memoria reservada por cifrado/descifrado.
    if(buffer != NULL)
    {
        free(buffer);
    }
}
