#include <iostream>
#include <spdlog/spdlog.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <chrono>
#include <numeric>
#include <cstring>

void runClient(const std::string& host, int port, int time) {
    int sock = 0;
    struct sockaddr_in serv_addr;

    if ((sock = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
        spdlog::error("Socket creation error");
        exit(EXIT_FAILURE);
    }

    serv_addr.sin_family = AF_INET;
    serv_addr.sin_port = htons(port);

    if (inet_pton(AF_INET, host.c_str(), &serv_addr.sin_addr) <= 0) {
        spdlog::error("Invalid address/ Address not supported");
        exit(EXIT_FAILURE);
    }

    if (connect(sock, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) < 0) {
        spdlog::error("Connection Failed");
        exit(EXIT_FAILURE);
    }

    // RTT estimation phase
    std::vector<long long> rtts;
    for (int i = 0; i < 8; ++i) {
        auto start = std::chrono::high_resolution_clock::now();
        send(sock, "M", 1, 0); // Send 'M' to server
        char buffer[1];
        recv(sock, buffer, 1, 0); // Receive 'A' from server
        auto end = std::chrono::high_resolution_clock::now();
        long long rtt = std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count();
        rtts.push_back(rtt);
    }
    long long sum = 0;
    for (int i = 4; i < 8; ++i) {
        sum += rtts[i];
    }
    long long rtt = sum / 4;

    char buffer[81920] = {0}; // 80KB buffer
    long total_bytes_sent = 0;
    auto start_time = std::chrono::high_resolution_clock::now();

    auto end_time = start_time + std::chrono::seconds(time);

    while (std::chrono::high_resolution_clock::now() < end_time) {
        int bytes_sent = send(sock, buffer, sizeof(buffer), 0);
        if (bytes_sent < 0) {
            spdlog::error("Send failed");
            break;
        }
        total_bytes_sent += bytes_sent;
    }

    auto final_time = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::microseconds>(final_time - start_time).count();
    spdlog::info("Duration {} seconds", duration / 1e6);

    double rate = (total_bytes_sent * 8.0) / (duration / 1e6) / 1e6; // Mbps

    spdlog::info("Sent={} KB, Rate={:.3f} Mbps, RTT={} ms", total_bytes_sent / 1024, rate, rtt);

    close(sock);
}