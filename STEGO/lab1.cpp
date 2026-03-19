#include <iostream>
#include <fstream>
#include <vector>
#include <string>
#include <cstdint>
#include <bitset>
#include <filesystem>
#include <cmath>
#include <map>
#include <iomanip>
#include <algorithm>

namespace fs = std::filesystem;

#pragma pack(push, 1)
struct BMPHeader {
    uint16_t bfType;
    uint32_t bfSize;
    uint16_t bfReserved1;
    uint16_t bfReserved2;
    uint32_t bfOffBits;

    uint32_t biSize;
    int32_t  biWidth;
    int32_t  biHeight;
    uint16_t biPlanes;
    uint16_t biBitCount;
    uint32_t biCompression;
    uint32_t biSizeImage;
    int32_t  biXPelsPerMeter;
    int32_t  biYPelsPerMeter;
    uint32_t biClrUsed;
    uint32_t biClrImportant;
};
#pragma pack(pop)

class GrayBMP {
private:
    BMPHeader header;
    std::vector<uint8_t> palette;
    std::vector<uint8_t> pixels;
    int width, height;
    bool isLoaded;

    bool readBMP(const std::string& filename) {
        std::ifstream file(filename, std::ios::binary);
        if (!file) return false;

        file.read(reinterpret_cast<char*>(&header), sizeof(header));
        if (header.bfType != 0x4D42 || header.biBitCount != 8) {
            return false;
        }

        width = header.biWidth;
        height = std::abs(header.biHeight);
        isLoaded = false;

        palette.resize(1024);
        file.seekg(sizeof(header), std::ios::beg);
        file.read(reinterpret_cast<char*>(palette.data()), 1024);

        file.seekg(header.bfOffBits, std::ios::beg);
        int rowSize = (width * 8 + 31) / 32 * 4;
        int dataSize = rowSize * height;
        std::vector<uint8_t> rawData(dataSize);
        file.read(reinterpret_cast<char*>(rawData.data()), dataSize);

        pixels.resize(width * height);
        for (int y = 0; y < height; y++) {
            int srcY = (header.biHeight > 0) ? (height - 1 - y) : y;
            for (int x = 0; x < width; x++) {
                pixels[y * width + x] = rawData[srcY * rowSize + x];
            }
        }

        isLoaded = true;
        file.close();
        return true;
    }

    bool writeBMP(const std::string& filename) {
        if (!isLoaded) return false;

        std::ofstream file(filename, std::ios::binary);
        if (!file) return false;

        int rowSize = (width * 8 + 31) / 32 * 4;
        int dataSize = rowSize * height;
        std::vector<uint8_t> rawData(dataSize, 0);

        for (int y = 0; y < height; y++) {
            int dstY = (header.biHeight > 0) ? (height - 1 - y) : y;
            for (int x = 0; x < width; x++) {
                rawData[dstY * rowSize + x] = pixels[y * width + x];
            }
        }

        header.bfOffBits = sizeof(header) + 1024;
        header.bfSize = header.bfOffBits + dataSize;
        header.biSizeImage = dataSize;

        file.write(reinterpret_cast<char*>(&header), sizeof(header));
        file.write(reinterpret_cast<char*>(palette.data()), 1024);
        file.write(reinterpret_cast<char*>(rawData.data()), dataSize);

        file.close();
        return true;
    }

public:
    GrayBMP() : isLoaded(false), width(0), height(0) {}

    bool load(const std::string& filename) {
        return readBMP(filename);
    }

    bool save(const std::string& filename) {
        return writeBMP(filename);
    }

    int getWidth() const { return width; }
    int getHeight() const { return height; }
    int getSize() const { return width * height; }
    uint8_t* getPixelData() { return pixels.data(); }
    const uint8_t* getPixelData() const { return pixels.data(); }

    GrayBMP extractBitPlane(int k) {
        GrayBMP result;
        if (!isLoaded || k < 1 || k > 8) return result;

        result.header = this->header;
        result.palette = this->palette;
        result.width = this->width;
        result.height = this->height;
        result.isLoaded = true;

        for (int i = 0; i < 256; i++) {
            uint8_t val = (i == 0) ? 0 : (i == 255 ? 255 : i);
            result.palette[i*4 + 0] = val;
            result.palette[i*4 + 1] = val;
            result.palette[i*4 + 2] = val;
            result.palette[i*4 + 3] = 0;
        }

        result.pixels.resize(width * height);
        int bitPos = k - 1;
        for (size_t i = 0; i < pixels.size(); i++) {
            int bit = (pixels[i] >> bitPos) & 1;
            result.pixels[i] = bit ? 255 : 0;
        }

        return result;
    }

    int embedMessage(const std::string& messageFile, int k, const std::string& outputFile) {
        if (!isLoaded || k < 1 || k > 8) return -1;

        std::ifstream msgFile(messageFile, std::ios::binary);
        if (!msgFile) {
            std::cerr << "Не удалось открыть файл сообщения: " << messageFile << std::endl;
            return -1;
        }

        std::vector<uint8_t> messageData;
        msgFile.seekg(0, std::ios::end);
        std::streampos fileSize = msgFile.tellg();
        msgFile.seekg(0, std::ios::beg);

        if (fileSize <= 0) {
            std::cerr << "Файл сообщения пуст." << std::endl;
            return -1;
        }

        messageData.resize(fileSize);
        msgFile.read(reinterpret_cast<char*>(messageData.data()), fileSize);
        msgFile.close();

        int capacity = pixels.size();
        int messageBits = messageData.size() * 8;

        if (messageBits > capacity) {
            std::cerr << "Сообщение слишком большое! Нужно " << messageBits 
                      << " бит, доступно " << capacity << " бит." << std::endl;
            return -1;
        }

        int bitPos = k - 1;
        size_t pixelIdx = 0;
        int bitsWritten = 0;

        for (size_t byteIdx = 0; byteIdx < messageData.size(); byteIdx++) {
            for (int b = 0; b < 8; b++) {
                if (pixelIdx >= pixels.size()) break;
                
                int msgBit = (messageData[byteIdx] >> b) & 1;
                pixels[pixelIdx] = (pixels[pixelIdx] & ~(1 << bitPos)) | (msgBit << bitPos);
                
                pixelIdx++;
                bitsWritten++;
            }
        }

        if (!writeBMP(outputFile)) {
            std::cerr << "Не удалось сохранить BMP файл: " << outputFile << std::endl;
            return -1;
        }

        return bitsWritten;
    }

    bool extractMessage(int k, const std::string& outputFile, int messageBits = -1) {
        if (!isLoaded || k < 1 || k > 8) return false;

        int bitPos = k - 1;
        std::vector<uint8_t> extractedData;

        if (messageBits < 0) {
            messageBits = pixels.size();
        }

        int bytesNeeded = (messageBits + 7) / 8;
        extractedData.resize(bytesNeeded, 0);

        size_t pixelIdx = 0;
        int bitsExtracted = 0;

        for (int byteIdx = 0; byteIdx < bytesNeeded; byteIdx++) {
            for (int b = 0; b < 8; b++) {
                if (pixelIdx >= pixels.size() || bitsExtracted >= messageBits) break;
                
                int bit = (pixels[pixelIdx] >> bitPos) & 1;
                extractedData[byteIdx] |= (bit << b);
                
                pixelIdx++;
                bitsExtracted++;
            }
        }

        std::ofstream outFile(outputFile, std::ios::binary);
        if (!outFile) {
            std::cerr << "Не удалось создать файл: " << outputFile << std::endl;
            return false;
        }

        outFile.write(reinterpret_cast<char*>(extractedData.data()), 
                     (bitsExtracted + 7) / 8);
        outFile.close();

        std::cout << "Извлечено " << bitsExtracted << " бит (" 
                  << (bitsExtracted + 7) / 8 << " байт)" << std::endl;
        return true;
    }
};

int main(int argc, char* argv[]) {
    setlocale(LC_ALL, "");

    GrayBMP image;
    std::string command;
    while(true)
    {
        std::cout << "Введите режим работы:\n1 - Извлечь битовые плоскости\n2 - Внедрить сообщение в битовую плоскость\n3 - Извлечть сообщение из битовой плоскости\nq - Выйти из программы" << std::endl;
        std::cin >> command;
        if (command == "1")
        {
            std::cout << "Введите путь до файла *bmp: ";
            std::string inputFile;
            std::cin >> inputFile;

            if (!image.load(inputFile)) {
                std::cout << "Файл " << inputFile << " не найден.\n";
                continue;
            }

            std::cout << "Введите номер бита(1-8): ";
            int k;
            std::cin >> k;

            GrayBMP plane = image.extractBitPlane(k);

            std::cout << "Введите название выходного файла: ";
            std::string outFile;
            std::cin >> outFile;
            outFile += ".bmp";
            
            plane.save(outFile);
            std::cout << "Плоскость " << k << " сохранена в " << outFile << std::endl;
        }
        else if (command == "2")
        {
            std::cout << "Введите путь до файла *bmp: ";
            std::string inputFile;
            std::cin >> inputFile;

            if (!image.load(inputFile)) {
                std::cout << "Файл " << inputFile << " не найден.\n";
                continue;
            }

            std::cout << "Введите номер бита(1-8): ";
            int k;
            std::cin >> k;

            std::cout << "Введите путь до сообщения: ";
            std::string msgFile;
            std::cin >> msgFile;

            std::cout << "Введите название выходного файла: ";
            std::string outFile;
            std::cin >> outFile;
            outFile += ".bmp";

            int bits = image.embedMessage(msgFile, k, outFile);
            if (bits > 0) {
                std::cout << "Внедрено " << bits << " бит в плоскость " << k << std::endl;
            }
        }
        else if (command == "3")
        {
            std::cout << "Введите путь до файла *bmp: ";
            std::string inputFile;
            std::cin >> inputFile;

            if (!image.load(inputFile)) {
                std::cout << "Файл " << inputFile << " не найден.\n";
                continue;
            }

            std::cout << "Введите номер бита(1-8): ";
            int k;
            std::cin >> k;
            
            std::cout << "Введите количество записанных битов: ";
            int bits;
            std::cin >> bits;

            std::cout << "Введите название выходного файла: ";
            std::string outFile;
            std::cin >> outFile;
            outFile += ".txt";

            image.extractMessage(k, outFile, bits);
        }
        else if (command == "q")
            break;
    }

    return 0;
}