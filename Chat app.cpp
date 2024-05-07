#include <iostream>
#include <cstring>
#include <winsock2.h>
#include <thread>
#include <fstream>

#define MAX_CLIENTS 10
#define PORT 55555
#define BUFFER_SIZE 1024

SOCKET clients[MAX_CLIENTS]; // Array to store client sockets
int numClients = 0; // Number of connected clients

class StringArray {
private:
    std::string* elements;
    size_t capacity;
    size_t size;

    void resize() {
        capacity = (capacity == 0) ? 1 : capacity * 2;
        std::string* newElements = new std::string[capacity];
        for (size_t i = 0; i < size; ++i) {
            newElements[i] = elements[i];
        }
        delete[] elements;
        elements = newElements;
    }

public:
    StringArray() : elements(nullptr), capacity(0), size(0) {}

    ~StringArray() {
        delete[] elements;
    }

    void add(const std::string& str) {
        if (size == capacity) {
            resize();
        }
        elements[size++] = str;
    }

    size_t getSize() const {
        return size;
    }

    const std::string& operator[](size_t index) const {
        return elements[index]; // No bounds checking for simplicity
    }

    void clear() {
        delete[] elements;
        elements = nullptr;
        size = 0;
        capacity = 0;
    }

    void saveToFile(const std::string& filename) {
        std::ofstream outFile(filename);
        for (size_t i = 0; i < size; ++i) {
            outFile << elements[i] << std::endl;
        }
        outFile.close();
    }
};

StringArray chatHistory;

void clientThread(SOCKET clientSocket) {
    char buffer[BUFFER_SIZE] = { 0 };
    int valread;

    while (true) {
        valread = recv(clientSocket, buffer, BUFFER_SIZE, 0);
        if (valread > 0) {
            buffer[valread] = '\0'; // Null-terminate the string
            std::string message = "Client: " + std::string(buffer);
            chatHistory.add(message); // Store the message in history

            std::cout << message << std::endl;
            for (int i = 0; i < numClients; ++i) {
                if (clients[i] != clientSocket && clients[i] != INVALID_SOCKET) {
                    send(clients[i], buffer, valread, 0);
                }
            }
        }
        else {
            if (valread == 0) {
                std::cout << "Client disconnected" << std::endl;
            }
            else {
                std::cerr << "recv failed: " << WSAGetLastError() << std::endl;
            }

            for (int i = 0; i < numClients; ++i) {
                if (clients[i] == clientSocket) {
                    for (int j = i; j < numClients - 1; ++j) {
                        clients[j] = clients[j + 1];
                    }
                    clients[numClients - 1] = INVALID_SOCKET;
                    numClients--;
                    break;
                }
            }
            closesocket(clientSocket);
            break;
        }
    }

    if (numClients == 0 && chatHistory.getSize() > 0) {
        chatHistory.saveToFile("chat_history.txt");
        chatHistory.clear();
        std::cout << "Chat history saved to file." << std::endl;
    }
}



int main() {
    WSADATA wsData;
    if (WSAStartup(MAKEWORD(2, 2), &wsData) != 0) {
        std::cerr << "WSAStartup failed." << std::endl;
        return 1;
    }

    SOCKET server_fd, new_socket;
    struct sockaddr_in address;
    int opt = 1;
    int addrlen = sizeof(address);

    // Creating socket file descriptor
    if ((server_fd = socket(AF_INET, SOCK_STREAM, 0)) == INVALID_SOCKET) {
        std::cerr << "socket failed: " << WSAGetLastError() << std::endl;
        WSACleanup();
        return 1;
    }

    // Forcefully attaching socket to the port 55555
    if (setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, (char*)&opt, sizeof(opt)) == SOCKET_ERROR) {
        std::cerr << "setsockopt failed: " << WSAGetLastError() << std::endl;
        closesocket(server_fd);
        WSACleanup();
        return 1;
    }

    address.sin_family = AF_INET;
    address.sin_addr.s_addr = INADDR_ANY;
    address.sin_port = htons(PORT);

    // Bind the socket to localhost port 55555
    if (bind(server_fd, (struct sockaddr*)&address, sizeof(address)) == SOCKET_ERROR) {
        std::cerr << "bind failed: " << WSAGetLastError() << std::endl;
        closesocket(server_fd);
        WSACleanup();
        return 1;
    }

    // Listen for incoming connections
    if (listen(server_fd, 3) == SOCKET_ERROR) {
        std::cerr << "listen failed: " << WSAGetLastError() << std::endl;
        closesocket(server_fd);
        WSACleanup();
        return 1;
    }

    std::cout << "Server listening on port " << PORT << std::endl;

    // Accept incoming connections and handle them in separate threads
    while (true) {
        if ((new_socket = accept(server_fd, (struct sockaddr*)&address, &addrlen)) == INVALID_SOCKET) {
            std::cerr << "accept failed: " << WSAGetLastError() << std::endl;
            closesocket(server_fd);
            WSACleanup();
            return 1;
        }
        std::cout << "Client connected" << std::endl;

        if (numClients < MAX_CLIENTS) {
            // Add the new client socket to the array
            clients[numClients++] = new_socket;

            // Create a new thread to handle client communication
            std::thread clientThreadObj(clientThread, new_socket);
            clientThreadObj.detach(); // Detach the thread to allow it to run independently
        }
        else {
            std::cerr << "Max number of clients reached. Connection refused." << std::endl;
            closesocket(new_socket);
        }
    }

    // Close the server socket (never reaches here in this example)
    closesocket(server_fd);
    WSACleanup();
    return 0;
}
