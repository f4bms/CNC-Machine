#ifndef AES_H
#define AES_H

#include <stddef.h>
#include <stdint.h>

#define AES_KEY_SIZE 16
#define AES_IV_SIZE 16

// Lee una llave de 16 bytes desde un archivo de texto editable.
int aes_load_key_from_file(
    const char *path,
    uint8_t key[AES_KEY_SIZE]
);

// Genera un IV aleatorio para CBC.
int aes_generate_iv(uint8_t iv[AES_IV_SIZE]);

// Cifra un buffer usando AES-128-CBC con padding PKCS#7.
int aes_encrypt_cbc(
    const uint8_t *plaintext,
    size_t plaintext_len,
    const uint8_t key[AES_KEY_SIZE],
    const uint8_t iv[AES_IV_SIZE],
    uint8_t **ciphertext,
    size_t *ciphertext_len
);

// Descifra un buffer usando AES-128-CBC.
int aes_decrypt_cbc(
    const uint8_t *ciphertext,
    size_t ciphertext_len,
    const uint8_t key[AES_KEY_SIZE],
    const uint8_t iv[AES_IV_SIZE],
    uint8_t **plaintext,
    size_t *plaintext_len
);

// Libera buffers asignados por la biblioteca AES.
void aes_free_buffer(uint8_t *buffer);

#endif
