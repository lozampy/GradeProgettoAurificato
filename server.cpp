
#include <iostream>
#include <string>
#include <cstring>
#include <fstream>
#include <sstream>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#define PORT 8080
#define BUFFER_SIZE 4096

using namespace std;

string read_file(const string& path) {
    ifstream file(path);
    if (!file.is_open()) return "";
    ostringstream ss;
    ss << file.rdbuf();
    return ss.str();
}

string get_http_response(const string& request) {
    string html = read_file("public/index.html");

    if (html.empty()) {
        string body = "<h1>404 Not Found</h1>";
        return "HTTP/1.1 404 Not Found\r\nContent-Length: "
               + to_string(body.size()) + "\r\n\r\n" + body;
    }

    return "HTTP/1.1 200 OK\r\n"
           "Content-Type: text/html\r\n"
           "Content-Length: " + to_string(html.size()) + "\r\n"
           "Connection: close\r\n\r\n" + html;
}

int main() {
    int server_fd, new_socket;
    struct sockaddr_in address;
    int opt = 1;
    int addrlen = sizeof(address);
    char buffer[BUFFER_SIZE] = {0};

    // Create socket file descriptor
    if ((server_fd = socket(AF_INET, SOCK_STREAM, 0)) == 0) {
        perror("socket failed");
        exit(EXIT_FAILURE);
    }

    // Forcefully attaching socket to the port
    if (setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR | SO_REUSEPORT, &opt, sizeof(opt))) {
        perror("setsockopt");
        exit(EXIT_FAILURE);
    }

    address.sin_family = AF_INET;
    address.sin_addr.s_addr = INADDR_ANY;
    address.sin_port = htons(PORT);

    // Bind socket to the port
    if (bind(server_fd, (struct sockaddr *)&address, sizeof(address)) < 0) {
        perror("bind failed");
        exit(EXIT_FAILURE);
    }

    // Start listening
    if (listen(server_fd, 3) < 0) {
        perror("listen");
        exit(EXIT_FAILURE);
    }

    std::cout << "C++ HTTP Server running on http://localhost:" << PORT << std::endl;
    std::cout << "Press Ctrl+C to stop the server." << std::endl;

    while (true) {
        // Accept incoming connection
        if ((new_socket = accept(server_fd, (struct sockaddr *)&address, (socklen_t*)&addrlen)) < 0) {
            perror("accept");
            exit(EXIT_FAILURE);
        }

        // Read HTTP request
        read(new_socket, buffer, BUFFER_SIZE);
        std::cout << "Received request:\n" << buffer << std::endl;

        // Send HTTP response
        std::string response = get_http_response(buffer);
        send(new_socket, response.c_str(), response.length(), 0);

        // Close connection
        close(new_socket);
    }

    return 0;
}
