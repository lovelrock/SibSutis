#ifndef ECDH_H
#define ECDH_H

#include <gmp.h>
#include <vector>
#include <cstdint>

class ECDH_P256 {
private:
    mpz_t p, a, b, q;
    mpz_t Gx, Gy;
    
    static const char* p_hex;
    static const char* a_hex;
    static const char* b_hex;
    static const char* q_hex;
    static const char* Gx_hex;
    static const char* Gy_hex;
    
    void modAdd(const mpz_t a, const mpz_t b, const mpz_t m, mpz_t res);
    void modSub(const mpz_t a, const mpz_t b, const mpz_t m, mpz_t res);
    void modMul(const mpz_t a, const mpz_t b, const mpz_t m, mpz_t res);
    void modInv(const mpz_t a, const mpz_t m, mpz_t res);
    void pointDouble(const mpz_t x1, const mpz_t y1, mpz_t x3, mpz_t y3);
    void pointAdd(const mpz_t x1, const mpz_t y1, const mpz_t x2, const mpz_t y2, 
                  mpz_t x3, mpz_t y3);
    void scalarMult(const mpz_t k, const mpz_t x1, const mpz_t y1, 
                    mpz_t x3, mpz_t y3);
    
public:
    ECDH_P256();
    ~ECDH_P256();
    
    void generateKeyPair(mpz_t privateKey, mpz_t pubX, mpz_t pubY);
    void computeSharedSecret(const mpz_t privateKey, const mpz_t pubX, 
                             const mpz_t pubY, mpz_t sharedSecret);
    void exportPublicKey(const mpz_t pubX, const mpz_t pubY, 
                         uint8_t* outX, uint8_t* outY);
    void importPublicKey(const uint8_t* inX, const uint8_t* inY, 
                         mpz_t pubX, mpz_t pubY);
    
    const mpz_t& getQ() const { return q; }
};

#endif
