#ifndef SHA3_H
#define SHA3_H

#include <vector>
#include <cstdint>
#include <string>

class SHA3 {
public:
    static constexpr size_t SHA3_256_DIGEST = 32;
    static constexpr size_t SHA3_256_RATE = 136;
    
    static std::vector<uint8_t> hash(const std::vector<uint8_t>& data);
    static std::vector<uint8_t> hash(const uint8_t* data, size_t len);
    static std::string hashHex(const std::vector<uint8_t>& data);
    
private:
    static void keccak_f(uint64_t state[25]);
};

#endif
