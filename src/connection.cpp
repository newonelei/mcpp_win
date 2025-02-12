#include "../include/mcpp/connection.h"

#include <winsock2.h>
#include <ws2tcpip.h>
//#include <netdb.h>
//#include <sys/socket.h>
//#include <unistd.h>

#include <sstream>
#include <stdexcept>

namespace mcpp {
SocketConnection::SocketConnection(const std::string& address_str,
                                   uint16_t port) {
	WSADATA wsaData;
	int result;
	// Initialize Winsock
	result = WSAStartup(MAKEWORD(2, 2), &wsaData);
	if (result != 0) {
		std::cerr << "WSAStartup failed: " << result << std::endl;
	}

    std::string ipAddress = resolveHostname(address_str);

    // Using std libs only to avoid dependency on socket lib
    socketHandle = socket(AF_INET, SOCK_STREAM, 0);
    if (socketHandle == -1) {
		WSACleanup();
        throw std::runtime_error("Failed to create socket.");
    }

    sockaddr_in serverAddress{};
    serverAddress.sin_family = AF_INET;
    serverAddress.sin_port = htons(port);

	result = inet_pton(AF_INET, ipAddress.c_str(), &(serverAddress.sin_addr));
	if(result < 0){
		WSACleanup();
        throw std::runtime_error("Invalid address.");
    }

    if (connect(socketHandle, (struct sockaddr*)&serverAddress,
                sizeof(serverAddress)) < 0) {
		Disconnect();
        throw std::runtime_error(
            "Failed to connect to the server. Check if the server is running.");
    }
}

SocketConnection::~SocketConnection()
{
	Disconnect();
}

void SocketConnection::Disconnect() const
{
	closesocket(socketHandle);
	WSACleanup();
}

std::string SocketConnection::resolveHostname(const std::string& hostname) {
    struct addrinfo hints {};
    struct addrinfo* result;

    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_STREAM;

    if (getaddrinfo(hostname.c_str(), nullptr, &hints, &result) != 0) {
        throw std::runtime_error("Failed to resolve hostname.");
    }

    auto* address = reinterpret_cast<struct sockaddr_in*>(result->ai_addr);
    char ipAddress[INET_ADDRSTRLEN];
    inet_ntop(AF_INET, &(address->sin_addr), ipAddress, INET_ADDRSTRLEN);

    std::string ipString(ipAddress);
    freeaddrinfo(result);

    return ipString;
}

void SocketConnection::send(const std::string& dataString) {
    lastSent = dataString;
    int result =
		::send(socketHandle, dataString.c_str(), dataString.length(), 0);
    if (result < 0) {
		Disconnect();
        throw std::runtime_error("Failed to send data.");
    }
}

std::string SocketConnection::recv() const {
    std::stringstream responseStream;
    char buffer[1024];

    int bytesRead;
    do {
        bytesRead = ::recv(socketHandle, buffer, sizeof(buffer), 0);
        if (bytesRead < 0) {
			Disconnect();
            throw std::runtime_error("Failed to receive data.");
        }

        responseStream.write(buffer, bytesRead);
    } while (buffer[bytesRead - 1] != '\n');

    std::string response = responseStream.str();

    // Remove trailing \n
    if (!response.empty() && response[response.length() - 1] == '\n') {
        response.pop_back();
    }

    if (response == FAIL_RESPONSE) {
        std::string errorMsg = "Server failed to execute command: ";
        errorMsg += lastSent;
        throw std::runtime_error(errorMsg);
    }
    return response;
}
} // namespace mcpp