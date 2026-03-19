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
#include <numeric>

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

    bool isIdentical(const GrayBMP& other) const {
        if (width != other.width || height != other.height) return false;
        return pixels == other.pixels;
    }

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
    
public:
    struct EmbeddingResult {
        bool success;
        double psnr;
        int embeddedBits;
        int capacity;
        std::map<std::string, int> metadata;
        GrayBMP stego;
        GrayBMP restored;
        std::vector<uint8_t> extractedData;
    };
    
    EmbeddingResult embedAndExtract(GrayBMP& container, const std::vector<uint8_t>& data) {
        EmbeddingResult result;
        result.success = false;
        result.embeddedBits = 0;
        result.psnr = 0;
        
        int requiredCapacity = data.size() * 8;
        auto hist = computeHistogram(container);
        
        int totalPixels = container.getWidth() * container.getHeight();
        result.capacity = totalPixels;
        
        if (requiredCapacity > totalPixels) {
            return result;
        }
        
        pairs = findPeakZeroPairs(hist, requiredCapacity);
        
        if (pairs.empty()) {
            return result;
        }
        
        GrayBMP stego = container.clone();
        uint8_t* pixels = stego.data();
        int size = container.getWidth() * container.getHeight();
        
        std::sort(pairs.begin(), pairs.end(), 
                  [](const auto& a, const auto& b) { 
                      return a.peakCount > b.peakCount; 
                  });
        
        std::map<std::string, int> metadata;
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
        
        result.embeddedBits = bitIndex;
        result.psnr = Metrics::computePSNR(container, stego);
        result.stego = stego;
        
        // Извлечение
        GrayBMP restored = stego.clone();
        uint8_t* restoredPixels = restored.data();
        
        std::vector<PeakZeroPair> extractPairs;
        for (int i = 0; i < metadata["num_pairs"]; i++) {
            int peak = metadata["peak_" + std::to_string(i)];
            int zero = metadata["zero_" + std::to_string(i)];
            extractPairs.push_back({peak, zero, 0});
        }
        
        std::vector<uint8_t> bits;
        bits.reserve(data.size() * 8);
        
        for (int p = extractPairs.size() - 1; p >= 0; p--) {
            const auto& pair = extractPairs[p];
            int peak = pair.peak;
            int zero = pair.zero;
            bool shiftRight = (zero > peak);
            
            std::vector<int> extractedBits;
            
            for (int i = 0; i < size; i++) {
                if (shiftRight) {
                    if (restoredPixels[i] == peak + 1) {
                        extractedBits.push_back(1);
                        restoredPixels[i] = peak;
                    } else if (restoredPixels[i] == peak) {
                        extractedBits.push_back(0);
                    }
                } else {
                    if (restoredPixels[i] == peak - 1) {
                        extractedBits.push_back(1);
                        restoredPixels[i] = peak;
                    } else if (restoredPixels[i] == peak) {
                        extractedBits.push_back(0);
                    }
                }
            }
            
            bits.insert(bits.begin(), extractedBits.begin(), extractedBits.end());
            
            for (int i = 0; i < size; i++) {
                if (shiftRight) {
                    if (restoredPixels[i] > peak && restoredPixels[i] <= zero) {
                        restoredPixels[i]--;
                    }
                } else {
                    if (restoredPixels[i] < peak && restoredPixels[i] >= zero) {
                        restoredPixels[i]++;
                    }
                }
            }
        }
        
        result.extractedData.clear();
        result.extractedData.reserve(data.size());
        
        for (int i = 0; i < static_cast<int>(data.size()); i++) {
            uint8_t byte = 0;
            for (int b = 0; b < 8; b++) {
                int bitPos = i * 8 + b;
                if (bitPos < static_cast<int>(bits.size()) && bits[bitPos]) {
                    byte |= (1 << (7 - b));
                }
            }
            result.extractedData.push_back(byte);
        }
        
        result.restored = restored;
        result.metadata = metadata;
        result.success = true;
        
        return result;
    }
    
    int estimateMaxCapacity(const GrayBMP& container) {
        auto hist = computeHistogram(container);
        int totalCapacity = 0;
        
        std::vector<int> zeroPoints;
        for (int i = 0; i < 256; i++) {
            if (hist.at(i) == 0) {
                zeroPoints.push_back(i);
            }
        }
        
        int lastZero = -1;
        for (int zero : zeroPoints) {
            if (lastZero + 1 < zero) {
                int maxCount = 0;
                for (int i = lastZero + 1; i < zero; i++) {
                    if (hist.at(i) > maxCount) {
                        maxCount = hist.at(i);
                    }
                }
                totalCapacity += maxCount;
            }
            lastZero = zero;
        }
        
        return totalCapacity;
    }
};

struct DatasetStatistics {
    std::string name;
    int totalImages;
    int successfulRestorations;
    double restorationRate;
    std::vector<double> psnrValues;
    double meanPSNR;
    double psnrCI_low;
    double psnrCI_high;
    double maxCapacity;
    double avgCapacity;
    std::vector<int> failuresAtCapacity;
};

class ResearchAnalyzer {
private:
    double computeMean(const std::vector<double>& values) {
        if (values.empty()) return 0;
        double sum = std::accumulate(values.begin(), values.end(), 0.0);
        return sum / values.size();
    }
    
    double computeStdDev(const std::vector<double>& values, double mean) {
        if (values.size() < 2) return 0;
        double sq_sum = std::inner_product(values.begin(), values.end(), values.begin(), 0.0);
        double variance = (sq_sum - mean * mean * values.size()) / (values.size() - 1);
        return sqrt(variance);
    }
    
    std::pair<double, double> computeConfidenceInterval(const std::vector<double>& values, double alpha = 0.05) {
        if (values.size() < 2) return {0, 0};
        
        double mean = computeMean(values);
        double stdDev = computeStdDev(values, mean);
        double t = 1.96; // для 95% доверительного интервала при большом n
        
        double margin = t * stdDev / sqrt(values.size());
        return {mean - margin, mean + margin};
    }
    
public:
    DatasetStatistics analyzeDataset(const std::string& datasetPath, const std::string& datasetName, 
                        const std::string& dataFilePath, const std::string& outputDir) {
        
        std::cout << "\n========== RESEARCH ANALYSIS: " << datasetName << " ==========\n";
        
        fs::create_directories(outputDir + "/research/" + datasetName);
        
        HistogramShiftingEmbedder embedder;
        
        // Читаем данные из файла для разных объемов
        std::vector<uint8_t> baseData = embedder.readDataFromFile(dataFilePath);
        if (baseData.empty()) {
            std::cerr << "Failed to read data file\n";
            return {};
        }
        
        DatasetStatistics stats;
        stats.name = datasetName;
        stats.totalImages = 0;
        stats.successfulRestorations = 0;
        stats.maxCapacity = 0;
        stats.avgCapacity = 0;
        
        std::ofstream detailsFile(outputDir + "/research/" + datasetName + "_details.csv");
        detailsFile << "Image,Width,Height,PSNR,Restored,Capacity,BitsEmbedded,Success\n";
        
        std::vector<int> capacities;
        int imageCount = 0;
        
        for (const auto& entry : fs::directory_iterator(datasetPath)) {
            if (entry.path().extension() != ".bmp") continue;
            if (++imageCount > 30) break;
            
            stats.totalImages++;
            std::string filename = entry.path().stem().string();
            std::cout << "\n[" << imageCount << "] Analyzing: " << filename << ".bmp\n";
            
            GrayBMP container;
            if (!container.load(entry.path().string())) {
                std::cout << "  Failed to load\n";
                detailsFile << filename << ",ERROR,ERROR,ERROR,ERROR,ERROR,ERROR,LOAD_FAILED\n";
                continue;
            }
            
            // Оценка максимальной емкости
            int maxCapacity = embedder.estimateMaxCapacity(container);
            capacities.push_back(maxCapacity);
            stats.maxCapacity = std::max(stats.maxCapacity, (double)maxCapacity);
            
            std::cout << "  Max capacity: " << maxCapacity << " bits\n";
            
            // Тестируем с разными объемами данных
            std::vector<int> testCapacities = {
                maxCapacity / 4,
                maxCapacity / 2,
                maxCapacity * 3 / 4,
                maxCapacity
            };
            
            bool allSuccessful = true;
            
            for (int testCap : testCapacities) {
                if (testCap <= 0) continue;
                
                // Берем часть данных
                int dataBytes = testCap / 8;
                if (dataBytes == 0) dataBytes = 1;
                
                std::vector<uint8_t> testData(baseData.begin(), 
                                               baseData.begin() + std::min((int)baseData.size(), dataBytes));
                
                auto result = embedder.embedAndExtract(container, testData);
                
                if (result.success) {
                    bool restoredCorrectly = container.isIdentical(result.restored);
                    bool dataCorrect = (testData == result.extractedData);
                    
                    if (restoredCorrectly && dataCorrect) {
                        if (testCap == maxCapacity) {
                            stats.successfulRestorations++;
                        }
                        std::cout << "  Cap " << testCap << " bits: ✓ PSNR=" 
                                  << std::fixed << std::setprecision(2) << result.psnr << " dB\n";
                        
                        if (testCap == maxCapacity / 2) {
                            stats.psnrValues.push_back(result.psnr);
                        }
                    } else {
                        allSuccessful = false;
                        stats.failuresAtCapacity.push_back(testCap);
                        std::cout << "  Cap " << testCap << " bits: ✗ Restoration failed\n";
                    }
                } else {
                    allSuccessful = false;
                    stats.failuresAtCapacity.push_back(testCap);
                    std::cout << "  Cap " << testCap << " bits: ✗ Embedding failed\n";
                }
            }
            
            detailsFile << filename << ","
                       << container.getWidth() << ","
                       << container.getHeight() << ","
                       << std::fixed << std::setprecision(2) << (stats.psnrValues.empty() ? 0 : stats.psnrValues.back()) << ","
                       << (allSuccessful ? "YES" : "NO") << ","
                       << maxCapacity << ","
                       << (allSuccessful ? std::to_string(maxCapacity) : "FAIL") << ","
                       << (allSuccessful ? "SUCCESS" : "FAIL") << "\n";
        }
        
        // Вычисление статистики
        if (!stats.psnrValues.empty()) {
            stats.meanPSNR = computeMean(stats.psnrValues);
            auto ci = computeConfidenceInterval(stats.psnrValues);
            stats.psnrCI_low = ci.first;
            stats.psnrCI_high = ci.second;
        }
        
        stats.restorationRate = (double)stats.successfulRestorations / stats.totalImages * 100;
        
        if (!capacities.empty()) {
            stats.avgCapacity = computeMean(std::vector<double>(capacities.begin(), capacities.end()));
        }
        
        // Сохраняем результаты
        saveStatistics(stats, outputDir + "/research/" + datasetName + "_stats.txt");
        
        // Выводим на экран
        printStatistics(stats);

        return stats;
    }
    
    void saveStatistics(const DatasetStatistics& stats, const std::string& filename) {
        std::ofstream file(filename);
        
        file << "========================================\n";
        file << "RESEARCH RESULTS: " << stats.name << "\n";
        file << "========================================\n\n";
        
        file << "1. INTEGRITY ANALYSIS\n";
        file << "   Total images tested: " << stats.totalImages << "\n";
        file << "   Successful restorations: " << stats.successfulRestorations << "\n";
        file << "   Restoration rate: " << std::fixed << std::setprecision(2) << stats.restorationRate << "%\n\n";
        
        file << "   Failures observed at capacities: ";
        for (int cap : stats.failuresAtCapacity) {
            file << cap << " ";
        }
        file << "\n\n";
        
        file << "2. PSNR ANALYSIS\n";
        file << "   Number of samples: " << stats.psnrValues.size() << "\n";
        file << "   Mean PSNR: " << std::fixed << std::setprecision(2) << stats.meanPSNR << " dB\n";
        file << "   95% Confidence Interval: [" << stats.psnrCI_low << ", " << stats.psnrCI_high << "] dB\n\n";
        
        file << "3. CAPACITY ANALYSIS\n";
        file << "   Maximum capacity: " << stats.maxCapacity << " bits\n";
        file << "   Average capacity: " << stats.avgCapacity << " bits\n";
        file << "   Capacity in bpp: " << (stats.avgCapacity / (512*512)) << " bpp\n";
        
        file.close();
    }
    
    void printStatistics(const DatasetStatistics& stats) {
        std::cout << "\n--- RESEARCH RESULTS: " << stats.name << " ---\n";
        std::cout << "1. Integrity Analysis:\n";
        std::cout << "   Restoration rate: " << std::fixed << std::setprecision(2) 
                  << stats.restorationRate << "% (" 
                  << stats.successfulRestorations << "/" << stats.totalImages << ")\n";
        
        std::cout << "2. PSNR Analysis:\n";
        std::cout << "   Mean PSNR: " << std::fixed << std::setprecision(2) << stats.meanPSNR << " dB\n";
        std::cout << "   95% CI: [" << stats.psnrCI_low << ", " << stats.psnrCI_high << "] dB\n";
        
        std::cout << "3. Capacity Analysis:\n";
        std::cout << "   Max capacity: " << stats.maxCapacity << " bits\n";
        std::cout << "   Avg capacity: " << stats.avgCapacity << " bits\n";
    }
    
    void compareDatasets(const std::vector<DatasetStatistics>& allStats, const std::string& outputDir) {
        std::ofstream comparison(outputDir + "/research/comparison.txt");
        
        comparison << "========================================\n";
        comparison << "COMPARATIVE ANALYSIS OF ALL DATASETS\n";
        comparison << "========================================\n\n";
        
        comparison << std::left << std::setw(15) << "Dataset"
                  << std::setw(15) << "Restore Rate"
                  << std::setw(15) << "Mean PSNR"
                  << std::setw(25) << "PSNR 95% CI"
                  << std::setw(15) << "Max Capacity"
                  << std::setw(15) << "Avg Capacity"
                  << "\n";
        comparison << std::string(90, '-') << "\n";
        
        for (const auto& stats : allStats) {
            comparison << std::left << std::setw(15) << stats.name
                      << std::setw(15) << std::fixed << std::setprecision(2) << stats.restorationRate
                      << std::setw(15) << stats.meanPSNR
                      << std::setw(25) << ("[" + std::to_string(stats.psnrCI_low) + ", " + 
                                           std::to_string(stats.psnrCI_high) + "]")
                      << std::setw(15) << stats.maxCapacity
                      << std::setw(15) << stats.avgCapacity
                      << "\n";
        }
        
        comparison << "\n\n";
        comparison << "CONCLUSIONS:\n";
        comparison << "1. Best restoration rate: ";
        
        double bestRate = 0;
        std::string bestDataset;
        for (const auto& stats : allStats) {
            if (stats.restorationRate > bestRate) {
                bestRate = stats.restorationRate;
                bestDataset = stats.name;
            }
        }
        comparison << bestDataset << " (" << bestRate << "%)\n";
        
        comparison << "2. Best image quality (highest PSNR): ";
        double bestPSNR = 0;
        bestDataset = "";
        for (const auto& stats : allStats) {
            if (stats.meanPSNR > bestPSNR) {
                bestPSNR = stats.meanPSNR;
                bestDataset = stats.name;
            }
        }
        comparison << bestDataset << " (" << bestPSNR << " dB)\n";
        
        comparison << "3. Highest capacity: ";
        double bestCap = 0;
        bestDataset = "";
        for (const auto& stats : allStats) {
            if (stats.maxCapacity > bestCap) {
                bestCap = stats.maxCapacity;
                bestDataset = stats.name;
            }
        }
        comparison << bestDataset << " (" << bestCap << " bits)\n";
        
        comparison.close();
        
        // Вывод на экран
        std::cout << "\n========== COMPARATIVE ANALYSIS ==========\n";
        std::cout << std::left << std::setw(15) << "Dataset"
                  << std::setw(15) << "Restore %"
                  << std::setw(15) << "PSNR"
                  << std::setw(25) << "PSNR CI"
                  << "\n";
        std::cout << std::string(70, '-') << "\n";
        
        for (const auto& stats : allStats) {
            std::cout << std::left << std::setw(15) << stats.name
                      << std::setw(15) << std::fixed << std::setprecision(2) << stats.restorationRate
                      << std::setw(15) << stats.meanPSNR
                      << std::setw(25) << ("[" + std::to_string(stats.psnrCI_low).substr(0,5) + ", " + 
                                           std::to_string(stats.psnrCI_high).substr(0,5) + "]")
                      << "\n";
        }
    }
};

int main() {
    std::string bossPath = "../lab1/set1";
    std::string medicalPath = "../lab1/set2";
    std::string flowersPath = "../lab1/set3";
    
    std::string dataFilePath = "message.txt";
    std::string outputDir = "research_results";
    
    fs::create_directories(outputDir);
    fs::create_directories(outputDir + "/research");
    
    // Создаем файл с данными, если его нет
    if (!fs::exists(dataFilePath)) {
        std::ofstream sampleFile(dataFilePath);
        sampleFile << "Research test data for histogram shifting steganography.\n";
        sampleFile << "This file is used to test capacity and integrity.\n";
        for (int i = 0; i < 100; i++) {
            sampleFile << "Line " << i << ": Testing data for capacity analysis.\n";
        }
        sampleFile.close();
    }
    
    ResearchAnalyzer analyzer;
    std::vector<DatasetStatistics> allStats;
    
    // Анализ каждого датасета
    allStats.push_back(analyzer.analyzeDataset(bossPath, "BOSS", dataFilePath, outputDir));
    allStats.push_back(analyzer.analyzeDataset(medicalPath, "Medical", dataFilePath, outputDir));
    allStats.push_back(analyzer.analyzeDataset(flowersPath, "Flowers", dataFilePath, outputDir));
    
    // Сравнительный анализ
    analyzer.compareDatasets(allStats, outputDir);
    
    std::cout << "\n========================================\n";
    std::cout << "Research complete!\n";
    std::cout << "Results saved in: " << outputDir << "/research/\n";
    std::cout << "========================================\n";
    
    return 0;
}