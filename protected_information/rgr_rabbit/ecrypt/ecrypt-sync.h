#ifndef ECRYPT_SYNC
#define ECRYPT_SYNC

#include "ecrypt-portable.h"

/* Параметры шифра Rabbit */
#define ECRYPT_NAME "Rabbit"
#define ECRYPT_MAXKEYSIZE 256
#define ECRYPT_KEYSIZE(i) (128 + (i) * 128)
#define ECRYPT_MAXIVSIZE 128
#define ECRYPT_IVSIZE(i) (64 + (i) * 64)
#define ECRYPT_BLOCKLENGTH 16

/* Контекст шифра Rabbit */
typedef struct {
    u32 x[8];
    u32 c[8];
    u32 carry;
} RABBIT_CTX;

typedef struct {
    RABBIT_CTX master_ctx;
    RABBIT_CTX work_ctx;
} ECRYPT_CTX;

/* Функции инициализации */
void ECRYPT_init(void);
void ECRYPT_keysetup(ECRYPT_CTX* ctx, const u8* key, u32 keysize, u32 ivsize);
void ECRYPT_ivsetup(ECRYPT_CTX* ctx, const u8* iv);

/* Функции шифрования */
void ECRYPT_encrypt_bytes(ECRYPT_CTX* ctx, const u8* plaintext, u8* ciphertext, u32 msglen);
void ECRYPT_decrypt_bytes(ECRYPT_CTX* ctx, const u8* ciphertext, u8* plaintext, u32 msglen);
void ECRYPT_keystream_bytes(ECRYPT_CTX* ctx, u8* keystream, u32 length);

#endif
