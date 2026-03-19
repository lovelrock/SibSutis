#include <iostream>
#include <fstream>
#include <vector>
#include <string>
#include <cstdint>
#include <random>
#include <cmath>
#include <filesystem>
#include <algorithm>
#include <iomanip>
#include <cstring>
#include <bitset>

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
    bool loaded;

    bool readBMP(const std::string& filename) {
        std::ifstream file(filename, std::ios::binary);
        if (!file) return false;

        file.read(reinterpret_cast<char*>(&header), sizeof(header));
        if (header.bfType != 0x4D42 || header.biBitCount != 8)
            return false;

        width = header.biWidth;
        height = std::abs(header.biHeight);

        palette.resize(1024);
        file.seekg(sizeof(header), std::ios::beg);
        file.read(reinterpret_cast<char*>(palette.data()), 1024);

        file.seekg(header.bfOffBits, std::ios::beg);
        int rowSize = (width * 8 + 31) / 32 * 4;
        int dataSize = rowSize * height;
        std::vector<uint8_t> rawData(dataSize);
        file.read(reinterpret_cast<char*>(rawData.data()), dataSize);

        pixels.resize(width * height);
        for (int y = 0; y < height; ++y) {
            int srcY = (header.biHeight > 0) ? (height - 1 - y) : y;
            for (int x = 0; x < width; ++x) {
                pixels[y * width + x] = rawData[srcY * rowSize + x];
            }
        }
        loaded = true;
        file.close();
        return true;
    }

    bool writeBMP(const std::string& filename) {
        if (!loaded) return false;

        int rowSize = (width * 8 + 31) / 32 * 4;
        int dataSize = rowSize * height;
        std::vector<uint8_t> rawData(dataSize, 0);

        for (int y = 0; y < height; ++y) {
            int dstY = (header.biHeight > 0) ? (height - 1 - y) : y;
            for (int x = 0; x < width; ++x) {
                rawData[dstY * rowSize + x] = pixels[y * width + x];
            }
        }

        header.bfOffBits = sizeof(header) + 1024;
        header.bfSize = header.bfOffBits + dataSize;
        header.biSizeImage = dataSize;

        std::ofstream file(filename, std::ios::binary);
        if (!file) return false;
        file.write(reinterpret_cast<char*>(&header), sizeof(header));
        file.write(reinterpret_cast<char*>(palette.data()), 1024);
        file.write(reinterpret_cast<char*>(rawData.data()), dataSize);
        file.close();
        return true;
    }

public:
    GrayBMP() : loaded(false), width(0), height(0) {}

    bool load(const std::string& filename) { return readBMP(filename); }
    bool save(const std::string& filename) { return writeBMP(filename); }

    int getWidth() const { return width; }
    int getHeight() const { return height; }
    int getSize() const { return width * height; }

    uint8_t* data() { return pixels.data(); }
    const uint8_t* data() const { return pixels.data(); }

    std::vector<uint8_t> getPixels() const { return pixels; }
    void setPixels(const std::vector<uint8_t>& newPixels) { pixels = newPixels; }

    GrayBMP clone() const {
        GrayBMP copy;
        copy.header = this->header;
        copy.palette = this->palette;
        copy.width = this->width;
        copy.height = this->height;
        copy.pixels = this->pixels;
        copy.loaded = this->loaded;
        return copy;
    }

    GrayBMP extractBitPlane(int k) {
        GrayBMP result;
        if (!loaded || k < 1 || k > 8) return result;

        result.header = this->header;
        result.palette = this->palette;
        result.width = this->width;
        result.height = this->height;
        result.loaded = true;

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
};

class Metrics {
public:
    static double MSE(const std::vector<uint8_t>& a, const std::vector<uint8_t>& b) {
        if (a.size() != b.size()) return -1.0;
        double sum = 0.0;
        for (size_t i = 0; i < a.size(); ++i) {
            double diff = static_cast<double>(a[i]) - static_cast<double>(b[i]);
            sum += diff * diff;
        }
        return sum / a.size();
    }

    static double PSNR(double mse) {
        if (mse <= 0) return 100.0;
        return 10.0 * log10((255.0 * 255.0) / mse);
    }
};

class Watermark {
private:
    std::vector<uint8_t> bits;
    int w, h;
public:
    Watermark() : w(0), h(0) {}

    bool loadFromBMP(const std::string& filename) {
        GrayBMP img;
        if (!img.load(filename)) return false;
        w = img.getWidth();
        h = img.getHeight();
        bits.resize(w * h);
        const uint8_t* pixels = img.data();
        for (int i = 0; i < w * h; ++i) {
            bits[i] = (pixels[i] > 127) ? 1 : 0;
        }
        return true;
    }

    int getWidth() const { return w; }
    int getHeight() const { return h; }
    int totalBits() const { return w * h; }

    const std::vector<uint8_t>& getBits() const { return bits; }
};

class Embedder {
public:
    virtual std::string name() const = 0;
    virtual bool embed(GrayBMP& container, const Watermark& wm, const std::string& key, GrayBMP& stego) = 0;
    virtual bool extract(const GrayBMP& stego, const std::string& key, int bitsTotal, std::vector<uint8_t>& extractedBits) = 0;
    virtual bool createWatermarkImage(const std::vector<uint8_t>& bits, int width, int height, const std::string& filename) = 0;
    virtual ~Embedder() {}
};

class BlockLSBEmbedder : public Embedder {
private:
    static constexpr int BLOCK_SIZE = 2;
    std::mt19937 rng;
    
    std::vector<int> getBlockOrder(int totalBlocks, const std::string& key) {
        std::vector<int> indices(totalBlocks);
        for (int i = 0; i < totalBlocks; ++i) indices[i] = i;
        
        std::seed_seq seed(key.begin(), key.end());
        rng.seed(seed);
        std::shuffle(indices.begin(), indices.end(), rng);
        return indices;
    }
    
    uint8_t embedBitInBlock(const std::vector<uint8_t>& block, int bit) {
        int sum = 0;
        for (uint8_t val : block) {
            sum += val;
        }
        int parity = sum % 2;
        
        if (parity == bit) {
            return block[0];
        } else {
            if (block[0] > 0) {
                return block[0] - 1;
            } else {
                return block[0] + 1;
            }
        }
    }
    
    int extractBitFromBlock(const std::vector<uint8_t>& block) {
        int sum = 0;
        for (uint8_t val : block) {
            sum += val;
        }
        return sum % 2;
    }

public:
    std::string name() const override { return "BlockLSB"; }

    bool embed(GrayBMP& container, const Watermark& wm, const std::string& key, GrayBMP& stego) override {
        int w = container.getWidth();
        int h = container.getHeight();
        int wmBits = wm.totalBits();
        
        int blocksX = w / BLOCK_SIZE;
        int blocksY = h / BLOCK_SIZE;
        int totalBlocks = blocksX * blocksY;
        
        if (wmBits > totalBlocks) {
            std::cerr << "Watermark too large! Need " << wmBits << " blocks, have " << totalBlocks << "\n";
            return false;
        }

        stego = container.clone();
        uint8_t* pixels = stego.data();
        const auto& wmBitsVec = wm.getBits();
        
        std::vector<int> blockOrder = getBlockOrder(totalBlocks, key);

        for (int i = 0; i < wmBits; ++i) {
            int blockIdx = blockOrder[i];
            int blockX = (blockIdx % blocksX) * BLOCK_SIZE;
            int blockY = (blockIdx / blocksX) * BLOCK_SIZE;
            
            std::vector<uint8_t> block;
            for (int by = 0; by < BLOCK_SIZE; ++by) {
                for (int bx = 0; bx < BLOCK_SIZE; ++bx) {
                    int px = blockX + bx;
                    int py = blockY + by;
                    block.push_back(pixels[py * w + px]);
                }
            }
            
            uint8_t newFirstPixel = embedBitInBlock(block, wmBitsVec[i]);
            pixels[blockY * w + blockX] = newFirstPixel;
        }
        
        return true;
    }

    bool extract(const GrayBMP& stego, const std::string& key, int bitsTotal, std::vector<uint8_t>& extractedBits) override {
        int w = stego.getWidth();
        int h = stego.getHeight();
        
        int blocksX = w / BLOCK_SIZE;
        int blocksY = h / BLOCK_SIZE;
        int totalBlocks = blocksX * blocksY;
        
        if (bitsTotal > totalBlocks) return false;
        
        const uint8_t* pixels = stego.data();
        extractedBits.resize(bitsTotal);
        
        std::vector<int> blockOrder = getBlockOrder(totalBlocks, key);

        for (int i = 0; i < bitsTotal; ++i) {
            int blockIdx = blockOrder[i];
            int blockX = (blockIdx % blocksX) * BLOCK_SIZE;
            int blockY = (blockIdx / blocksX) * BLOCK_SIZE;
            
            std::vector<uint8_t> block;
            for (int by = 0; by < BLOCK_SIZE; ++by) {
                for (int bx = 0; bx < BLOCK_SIZE; ++bx) {
                    int px = blockX + bx;
                    int py = blockY + by;
                    block.push_back(pixels[py * w + px]);
                }
            }
            
            extractedBits[i] = extractBitFromBlock(block);
        }
        
        return true;
    }

    bool createWatermarkImage(const std::vector<uint8_t>& bits, 
                              int width, int height, 
                              const std::string& filename) override {
        
        if (bits.size() != static_cast<size_t>(width * height)) {
            return false;
        }
        
        int rowSize = (width * 8 + 31) / 32 * 4;
        int dataSize = rowSize * height;
        
        BMPHeader header;
        std::memset(&header, 0, sizeof(header));
        
        header.bfType = 0x4D42;
        header.bfOffBits = sizeof(header) + 1024;
        header.bfSize = header.bfOffBits + dataSize;
        header.biSize = 40;
        header.biWidth = width;
        header.biHeight = height;
        header.biPlanes = 1;
        header.biBitCount = 8;
        header.biSizeImage = dataSize;
        header.biClrUsed = 256;
        
        std::ofstream file(filename, std::ios::binary);
        if (!file) return false;
        
        file.write(reinterpret_cast<const char*>(&header), sizeof(header));
        
        for (int i = 0; i < 256; ++i) {
            uint8_t paletteEntry[4] = {
                static_cast<uint8_t>(i),
                static_cast<uint8_t>(i),
                static_cast<uint8_t>(i),
                0
            };
            file.write(reinterpret_cast<const char*>(paletteEntry), 4);
        }
        
        std::vector<uint8_t> rawData(dataSize, 0);
        
        for (int y = 0; y < height; ++y) {
            for (int x = 0; x < width; ++x) {
                int pixelIndex = y * width + x;
                uint8_t value = bits[pixelIndex] ? 255 : 0;
                
                int dstY = (header.biHeight > 0) ? (height - 1 - y) : y;
                rawData[dstY * rowSize + x] = value;
            }
        }
        
        file.write(reinterpret_cast<const char*>(rawData.data()), dataSize);
        file.close();
        
        return true;
    }
};

class BlockAdaptiveEmbedder : public Embedder {
private:
    static constexpr int BLOCK_SIZE = 4;
    
    double computeBlockGradient(const GrayBMP& img, int startX, int startY) {
        int w = img.getWidth();
        const uint8_t* pixels = img.data();
        double totalGradient = 0.0;
        int count = 0;
        
        for (int y = startY; y < startY + BLOCK_SIZE; ++y) {
            for (int x = startX; x < startX + BLOCK_SIZE; ++x) {
                if (x > 0 && x < w - 1 && y > 0 && y < img.getHeight() - 1) {
                    double gx = pixels[y * w + x + 1] - pixels[y * w + x - 1];
                    double gy = pixels[(y + 1) * w + x] - pixels[(y - 1) * w + x];
                    totalGradient += std::sqrt(gx * gx + gy * gy);
                    count++;
                }
            }
        }
        
        return count > 0 ? totalGradient / count : 0.0;
    }
    
    uint8_t embedBitInBlock(const std::vector<uint8_t>& block, int bit) {
        int sum = 0;
        for (uint8_t val : block) {
            sum += val;
        }
        int parity = sum % 2;
        
        if (parity == bit) {
            return block[0];
        } else {
            if (block[0] > 0) {
                return block[0] - 1;
            } else {
                return block[0] + 1;
            }
        }
    }
    
    int extractBitFromBlock(const std::vector<uint8_t>& block) {
        int sum = 0;
        for (uint8_t val : block) {
            sum += val;
        }
        return sum % 2;
    }

public:
    std::string name() const override { return "BlockAdaptive"; }

    bool embed(GrayBMP& container, const Watermark& wm, const std::string& key, GrayBMP& stego) override {
        int w = container.getWidth();
        int h = container.getHeight();
        int wmBits = wm.totalBits();
        
        int blocksX = w / BLOCK_SIZE;
        int blocksY = h / BLOCK_SIZE;
        int totalBlocks = blocksX * blocksY;
        
        if (wmBits > totalBlocks) {
            std::cerr << "Watermark too large! Need " << wmBits << " blocks, have " << totalBlocks << "\n";
            return false;
        }

        stego = container.clone();
        uint8_t* pixels = stego.data();
        const auto& wmBitsVec = wm.getBits();
        
        std::vector<std::pair<double, int>> blockGradients;
        for (int by = 0; by < blocksY; ++by) {
            for (int bx = 0; bx < blocksX; ++bx) {
                int blockIdx = by * blocksX + bx;
                double gradient = computeBlockGradient(container, bx * BLOCK_SIZE, by * BLOCK_SIZE);
                blockGradients.push_back({gradient, blockIdx});
            }
        }
        
        std::sort(blockGradients.begin(), blockGradients.end(),
                  [](const auto& a, const auto& b) { return a.first > b.first; });

        for (int i = 0; i < wmBits; ++i) {
            int blockIdx = blockGradients[i].second;
            int blockX = (blockIdx % blocksX) * BLOCK_SIZE;
            int blockY = (blockIdx / blocksX) * BLOCK_SIZE;
            
            std::vector<uint8_t> block;
            for (int by = 0; by < BLOCK_SIZE; ++by) {
                for (int bx = 0; bx < BLOCK_SIZE; ++bx) {
                    int px = blockX + bx;
                    int py = blockY + by;
                    block.push_back(pixels[py * w + px]);
                }
            }
            
            uint8_t newFirstPixel = embedBitInBlock(block, wmBitsVec[i]);
            pixels[blockY * w + blockX] = newFirstPixel;
        }
        
        return true;
    }

    bool extract(const GrayBMP& stego, const std::string& key, int bitsTotal, std::vector<uint8_t>& extractedBits) override {
        int w = stego.getWidth();
        int h = stego.getHeight();
        
        int blocksX = w / BLOCK_SIZE;
        int blocksY = h / BLOCK_SIZE;
        int totalBlocks = blocksX * blocksY;
        
        if (bitsTotal > totalBlocks) return false;
        
        const uint8_t* pixels = stego.data();
        extractedBits.resize(bitsTotal);
        
        std::vector<std::pair<double, int>> blockGradients;
        for (int by = 0; by < blocksY; ++by) {
            for (int bx = 0; bx < blocksX; ++bx) {
                int blockIdx = by * blocksX + bx;
                double gradient = computeBlockGradient(stego, bx * BLOCK_SIZE, by * BLOCK_SIZE);
                blockGradients.push_back({gradient, blockIdx});
            }
        }
        
        std::sort(blockGradients.begin(), blockGradients.end(),
                  [](const auto& a, const auto& b) { return a.first > b.first; });

        for (int i = 0; i < bitsTotal; ++i) {
            int blockIdx = blockGradients[i].second;
            int blockX = (blockIdx % blocksX) * BLOCK_SIZE;
            int blockY = (blockIdx / blocksX) * BLOCK_SIZE;
            
            std::vector<uint8_t> block;
            for (int by = 0; by < BLOCK_SIZE; ++by) {
                for (int bx = 0; bx < BLOCK_SIZE; ++bx) {
                    int px = blockX + bx;
                    int py = blockY + by;
                    block.push_back(pixels[py * w + px]);
                }
            }
            
            extractedBits[i] = extractBitFromBlock(block);
        }
        
        return true;
    }

    bool createWatermarkImage(const std::vector<uint8_t>& extractedBits, int width, int height, const std::string& filename) override
    {
        
        if (extractedBits.size() != static_cast<size_t>(width * height)) {
            std::cerr << "Error: size bits" << std::endl;
            return false;
        }
        
        int rowSize = (width * 8 + 31) / 32 * 4;
        int dataSize = rowSize * height;
        
        BMPHeader header;
        std::memset(&header, 0, sizeof(header));
        
        header.bfType = 0x4D42;
        header.bfOffBits = sizeof(header) + 1024;
        header.bfSize = header.bfOffBits + dataSize;
        header.biSize = 40;
        header.biWidth = width;
        header.biHeight = height;
        header.biPlanes = 1;
        header.biBitCount = 8;
        header.biSizeImage = dataSize;
        header.biClrUsed = 256;
        
        std::ofstream file(filename, std::ios::binary);
        if (!file) {
            std::cerr << "Error: open file" << filename << std::endl;
            return false;
        }
        
        file.write(reinterpret_cast<const char*>(&header), sizeof(header));
        
        for (int i = 0; i < 256; ++i) {
            uint8_t paletteEntry[4] = {
                static_cast<uint8_t>(i),  
                static_cast<uint8_t>(i),  
                static_cast<uint8_t>(i),  
                0                          
            };
            file.write(reinterpret_cast<const char*>(paletteEntry), 4);
        }
        
        std::vector<uint8_t> rawData(dataSize, 0);
        
        for (int y = 0; y < height; ++y) {
            for (int x = 0; x < width; ++x) {
                int pixelIndex = y * width + x;
                uint8_t value = extractedBits[pixelIndex] ? 255 : 0;
                
                int dstY = (header.biHeight > 0) ? (height - 1 - y) : y;
                rawData[dstY * rowSize + x] = value;
            }
        }
        
        file.write(reinterpret_cast<const char*>(rawData.data()), dataSize);
        
        file.close();
        
        return true;
    }
};

bool verifyWatermark(const std::vector<uint8_t>& extracted, const Watermark& wm) {
    const auto& original = wm.getBits();
    if (extracted.size() != original.size()) return false;
    int errors = 0;
    for (size_t i = 0; i < extracted.size(); ++i) {
        if (extracted[i] != original[i]) errors++;
    }
    double errorRate = 100.0 * errors / original.size();
    std::cout << "  Verification: errors = " << errors << "/" << original.size()
              << " (" << std::fixed << std::setprecision(2) << errorRate << "%)\n";
    return errors == 0;
}

void testOnDataset(const std::string& datasetPath, const std::string& datasetName,
                   Embedder& embedder, const Watermark& wm, const std::string& key) {
    std::cout << "\n===== Testing on " << datasetName << " =====\n";
    std::cout << "Embedder: " << embedder.name() << "\n";

    int n = 30;
    int count = 0;
    double totalPSNR = 0.0;
    std::vector<double> PSNR_i;

    for (const auto& entry : fs::directory_iterator(datasetPath)) {
        if (entry.path().extension() != ".bmp") continue;
        if (++count > n) break;

        GrayBMP container;
        if (!container.load(entry.path().string())) {
            std::cerr << "Failed to load " << entry.path() << "\n";
            continue;
        }

        if (container.getSize() < wm.totalBits()) {
            std::cout << "  Skipping " << entry.path().filename() << " (too small)\n";
            continue;
        }

        GrayBMP stego;
        if (!embedder.embed(container, wm, key, stego)) {
            std::cerr << "Embedding failed for " << entry.path() << "\n";
            continue;
        }

        std::vector<uint8_t> extracted;
        if (!embedder.extract(stego, key, wm.totalBits(), extracted)) {
            std::cerr << "Extraction failed for " << entry.path() << "\n";
            continue;
        }
        
        if (!embedder.createWatermarkImage(extracted, wm.getWidth(), wm.getHeight(), "stego/" + datasetName + "/" + embedder.name() + "/extracted/" + entry.path().stem().string() + ".bmp")) {
            std::cerr << "Create failed for " << entry.path() << "\n";
            continue;
        }

        std::cout << "\nImage: " << entry.path().filename() << "\n";
        bool ok = verifyWatermark(extracted, wm);

        double mse = Metrics::MSE(container.getPixels(), stego.getPixels());
        double psnr = Metrics::PSNR(mse);
        PSNR_i.push_back(psnr);
        totalPSNR += psnr;
        std::cout << "  PSNR = " << std::fixed << std::setprecision(2) << psnr << " dB\n";

        std::string outName = "stego/" + datasetName + "/" + embedder.name() + "/" + entry.path().stem().string() + ".bmp";
        stego.save(outName);
    }

    if (count > 0) {
        std::cout << "\nAverage PSNR for " << datasetName << ": "
                  << std::fixed << std::setprecision(2) << (totalPSNR / count) << " dB\n";
    }

    if (embedder.name() != "LSB")
    {
        double _x = totalPSNR / count;
        double sum = 0.0;
        for (auto psnr : PSNR_i)
        {
            sum += pow((psnr - _x), 2);
        }
        sum = sum / (n - 1);
        double S = sqrt(sum);
        std::cout << "\nS = "  << S << " dB\n"; 


        double t = 1.96;

        std::cout << "\nConfidence interval = [" << _x - t * (S / sqrt(n)) << ", " << _x + t * (S / sqrt(n)) << "]\n"; 
    }
}

int main() {
    fs::create_directories("stego");
    fs::create_directories("stego/BOSS");
    fs::create_directories("stego/Medical");
    fs::create_directories("stego/Flowers");
    fs::create_directories("stego/BOSS/BlockAdaptive");
    fs::create_directories("stego/BOSS/BlockAdaptive/extracted");
    fs::create_directories("stego/BOSS/BlockLSB");
    fs::create_directories("stego/BOSS/BlockLSB/extracted");
    fs::create_directories("stego/Medical/BlockAdaptive");
    fs::create_directories("stego/Medical/BlockAdaptive/extracted");
    fs::create_directories("stego/Medical/BlockLSB");
    fs::create_directories("stego/Medical/BlockLSB/extracted");
    fs::create_directories("stego/Flowers/BlockAdaptive");
    fs::create_directories("stego/Flowers/BlockAdaptive/extracted");
    fs::create_directories("stego/Flowers/BlockLSB");
    fs::create_directories("stego/Flowers/BlockLSB/extracted");


    std::string bossPath   = "../lab1/set1";
    std::string medicalPath = "../lab1/set2";
    std::string otherPath  = "../lab1/set3";

    Watermark wm;
    if (!wm.loadFromBMP("./watermark4.bmp")) {
        std::cerr << "Please provide a logo.bmp (binary image) as watermark.\n";
        return 1;
    }
    std::cout << "Watermark loaded: " << wm.getWidth() << "x" << wm.getHeight()
              << " (" << wm.totalBits() << " bits)\n";

    std::string secretKey = "my_secret_phrase_123";

    BlockLSBEmbedder blockLsbEmbedder;
    BlockAdaptiveEmbedder blockAdaptiveEmbedder;

    // testOnDataset(bossPath, "BOSS", blockLsbEmbedder, wm, secretKey);
    testOnDataset(bossPath, "BOSS", blockAdaptiveEmbedder, wm, secretKey);

    // testOnDataset(medicalPath, "Medical", blockLsbEmbedder, wm, secretKey);
    testOnDataset(medicalPath, "Medical", blockAdaptiveEmbedder, wm, secretKey);

    // testOnDataset(otherPath, "Flowers", blockLsbEmbedder, wm, secretKey);
    testOnDataset(otherPath, "Flowers", blockAdaptiveEmbedder, wm, secretKey);

    //  GrayBMP image;
    // if (!image.load("stego/BOSS/BlockAdaptive/1.bmp"))
    //             return -1;
    // GrayBMP plane = image.extractBitPlane(1);
    // plane.save("adapt2_plane_1.bmp");
    // plane = image.extractBitPlane(2);
    // plane.save("adapt2_plane_2.bmp");
    // plane = image.extractBitPlane(3);
    // plane.save("adapt2_plane_3.bmp");
    // plane = image.extractBitPlane(4);
    // plane.save("adapt2_plane_4.bmp");
    // plane = image.extractBitPlane(5);
    // plane.save("adapt2_plane_5.bmp");
    // plane = image.extractBitPlane(6);
    // plane.save("adapt2_plane_6.bmp");
    // plane = image.extractBitPlane(7);
    // plane.save("adapt2_plane_7.bmp");
    // plane = image.extractBitPlane(8);
    // plane.save("adapt2_plane_8.bmp");

    return 0;
}