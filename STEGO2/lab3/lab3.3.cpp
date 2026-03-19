#include <iostream>
#include <fstream>
#include <vector>
#include <string>
#include <map>
#include <iomanip>
#include <filesystem>
#include <random>
#include <ctime>
#include <sstream>
#include <algorithm>
#include <cmath>

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
    static double computePSNR(const GrayBMP& original, const GrayBMP& stego) {
        double mse = 0;
        int size = original.getWidth() * original.getHeight();
        const uint8_t* origPixels = original.data();
        const uint8_t* stegoPixels = stego.data();
        
        for (int i = 0; i < size; i++) {
            double diff = origPixels[i] - stegoPixels[i];
            mse += diff * diff;
        }
        mse /= size;
        
        if (mse == 0) return INFINITY;
        return 10 * log10(255 * 255 / mse);
    }
};

class HistogramShiftingEmbedder {
private:
    struct PeakZeroPair {
        int peak;
        int zero;
        int peakCount;
    };
    
    std::vector<PeakZeroPair> pairs;
    
    std::map<int, int> computeHistogram(const GrayBMP& image) {
        std::map<int, int> hist;
        const uint8_t* pixels = image.data();
        int size = image.getWidth() * image.getHeight();
        
        for (int i = 0; i < 256; i++) hist[i] = 0;
        for (int i = 0; i < size; i++) {
            hist[pixels[i]]++;
        }
        return hist;
    }
    
    std::vector<PeakZeroPair> findPeakZeroPairs(const std::map<int, int>& hist, 
                                                 int requiredCapacity) {
        std::vector<PeakZeroPair> pairs;
        
        std::vector<int> zeroPoints;
        for (int i = 0; i < 256; i++) {
            if (hist.at(i) == 0) {
                zeroPoints.push_back(i);
            }
        }
        
        int totalCapacity = 0;
        int lastZero = -1;
        
        for (int zero : zeroPoints) {
            if (totalCapacity >= requiredCapacity) break;
            
            if (lastZero + 1 < zero) {
                int peak = lastZero + 1;
                int maxCount = hist.at(peak);
                
                for (int i = lastZero + 2; i < zero; i++) {
                    if (hist.at(i) > maxCount) {
                        maxCount = hist.at(i);
                        peak = i;
                    }
                }
                
                if (maxCount > 0) {
                    pairs.push_back({peak, zero, maxCount});
                    totalCapacity += maxCount;
                }
            }
            lastZero = zero;
        }
        
        return pairs;
    }
public:    
    std::vector<uint8_t> readDataFromFile(const std::string& filename) {
        std::vector<uint8_t> data;
        std::ifstream file(filename, std::ios::binary);
        
        if (!file) {
            std::cerr << "Error: Cannot open file " << filename << std::endl;
            return data;
        }
        
        file.seekg(0, std::ios::end);
        std::streampos fileSize = file.tellg();
        file.seekg(0, std::ios::beg);
        
        data.resize(fileSize);
        file.read(reinterpret_cast<char*>(data.data()), fileSize);
        file.close();
        
        return data;
    }
    
    bool writeDataToFile(const std::vector<uint8_t>& data, const std::string& filename) {
        std::ofstream file(filename, std::ios::binary);
        if (!file) {
            std::cerr << "Error: Cannot create file " << filename << std::endl;
            return false;
        }
        
        file.write(reinterpret_cast<const char*>(data.data()), data.size());
        file.close();
        return true;
    }
    
public:
    bool embed(GrayBMP& container, const std::vector<uint8_t>& data, 
               GrayBMP& stego, std::map<std::string, int>& metadata) {
        
        int requiredCapacity = data.size() * 8;
        auto hist = computeHistogram(container);
        int totalPixels = container.getWidth() * container.getHeight();
        if (requiredCapacity > totalPixels) {
            std::cerr << "Error: Data too large. Required: " << requiredCapacity 
                      << " bits, Available: " << totalPixels << " bits\n";
            return false;
        }
        pairs = findPeakZeroPairs(hist, requiredCapacity);
        if (pairs.empty()) {
            std::cerr << "Not enough capacity! Could not find suitable peak-zero pairs.\n";
            return false;
        }
        stego = container.clone();
        uint8_t* pixels = stego.data();
        int size = container.getWidth() * container.getHeight();
        std::sort(pairs.begin(), pairs.end(), 
                  [](const auto& a, const auto& b) { 
                      return a.peakCount > b.peakCount; 
                  });
        
        metadata["num_pairs"] = pairs.size();
        metadata["data_size"] = data.size();
        for (size_t i = 0; i < pairs.size(); i++) {
            metadata["peak_" + std::to_string(i)] = pairs[i].peak;
            metadata["zero_" + std::to_string(i)] = pairs[i].zero;
        }
        
        int bitIndex = 0;
        int totalBits = data.size() * 8;
        
        for (const auto& pair : pairs) {
            if (bitIndex >= totalBits) break;
            
            int peak = pair.peak;
            int zero = pair.zero;
            bool shiftRight = (zero > peak);
            
            for (int i = 0; i < size; i++) {
                if (shiftRight) {
                    if (pixels[i] > peak && pixels[i] < zero) {
                        pixels[i]++;
                    }
                } else {
                    if (pixels[i] < peak && pixels[i] > zero) {
                        pixels[i]--;
                    }
                }
            }
            
            for (int i = 0; i < size && bitIndex < totalBits; i++) {
                if (pixels[i] == peak) {
                    uint8_t bit = (data[bitIndex / 8] >> (7 - (bitIndex % 8))) & 1;
                    if (bit == 1) {
                        if (shiftRight) {
                            pixels[i]++;
                        } else {
                            pixels[i]--;
                        }
                    }
                    bitIndex++;
                }
            }
        }
        
        return true;
    }
    
    bool extract(const GrayBMP& stego, const std::map<std::string, int>& metadata,
                 std::vector<uint8_t>& extractedData, GrayBMP& restored) {
        
        restored = stego.clone();
        uint8_t* pixels = restored.data();
        int size = restored.getWidth() * restored.getHeight();
        int numPairs = metadata.at("num_pairs");
        int dataSize = metadata.at("data_size");
        std::vector<PeakZeroPair> pairs;
        for (int i = 0; i < numPairs; i++) {
            int peak = metadata.at("peak_" + std::to_string(i));
            int zero = metadata.at("zero_" + std::to_string(i));
            pairs.push_back({peak, zero, 0});
        }
        std::vector<uint8_t> bits;
        bits.reserve(dataSize * 8);
        for (int p = pairs.size() - 1; p >= 0; p--) {
            const auto& pair = pairs[p];
            int peak = pair.peak;
            int zero = pair.zero;
            bool shiftRight = (zero > peak);
            std::vector<int> extractedBits;
            for (int i = 0; i < size; i++) {
                if (shiftRight) {
                    if (pixels[i] == peak + 1) {
                        extractedBits.push_back(1);
                        pixels[i] = peak;
                    } else if (pixels[i] == peak) {
                        extractedBits.push_back(0);
                    }
                } else {
                    if (pixels[i] == peak - 1) {
                        extractedBits.push_back(1);
                        pixels[i] = peak;
                    } else if (pixels[i] == peak) {
                        extractedBits.push_back(0);
                    }
                }
            }
            bits.insert(bits.begin(), extractedBits.begin(), extractedBits.end());
            
            for (int i = 0; i < size; i++) {
                if (shiftRight) {
                    if (pixels[i] > peak && pixels[i] <= zero) {
                        pixels[i]--;
                    }
                } else {
                    if (pixels[i] < peak && pixels[i] >= zero) {
                        pixels[i]++;
                    }
                }
            }
        }
        
        extractedData.clear();
        extractedData.reserve(dataSize);
        
        for (int i = 0; i < dataSize; i++) {
            uint8_t byte = 0;
            for (int b = 0; b < 8; b++) {
                int bitPos = i * 8 + b;
                if (bitPos < static_cast<int>(bits.size()) && bits[bitPos]) {
                    byte |= (1 << (7 - b));
                }
            }
            extractedData.push_back(byte);
        }
        
        return true;
    }
    
    void saveMetadata(const std::map<std::string, int>& metadata, const std::string& filename) {
        std::ofstream file(filename);
        for (const auto& item : metadata) {
            file << item.first << " " << item.second << "\n";
        }
        file.close();
    }
    
    std::map<std::string, int> loadMetadata(const std::string& filename) {
        std::map<std::string, int> metadata;
        std::ifstream file(filename);
        std::string key;
        int value;
        while (file >> key >> value) {
            metadata[key] = value;
        }
        file.close();
        return metadata;
    }
};

bool verifyData(const std::vector<uint8_t>& original, const std::vector<uint8_t>& extracted) {
    if (original.size() != extracted.size()) {
        return false;
    }
    
    for (size_t i = 0; i < original.size(); i++) {
        if (original[i] != extracted[i]) {
            return false;
        }
    }
    return true;
}

void testDataset(const std::string& datasetPath, const std::string& datasetName, 
                 const std::string& dataFilePath, const std::string& outputDir) {
    
    std::cout << "\n========== Testing on " << datasetName << " dataset ==========\n";
    
    fs::create_directories(outputDir + "/" + datasetName + "/stego");
    fs::create_directories(outputDir + "/" + datasetName + "/restored");
    fs::create_directories(outputDir + "/" + datasetName + "/extracted");
    fs::create_directories(outputDir + "/" + datasetName + "/metadata");
    
    HistogramShiftingEmbedder embedder;
    double totalPSNR = 0.0;
    int successCount = 0;
    int totalImages = 0;
    std::vector<double> psnrValues;
    
    std::vector<uint8_t> testData = embedder.readDataFromFile(dataFilePath);
    if (testData.empty()) {
        std::cerr << "Failed to read data file: " << dataFilePath << std::endl;
        return;
    }
    
    std::cout << "Data file: " << dataFilePath << " (" << testData.size() << " bytes)\n";
    
    std::ofstream resultsFile(outputDir + "/" + datasetName + "_results.txt");
    resultsFile << "Dataset: " << datasetName << "\n";
    resultsFile << "Data file: " << dataFilePath << " (" << testData.size() << " bytes)\n";
    resultsFile << "========================================\n\n";
    
    for (const auto& entry : fs::directory_iterator(datasetPath)) {
        if (entry.path().extension() != ".bmp") continue;
        
        totalImages++;
        std::string filename = entry.path().stem().string();
        std::cout << "\n[" << totalImages << "] Processing: " << filename << ".bmp\n";
        
        GrayBMP container;
        if (!container.load(entry.path().string())) {
            std::cerr << "  Failed to load image\n";
            resultsFile << filename << ".bmp: FAILED (cannot load)\n";
            continue;
        }
        
        int requiredBits = testData.size() * 8;
        int totalPixels = container.getWidth() * container.getHeight();
        
        if (requiredBits > totalPixels) {
            std::cout << "  Skipping - image too small\n";
            resultsFile << filename << ".bmp: SKIPPED (too small)\n";
            continue;
        }
        
        GrayBMP stego;
        std::map<std::string, int> metadata;
        
        if (!embedder.embed(container, testData, stego, metadata)) {
            std::cerr << "  Embedding failed\n";
            resultsFile << filename << ".bmp: FAILED (embedding)\n";
            continue;
        }
        
        double psnr = Metrics::computePSNR(container, stego);
        psnrValues.push_back(psnr);
        totalPSNR += psnr;
        successCount++;
        
        std::cout << "  PSNR = " << std::fixed << std::setprecision(2) << psnr << " dB\n";
        resultsFile << filename << ".bmp: PSNR = " << std::fixed << std::setprecision(2) << psnr << " dB\n";
        
        std::string stegoPath = outputDir + "/" + datasetName + "/stego/" + filename + "_stego.bmp";
        stego.save(stegoPath);
        
        std::string metadataPath = outputDir + "/" + datasetName + "/metadata/" + filename + "_metadata.txt";
        embedder.saveMetadata(metadata, metadataPath);
        
        GrayBMP restored;
        std::vector<uint8_t> extractedData;
        
        if (!embedder.extract(stego, metadata, extractedData, restored)) {
            std::cerr << "  Extraction failed\n";
            resultsFile << "  Extraction: FAILED\n";
            continue;
        }
        
        std::string restoredPath = outputDir + "/" + datasetName + "/restored/" + filename + "_restored.bmp";
        restored.save(restoredPath);
        
        std::string extractedPath = outputDir + "/" + datasetName + "/extracted/" + filename + "_extracted.txt";
        if (embedder.writeDataToFile(extractedData, extractedPath)) {
            std::cout << "  Extracted data saved to: " << extractedPath << "\n";
        }
        
        if (verifyData(testData, extractedData)) {
            std::cout << "   Data successfully verified\n";
            resultsFile << "  Extraction: SUCCESS (data matches)\n";
        } else {
            std::cout << "   Data verification failed\n";
            resultsFile << "  Extraction: FAILED (data mismatch)\n";
        }
        
        double restorePSNR = Metrics::computePSNR(container, restored);
        if (restorePSNR > 99.0) {
            std::cout << "   Image perfectly restored\n";
        } else {
            std::cout << "   Image restoration error: " << restorePSNR << " dB\n";
        }
    }
    
    resultsFile << "\n========================================\n";
    resultsFile << "Total images processed: " << totalImages << "\n";
    resultsFile << "Successfully embedded: " << successCount << "\n";
    
    if (successCount > 0) {
        double avgPSNR = totalPSNR / successCount;
        resultsFile << "Average PSNR: " << std::fixed << std::setprecision(2) << avgPSNR << " dB\n";
        
        if (psnrValues.size() > 1) {
            double variance = 0;
            for (double psnr : psnrValues) {
                variance += (psnr - avgPSNR) * (psnr - avgPSNR);
            }
            variance /= (psnrValues.size() - 1);
            double stdDev = sqrt(variance);
            double t = 1.96;
            
            resultsFile << "Std deviation: " << std::fixed << std::setprecision(2) << stdDev << " dB\n";
            resultsFile << "95% CI: [" << avgPSNR - t * stdDev / sqrt(psnrValues.size()) 
                      << ", " << avgPSNR + t * stdDev / sqrt(psnrValues.size()) << "] dB\n";
        }
        
        std::cout << "\n--- Results for " << datasetName << " ---\n";
        std::cout << "Successfully processed: " << successCount << "/" << totalImages << " images\n";
        std::cout << "Average PSNR: " << std::fixed << std::setprecision(2) << avgPSNR << " dB\n";
    }
    
    resultsFile.close();
}

int main() {
    std::string bossPath = "../lab1/set1";
    std::string medicalPath = "../lab1/set2";
    std::string flowersPath = "../lab1/set3";
    
    std::string dataFilePath = "message.txt";
    
    std::string outputDir = "results";
    
    fs::create_directories(outputDir);
    
    if (!fs::exists(dataFilePath)) {
        std::cout << "\nFile message.txt not found. Creating sample file...\n";
        std::ofstream sampleFile("message.txt");
        sampleFile << "This is a sample secret message for steganography testing.\n";
        sampleFile << "The quick brown fox jumps over the lazy dog.\n";
        sampleFile << "0123456789 !@#$%^&*()_+{}[]|\\:;\"'<>,.?/~\n";
        sampleFile << "Histogram shifting is a reversible data hiding technique.\n";
        sampleFile << "Multiple pairs of peak and zero points are used for embedding.\n";
        sampleFile.close();
        std::cout << "Sample message.txt created.\n";
    }
    
    testDataset(bossPath, "BOSS", dataFilePath, outputDir);
    testDataset(medicalPath, "Medical", dataFilePath, outputDir);
    testDataset(flowersPath, "Flowers", dataFilePath, outputDir);
    
    std::ofstream summary(outputDir + "/summary.txt");
    summary << "========================================\n";
    summary << "HISTOGRAM SHIFTING STEGANOGRAPHY RESULTS\n";
    summary << "========================================\n\n";
    summary << "Data file: " << dataFilePath << "\n\n";
    
    for (const auto& dataset : {"BOSS", "Medical", "Flowers"}) {
        std::string resultsPath = outputDir + "/" + std::string(dataset) + "_results.txt";
        std::ifstream results(resultsPath);
        if (results) {
            summary << results.rdbuf();
            summary << "\n";
        }
        results.close();
    }
    
    summary << "========================================\n";
    summary.close();
    
    std::cout << "\n========================================\n";
    std::cout << "Testing complete!\n";
    std::cout << "Results saved in: " << outputDir << "/\n";
    std::cout << "Summary: " << outputDir << "/summary.txt\n";
    std::cout << "========================================\n";
    
    return 0;
}