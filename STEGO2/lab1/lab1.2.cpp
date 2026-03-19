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

class ImageQualityMetrics {
public:
    static double calculateMSE(const std::vector<uint8_t>& original, 
                              const std::vector<uint8_t>& modified) {
        if (original.size() != modified.size()) return -1.0;
        double sum = 0.0;
        for (size_t i = 0; i < original.size(); i++) {
            double diff = static_cast<double>(original[i]) - static_cast<double>(modified[i]);
            sum += diff * diff;
        }
        return sum / original.size();
    }
    
    static double calculatePSNR(double mse) {
        if (mse <= 0) return 100.0;
        double maxPixel = 255.0;
        return 10.0 * log10((maxPixel * maxPixel) / mse);
    }
    
    static double calculateSSIM(const std::vector<uint8_t>& img1, 
                               const std::vector<uint8_t>& img2) {
        if (img1.size() != img2.size()) return -1.0;
        
        double C1 = 6.5025, C2 = 58.5225;
        
        double mu1 = 0.0, mu2 = 0.0;
        for (size_t i = 0; i < img1.size(); i++) {
            mu1 += img1[i];
            mu2 += img2[i];
        }
        mu1 /= img1.size();
        mu2 /= img2.size();
        
        double sigma1_sq = 0.0, sigma2_sq = 0.0, sigma12 = 0.0;
        for (size_t i = 0; i < img1.size(); i++) {
            sigma1_sq += (img1[i] - mu1) * (img1[i] - mu1);
            sigma2_sq += (img2[i] - mu2) * (img2[i] - mu2);
            sigma12 += (img1[i] - mu1) * (img2[i] - mu2);
        }
        sigma1_sq /= (img1.size() - 1);
        sigma2_sq /= (img2.size() - 1);
        sigma12 /= (img2.size() - 1);
        
        double numerator = (2 * mu1 * mu2 + C1) * (2 * sigma12 + C2);
        double denominator = (mu1 * mu1 + mu2 * mu2 + C1) * (sigma1_sq + sigma2_sq + C2);
        
        return numerator / denominator;
    }
    
    static double calculateEntropy(const std::vector<uint8_t>& data) {
        std::vector<int> histogram(256, 0);
        for (uint8_t val : data) {
            histogram[val]++;
        }
        
        double entropy = 0.0;
        double size = data.size();
        for (int count : histogram) {
            if (count > 0) {
                double p = count / size;
                entropy -= p * log2(p);
            }
        }
        return entropy;
    }
    
    static double calculateAdjacentCorrelation(const std::vector<uint8_t>& data, int width, int height) {
        if (data.size() != width * height) return 0.0;
        
        std::vector<double> horizontal;
        std::vector<double> vertical;
        
        for (int y = 0; y < height; y++) {
            for (int x = 0; x < width - 1; x++) {
                horizontal.push_back(data[y * width + x]);
                horizontal.push_back(data[y * width + x + 1]);
            }
        }
        
        for (int x = 0; x < width; x++) {
            for (int y = 0; y < height - 1; y++) {
                vertical.push_back(data[y * width + x]);
                vertical.push_back(data[(y + 1) * width + x]);
            }
        }
        
        return calculateCorrelation(horizontal) + calculateCorrelation(vertical) / 2.0;
    }
    
private:
    static double calculateCorrelation(const std::vector<double>& values) {
        if (values.size() % 2 != 0) return 0.0;
        int n = values.size() / 2;
        
        double mean1 = 0.0, mean2 = 0.0;
        for (int i = 0; i < n; i++) {
            mean1 += values[2 * i];
            mean2 += values[2 * i + 1];
        }
        mean1 /= n;
        mean2 /= n;
        
        double cov = 0.0, var1 = 0.0, var2 = 0.0;
        for (int i = 0; i < n; i++) {
            double diff1 = values[2 * i] - mean1;
            double diff2 = values[2 * i + 1] - mean2;
            cov += diff1 * diff2;
            var1 += diff1 * diff1;
            var2 += diff2 * diff2;
        }
        
        if (var1 == 0 || var2 == 0) return 0.0;
        return cov / sqrt(var1 * var2);
    }
};

class GrayBMP {
private:
    BMPHeader header;
    std::vector<uint8_t> palette;
    std::vector<uint8_t> pixels;
    std::string filename;
    int width, height;
    bool isLoaded;
    std::string datasetType;

    bool readBMP(const std::string& file) {
        std::ifstream f(file, std::ios::binary);
        if (!f) return false;

        f.read(reinterpret_cast<char*>(&header), sizeof(header));
        if (header.bfType != 0x4D42 || header.biBitCount != 8) {
            return false;
        }

        width = header.biWidth;
        height = std::abs(header.biHeight);
        
        palette.resize(1024);
        f.seekg(sizeof(header), std::ios::beg);
        f.read(reinterpret_cast<char*>(palette.data()), 1024);

        f.seekg(header.bfOffBits, std::ios::beg);
        int rowSize = (width * 8 + 31) / 32 * 4;
        int dataSize = rowSize * height;
        std::vector<uint8_t> rawData(dataSize);
        f.read(reinterpret_cast<char*>(rawData.data()), dataSize);

        pixels.resize(width * height);
        for (int y = 0; y < height; y++) {
            int srcY = (header.biHeight > 0) ? (height - 1 - y) : y;
            for (int x = 0; x < width; x++) {
                pixels[y * width + x] = rawData[srcY * rowSize + x];
            }
        }

        isLoaded = true;
        filename = file;
        f.close();
        return true;
    }

    bool writeBMP(const std::string& file) {
        if (!isLoaded) return false;

        std::ofstream f(file, std::ios::binary);
        if (!f) return false;

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

        f.write(reinterpret_cast<char*>(&header), sizeof(header));
        f.write(reinterpret_cast<char*>(palette.data()), 1024);
        f.write(reinterpret_cast<char*>(rawData.data()), dataSize);

        f.close();
        return true;
    }

public:
    GrayBMP() : isLoaded(false), width(0), height(0), datasetType("Unknown") {}
    
    void setDatasetType(const std::string& type) { datasetType = type; }
    std::string getDatasetType() const { return datasetType; }
    std::string getFilename() const { return filename; }

    bool load(const std::string& file) {
        return readBMP(file);
    }

    bool save(const std::string& file) {
        return writeBMP(file);
    }

    int getWidth() const { return width; }
    int getHeight() const { return height; }
    int getSize() const { return width * height; }
    uint8_t* getPixelData() { return pixels.data(); }
    const uint8_t* getPixelData() const { return pixels.data(); }
    std::vector<uint8_t> getPixels() const { return pixels; }

    GrayBMP extractBitPlane(int k) {
        GrayBMP result;
        if (!isLoaded || k < 1 || k > 8) return result;

        result.header = this->header;
        result.palette = this->palette;
        result.width = this->width;
        result.height = this->height;
        result.isLoaded = true;
        result.datasetType = this->datasetType;

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
        if (!msgFile) return -1;

        std::vector<uint8_t> messageData;
        msgFile.seekg(0, std::ios::end);
        std::streampos fileSize = msgFile.tellg();
        msgFile.seekg(0, std::ios::beg);
        messageData.resize(fileSize);
        msgFile.read(reinterpret_cast<char*>(messageData.data()), fileSize);
        msgFile.close();

        int capacity = pixels.size();
        int messageBits = messageData.size() * 8;

        if (messageBits > capacity) {
            std::cerr << "Сообщение слишком большое!\n";
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

        if (!writeBMP(outputFile)) return -1;
        return bitsWritten;
    }

    std::vector<int> getHistogram() const {
        std::vector<int> hist(256, 0);
        for (uint8_t val : pixels) {
            hist[val]++;
        }
        return hist;
    }

    void saveHistogram(const std::string& filename) {
        auto hist = getHistogram();
        std::ofstream file(filename);
        file << "Brightness,Count\n";
        for (int i = 0; i < 256; i++) {
            file << i << "," << hist[i] << "\n";
        }
        file.close();
    }
};

class SteganographyResearcher {
private:
    std::vector<GrayBMP> set1;
    std::vector<GrayBMP> set2;
    std::vector<GrayBMP> set3;
    std::string messageFile;
    
    struct ResearchResult {
        std::string dataset;
        std::string imageFile;
        int bitPlane;
        double mse;
        double psnr;
        double ssim;
        double entropyOriginal;
        double entropyModified;
        double correlationOriginal;
        double correlationModified;
    };
    
    std::vector<ResearchResult> allResults;

public:
    SteganographyResearcher(const std::string& msgFile) : messageFile(msgFile) {}
    
    bool loadDatasets(const std::string& pathSet1, const std::string& pathSet2, 
                      const std::string& pathSet3, int maxImages = 100) {
        
        loadImagesFromPath(pathSet1, set1, "set1", maxImages);
        loadImagesFromPath(pathSet2, set2, "set2", maxImages);
        loadImagesFromPath(pathSet3, set3, "set3", maxImages);
        
        return !set1.empty() || !set2.empty() || !set3.empty();
    }
    
    void visualizeBitPlanes(int numRepresentative = 5) {
        visualizeForDataset(set1, "set1", numRepresentative);
        visualizeForDataset(set2, "set2", numRepresentative);
        visualizeForDataset(set3, "set3", numRepresentative);
    }
    
    void evaluateStructure() {
        evaluateDatasetStructure(set1, "set1");
        evaluateDatasetStructure(set2, "set2");
        evaluateDatasetStructure(set3, "set3");
    }
    
    void embedAndEvaluate() {
        if (!set1.empty()) {
            evaluateEmbeddingForImage(set1[0], "set1_sample");
        }
        if (!set2.empty()) {
            evaluateEmbeddingForImage(set2[0], "set2_sample");
        }
        if (!set3.empty()) {
            evaluateEmbeddingForImage(set3[0], "set3_sample");
        }
    }
    
    void generateHistograms() {
        for (int i = 0; i < std::min(3, (int)set1.size()); i++) {
            generateHistogramPair(set1[i], "set1_" + std::to_string(i+1));
        }
        
        for (int i = 0; i < std::min(3, (int)set2.size()); i++) {
            generateHistogramPair(set2[i], "set2_" + std::to_string(i+1));
        }
        
        for (int i = 0; i < std::min(3, (int)set3.size()); i++) {
            generateHistogramPair(set3[i], "set3_" + std::to_string(i+1));
        }
    }
    
    void systematicComparison() {
        compareDataset(set1, "set1", 10);
        compareDataset(set2, "set2", 10);
        compareDataset(set3, "set3", 10);
        
        printSummaryTable();
    }
    
private:
    void loadImagesFromPath(const std::string& path, std::vector<GrayBMP>& images, 
                           const std::string& type, int maxImages) {
        if (!fs::exists(path)) return;
        
        int count = 0;
        for (const auto& entry : fs::directory_iterator(path)) {
            if (count >= maxImages) break;
            
            if (entry.path().extension() == ".bmp" || entry.path().extension() == ".BMP") {
                GrayBMP img;
                if (img.load(entry.path().string())) {
                    img.setDatasetType(type);
                    images.push_back(img);
                    count++;
                }
            }
        }
    }
    
    void visualizeForDataset(std::vector<GrayBMP>& images, const std::string& name, int count) {
        int numToProcess = std::min(count, (int)images.size());
        for (int i = 0; i < numToProcess; i++) {
            for (int k = 1; k <= 8; k++) {
                GrayBMP plane = images[i].extractBitPlane(k);
                std::string planeFile = "visual\\plane_" + name + "_img" + std::to_string(i+1) + "_k" + std::to_string(k) + ".bmp";
                plane.save(planeFile);
            }
        }
    }
    
    void evaluateDatasetStructure(std::vector<GrayBMP>& images, const std::string& name) {
        if (images.empty()) return;
        
        std::cout << "\n" << name << ":\n";
        
        for (int i = 0; i < std::min(5, (int)images.size()); i++) {
            std::cout << "  Изображение " << (i+1) << ":\n";
            
            for (int k = 1; k <= 6; k++) {
                GrayBMP plane = images[i].extractBitPlane(k);
                double entropy = ImageQualityMetrics::calculateEntropy(plane.getPixels());
                double correlation = ImageQualityMetrics::calculateAdjacentCorrelation(
                    plane.getPixels(), plane.getWidth(), plane.getHeight());
                
                std::cout << "    Плоскость " << k << ": Энтропия=" << std::fixed << std::setprecision(2) 
                         << entropy << ", Корреляция=" << std::setprecision(3) << correlation << "\n";
            }
        }
    }
    
    void evaluateEmbeddingForImage(GrayBMP& image, const std::string& baseName) {
        std::vector<uint8_t> originalPixels = image.getPixels();
        
        std::cout << "\n  Исходное изображение: " << fs::path(image.getFilename()).filename().string() << "\n";
        
        std::cout << "  Результаты внедрения:\n";
        std::cout << "  -------------------------------\n";
        std::cout << "  k |   MSE   |  PSNR  |  SSIM  |\n";
        std::cout << "  -------------------------------\n";
        
        for (int k = 1; k <= 3; k++) {
            GrayBMP stego = image;
            std::string outputFile = "stego\\" + baseName + "_stego_k" + std::to_string(k) + ".bmp";
            
            int bitsWritten = stego.embedMessage(messageFile, k, outputFile);
            
            if (bitsWritten > 0) {
                double mse = ImageQualityMetrics::calculateMSE(originalPixels, stego.getPixels());
                double psnr = ImageQualityMetrics::calculatePSNR(mse);
                double ssim = ImageQualityMetrics::calculateSSIM(originalPixels, stego.getPixels());
                
                std::cout << "  " << k << " | " << std::setw(7) << std::setprecision(2) << mse 
                         << " | " << std::setw(6) << std::setprecision(2) << psnr 
                         << " | " << std::setw(6) << std::setprecision(3) << ssim << "\n";
                
                ResearchResult res;
                res.dataset = image.getDatasetType();
                res.imageFile = fs::path(image.getFilename()).filename().string();
                res.bitPlane = k;
                res.mse = mse;
                res.psnr = psnr;
                res.ssim = ssim;
                res.entropyOriginal = ImageQualityMetrics::calculateEntropy(originalPixels);
                res.entropyModified = ImageQualityMetrics::calculateEntropy(stego.getPixels());
                res.correlationOriginal = ImageQualityMetrics::calculateAdjacentCorrelation(
                    originalPixels, image.getWidth(), image.getHeight());
                res.correlationModified = ImageQualityMetrics::calculateAdjacentCorrelation(
                    stego.getPixels(), stego.getWidth(), stego.getHeight());
                
                allResults.push_back(res);
            }
        }
        std::cout << "  -------------------------------\n";
    }
    
    void generateHistogramPair(GrayBMP& image, const std::string& baseName) {
        std::string histOrigFile = "hist_" + baseName + "_original.csv";
        image.saveHistogram(histOrigFile);
        
        GrayBMP stego = image;
        std::string stegoFile = "stego\\" + baseName + "_stego_k1.bmp";
        stego.embedMessage(messageFile, 1, stegoFile);
        std::string histStegoFile = "hist_" + baseName + "_stego.csv";
        stego.saveHistogram(histStegoFile);
    }
    
    void compareDataset(const std::vector<GrayBMP>& images, const std::string& name, int count) {
        if (images.empty()) return;
        
        std::cout << "\n--- Сравнение для набора " << name << " ---\n";
        std::cout << "======================================================================\n";
        std::cout << "| Изобр. | Пл. |   MSE   |  PSNR  |  SSIM  | Энтропия |  Корреляция  |\n";
        std::cout << "|        |     |         |        |        | исх/ст   |   исх/ст     |\n";
        std::cout << "======================================================================\n";
        
        int numToProcess = std::min(count, (int)images.size());
        for (int i = 0; i < numToProcess; i++) {
            std::vector<uint8_t> originalPixels = images[i].getPixels();
            double origEntropy = ImageQualityMetrics::calculateEntropy(originalPixels);
            double origCorr = ImageQualityMetrics::calculateAdjacentCorrelation(
                originalPixels, images[i].getWidth(), images[i].getHeight());
            
            for (int k = 1; k <= 3; k++) {
                GrayBMP stego = images[i];
                std::string outputFile = "compare\\compare_" + name + "_img" + std::to_string(i+1) + "_k" + std::to_string(k) + ".bmp";
                int bitsWritten = stego.embedMessage(messageFile, k, outputFile);
                
                if (bitsWritten > 0) {
                    double mse = ImageQualityMetrics::calculateMSE(originalPixels, stego.getPixels());
                    double psnr = ImageQualityMetrics::calculatePSNR(mse);
                    double ssim = ImageQualityMetrics::calculateSSIM(originalPixels, stego.getPixels());
                    double stegoEntropy = ImageQualityMetrics::calculateEntropy(stego.getPixels());
                    double stegoCorr = ImageQualityMetrics::calculateAdjacentCorrelation(
                        stego.getPixels(), stego.getWidth(), stego.getHeight());
                    
                    std::cout << "|  " << std::setw(3) << (i+1) << "   |  " << k 
                             << "  | " << std::setw(7) << std::setprecision(2) << mse 
                             << " | " << std::setw(6) << std::setprecision(2) << psnr 
                             << " | " << std::setw(6) << std::setprecision(3) << ssim
                             << " |  " << std::setw(5) << std::setprecision(2) << origEntropy
                             << "/" << std::setw(5) << std::setprecision(2) << stegoEntropy
                             << " |  " << std::setw(6) << std::setprecision(3) << origCorr
                             << "/" << std::setw(6) << std::setprecision(3) << stegoCorr << " |\n";
                }
            }
            if (i < numToProcess - 1) {
                std::cout << "----------------------------------------------------------------------\n";
            }
        }
        std::cout << "======================================================================\n";
    }
    
    void printSummaryTable() {
        std::cout << "=============================================================\n";
        std::cout << "| Набор данных  | Плоскость | Ср. MSE | Ср. PSNR | Ср. SSIM |\n";
        std::cout << "=============================================================\n";
        
        std::map<std::string, std::map<int, std::vector<ResearchResult>>> grouped;
        
        for (const auto& res : allResults) {
            grouped[res.dataset][res.bitPlane].push_back(res);
        }
        
        for (const auto& dataset : grouped) {
            for (int k = 1; k <= 3; k++) {
                if (dataset.second.find(k) != dataset.second.end()) {
                    const auto& results = dataset.second.at(k);
                    
                    double avgMSE = 0, avgPSNR = 0, avgSSIM = 0;
                    for (const auto& res : results) {
                        avgMSE += res.mse;
                        avgPSNR += res.psnr;
                        avgSSIM += res.ssim;
                    }
                    avgMSE /= results.size();
                    avgPSNR /= results.size();
                    avgSSIM /= results.size();
                    
                    
                    std::cout << "| " << std::setw(13) << dataset.first << " |     " << k 
                             << "     | " << std::setw(7) << std::setprecision(2) << avgMSE
                             << " | " << std::setw(8) << std::setprecision(2) << avgPSNR
                             << " | " << std::setw(8) << std::setprecision(3) << avgSSIM << " |\n";
                }
            }
            std::cout << "-------------------------------------------------------------\n";
        }
    }
    
};

int main(int argc, char* argv[]) {
    setlocale(LC_ALL, "");

    fs::create_directories("compare");
    fs::create_directories("stego");
    fs::create_directories("visual");

    std::string messageFile = "message.txt";

    SteganographyResearcher researcher(messageFile);

    std::string bossPath = "./set1";
    std::string medicalPath = "./set2";
    std::string otherPath = "./set3";

    if (!researcher.loadDatasets(bossPath, medicalPath, otherPath, 100)) {
        std::cout << "Не найдены наборы изображений\n";
        return -1;
    }

    researcher.visualizeBitPlanes(5);
    researcher.evaluateStructure();
    researcher.embedAndEvaluate();
    researcher.generateHistograms();
    researcher.systematicComparison();

    return 0;
}