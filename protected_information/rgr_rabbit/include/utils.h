#ifndef UTILS_H
#define UTILS_H

#include <vector>
#include <string>
#include <cstdint>
#include <gmp.h>
std::vector<uint8_t> mpzToVector(const mpz_t value);
std::string bytesToHex(const uint8_t* bytes, size_t len);
std::vector<uint8_t> hexToBytes(const std::string& hex);
void printProgress(double percentage);

#endif
