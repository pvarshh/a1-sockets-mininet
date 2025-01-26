#include <iostream>
#include <cstring>
#include <chrono>
#include <thread>
#include <spdlog/spdlog.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>
#include <arpa/inet.h>
#include "server.h"

void runServer(int port) {
    int serverSocket, clientSocket;
    struct sockaddr_in serverAddr, clientAddr;
    socklen_t clientAddrLen = sizeof(clientAddr);

    // Create socket
    serverSocket = socket(AF_INET, SOCK_STREAM, 0);
    if (serverSocket < 0) {
        spdlog::error("Error creating socket");
        exit(1);
    }

    // Allow port reuse
    int opt = 1;
    if (setsockopt(serverSocket, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) < 0) {
        spdlog::error("Error setting socket options");
        close(serverSocket);
        exit(1);
    }

    // Bind socket
    serverAddr.sin_family = AF_INET;
    serverAddr.sin_addr.s_addr = INADDR_ANY;
    serverAddr.sin_port = htons(port);
    if (bind(serverSocket, (struct sockaddr*)&serverAddr, sizeof(serverAddr)) < 0) {
        spdlog::error("Error binding socket");
        close(serverSocket);
        exit(1);
    }

    // Listen for connections
    if (listen(serverSocket, 1) < 0) {
        spdlog::error("Error listening on socket");
        close(serverSocket);
        exit(1);
    }

    spdlog::info("iPerfer server started");

    // Accept client connection
    clientSocket = accept(serverSocket, (struct sockaddr*)&clientAddr, &clientAddrLen);
    if (clientSocket < 0) {
        spdlog::error("Error accepting connection");
        close(serverSocket);
        exit(1);
    }
    spdlog::info("Client connected");

    // RTT estimation
    spdlog::info("RTT Calculation Start");
    char buffer[1];
    std::vector<double> rttMeasurements;
    auto startTime = std::chrono::high_resolution_clock::now();
    for (int i = 0; i < 8; ++i) {
        if (recv(clientSocket, buffer, 1, 0) < 0) {
            spdlog::error("Error receiving data");
            close(clientSocket);
            close(serverSocket);
            exit(1);
        }
        if (buffer[0] != 'M') {
            spdlog::error("Invalid data received");
            close(clientSocket);
            close(serverSocket);
            exit(1);
        }

        // Send ACK
        buffer[0] = 'A';
        if (send(clientSocket, buffer, 1, 0) < 0) {
            spdlog::error("Error sending data");
            close(clientSocket);
            close(serverSocket);
            exit(1);
        }

        // Measure RTT
        auto endTime = std::chrono::high_resolution_clock::now();
        double rtt = std::chrono::duration<double, std::milli>(endTime - startTime).count();
        if (i >= 4) { // Exclude first 4 measurements
            spdlog::info("RTT Calculation on {}th iteration: {} ms", i, static_cast<int>(rtt));
            rttMeasurements.push_back(rtt);
        }
        startTime = endTime;
    }

    // Calculate average RTT
    spdlog::info("RTT Computation Start");
    double avgRtt = 0;
    for (double rtt : rttMeasurements) {
        avgRtt += rtt;
    }
    avgRtt /= rttMeasurements.size();
    spdlog::info("RTT Computation End");
    spdlog::info("RTT = {} ms", static_cast<int>(avgRtt));
    spdlog::info("RTT Calculation Complete");


    // Data transmission
    int totalBytesReceived = 0;
    auto dataStartTime = std::chrono::high_resolution_clock::now();
    while (true) {
        char dataBuffer[81920]; // 80KB
        int bytesReceived = 0;
        while (bytesReceived < sizeof(dataBuffer)) {
            int result = recv(clientSocket, dataBuffer + bytesReceived, sizeof(dataBuffer) - bytesReceived, 0);
            if (result <= 0) {
                // Connection closed or error
                goto end_transmission;
            }
            bytesReceived += result;
        }
        totalBytesReceived += bytesReceived;

        // Send ACK
        buffer[0] = 'A';
        if (send(clientSocket, buffer, 1, 0) < 0) {
            spdlog::error("Error sending ACK");
            close(clientSocket);
            close(serverSocket);
            exit(1);
        }
    }
    end_transmission:
    auto dataEndTime = std::chrono::high_resolution_clock::now();

    // Calculate throughput
    double elapsedTime = std::chrono::duration<double>(dataEndTime - dataStartTime).count();
    double rate = (totalBytesReceived * 8) / (elapsedTime * 1000000); // Mbps

    // Log summary
    spdlog::info("Received={} KB, Rate={:.3f} Mbps, RTT={} ms", totalBytesReceived / 1024, rate, static_cast<int>(avgRtt));

    // Cleanup
    close(clientSocket);
    close(serverSocket);
}