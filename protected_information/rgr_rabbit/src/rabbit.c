#include "rabbit.h"
#include <string.h>

#define ROTL32(v, n) (U32V(((v) << (n)) | ((v) >> (32 - (n)))))
#define U32V(x) ((x) & 0xFFFFFFFFUL)

/* Функция g(x) - ядро Rabbit */
static u32 rabbit_g_func(u32 x) {
    u32 a, b, h;
    a = x & 0xFFFF;
    b = x >> 16;
    h = (((U32V(a * a) >> 17) + U32V(a * b)) >> 15) + b * b;
    return U32V(h ^ (x * x));
}

/* Обновление внутреннего состояния */
static void rabbit_next_state(RABBIT_CTX* ctx) {
    u32 g[8], c_old[8];
    int i;
    
    /* Сохраняем старые счетчики */
    for (i = 0; i < 8; i++)
        c_old[i] = ctx->c[i];
    
    /* Обновляем счетчики */
    ctx->c[0] = U32V(ctx->c[0] + 0x4D34D34D + ctx->carry);
    ctx->c[1] = U32V(ctx->c[1] + 0xD34D34D3 + (ctx->c[0] < c_old[0]));
    ctx->c[2] = U32V(ctx->c[2] + 0x34D34D34 + (ctx->c[1] < c_old[1]));
    ctx->c[3] = U32V(ctx->c[3] + 0x4D34D34D + (ctx->c[2] < c_old[2]));
    ctx->c[4] = U32V(ctx->c[4] + 0xD34D34D3 + (ctx->c[3] < c_old[3]));
    ctx->c[5] = U32V(ctx->c[5] + 0x34D34D34 + (ctx->c[4] < c_old[4]));
    ctx->c[6] = U32V(ctx->c[6] + 0x4D34D34D + (ctx->c[5] < c_old[5]));
    ctx->c[7] = U32V(ctx->c[7] + 0xD34D34D3 + (ctx->c[6] < c_old[6]));
    ctx->carry = (ctx->c[7] < c_old[7]);
    
    /* Вычисляем g-значения */
    for (i = 0; i < 8; i++)
        g[i] = rabbit_g_func(U32V(ctx->x[i] + ctx->c[i]));
    
    /* Обновляем переменные состояния */
    ctx->x[0] = U32V(g[0] + ROTL32(g[7], 16) + ROTL32(g[6], 16));
    ctx->x[1] = U32V(g[1] + ROTL32(g[0], 8) + g[7]);
    ctx->x[2] = U32V(g[2] + ROTL32(g[1], 16) + ROTL32(g[0], 16));
    ctx->x[3] = U32V(g[3] + ROTL32(g[2], 8) + g[1]);
    ctx->x[4] = U32V(g[4] + ROTL32(g[3], 16) + ROTL32(g[2], 16));
    ctx->x[5] = U32V(g[5] + ROTL32(g[4], 8) + g[3]);
    ctx->x[6] = U32V(g[6] + ROTL32(g[5], 16) + ROTL32(g[4], 16));
    ctx->x[7] = U32V(g[7] + ROTL32(g[6], 8) + g[5]);
}

/* Извлечение ключевого потока */
static void rabbit_extract_keystream(RABBIT_CTX* ctx, u8* keystream) {
    u32 tmp[4];
    
    tmp[0] = ctx->x[0] ^ (ctx->x[5] >> 16) ^ (ctx->x[3] << 16);
    tmp[1] = ctx->x[2] ^ (ctx->x[7] >> 16) ^ (ctx->x[5] << 16);
    tmp[2] = ctx->x[4] ^ (ctx->x[1] >> 16) ^ (ctx->x[7] << 16);
    tmp[3] = ctx->x[6] ^ (ctx->x[3] >> 16) ^ (ctx->x[1] << 16);
    
    for (int i = 0; i < 4; i++) {
        keystream[i * 4 + 0] = (tmp[i] >> 0) & 0xFF;
        keystream[i * 4 + 1] = (tmp[i] >> 8) & 0xFF;
        keystream[i * 4 + 2] = (tmp[i] >> 16) & 0xFF;
        keystream[i * 4 + 3] = (tmp[i] >> 24) & 0xFF;
    }
}

/* Инициализация */
void rabbit_init(void) {}

/* Установка ключа */
void rabbit_keysetup(RABBIT_CIPHER_CTX* ctx, const u8* key, size_t keybits) {
    u32 k0, k1, k2, k3;
    int i;
    
    (void)keybits; /* Не используется, ключ всегда 256 бит */
    
    /* Извлекаем 4 32-битных слова из 128-битного ключа */
    k0 = ((u32)key[3] << 24) | ((u32)key[2] << 16) | ((u32)key[1] << 8) | key[0];
    k1 = ((u32)key[7] << 24) | ((u32)key[6] << 16) | ((u32)key[5] << 8) | key[4];
    k2 = ((u32)key[11] << 24) | ((u32)key[10] << 16) | ((u32)key[9] << 8) | key[8];
    k3 = ((u32)key[15] << 24) | ((u32)key[14] << 16) | ((u32)key[13] << 8) | key[12];
    
    ctx->master_ctx.x[0] = k0;
    ctx->master_ctx.x[2] = k1;
    ctx->master_ctx.x[4] = k2;
    ctx->master_ctx.x[6] = k3;
    ctx->master_ctx.x[1] = U32V(k3 << 16) | (k2 >> 16);
    ctx->master_ctx.x[3] = U32V(k0 << 16) | (k3 >> 16);
    ctx->master_ctx.x[5] = U32V(k1 << 16) | (k0 >> 16);
    ctx->master_ctx.x[7] = U32V(k2 << 16) | (k1 >> 16);
    
    ctx->master_ctx.c[0] = ROTL32(k2, 16);
    ctx->master_ctx.c[2] = ROTL32(k3, 16);
    ctx->master_ctx.c[4] = ROTL32(k0, 16);
    ctx->master_ctx.c[6] = ROTL32(k1, 16);
    ctx->master_ctx.c[1] = (k0 & 0xFFFF0000) | (k1 & 0xFFFF);
    ctx->master_ctx.c[3] = (k1 & 0xFFFF0000) | (k2 & 0xFFFF);
    ctx->master_ctx.c[5] = (k2 & 0xFFFF0000) | (k3 & 0xFFFF);
    ctx->master_ctx.c[7] = (k3 & 0xFFFF0000) | (k0 & 0xFFFF);
    
    ctx->master_ctx.carry = 0;
    
    for (i = 0; i < 4; i++)
        rabbit_next_state(&ctx->master_ctx);
    
    for (i = 0; i < 8; i++)
        ctx->master_ctx.c[i] ^= ctx->master_ctx.x[(i + 4) & 7];
    
    memcpy(&ctx->work_ctx, &ctx->master_ctx, sizeof(RABBIT_CTX));
}

/* Установка IV */
void rabbit_ivsetup(RABBIT_CIPHER_CTX* ctx, const u8* iv) {
    u32 i0, i1, i2, i3;
    int i;
    
    i0 = ((u32)iv[3] << 24) | ((u32)iv[2] << 16) | ((u32)iv[1] << 8) | iv[0];
    i2 = ((u32)iv[7] << 24) | ((u32)iv[6] << 16) | ((u32)iv[5] << 8) | iv[4];
    i1 = (i0 >> 16) | (i2 & 0xFFFF0000);
    i3 = (i2 << 16) | (i0 & 0x0000FFFF);
    
    memcpy(&ctx->work_ctx, &ctx->master_ctx, sizeof(RABBIT_CTX));
    
    ctx->work_ctx.c[0] ^= i0;
    ctx->work_ctx.c[1] ^= i1;
    ctx->work_ctx.c[2] ^= i2;
    ctx->work_ctx.c[3] ^= i3;
    ctx->work_ctx.c[4] ^= i0;
    ctx->work_ctx.c[5] ^= i1;
    ctx->work_ctx.c[6] ^= i2;
    ctx->work_ctx.c[7] ^= i3;
    
    for (i = 0; i < 4; i++)
        rabbit_next_state(&ctx->work_ctx);
}

/* Шифрование байт */
void rabbit_encrypt_bytes(RABBIT_CIPHER_CTX* ctx, const u8* plaintext, 
                          u8* ciphertext, size_t msglen) {
    u8 keystream[16];
    size_t i;
    
    while (msglen >= 16) {
        rabbit_next_state(&ctx->work_ctx);
        rabbit_extract_keystream(&ctx->work_ctx, keystream);
        
        for (i = 0; i < 16; i++) {
            if (plaintext)
                ciphertext[i] = plaintext[i] ^ keystream[i];
            else
                ciphertext[i] = keystream[i];
        }
        
        if (plaintext) plaintext += 16;
        ciphertext += 16;
        msglen -= 16;
    }
    
    if (msglen > 0) {
        rabbit_next_state(&ctx->work_ctx);
        rabbit_extract_keystream(&ctx->work_ctx, keystream);
        
        for (i = 0; i < msglen; i++) {
            if (plaintext)
                ciphertext[i] = plaintext[i] ^ keystream[i];
            else
                ciphertext[i] = keystream[i];
        }
    }
}

/* Дешифрование байт (то же самое) */
void rabbit_decrypt_bytes(RABBIT_CIPHER_CTX* ctx, const u8* ciphertext, 
                          u8* plaintext, size_t msglen) {
    rabbit_encrypt_bytes(ctx, ciphertext, plaintext, msglen);
}
