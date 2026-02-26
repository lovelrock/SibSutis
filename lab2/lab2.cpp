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
    virtual bool createImageFromBits(const std::vector<uint8_t>& extractedBits, int width, int height, const std::string& filename) = 0;
    virtual ~Embedder() {}
};

class LSBKeyEmbedder : public Embedder {
private:
    std::mt19937 rng;
public:
    std::string name() const override { return "LSB + Secret Key"; }

    bool embed(GrayBMP& container, const Watermark& wm, const std::string& key, GrayBMP& stego) override {
        int totalPixels = container.getSize();
        int wmBits = wm.totalBits();

        if (wmBits > totalPixels) {
            std::cerr << "Watermark too large for container!\n";
            return false;
        }

        std::seed_seq seed(key.begin(), key.end());
        rng.seed(seed);

        std::vector<int> indices(totalPixels);
        for (int i = 0; i < totalPixels; ++i) indices[i] = i;
        std::shuffle(indices.begin(), indices.end(), rng);

        stego = container.clone();
        uint8_t* pixels = stego.data();
        const auto& wmBitsVec = wm.getBits();

        for (int i = 0; i < wmBits; ++i) {
            int pos = indices[i];
            pixels[pos] = (pixels[pos] & 0xFE) | wmBitsVec[i];
        }
        return true;
    }

    bool extract(const GrayBMP& stego, const std::string& key, int bitsTotal, std::vector<uint8_t>& extractedBits) override {
        int totalPixels = stego.getSize();
        if (bitsTotal > totalPixels) return false;

        std::seed_seq seed(key.begin(), key.end());
        rng.seed(seed);

        std::vector<int> indices(totalPixels);
        for (int i = 0; i < totalPixels; ++i) indices[i] = i;
        std::shuffle(indices.begin(), indices.end(), rng);

        const uint8_t* pixels = stego.data();
        extractedBits.resize(bitsTotal);
        for (int i = 0; i < bitsTotal; ++i) {
            int pos = indices[i];
            extractedBits[i] = pixels[pos] & 1;
        }
        return true;
    }

    bool createImageFromBits(const std::vector<uint8_t>& extractedBits, int width, int height, const std::string& filename) override {
        if (extractedBits.size() != static_cast<size_t>(width * height)) {
            std::cerr << "Ошибка: размер битов (" << extractedBits.size() 
                    << ") не соответствует размеру изображения (" 
                    << width * height << ")" << std::endl;
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
        header.biCompression = 0;
        header.biSizeImage = dataSize;
        header.biXPelsPerMeter = 2835;
        header.biYPelsPerMeter = 2835;
        header.biClrUsed = 256;
        header.biClrImportant = 256;
        
        std::ofstream file(filename, std::ios::binary);
        if (!file) {
            std::cerr << "Ошибка: не удалось создать файл " << filename << std::endl;
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

class AdaptiveContrastEmbedder : public Embedder {
private:
    std::mt19937 rng;
    
    double localContrast(const GrayBMP& img, int x, int y) {
        int w = img.getWidth();
        int h = img.getHeight();
        const uint8_t* p = img.data();
        
        double localMean = 0.0;
        double localMax = 0.0;
        double localMin = 255.0;
        int count = 0;
        
        // Анализируем окрестность 3x3
        for (int dy = -1; dy <= 1; ++dy) {
            for (int dx = -1; dx <= 1; ++dx) {
                int nx = x + dx;
                int ny = y + dy;
                if (nx >= 0 && nx < w && ny >= 0 && ny < h) {
                    uint8_t val = p[ny * w + nx];
                    localMean += val;
                    localMax = std::max(localMax, (double)val);
                    localMin = std::min(localMin, (double)val);
                    count++;
                }
            }
        }
        
        if (count == 0) return 0.0;
        
        localMean /= count;
        
        // Несколько метрик контраста:
        // 1. Размах (max - min)
        double rangeContrast = localMax - localMin;
        
        // 2. RMS контраст относительно среднего
        double rmsContrast = 0.0;
        for (int dy = -1; dy <= 1; ++dy) {
            for (int dx = -1; dx <= 1; ++dx) {
                int nx = x + dx;
                int ny = y + dy;
                if (nx >= 0 && nx < w && ny >= 0 && ny < h) {
                    double diff = p[ny * w + nx] - localMean;
                    rmsContrast += diff * diff;
                }
            }
        }
        rmsContrast = std::sqrt(rmsContrast / count);
        
        // Комбинируем метрики
        return rangeContrast * 0.6 + rmsContrast * 0.4;
    }

public:
    std::string name() const override { return "Adaptive (Local Contrast + Key)"; }

    bool embed(GrayBMP& container, const Watermark& wm, const std::string& key, GrayBMP& stego) override {
        int w = container.getWidth();
        int h = container.getHeight();
        int totalPixels = w * h;
        int wmBits = wm.totalBits();

        if (wmBits > totalPixels) {
            std::cerr << "Watermark too large!\n";
            return false;
        }

        stego = container.clone();
        uint8_t* pixels = stego.data();

        // Вычисляем локальный контраст для каждого пикселя
        std::vector<double> contrastValues(totalPixels);
        for (int y = 0; y < h; ++y) {
            for (int x = 0; x < w; ++x) {
                contrastValues[y * w + x] = localContrast(container, x, y);
            }
        }

        // Создаем индексы
        std::vector<int> indices(totalPixels);
        for (int i = 0; i < totalPixels; ++i) indices[i] = i;
        
        // Сортируем по убыванию контраста (детерминированно)
        std::sort(indices.begin(), indices.end(),
            [&](int a, int b) { 
                if (contrastValues[a] != contrastValues[b])
                    return contrastValues[a] > contrastValues[b];
                return a < b; // при равном контрасте - по индексу
            });

        // Перемешиваем с использованием ключа ТОЛЬКО первые wmBits позиций
        std::seed_seq seed(key.begin(), key.end());
        rng.seed(seed);
        
        std::vector<int> embedPositions(indices.begin(), indices.begin() + wmBits);
        std::shuffle(embedPositions.begin(), embedPositions.end(), rng);

        const auto& wmBitsVec = wm.getBits();

        // Встраиваем в перемешанные позиции
        for (int i = 0; i < wmBits; ++i) {
            int pos = embedPositions[i];
            pixels[pos] = (pixels[pos] & 0xFE) | wmBitsVec[i];
        }
        
        return true;
    }

    bool extract(const GrayBMP& stego, const std::string& key, int bitsTotal, std::vector<uint8_t>& extractedBits) override {
        int w = stego.getWidth();
        int h = stego.getHeight();
        int totalPixels = w * h;

        if (bitsTotal > totalPixels) return false;

        // Вычисляем контраст на stego-изображении
        std::vector<double> contrastValues(totalPixels);
        for (int y = 0; y < h; ++y) {
            for (int x = 0; x < w; ++x) {
                contrastValues[y * w + x] = localContrast(stego, x, y);
            }
        }

        // Сортируем ТАК ЖЕ, как при встраивании
        std::vector<int> indices(totalPixels);
        for (int i = 0; i < totalPixels; ++i) indices[i] = i;
        
        std::sort(indices.begin(), indices.end(),
            [&](int a, int b) { 
                if (contrastValues[a] != contrastValues[b])
                    return contrastValues[a] > contrastValues[b];
                return a < b;
            });

        // Используем тот же ключ для перемешивания
        std::seed_seq seed(key.begin(), key.end());
        rng.seed(seed);
        
        std::vector<int> embedPositions(indices.begin(), indices.begin() + bitsTotal);
        std::shuffle(embedPositions.begin(), embedPositions.end(), rng);

        const uint8_t* pixels = stego.data();
        extractedBits.resize(bitsTotal);
        
        // Извлекаем из тех же позиций
        for (int i = 0; i < bitsTotal; ++i) {
            int pos = embedPositions[i];
            extractedBits[i] = pixels[pos] & 1;
        }
        
        return true;
    }

    bool createImageFromBits(const std::vector<uint8_t>& extractedBits, int width, int height, const std::string& filename) override {
        if (extractedBits.size() != static_cast<size_t>(width * height)) {
            std::cerr << "Ошибка: размер битов (" << extractedBits.size() 
                    << ") не соответствует размеру изображения (" 
                    << width * height << ")" << std::endl;
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
        header.biCompression = 0;
        header.biSizeImage = dataSize;
        header.biXPelsPerMeter = 2835;
        header.biYPelsPerMeter = 2835;
        header.biClrUsed = 256;
        header.biClrImportant = 256;
        
        std::ofstream file(filename, std::ios::binary);
        if (!file) {
            std::cerr << "Ошибка: не удалось создать файл " << filename << std::endl;
            return false;
        }
        
        file.write(reinterpret_cast<const char*>(&header), sizeof(header));
        
        // Записываем палитру (grayscale)
        for (int i = 0; i < 256; ++i) {
            uint8_t paletteEntry[4] = {
                static_cast<uint8_t>(i),  
                static_cast<uint8_t>(i),  
                static_cast<uint8_t>(i),  
                0                          
            };
            file.write(reinterpret_cast<const char*>(paletteEntry), 4);
        }
        
        // Записываем пиксельные данные
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

    int count = 0;
    double totalPSNR = 0.0;

    for (const auto& entry : fs::directory_iterator(datasetPath)) {
        if (entry.path().extension() != ".bmp") continue;
        if (++count > 10) break;

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
        
        if (!embedder.createImageFromBits(extracted, wm.getWidth(), wm.getHeight(), 
                "stego/" + datasetName + "/extracted_" + embedder.name() + "_" + entry.path().stem().string() + ".bmp")) {
            std::cerr << "Create failed for " << entry.path() << "\n";
            continue;
        }

        std::cout << "\nImage: " << entry.path().filename() << "\n";
        bool ok = verifyWatermark(extracted, wm);

        double mse = Metrics::MSE(container.getPixels(), stego.getPixels());
        double psnr = Metrics::PSNR(mse);
        totalPSNR += psnr;
        std::cout << "  PSNR = " << std::fixed << std::setprecision(2) << psnr << " dB\n";

        std::string outName = "stego/" + datasetName + "/" + embedder.name() + "_" + entry.path().stem().string() + ".bmp";
        stego.save(outName);
    }

    if (count > 0) {
        std::cout << "\nAverage PSNR for " << datasetName << ": "
                  << std::fixed << std::setprecision(2) << (totalPSNR / count) << " dB\n";
    }
}

int main() {
    fs::create_directories("stego");
    fs::create_directories("stego/BOSS");
    fs::create_directories("stego/Medical");
    fs::create_directories("stego/Flowers");

    std::string bossPath   = "../lab1/set1";
    std::string medicalPath = "../lab1/set2";
    std::string otherPath  = "../lab1/set3";

    Watermark wm;
    if (!wm.loadFromBMP("./logo.bmp")) {
        std::cerr << "Please provide a logo.bmp (binary image) as watermark.\n";
        return 1;
    }
    std::cout << "Watermark loaded: " << wm.getWidth() << "x" << wm.getHeight()
              << " (" << wm.totalBits() << " bits)\n";

    std::string secretKey = "my_secret_phrase_123";

    LSBKeyEmbedder lsbEmbedder;
    AdaptiveContrastEmbedder adaptiveEmbedder;

    testOnDataset(bossPath, "BOSS", lsbEmbedder, wm, secretKey);
    testOnDataset(bossPath, "BOSS", adaptiveEmbedder, wm, secretKey);

    testOnDataset(medicalPath, "Medical", lsbEmbedder, wm, secretKey);
    testOnDataset(medicalPath, "Medical", adaptiveEmbedder, wm, secretKey);

    testOnDataset(otherPath, "Flowers", lsbEmbedder, wm, secretKey);
    testOnDataset(otherPath, "Flowers", adaptiveEmbedder, wm, secretKey);

    return 0;
}