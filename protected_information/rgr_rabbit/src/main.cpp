#include <iostream>
#include <fstream>
#include <vector>
#include <cstring>
#include <iomanip>
#include <random>
#include <getopt.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>

#include "ecdh.h"
#include "sha3.h"
#include "rabbit.h"
#include "utils.h"
#include "rdtsc.h"

#define PORT 8080
#define BUFFER_SIZE 4096

using namespace std;

class SecureFileTransfer {
private:
    vector<uint8_t> key;
    vector<uint8_t> iv;
    
public:
    SecureFileTransfer(const vector<uint8_t>& encryption_key, const vector<uint8_t>& init_vector)
        : key(encryption_key), iv(init_vector) {}
    
    void encryptFile(const string& input_file, const string& output_file) {
        ifstream in(input_file, ios::binary);
        if (!in.is_open()) {
            throw runtime_error("Cannot open input file: " + input_file);
        }
        
        in.seekg(0, ios::end);
        size_t file_size = in.tellg();
        in.seekg(0, ios::beg);
        
        vector<uint8_t> plaintext(file_size);
        in.read(reinterpret_cast<char*>(plaintext.data()), file_size);
        in.close();
        
        // Вычисляем хеш для проверки целостности
        auto file_hash = SHA3::hash(plaintext);
        cout << "File hash (SHA3-256): " << bytesToHex(file_hash.data(), file_hash.size()) << endl;
        
        // Инициализируем Rabbit
        RABBIT_CIPHER_CTX ctx;
        rabbit_init();
        rabbit_keysetup(&ctx, key.data(), 256);
        rabbit_ivsetup(&ctx, iv.data());
        
        // Шифруем
        vector<uint8_t> ciphertext(file_size);
        uint64_t start = rdtsc();
        rabbit_encrypt_bytes(&ctx, plaintext.data(), ciphertext.data(), file_size);
        uint64_t end = rdtsc();
        
        cout << "Encryption time: " << (end - start) << " cycles" << endl;
        
        // Сохраняем зашифрованный файл (хеш + зашифрованные данные)
        ofstream out(output_file, ios::binary);
        out.write(reinterpret_cast<const char*>(file_hash.data()), file_hash.size());
        out.write(reinterpret_cast<const char*>(ciphertext.data()), ciphertext.size());
        out.close();
        
        cout << "File encrypted: " << input_file << " -> " << output_file << endl;
    }
    
    bool decryptFile(const string& input_file, const string& output_file) {
        ifstream in(input_file, ios::binary);
        if (!in.is_open()) {
            throw runtime_error("Cannot open input file: " + input_file);
        }
        
        // Считываем сохраненный хеш
        vector<uint8_t> saved_hash(32);
        in.read(reinterpret_cast<char*>(saved_hash.data()), 32);
        
        // Считываем зашифрованные данные
        in.seekg(0, ios::end);
        size_t total_size = in.tellg();
        size_t cipher_size = total_size - 32;
        in.seekg(32, ios::beg);
        
        vector<uint8_t> ciphertext(cipher_size);
        in.read(reinterpret_cast<char*>(ciphertext.data()), cipher_size);
        in.close();
        
        // Расшифровываем
        RABBIT_CIPHER_CTX ctx;
        rabbit_init();
        rabbit_keysetup(&ctx, key.data(), 256);
        rabbit_ivsetup(&ctx, iv.data());
        
        vector<uint8_t> plaintext(cipher_size);
        uint64_t start = rdtsc();
        rabbit_encrypt_bytes(&ctx, ciphertext.data(), plaintext.data(), cipher_size);
        uint64_t end = rdtsc();
        
        cout << "Decryption time: " << (end - start) << " cycles" << endl;
        
        // Проверяем целостность
        auto computed_hash = SHA3::hash(plaintext);
        
        if (computed_hash == saved_hash) {
            // Сохраняем расшифрованный файл
            ofstream out(output_file, ios::binary);
            out.write(reinterpret_cast<const char*>(plaintext.data()), plaintext.size());
            out.close();
            cout << "File decrypted: " << input_file << " -> " << output_file << endl;
            cout << "Hash verification: PASSED" << endl;
            return true;
        } else {
            cout << "Hash verification: FAILED - file may be corrupted!" << endl;
            return false;
        }
    }
};

// Генерация случайного IV
vector<uint8_t> generateIV() {
    vector<uint8_t> iv(16);
    random_device rd;
    for (int i = 0; i < 16; i++) {
        iv[i] = rd() & 0xFF;
    }
    return iv;
}

// Клиентская часть
void runClient(const string& server_ip, int port, const string& filename) {
    cout << "=== SECURE FILE TRANSFER CLIENT ===" << endl;
    
    // Создаем сокет
    int sock = socket(AF_INET, SOCK_STREAM, 0);
    if (sock < 0) {
        perror("Socket creation failed");
        return;
    }
    
    sockaddr_in serv_addr{};
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_port = htons(port);
    
    if (inet_pton(AF_INET, server_ip.c_str(), &serv_addr.sin_addr) <= 0) {
        perror("Invalid address");
        close(sock);
        return;
    }
    
    if (connect(sock, (sockaddr*)&serv_addr, sizeof(serv_addr)) < 0) {
        perror("Connection failed");
        close(sock);
        return;
    }
    
    cout << "Connected to server" << endl;
    
    // ========== ECDH Key Exchange ==========
    cout << "ECDH key exchange in progress..." << endl;
    
    ECDH_P256 ecdh;
    mpz_t private_key, pubX, pubY;
    mpz_inits(private_key, pubX, pubY, nullptr);
    
    uint64_t start = rdtsc();
    ecdh.generateKeyPair(private_key, pubX, pubY);
    uint64_t end = rdtsc();
    cout << "Key generation time: " << (end - start) << " cycles" << endl;
    
    // Отправляем свой публичный ключ
    uint8_t clientPubX[32], clientPubY[32];
    ecdh.exportPublicKey(pubX, pubY, clientPubX, clientPubY);
    send(sock, clientPubX, 32, 0);
    send(sock, clientPubY, 32, 0);
    
    // Получаем публичный ключ сервера
    uint8_t serverPubX[32], serverPubY[32];
    recv(sock, serverPubX, 32, MSG_WAITALL);
    recv(sock, serverPubY, 32, MSG_WAITALL);
    
    mpz_t serverX, serverY;
    mpz_inits(serverX, serverY, nullptr);
    ecdh.importPublicKey(serverPubX, serverPubY, serverX, serverY);
    
    // Вычисляем общий секрет
    mpz_t sharedSecret;
    mpz_init(sharedSecret);
    start = rdtsc();
    ecdh.computeSharedSecret(private_key, serverX, serverY, sharedSecret);
    end = rdtsc();
    cout << "Shared secret computation time: " << (end - start) << " cycles" << endl;
    
    // Получаем ключ шифрования через SHA3
    auto secretBytes = mpzToVector(sharedSecret);
    auto encryptionKey = SHA3::hash(secretBytes);
    auto iv = generateIV();
    
    cout << "Shared key: " << bytesToHex(encryptionKey.data(), 32) << endl;
    cout << "IV: " << bytesToHex(iv.data(), 16) << endl;
    
    // Отправляем IV серверу
    send(sock, iv.data(), 16, 0);
    
    // Шифруем и отправляем файл
    SecureFileTransfer transfer(encryptionKey, iv);
    
    // Создаем тестовый файл если его нет
    ifstream checkFile(filename);
    if (!checkFile.is_open()) {
        ofstream createFile(filename);
        createFile << "This is a secret message for secure transfer!" << endl;
        createFile << "Line 2: Testing Rabbit cipher" << endl;
        createFile << "Line 3: ECDH + SHA3 integration" << endl;
        createFile.close();
        cout << "Created test file: " << filename << endl;
    }
    checkFile.close();
    
    transfer.encryptFile(filename, "temp_encrypted.bin");
    
    // Отправляем зашифрованный файл
    ifstream encFile("temp_encrypted.bin", ios::binary);
    vector<uint8_t> buffer(BUFFER_SIZE);
    
    cout << "Sending encrypted file..." << endl;
    while (encFile.good()) {
        encFile.read(reinterpret_cast<char*>(buffer.data()), BUFFER_SIZE);
        size_t bytes = encFile.gcount();
        if (bytes > 0) {
            send(sock, buffer.data(), bytes, 0);
        }
    }
    encFile.close();
    
    cout << "File sent successfully!" << endl;
    
    // Очистка
    close(sock);
    mpz_clears(private_key, pubX, pubY, serverX, serverY, sharedSecret, nullptr);
}

// Серверная часть
void runServer(int port) {
    cout << "=== SECURE FILE TRANSFER SERVER ===" << endl;
    
    int server_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (server_fd < 0) {
        perror("Socket creation failed");
        return;
    }
    
    int opt = 1;
    setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));
    
    sockaddr_in address{};
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = INADDR_ANY;
    address.sin_port = htons(port);
    
    if (bind(server_fd, (sockaddr*)&address, sizeof(address)) < 0) {
        perror("Bind failed");
        close(server_fd);
        return;
    }
    
    if (listen(server_fd, 3) < 0) {
        perror("Listen failed");
        close(server_fd);
        return;
    }
    
    cout << "Waiting for connection on port " << port << "..." << endl;
    
    int addrlen = sizeof(address);
    int client_socket = accept(server_fd, (sockaddr*)&address, (socklen_t*)&addrlen);
    if (client_socket < 0) {
        perror("Accept failed");
        close(server_fd);
        return;
    }
    
    cout << "Client connected!" << endl;
    
    // ========== ECDH Key Exchange ==========
    cout << "ECDH key exchange in progress..." << endl;
    
    ECDH_P256 ecdh;
    mpz_t private_key, pubX, pubY;
    mpz_inits(private_key, pubX, pubY, nullptr);
    
    uint64_t start = rdtsc();
    ecdh.generateKeyPair(private_key, pubX, pubY);
    uint64_t end = rdtsc();
    cout << "Key generation time: " << (end - start) << " cycles" << endl;
    
    // Получаем публичный ключ клиента
    uint8_t clientPubX[32], clientPubY[32];
    recv(client_socket, clientPubX, 32, MSG_WAITALL);
    recv(client_socket, clientPubY, 32, MSG_WAITALL);
    
    // Отправляем свой публичный ключ
    uint8_t serverPubX[32], serverPubY[32];
    ecdh.exportPublicKey(pubX, pubY, serverPubX, serverPubY);
    send(client_socket, serverPubX, 32, 0);
    send(client_socket, serverPubY, 32, 0);
    
    mpz_t clientX, clientY;
    mpz_inits(clientX, clientY, nullptr);
    ecdh.importPublicKey(clientPubX, clientPubY, clientX, clientY);
    
    // Вычисляем общий секрет
    mpz_t sharedSecret;
    mpz_init(sharedSecret);
    start = rdtsc();
    ecdh.computeSharedSecret(private_key, clientX, clientY, sharedSecret);
    end = rdtsc();
    cout << "Shared secret computation time: " << (end - start) << " cycles" << endl;
    
    // Получаем ключ шифрования через SHA3
    auto secretBytes = mpzToVector(sharedSecret);
    auto encryptionKey = SHA3::hash(secretBytes);
    
    // Получаем IV от клиента
    vector<uint8_t> iv(16);
    recv(client_socket, iv.data(), 16, MSG_WAITALL);
    
    cout << "Shared key: " << bytesToHex(encryptionKey.data(), 32) << endl;
    cout << "IV: " << bytesToHex(iv.data(), 16) << endl;
    
    // Получаем и расшифровываем файл
    SecureFileTransfer transfer(encryptionKey, iv);
    
    // Получаем зашифрованный файл
    ofstream encFile("received_encrypted.bin", ios::binary);
    vector<uint8_t> buffer(BUFFER_SIZE);
    ssize_t received;
    
    cout << "Receiving encrypted file..." << endl;
    while ((received = recv(client_socket, buffer.data(), BUFFER_SIZE, 0)) > 0) {
        encFile.write(reinterpret_cast<const char*>(buffer.data()), received);
    }
    encFile.close();
    
    cout << "File received, decrypting..." << endl;
    transfer.decryptFile("received_encrypted.bin", "received_file.txt");
    
    close(client_socket);
    close(server_fd);
    
    mpz_clears(private_key, pubX, pubY, clientX, clientY, sharedSecret, nullptr);
}

void printUsage(const char* prog_name) {
    cout << "Usage:" << endl;
    cout << "  " << prog_name << " --server [--port PORT]" << endl;
    cout << "  " << prog_name << " --client --ip IP --port PORT --file FILE" << endl;
    cout << endl;
    cout << "Options:" << endl;
    cout << "  --server          Run as server" << endl;
    cout << "  --client          Run as client" << endl;
    cout << "  --ip IP           Server IP address (default: 127.0.0.1)" << endl;
    cout << "  --port PORT       Port number (default: 8080)" << endl;
    cout << "  --file FILE       File to transfer" << endl;
    cout << "  --help            Show this help" << endl;
}

int main(int argc, char* argv[]) {
    bool is_server = false;
    bool is_client = false;
    string server_ip = "127.0.0.1";
    int port = 8080;
    string filename = "test.txt";
    
    // Простой разбор аргументов командной строки
    for (int i = 1; i < argc; i++) {
        string arg = argv[i];
        if (arg == "--server") {
            is_server = true;
        } else if (arg == "--client") {
            is_client = true;
        } else if (arg == "--ip" && i + 1 < argc) {
            server_ip = argv[++i];
        } else if (arg == "--port" && i + 1 < argc) {
            port = atoi(argv[++i]);
        } else if (arg == "--file" && i + 1 < argc) {
            filename = argv[++i];
        } else if (arg == "--help") {
            printUsage(argv[0]);
            return 0;
        }
    }
    
    try {
        if (is_server) {
            runServer(port);
        } else if (is_client) {
            runClient(server_ip, port, filename);
        } else {
            printUsage(argv[0]);
            return 1;
        }
    } catch (const exception& e) {
        cerr << "Error: " << e.what() << endl;
        return 1;
    }
    
    return 0;
}
