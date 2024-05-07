#include <iostream>
#include <string>
#include <fstream>
#include <sstream>
#include <vector>
#include <WinSock2.h>
#include <WS2tcpip.h>
#include <thread>

#pragma comment(lib, "ws2_32.lib")

// Function to check if a username exists in the credentials file
bool doesUsernameExist(const std::string& username, const std::string& filename) {
    std::ifstream file(filename);
    std::string line;
    while (std::getline(file, line)) {
        std::istringstream iss(line);
        std::string storedUsername;
        if (std::getline(iss, storedUsername, ',')) {
            if (storedUsername == username) {
                return true;
            }
        }
    }
    return false;
}

// Function to validate login credentials
bool login(const std::string& username, const std::string& password, const std::string& filename) {
    std::ifstream file(filename);
    std::string line;
    while (std::getline(file, line)) {
        std::istringstream iss(line);
        std::string storedUsername, storedPassword;
        if (std::getline(iss, storedUsername, ',') && std::getline(iss, storedPassword, ',')) {
            if (storedUsername == username && storedPassword == password) {
                return true;
            }
        }
    }
    return false;
}

// Function to create a new user account
void signup(const std::string& username, const std::string& password, const std::string& filename) {
    std::ofstream file(filename, std::ios_base::app);
    if (file.is_open()) {
        file << username << "," << password << "\n";
        std::cout << "Account created successfully!\n";
    }
    else {
        std::cerr << "Error creating account.\n";
    }
}

// Function to decrypt
std::string decryptCaesarCipher(const std::string& message, int shift) {
    std::string decryptedMessage = "";
    for (char ch : message) {
        // Decrypt uppercase letters
        if (std::isupper(ch)) {
            decryptedMessage += static_cast<char>((ch - 'A' - shift + 26) % 26 + 'A');
        }
        // Decrypt lowercase letters
        else if (std::islower(ch)) {
            decryptedMessage += static_cast<char>((ch - 'a' - shift + 26) % 26 + 'a');
        }
        // Leave non-alphabetic characters unchanged
        else {
            decryptedMessage += ch;
        }
    }
    return decryptedMessage;
}

// Function to receive messages from the server
void receiveMessages(SOCKET clientSocket, int shift) {
    char buffer[1024];
    int bytesReceived;
    while (true) {
        bytesReceived = recv(clientSocket, buffer, sizeof(buffer), 0);
        if (bytesReceived > 0) {
            buffer[bytesReceived] = '\0'; // Null-terminate the received data
            std::string decryptedMessage = decryptCaesarCipher(std::string(buffer), shift);
            std::cout << "Received from server: " << decryptedMessage << std::endl;
        }
        else if (bytesReceived == 0) {
            std::cerr << "Connection closed by server.\n";
            break;
        }
        else {
            std::cerr << "recv failed: " << WSAGetLastError() << std::endl;
            break;
        }
    }
}

// Function to encrypt a message using Caesar cipher
std::string encryptCaesarCipher(const std::string& message, int shift) {
    std::string encryptedMessage = "";
    for (char ch : message) {
        // Encrypt uppercase letters
        if (std::isupper(ch)) {
            encryptedMessage += static_cast<char>((ch - 'A' + shift) % 26 + 'A');
        }
        // Encrypt lowercase letters
        else if (std::islower(ch)) {
            encryptedMessage += static_cast<char>((ch - 'a' + shift) % 26 + 'a');
        }
        // Leave non-alphabetic characters unchanged
        else {
            encryptedMessage += ch;
        }
    }
    return encryptedMessage;
}

// Function to send an encrypted message to the server
void sendEncryptedMessage(SOCKET clientSocket, const std::string& message, int shift) {
    std::string encryptedMessage = encryptCaesarCipher(message, shift);
    send(clientSocket, encryptedMessage.c_str(), encryptedMessage.length(), 0);
}

int main() {
    WSADATA wsaData;
    int iResult;

    // Initialize Winsock
    iResult = WSAStartup(MAKEWORD(2, 2), &wsaData);
    if (iResult != 0) {
        std::cerr << "WSAStartup failed: " << iResult << std::endl;
        return 1;
    }

    // File to store user credentials
    std::string credentialsFile = "credentials.txt";

    // Login or Signup
    int choice;
    std::cout << "Choose an option:\n";
    std::cout << "1. Login\n";
    std::cout << "2. Signup\n";
    std::cin >> choice;
    std::cin.ignore(); // Ignore the newline character

    std::string username, password;

    switch (choice) {
    case 1: {
        std::cout << "Enter username: ";
        std::getline(std::cin, username);
        std::cout << "Enter password: ";
        std::getline(std::cin, password);
        if (login(username, password, credentialsFile)) {
            std::cout << "Login successful!\n";
        }
        else {
            std::cerr << "Invalid username or password.\n";
            return 1;
        }
        break;
    }
    case 2: {
        std::cout << "Enter username: ";
        std::getline(std::cin, username);
        if (doesUsernameExist(username, credentialsFile)) {
            std::cerr << "Username already exists. Please choose another.\n";
            return 1;
        }
        std::cout << "Enter password: ";
        std::getline(std::cin, password);
        signup(username, password, credentialsFile);
        break;
    }
    default:
        std::cerr << "Invalid choice.\n";
        return 1;
    }

    // Create a socket
    SOCKET clientSocket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (clientSocket == INVALID_SOCKET) {
        std::cerr << "Error creating socket: " << WSAGetLastError() << std::endl;
        WSACleanup();
        return 1;
    }

    // Server address configuration
    sockaddr_in serverAddr;
    serverAddr.sin_family = AF_INET;
    serverAddr.sin_port = htons(55555);
    inet_pton(AF_INET, "127.0.0.1", &serverAddr.sin_addr);

    // Connect to server
    if (connect(clientSocket, reinterpret_cast<SOCKADDR*>(&serverAddr), sizeof(serverAddr)) == SOCKET_ERROR) {
        std::cerr << "Failed to connect to server: " << WSAGetLastError() << std::endl;
        closesocket(clientSocket);
        WSACleanup();
        return 1;
    }
    int shift;

    std::string message;
    std::cout << "Enter shift value for Caesar cipher: ";
    std::cin >> shift;
    std::cin.ignore(); // Ignore the newline character

    // Start a thread to receive messages from the server
    std::thread receiveThread(receiveMessages, clientSocket, shift);

    // Input message from user and send to server
    
    while (true) {
        std::cout << "Enter message (or type 'exit' to quit): ";
        std::getline(std::cin, message);
        if (message == "exit") {
            break;
        }
        sendEncryptedMessage(clientSocket, message, shift);
    }

    // Close socket and cleanup
    closesocket(clientSocket);
    WSACleanup();

    // Join the receive thread before exiting
    receiveThread.join();

    return 0;
}