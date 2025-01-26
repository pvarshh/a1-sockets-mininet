#include <iostream>
#include <cstring>
#include <chrono>
#include <thread>
#include <spdlog/spdlog.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>
#include <arpa/inet.h>
#include "client.h"

void runClient(const std::string& host, int port, int time) {
    spdlog::set_level(spdlog::level::debug); 

    int clientSocket;
    struct sockaddr_in serverAddr;

    // Create socket
    clientSocket = socket(AF_INET, SOCK_STREAM, 0);
    if (clientSocket < 0) {
        spdlog::error("Error creating socket");
        exit(1);
    }

    // Connect to server
    serverAddr.sin_family = AF_INET;
    serverAddr.sin_port = htons(port);
    if (inet_pton(AF_INET, host.c_str(), &serverAddr.sin_addr) <= 0) {
        spdlog::error("Invalid address/Address not supported");
        close(clientSocket);
        exit(1);
    }
    if (connect(clientSocket, (struct sockaddr*)&serverAddr, sizeof(serverAddr)) < 0) {
        spdlog::error("Connection failed");
        close(clientSocket);
        exit(1);
    }

    // RTT estimation
    spdlog::debug("RTT Calculation Start");
    char buffer[1];
    std::vector<double> rttMeasurements;
    auto startTime = std::chrono::high_resolution_clock::now();
    for (int i = 0; i < 8; ++i) {
        buffer[0] = 'M';
        if (send(clientSocket, buffer, 1, 0) < 0) {
            spdlog::error("Error sending data");
            close(clientSocket);
            exit(1);
        }

        if (recv(clientSocket, buffer, 1, 0) < 0) {
            spdlog::error("Error receiving data");
            close(clientSocket);
            exit(1);
        }
        if (buffer[0] != 'A') {
            spdlog::error("Invalid ACK received");
            close(clientSocket);
            exit(1);
        }

        // Measure RTT
        auto endTime = std::chrono::high_resolution_clock::now();
        double rtt = std::chrono::duration<double, std::milli>(endTime - startTime).count();
        if (i >= 4) { // Exclude first 4 measurements
            spdlog::debug("RTT Calculation on {}th iteration: {} ms", i, static_cast<int>(rtt));
            rttMeasurements.push_back(rtt);
        }
        startTime = endTime;
    }

    // Calculate average RTT
    spdlog::debug("RTT Computation Start");
    double avgRtt = 0;
    for (double rtt : rttMeasurements) {
        avgRtt += rtt;
    }
    avgRtt /= rttMeasurements.size();
    spdlog::debug("RTT Computation End");
    spdlog::debug("RTT = {} ms", static_cast<int>(avgRtt));
    spdlog::debug("RTT Calculation Complete");

    // Data transmission
    int totalBytesSent = 0;
    int ackCount = 0; // Number of ACKs received
    char dataBuffer[81920] = {0}; // 80KB of zeros
    auto dataStartTime = std::chrono::high_resolution_clock::now();
    auto endTime = dataStartTime + std::chrono::seconds(time);
    while (std::chrono::high_resolution_clock::now() < endTime) {
        int bytesSent = 0;
        while (bytesSent < sizeof(dataBuffer)) {
            int result = send(clientSocket, dataBuffer + bytesSent, sizeof(dataBuffer) - bytesSent, 0);
            if (result < 0) {
                spdlog::error("Error sending data");
                close(clientSocket);
                exit(1);
            }
            bytesSent += result;
        }
        totalBytesSent += bytesSent;

        // Wait for ACK
        if (recv(clientSocket, buffer, 1, 0) < 0) {
            spdlog::error("Error receiving ACK");
            close(clientSocket);
            exit(1);
        }
        if (buffer[0] != 'A') {
            spdlog::error("Invalid ACK received");
            close(clientSocket);
            exit(1);
        }
        ackCount++;
    }
    auto dataEndTime = std::chrono::high_resolution_clock::now();

    // Calculate throughput
    double duration = std::chrono::duration<double>(dataEndTime - dataStartTime).count();
    double rate = (totalBytesSent * 8.0 / 1'000'000.0) / (duration - ((avgRtt * ackCount) / 1000));

    // Log summary
    spdlog::info("Sent={} KB, Rate={:.3f} Mbps, RTT={} ms", totalBytesSent / 1024, rate, static_cast<int>(avgRtt));

    // Cleanup
    close(clientSocket);
}