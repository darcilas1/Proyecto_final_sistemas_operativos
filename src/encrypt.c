#include "encrypt.h"

#include <errno.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <unistd.h>

#define RC4_STATE_SIZE 256U
#define MAX_KEY_SIZE 1024U

struct secure_key {
    uint8_t *data;
    size_t len;
    size_t locked_len;
};

static void rc4_swap(uint8_t *a, uint8_t *b)
{
    uint8_t tmp;

    tmp = *a;
    *a = *b;
    *b = tmp;
}

static void rc4_crypt(
    const uint8_t *in,
    size_t in_len,
    const uint8_t *key,
    size_t key_len,
    uint8_t *out
)
{
    uint8_t s[RC4_STATE_SIZE];
    unsigned int i;
    unsigned int j;
    size_t n;

    for (i = 0U; i < RC4_STATE_SIZE; i++) {
        s[i] = (uint8_t)i;
    }

    j = 0U;
    for (i = 0U; i < RC4_STATE_SIZE; i++) {
        j = (j + s[i] + key[i % key_len]) & 0xffU;
        rc4_swap(&s[i], &s[j]);
    }

    i = 0U;
    j = 0U;
    for (n = 0U; n < in_len; n++) {
        uint8_t k;

        i = (i + 1U) & 0xffU;
        j = (j + s[i]) & 0xffU;
        rc4_swap(&s[i], &s[j]);
        k = s[(s[i] + s[j]) & 0xffU];
        out[n] = in[n] ^ k;
    }

    explicit_bzero(s, sizeof(s));
}

static int read_secure_key(const char *prompt, struct secure_key *key)
{
    char *typed_key;
    size_t typed_len;
    uint8_t *locked_key;

    if (prompt == NULL || key == NULL) {
        errno = EINVAL;
        return -1;
    }

    key->data = NULL;
    key->len = 0U;
    key->locked_len = 0U;

    typed_key = getpass(prompt);
    if (typed_key == NULL) {
        return -1;
    }

    typed_len = strnlen(typed_key, MAX_KEY_SIZE + 1U);
    if (typed_len == 0U || typed_len > MAX_KEY_SIZE) {
        explicit_bzero(typed_key, typed_len);
        errno = EINVAL;
        return -1;
    }

    locked_key = (uint8_t *)malloc(typed_len);
    if (locked_key == NULL) {
        explicit_bzero(typed_key, typed_len);
        return -1;
    }

    if (mlock(locked_key, typed_len) != 0) {
        explicit_bzero(typed_key, typed_len);
        explicit_bzero(locked_key, typed_len);
        free(locked_key);
        return -1;
    }

    memcpy(locked_key, typed_key, typed_len);
    explicit_bzero(typed_key, typed_len);

    key->data = locked_key;
    key->len = typed_len;
    key->locked_len = typed_len;
    return 0;
}

static void destroy_secure_key(struct secure_key *key)
{
    if (key == NULL || key->data == NULL) {
        return;
    }

    explicit_bzero(key->data, key->locked_len);
    (void)munlock(key->data, key->locked_len);
    free(key->data);

    key->data = NULL;
    key->len = 0U;
    key->locked_len = 0U;
}

void destroy_secure_key_data(uint8_t *key_data, size_t key_len)
{
    if (key_data == NULL) {
        return;
    }

    explicit_bzero(key_data, key_len);
    (void)munlock(key_data, key_len);
    free(key_data);
}

int prompt_secure_key(const char *prompt, uint8_t **key_data, size_t *key_len)
{
    struct secure_key key;

    if (prompt == NULL || key_data == NULL || key_len == NULL) {
        errno = EINVAL;
        return -1;
    }

    *key_data = NULL;
    *key_len = 0U;

    if (read_secure_key(prompt, &key) != 0) {
        return -1;
    }

    *key_data = key.data;
    *key_len = key.len;
    return 0;
}

static int crypt_buffer_with_key(
    const uint8_t *in,
    size_t in_len,
    const uint8_t *key,
    size_t key_len,
    uint8_t **out,
    size_t *out_len
)
{
    uint8_t *encrypted_data;

    if ((in == NULL && in_len > 0U) || key == NULL || key_len == 0U ||
        out == NULL || out_len == NULL) {
        errno = EINVAL;
        return -1;
    }

    *out = NULL;
    *out_len = 0U;

    encrypted_data = NULL;
    if (in_len > 0U) {
        encrypted_data = (uint8_t *)malloc(in_len);
        if (encrypted_data == NULL) {
            return -1;
        }

        rc4_crypt(in, in_len, key, key_len, encrypted_data);
    }

    *out = encrypted_data;
    *out_len = in_len;
    return 0;
}

static int crypt_buffer_with_prompt(
    const uint8_t *in,
    size_t in_len,
    uint8_t **out,
    size_t *out_len,
    const char *prompt
)
{
    struct secure_key key;
    int status;

    if ((in == NULL && in_len > 0U) || out == NULL || out_len == NULL) {
        errno = EINVAL;
        return -1;
    }

    if (read_secure_key(prompt, &key) != 0) {
        return -1;
    }

    status = crypt_buffer_with_key(in, in_len, key.data, key.len, out, out_len);
    destroy_secure_key(&key);
    return status;
}

int encrypt_buffer_with_key(
    const uint8_t *in,
    size_t in_len,
    const uint8_t *key,
    size_t key_len,
    uint8_t **out,
    size_t *out_len
)
{
    return crypt_buffer_with_key(in, in_len, key, key_len, out, out_len);
}

int decrypt_buffer_with_key(
    const uint8_t *in,
    size_t in_len,
    const uint8_t *key,
    size_t key_len,
    uint8_t **out,
    size_t *out_len
)
{
    return crypt_buffer_with_key(in, in_len, key, key_len, out, out_len);
}

int encrypt_buffer(const uint8_t *in, size_t in_len, uint8_t **out, size_t *out_len)
{
    return crypt_buffer_with_prompt(in, in_len, out, out_len, "Llave de cifrado: ");
}

int decrypt_buffer(const uint8_t *in, size_t in_len, uint8_t **out, size_t *out_len)
{
    return crypt_buffer_with_prompt(in, in_len, out, out_len, "Llave de descifrado: ");
}
