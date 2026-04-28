#ifndef ECRYPT_MACHINE
#define ECRYPT_MACHINE

/* Простая реализация циклического сдвига (работает на всех компиляторах) */

#define ROTL8(v, n)  (U8V(((v) << (n)) | ((v) >> (8 - (n)))))
#define ROTL16(v, n) (U16V(((v) << (n)) | ((v) >> (16 - (n)))))
#define ROTL32(v, n) (U32V(((v) << (n)) | ((v) >> (32 - (n)))))
#define ROTL64(v, n) (U64V(((v) << (n)) | ((v) >> (64 - (n)))))

#define ROTR8(v, n)  ROTL8(v, 8 - (n))
#define ROTR16(v, n) ROTL16(v, 16 - (n))
#define ROTR32(v, n) ROTL32(v, 32 - (n))
#define ROTR64(v, n) ROTL64(v, 64 - (n))

#endif
