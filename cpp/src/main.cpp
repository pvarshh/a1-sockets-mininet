#include <iostream>
#include <cxxopts.hpp>
#include "server.h"
#include "client.h"

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