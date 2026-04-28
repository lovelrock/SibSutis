#include "utils.h"
#include <gmp.h>
#include <iomanip>
#include <sstream>
#include <iostream>

std::vector<uint8_t> mpzToVector(const mpz_t value) {
    size_t size = (mpz_sizeinbase(value, 2) + 7) / 8;
    if (size == 0) size = 1;
    
    std::vector<uint8_t> result(size);
    mpz_export(result.data(), nullptr, 1, 1, 1, 0, value);
    return result;
}

std::string bytesToHex(const uint8_t* bytes, size_t len) {
    std::ostringstream oss;
    oss << std::hex << std::setfill('0');
    for (size_t i = 0; i < len; i++) {
        oss << std::setw(2) << static_cast<int>(bytes[i]);
    }
    return oss.str();
}

std::vector<uint8_t> hexToBytes(const std::string& hex) {
    std::vector<uint8_t> bytes;
    for (size_t i = 0; i < hex.length(); i += 2) {
        uint8_t byte = static_cast<uint8_t>(std::stoi(hex.substr(i, 2), nullptr, 16));
        bytes.push_back(byte);
    }
    return bytes;
}

void printProgress(double percentage) {
    int barWidth = 50;
    int pos = static_cast<int>(barWidth * percentage / 100.0);
    
    std::cout << "[";
    for (int i = 0; i < barWidth; i++) {
        if (i < pos) std::cout << "=";
        else if (i == pos) std::cout << ">";
        else std::cout << " ";
    }
    std::cout << "] " << std::fixed << std::setprecision(1) << percentage << "%\r";
    std::cout.flush();
}
