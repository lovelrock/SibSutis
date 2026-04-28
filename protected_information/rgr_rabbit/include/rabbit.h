#ifndef RABBIT_H
#define RABBIT_H

#include <stdint.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef uint32_t u32;
typedef uint8_t u8;

typedef struct {
    u32 x[8];
    u32 c[8];
    u32 carry;
} RABBIT_CTX;

typedef struct {
    RABBIT_CTX master_ctx;
    RABBIT_CTX work_ctx;
} RABBIT_CIPHER_CTX;

void rabbit_init(void);
void rabbit_keysetup(RABBIT_CIPHER_CTX* ctx, const u8* key, size_t keybits);
void rabbit_ivsetup(RABBIT_CIPHER_CTX* ctx, const u8* iv);
void rabbit_encrypt_bytes(RABBIT_CIPHER_CTX* ctx, const u8* plaintext, 
                          u8* ciphertext, size_t msglen);
void rabbit_decrypt_bytes(RABBIT_CIPHER_CTX* ctx, const u8* ciphertext, 
                          u8* plaintext, size_t msglen);

#ifdef __cplusplus
}
#endif

#endif
