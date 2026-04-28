#ifndef ECRYPT_CONFIG
#define ECRYPT_CONFIG

/* Определяем little-endian для x86/x86_64 */
#if defined(__i386__) || defined(__x86_64__) || defined(_M_IX86) || defined(_M_X64)
#define ECRYPT_LITTLE_ENDIAN
#elif defined(__ppc__) || defined(__PPC__)
#define ECRYPT_BIG_ENDIAN
#else
#define ECRYPT_UNKNOWN
#endif

/* Определяем типы минимальной ширины */
#include <stdint.h>

typedef uint8_t u8;
typedef uint16_t u16;
typedef uint32_t u32;
typedef uint64_t u64;

#define U8C(v) (v##U)
#define U16C(v) (v##U)
#define U32C(v) (v##U)
#define U64C(v) (v##ULL)

#define U8V(v) ((u8)(v) & U8C(0xFF))
#define U16V(v) ((u16)(v) & U16C(0xFFFF))
#define U32V(v) ((u32)(v) & U32C(0xFFFFFFFF))
#define U64V(v) ((u64)(v) & U64C(0xFFFFFFFFFFFFFFFF))

#endif
