#ifndef ECRYPT_PORTABLE
#define ECRYPT_PORTABLE

#include "ecrypt-config.h"
#include "ecrypt-machine.h"

/* Функции для работы с байтовым порядком */
#ifdef ECRYPT_LITTLE_ENDIAN

#define U8TO16_LITTLE(p) (*(const u16*)(p))
#define U8TO32_LITTLE(p) (*(const u32*)(p))
#define U8TO64_LITTLE(p) (*(const u64*)(p))

#define U16TO8_LITTLE(p, v) (*(u16*)(p) = (v))
#define U32TO8_LITTLE(p, v) (*(u32*)(p) = (v))
#define U64TO8_LITTLE(p, v) (*(u64*)(p) = (v))

#define U8TO16_BIG(p) SWAP16(*(const u16*)(p))
#define U8TO32_BIG(p) SWAP32(*(const u32*)(p))
#define U8TO64_BIG(p) SWAP64(*(const u64*)(p))

#else

#define U8TO16_BIG(p) (*(const u16*)(p))
#define U8TO32_BIG(p) (*(const u32*)(p))
#define U8TO64_BIG(p) (*(const u64*)(p))

#define U16TO8_BIG(p, v) (*(u16*)(p) = (v))
#define U32TO8_BIG(p, v) (*(u32*)(p) = (v))
#define U64TO8_BIG(p, v) (*(u64*)(p) = (v))

#define U8TO16_LITTLE(p) SWAP16(*(const u16*)(p))
#define U8TO32_LITTLE(p) SWAP32(*(const u32*)(p))
#define U8TO64_LITTLE(p) SWAP64(*(const u64*)(p))

#endif

/* Swap операции */
#define SWAP16(v) ROTL16(v, 8)
#define SWAP32(v) ((ROTL32(v, 8) & 0x00FF00FF) | (ROTL32(v, 24) & 0xFF00FF00))
#define SWAP64(v) ((ROTL64(v, 8) & 0x000000FF000000FF) | \
                   (ROTL64(v, 24) & 0x0000FF000000FF00) | \
                   (ROTL64(v, 40) & 0x00FF000000FF0000) | \
                   (ROTL64(v, 56) & 0xFF000000FF000000))

/* Альтернативные реализации для больших данных */
#define U8TO16_LITTLE_ALT(p) (((u16)((p)[0])) | ((u16)((p)[1]) << 8))
#define U8TO32_LITTLE_ALT(p) (((u32)((p)[0])) | ((u32)((p)[1]) << 8) | \
                              ((u32)((p)[2]) << 16) | ((u32)((p)[3]) << 24))

#endif
