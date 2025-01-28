#include <iostream>
#include <cxxopts.hpp>
#include <iostream>
#include <cstring>
#include <chrono>
#include <thread>
#include <spdlog/spdlog.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>
#include <arpa/inet.h>

void runServer(int port) {
    spdlog::set_level(spdlog::level::debug); 

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
    spdlog::debug("RTT Calculation Start");
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
    int totalBytesReceived = 0;
    int ackCount = 0; // Number of ACKs sent
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
        ackCount++;
    }
    end_transmission:
    auto dataEndTime = std::chrono::high_resolution_clock::now();

    // Calculate throughput
    
    double duration = std::chrono::duration<double, std::milli>(dataEndTime - dataStartTime).count();

    spdlog::debug("megabits: {}", totalBytesReceived * 8.0 / 1'000'000.0);
    spdlog::debug("start: {}", std::chrono::duration_cast<std::chrono::milliseconds>(dataStartTime.time_since_epoch()).count());
    spdlog::debug("end: {}", std::chrono::duration_cast<std::chrono::milliseconds>(dataEndTime.time_since_epoch()).count());
    spdlog::debug("Duration: {}", duration);
    spdlog::debug("avgRtt: {}", avgRtt);
    spdlog::debug("ackCount: {}", ackCount);
    spdlog::debug("product: {}", avgRtt * ackCount);
    spdlog::debug("milliseconds: {}", (duration - (avgRtt * ackCount)));;
    spdlog::debug("time: {}", ((duration - (avgRtt * ackCount)) / 1'000.0));
    double rate = (totalBytesReceived * (8.0 / 1'000'000.0)) / ((duration - (avgRtt * ackCount)) / 1'000.0);

    // Log summary
    spdlog::info("Received={} KB, Rate={:.3f} Mbps, RTT={} ms", totalBytesReceived / 1024, rate, static_cast<int>(avgRtt));

    // Cleanup
    close(clientSocket);
    close(serverSocket);
}


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
    double duration = std::chrono::duration<double, std::milli>(dataEndTime - dataStartTime).count();
    spdlog::debug("megabits: {}", totalBytesSent * 8.0 / 1'000'000.0);
    spdlog::debug("start: {}", std::chrono::duration_cast<std::chrono::milliseconds>(dataStartTime.time_since_epoch()).count());
    spdlog::debug("end: {}", std::chrono::duration_cast<std::chrono::milliseconds>(dataEndTime.time_since_epoch()).count());
    spdlog::debug("Duration: {}", duration);
    spdlog::debug("avgRtt: {}", avgRtt);
    spdlog::debug("ackCount: {}", ackCount);
    spdlog::debug("product: {}", avgRtt * ackCount);
    spdlog::debug("milliseconds: {}", (duration - (avgRtt * ackCount)));;
    spdlog::debug("time: {}", ((duration - (avgRtt * ackCount)) / 1'000.0));
    double rate = (totalBytesSent * (8.0 / 1'000'000.0)) / ((duration - (avgRtt * ackCount)) / 1'000.0);

    // Log summary
    spdlog::info("Sent={} KB, Rate={:.3f} Mbps, RTT={} ms", totalBytesSent / 1024, rate, static_cast<int>(avgRtt));

    // Cleanup
    close(clientSocket);
}

int main(int argc, char* argv[]) {
    cxxopts::Options options("iPerfer", "A simple network performance measurement tool");
    options.add_options()
        ("s,server", "Enable server mode", cxxopts::value<bool>()->default_value("false"))
        ("c,client", "Enable client mode", cxxopts::value<bool>()->default_value("false"))
        ("p,port", "Port number", cxxopts::value<int>())
        ("h,host", "Server hostname", cxxopts::value<std::string>())
        ("t,time", "Time in seconds to send data", cxxopts::value<int>());

    auto result = options.parse(argc, argv);

    if (result.count("server") && result["server"].as<bool>()) {
        if (!result.count("port")) {
            std::cerr << "Error: port number is required in server mode" << std::endl;
            return 1;
        }
        int port = result["port"].as<int>();
        if (port < 1024 || port > 65535) {
            std::cerr << "Error: port number must be in the range [1024, 65535]" << std::endl;
            return 1;
        }
        runServer(port);
    } else if (result.count("client") && result["client"].as<bool>()) {
        if (!result.count("host") || !result.count("port") || !result.count("time")) {
            std::cerr << "Error: host, port, and time are required in client mode" << std::endl;
            return 1;
        }
        std::string host = result["host"].as<std::string>();
        int port = result["port"].as<int>();
        int time = result["time"].as<int>();
        if (port < 1024 || port > 65535) {
            std::cerr << "Error: port number must be in the range [1024, 65535]" << std::endl;
            return 1;
        }
        if (time <= 0) {
            std::cerr << "Error: time must be greater than 0" << std::endl;
            return 1;
        }
        runClient(host, port, time);
    } else {
        std::cerr << "Error: must specify either server or client mode" << std::endl;
        return 1;
    }

    return 0;
}