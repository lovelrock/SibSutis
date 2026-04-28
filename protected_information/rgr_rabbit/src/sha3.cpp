#include "sha3.h"
#include <cstring>
#include <sstream>
#include <iomanip>
#include <algorithm>

static const uint64_t keccakf_rndc[24] = {
    0x0000000000000001, 0x0000000000008082, 0x800000000000808a,
    0x8000000080008000, 0x000000000000808b, 0x0000000080000001,
    0x8000000080008081, 0x8000000000008009, 0x000000000000008a,
    0x0000000000000088, 0x0000000080008009, 0x000000008000000a,
    0x000000008000808b, 0x800000000000008b, 0x8000000000008089,
    0x8000000000008003, 0x8000000000008002, 0x8000000000000080,
    0x000000000000800a, 0x800000008000000a, 0x8000000080008081,
    0x8000000000008080, 0x0000000080000001, 0x8000000080008008
};

static const int keccakf_rotc[24] = {
    1, 3, 6, 10, 15, 21, 28, 36, 45, 55, 2, 14,
    27, 41, 56, 8, 25, 43, 62, 18, 39, 61, 20, 44
};

static const int keccakf_piln[24] = {
    10, 7, 11, 17, 18, 3, 5, 16, 8, 21, 24, 4,
    15, 23, 19, 13, 12, 2, 20, 14, 22, 9, 6, 1
};

static inline uint64_t rotl64(uint64_t x, int y) {
    return (x << y) | (x >> (64 - y));
}

void SHA3::keccak_f(uint64_t state[25]) {
    uint64_t t, bc[5];
    
    for (int round = 0; round < 24; round++) {
        for (int i = 0; i < 5; i++) {
            bc[i] = state[i] ^ state[i + 5] ^ state[i + 10] ^ state[i + 15] ^ state[i + 20];
        }
        
        for (int i = 0; i < 5; i++) {
            t = bc[(i + 4) % 5] ^ rotl64(bc[(i + 1) % 5], 1);
            for (int j = 0; j < 25; j += 5) {
                state[j + i] ^= t;
            }
        }
        
        t = state[1];
        for (int i = 0; i < 24; i++) {
            int j = keccakf_piln[i];
            bc[0] = state[j];
            state[j] = rotl64(t, keccakf_rotc[i]);
            t = bc[0];
        }
        
        for (int j = 0; j < 25; j += 5) {
            for (int i = 0; i < 5; i++) {
                bc[i] = state[j + i];
            }
            for (int i = 0; i < 5; i++) {
                state[j + i] ^= (~bc[(i + 1) % 5]) & bc[(i + 2) % 5];
            }
        }
        
        state[0] ^= keccakf_rndc[round];
    }
}

std::vector<uint8_t> SHA3::hash(const std::vector<uint8_t>& data) {
    return hash(data.data(), data.size());
}

std::vector<uint8_t> SHA3::hash(const uint8_t* data, size_t len) {
    uint64_t state[25] = {0};
    size_t rate = SHA3_256_RATE;
    size_t offset = 0;
    
    while (len > 0) {
        size_t chunk = std::min(len, rate - offset);
        for (size_t i = 0; i < chunk; i++) {
            ((uint8_t*)state)[offset + i] ^= data[i];
        }
        data += chunk;
        len -= chunk;
        offset += chunk;
        
        if (offset == rate) {
            keccak_f(state);
            offset = 0;
        }
    }
    
    ((uint8_t*)state)[offset] ^= 0x06;
    ((uint8_t*)state)[rate - 1] ^= 0x80;
    keccak_f(state);
    
    std::vector<uint8_t> digest(SHA3_256_DIGEST);
    memcpy(digest.data(), state, SHA3_256_DIGEST);
    
    return digest;
}

std::string SHA3::hashHex(const std::vector<uint8_t>& data) {
    auto digest = hash(data);
    std::ostringstream oss;
    oss << std::hex << std::setfill('0');
    for (uint8_t b : digest) {
        oss << std::setw(2) << static_cast<int>(b);
    }
    return oss.str();
}
