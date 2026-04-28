#include "ecdh.h"
#include <cstring>
#include <ctime>
#include <unistd.h>
#include <random>

const char* ECDH_P256::p_hex = "FFFFFFFF00000001000000000000000000000000FFFFFFFFFFFFFFFFFFFFFFFF";
const char* ECDH_P256::a_hex = "FFFFFFFF00000001000000000000000000000000FFFFFFFFFFFFFFFFFFFFFFFC";
const char* ECDH_P256::b_hex = "5AC635D8AA3A93E7B3EBBD55769886BC651D06B0CC53B0F63BCE3C3E27D2604B";
const char* ECDH_P256::Gx_hex = "6B17D1F2E12C4247F8BCE6E563A440F277037D812DEB33A0F4A13945D898C296";
const char* ECDH_P256::Gy_hex = "4FE342E2FE1A7F9B8EE7EB4A7C0F9E162BCE33576B315ECCEBB6406837BF51F5";
const char* ECDH_P256::q_hex = "FFFFFFFF00000000FFFFFFFFFFFFFFFFBCE6FAADA7179E84F3B9CAC2FC632551";

ECDH_P256::ECDH_P256() {
    mpz_init_set_str(p, p_hex, 16);
    mpz_init_set_str(a, a_hex, 16);
    mpz_init_set_str(b, b_hex, 16);
    mpz_init_set_str(q, q_hex, 16);
    mpz_init_set_str(Gx, Gx_hex, 16);
    mpz_init_set_str(Gy, Gy_hex, 16);
}

ECDH_P256::~ECDH_P256() {
    mpz_clear(p);
    mpz_clear(a);
    mpz_clear(b);
    mpz_clear(q);
    mpz_clear(Gx);
    mpz_clear(Gy);
}

void ECDH_P256::modAdd(const mpz_t a, const mpz_t b, const mpz_t m, mpz_t res) {
    mpz_add(res, a, b);
    mpz_mod(res, res, m);
}

void ECDH_P256::modSub(const mpz_t a, const mpz_t b, const mpz_t m, mpz_t res) {
    mpz_sub(res, a, b);
    mpz_mod(res, res, m);
}

void ECDH_P256::modMul(const mpz_t a, const mpz_t b, const mpz_t m, mpz_t res) {
    mpz_mul(res, a, b);
    mpz_mod(res, res, m);
}

void ECDH_P256::modInv(const mpz_t a, const mpz_t m, mpz_t res) {
    mpz_invert(res, a, m);
}

void ECDH_P256::pointDouble(const mpz_t x1, const mpz_t y1, mpz_t x3, mpz_t y3) {
    mpz_t lambda, num, den;
    mpz_inits(lambda, num, den, nullptr);
    
    mpz_mul(num, x1, x1);
    mpz_mod(num, num, p);
    mpz_mul_ui(num, num, 3);
    mpz_add(num, num, a);
    mpz_mod(num, num, p);
    
    mpz_mul_ui(den, y1, 2);
    mpz_mod(den, den, p);
    mpz_invert(den, den, p);
    
    mpz_mul(lambda, num, den);
    mpz_mod(lambda, lambda, p);
    
    mpz_mul(x3, lambda, lambda);
    mpz_mod(x3, x3, p);
    mpz_mul_ui(num, x1, 2);
    mpz_sub(x3, x3, num);
    mpz_mod(x3, x3, p);
    
    mpz_sub(y3, x1, x3);
    mpz_mul(y3, lambda, y3);
    mpz_mod(y3, y3, p);
    mpz_sub(y3, y3, y1);
    mpz_mod(y3, y3, p);
    
    mpz_clears(lambda, num, den, nullptr);
}

void ECDH_P256::pointAdd(const mpz_t x1, const mpz_t y1, const mpz_t x2, const mpz_t y2, 
                         mpz_t x3, mpz_t y3) {
    mpz_t lambda, num, den;
    mpz_inits(lambda, num, den, nullptr);
    
    mpz_sub(num, y2, y1);
    mpz_mod(num, num, p);
    mpz_sub(den, x2, x1);
    mpz_mod(den, den, p);
    mpz_invert(den, den, p);
    mpz_mul(lambda, num, den);
    mpz_mod(lambda, lambda, p);
    
    mpz_mul(x3, lambda, lambda);
    mpz_mod(x3, x3, p);
    mpz_sub(x3, x3, x1);
    mpz_sub(x3, x3, x2);
    mpz_mod(x3, x3, p);
    
    mpz_sub(y3, x1, x3);
    mpz_mul(y3, lambda, y3);
    mpz_mod(y3, y3, p);
    mpz_sub(y3, y3, y1);
    mpz_mod(y3, y3, p);
    
    mpz_clears(lambda, num, den, nullptr);
}

void ECDH_P256::scalarMult(const mpz_t k, const mpz_t x1, const mpz_t y1, 
                           mpz_t x3, mpz_t y3) {
    mpz_set_ui(x3, 0);
    mpz_set_ui(y3, 0);
    
    mpz_t tx, ty, kk;
    mpz_init_set(tx, x1);
    mpz_init_set(ty, y1);
    mpz_init_set(kk, k);
    
    while (mpz_sgn(kk) > 0) {
        if (mpz_odd_p(kk)) {
            if (mpz_sgn(x3) == 0 && mpz_sgn(y3) == 0) {
                mpz_set(x3, tx);
                mpz_set(y3, ty);
            } else {
                mpz_t newX, newY;
                mpz_inits(newX, newY, nullptr);
                pointAdd(x3, y3, tx, ty, newX, newY);
                mpz_set(x3, newX);
                mpz_set(y3, newY);
                mpz_clears(newX, newY, nullptr);
            }
        }
        
        mpz_t newX, newY;
        mpz_inits(newX, newY, nullptr);
        pointDouble(tx, ty, newX, newY);
        mpz_set(tx, newX);
        mpz_set(ty, newY);
        mpz_clears(newX, newY, nullptr);
        
        mpz_fdiv_q_2exp(kk, kk, 1);
    }
    
    mpz_clears(tx, ty, kk, nullptr);
}

void ECDH_P256::generateKeyPair(mpz_t privateKey, mpz_t pubX, mpz_t pubY) {
    gmp_randstate_t state;
    gmp_randinit_default(state);
    
    std::random_device rd;
    unsigned long seed = rd() ^ (time(nullptr) << 16) ^ (getpid() << 8);
    gmp_randseed_ui(state, seed);
    
    do {
        mpz_urandomm(privateKey, state, q);
    } while (mpz_sgn(privateKey) == 0);
    
    scalarMult(privateKey, Gx, Gy, pubX, pubY);
    gmp_randclear(state);
}

void ECDH_P256::computeSharedSecret(const mpz_t privateKey, const mpz_t pubX, 
                                    const mpz_t pubY, mpz_t sharedSecret) {
    mpz_t Sx, Sy;
    mpz_inits(Sx, Sy, nullptr);
    scalarMult(privateKey, pubX, pubY, Sx, Sy);
    mpz_set(sharedSecret, Sx);
    mpz_clears(Sx, Sy, nullptr);
}

void ECDH_P256::exportPublicKey(const mpz_t pubX, const mpz_t pubY, 
                                uint8_t* outX, uint8_t* outY) {
    memset(outX, 0, 32);
    memset(outY, 0, 32);
    
    size_t countX, countY;
    mpz_export(outX, &countX, 1, 1, 1, 0, pubX);
    if (countX < 32) {
        memmove(outX + (32 - countX), outX, countX);
        memset(outX, 0, 32 - countX);
    }
    
    mpz_export(outY, &countY, 1, 1, 1, 0, pubY);
    if (countY < 32) {
        memmove(outY + (32 - countY), outY, countY);
        memset(outY, 0, 32 - countY);
    }
}

void ECDH_P256::importPublicKey(const uint8_t* inX, const uint8_t* inY, 
                                mpz_t pubX, mpz_t pubY) {
    mpz_import(pubX, 32, 1, 1, 1, 0, inX);
    mpz_import(pubY, 32, 1, 1, 1, 0, inY);
}
