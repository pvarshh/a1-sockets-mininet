#include <iostream>
#include <spdlog/spdlog.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>
#include <chrono>
#include <numeric>
#include <cstring>

void runServer(int port) {
    int server_fd, new_socket;
    struct sockaddr_in address;
    int opt = 1;
    int addrlen = sizeof(address);

    if ((server_fd = socket(AF_INET, SOCK_STREAM, 0)) == 0) {
        spdlog::error("Socket creation failed");
        exit(EXIT_FAILURE);
    }

    if (setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR | SO_REUSEPORT, &opt, sizeof(opt))) {
        spdlog::error("Setsockopt failed");
        exit(EXIT_FAILURE);
    }

    address.sin_family = AF_INET;
    address.sin_addr.s_addr = INADDR_ANY;
    address.sin_port = htons(port);

    if (bind(server_fd, (struct sockaddr *)&address, sizeof(address)) < 0) {
        spdlog::error("Bind failed");
        exit(EXIT_FAILURE);
    }

    if (listen(server_fd, 3) < 0) {
        spdlog::error("Listen failed");
        exit(EXIT_FAILURE);
    }

    spdlog::info("iPerfer server started");

    if ((new_socket = accept(server_fd, (struct sockaddr *)&address, (socklen_t*)&addrlen)) < 0) {
        spdlog::error("Accept failed");
        exit(EXIT_FAILURE);
    }

    spdlog::info("Client connected");

    // RTT estimation phase
    std::vector<long long> rtts;
    for (int i = 0; i < 8; ++i) {
        char buffer[1];
        auto start = std::chrono::high_resolution_clock::now();
        recv(new_socket, buffer, 1, 0); // Receive message
        send(new_socket, "A", 1, 0); // Send ACK
        auto end = std::chrono::high_resolution_clock::now();
        auto rtt = std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count();
        rtts.push_back(rtt);
    }
    long long sum = 0;
    for (int i = 4; i < 8; ++i) {
        sum += rtts[i];
    }
    long long rtt = sum / 4;

    char buffer[1024] = {0};
    int valread;
    long total_bytes_received = 0;
    auto start_time = std::chrono::high_resolution_clock::now();

    while ((valread = read(new_socket, buffer, sizeof(buffer))) > 0) {
        total_bytes_received += valread;
    }

    auto end_time = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::microseconds>(end_time - start_time).count();
    spdlog::info("Duration {} seconds", duration / 1e6);
    
    double rate = (total_bytes_received * 8.0) / (duration / 1e6) / 1e6; // Mbps

    spdlog::info("Received={} KB, Rate={:.3f} Mbps, RTT={} ms", total_bytes_received / 1024, rate, rtt);

    close(new_socket);
    close(server_fd);
}