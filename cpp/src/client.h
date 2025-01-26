#ifndef CLIENT_H
#define CLIENT_H

#include <string>

/**
 * Runs the iPerfer client.
 * 
 * @param host The hostname or IP address of the server.
 * @param port The port number on which the server is listening.
 * @param time The duration in seconds for which the client should send data.
 */
void runClient(const std::string& host, int port, int time);

#endif // CLIENT_H