#include <iostream>
#include <fstream>
#include <sstream>
#include <string>
#include <cstring>
#include <unordered_map>
#include <thread>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#define PORT 8080
#define BUFFER_SIZE 8192

// ─────────────────────────────────────────────
//  Helpers
// ─────────────────────────────────────────────

std::string read_file(const std::string& path) {
    std::ifstream file(path, std::ios::binary);
    if (!file.is_open()) return "";
    std::ostringstream ss;
    ss << file.rdbuf();
    return ss.str();
}

std::string content_type(const std::string& path) {
    static const std::unordered_map<std::string, std::string> types = {
        {".html", "text/html"},
        {".css",  "text/css"},
        {".js",   "application/javascript"},
        {".json", "application/json"},
        {".png",  "image/png"},
        {".jpg",  "image/jpeg"},
        {".jpeg", "image/jpeg"},
        {".svg",  "image/svg+xml"},
        {".ico",  "image/x-icon"},
        {".txt",  "text/plain"},
    };
    auto dot = path.rfind('.');
    if (dot != std::string::npos) {
        auto it = types.find(path.substr(dot));
        if (it != types.end()) return it->second;
    }
    return "application/octet-stream";
}

struct Request {
    std::string method;
    std::string path;
    std::string body;
};

Request parse_request(const std::string& raw) {
    Request req;
    std::istringstream stream(raw);
    std::string version;
    stream >> req.method >> req.path >> version;
    auto pos = raw.find("\r\n\r\n");
    if (pos != std::string::npos)
        req.body = raw.substr(pos + 4);
    return req;
}

std::string make_response(int status, const std::string& status_text,
                          const std::string& mime, const std::string& body) {
    return "HTTP/1.1 " + std::to_string(status) + " " + status_text + "\r\n"
           "Content-Type: " + mime + "\r\n"
           "Content-Length: " + std::to_string(body.size()) + "\r\n"
           "Access-Control-Allow-Origin: *\r\n"
           "Connection: close\r\n\r\n" + body;
}

// ─────────────────────────────────────────────
//  REST API  (/api/*)
// ─────────────────────────────────────────────

std::string handle_api(const Request& req) {
    // GET /api/status
    if (req.method == "GET" && req.path == "/api/status") {
        return make_response(200, "OK", "application/json",
            R"({"status":"ok","server":"C++ HTTP Server","port":)" +
            std::to_string(PORT) + "}");
    }

    // POST /api/echo
    if (req.method == "POST" && req.path == "/api/echo") {
        std::string escaped;
        for (char c : req.body) {
            if (c == '\\') escaped += "\\\\";
            else if (c == '"') escaped += "\\\"";
            else escaped += c;
        }
        return make_response(200, "OK", "application/json",
            R"({"echo":")" + escaped + R"("})");
    }

    return make_response(404, "Not Found", "application/json",
        R"({"error":"API route not found"})");
}

// ─────────────────────────────────────────────
//  Static file serving  (public/*)
// ─────────────────────────────────────────────

std::string handle_static(const Request& req) {
    std::string url = req.path;
    // Strip query string
    auto q = url.find('?');
    if (q != std::string::npos) url = url.substr(0, q);
    // Block directory traversal
    if (url.find("..") != std::string::npos)
        return make_response(403, "Forbidden", "text/plain", "Forbidden");
    // Default to index.html
    if (url == "/") url = "/index.html";

    std::string filepath = "public" + url;
    std::string body = read_file(filepath);

    if (body.empty())
        return make_response(404, "Not Found", "text/html",
            "<h1>404 – Not Found</h1><p>" + filepath + "</p>");

    return make_response(200, "OK", content_type(filepath), body);
}

// ─────────────────────────────────────────────
//  Per-connection handler (runs in its own thread)
// ─────────────────────────────────────────────

void handle_connection(int socket_fd) {
    char buffer[BUFFER_SIZE] = {};
    ssize_t bytes = read(socket_fd, buffer, BUFFER_SIZE - 1);
    if (bytes > 0) {
        buffer[bytes] = '\0';
        Request req = parse_request(buffer);
        std::cout << "[" << req.method << "] " << req.path << "\n";

        std::string response;
        if (req.path.rfind("/api/", 0) == 0)
            response = handle_api(req);
        else
            response = handle_static(req);

        send(socket_fd, response.c_str(), response.size(), 0);
    }
    close(socket_fd);
}

// ─────────────────────────────────────────────
//  main
// ─────────────────────────────────────────────

int main() {
    int server_fd;
    struct sockaddr_in address;
    int opt = 1;
    socklen_t addrlen = sizeof(address);

    if ((server_fd = socket(AF_INET, SOCK_STREAM, 0)) == 0) {
        perror("socket"); exit(EXIT_FAILURE);
    }
    setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));
    setsockopt(server_fd, SOL_SOCKET, SO_REUSEPORT, &opt, sizeof(opt));

    address.sin_family      = AF_INET;
    address.sin_addr.s_addr = INADDR_ANY;
    address.sin_port        = htons(PORT);

    if (bind(server_fd, (struct sockaddr*)&address, sizeof(address)) < 0) {
        perror("bind"); exit(EXIT_FAILURE);
    }
    if (listen(server_fd, 64) < 0) {
        perror("listen"); exit(EXIT_FAILURE);
    }

    std::cout << "Server running on http://localhost:" << PORT << "\n";

    while (true) {
        int new_socket = accept(server_fd, (struct sockaddr*)&address, &addrlen);
        if (new_socket < 0) { perror("accept"); continue; }
        // Each connection gets its own thread — no client blocks another
        std::thread(handle_connection, new_socket).detach();
    }

    close(server_fd);
    return 0;
}
