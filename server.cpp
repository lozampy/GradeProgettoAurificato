#include <iostream>
#include <fstream>
#include <sstream>
#include <string>
#include <map>

// Winsock headers — must come before any windows.h
#include <winsock2.h>
#include <ws2tcpip.h>

// Link against Ws2_32.lib automatically
#pragma comment(lib, "Ws2_32.lib")

using namespace std;

// ── CONFIG ───────────────────────────────────────────────────────────────────
const int    PORT = 8080;
const int    BACKLOG = 10;
const string ROOT_DIR = "./public";
const string DEFAULT_DOC = "index.html";

// ── MIME TYPES ────────────────────────────────────────────────────────────────
map<string, string> MIME_TYPES = {
    {".html", "text/html; charset=utf-8"},
    {".htm",  "text/html; charset=utf-8"},
    {".css",  "text/css"},
    {".js",   "application/javascript"},
    {".json", "application/json"},
    {".png",  "image/png"},
    {".jpg",  "image/jpeg"},
    {".jpeg", "image/jpeg"},
    {".ico",  "image/x-icon"},
    {".svg",  "image/svg+xml"},
    {".txt",  "text/plain"},
};

// ── HELPERS ───────────────────────────────────────────────────────────────────
string getMime(const string& path) {
    size_t dot = path.rfind('.');
    if (dot != string::npos) {
        string ext = path.substr(dot);
        auto it = MIME_TYPES.find(ext);
        if (it != MIME_TYPES.end()) return it->second;
    }
    return "application/octet-stream";
}

string readFile(const string& path, bool& ok) {
    ifstream f(path, ios::binary);
    if (!f) { ok = false; return ""; }
    ok = true;
    return string(istreambuf_iterator<char>(f), {});
}

string buildResponse(int code, const string& status,
    const string& contentType,
    const string& body) {
    ostringstream oss;
    oss << "HTTP/1.1 " << code << " " << status << "\r\n"
        << "Content-Type: " << contentType << "\r\n"
        << "Content-Length: " << body.size() << "\r\n"
        << "Connection: close\r\n"
        << "\r\n"
        << body;
    return oss.str();
}

string parsePath(const string& request) {
    istringstream iss(request);
    string method, path, version;
    iss >> method >> path >> version;

    size_t q = path.find('?');
    if (q != string::npos) path = path.substr(0, q);

    if (path == "/") path = "/" + DEFAULT_DOC;
    return path;
}

// ── HANDLE ONE CLIENT ─────────────────────────────────────────────────────────
void handleClient(SOCKET clientSock) {
    char buf[8192] = {};
    int received = recv(clientSock, buf, sizeof(buf) - 1, 0);
    if (received <= 0) { closesocket(clientSock); return; }

    string request(buf, received);
    string path = parsePath(request);

    // Block directory traversal
    if (path.find("..") != string::npos) {
        string resp = buildResponse(403, "Forbidden", "text/plain", "403 Forbidden");
        send(clientSock, resp.c_str(), (int)resp.size(), 0);
        closesocket(clientSock);
        return;
    }

    // Convert forward slashes to backslashes for Windows file paths
    string filePath = ROOT_DIR + path;
    for (char& c : filePath)
        if (c == '/') c = '\\';

    bool ok;
    string body = readFile(filePath, ok);

    string response;
    if (ok) {
        response = buildResponse(200, "OK", getMime(path), body);
        cout << "[200] " << path << "\n";
    }
    else {
        string notFound = "<html><body><h1>404 Not Found</h1><p>" + path + "</p></body></html>";
        response = buildResponse(404, "Not Found", "text/html; charset=utf-8", notFound);
        cout << "[404] " << path << "\n";
    }

    send(clientSock, response.c_str(), (int)response.size(), 0);
    closesocket(clientSock);
}

// ── MAIN ──────────────────────────────────────────────────────────────────────
int main() {
    // 1. Initialise Winsock
    WSADATA wsaData;
    int result = WSAStartup(MAKEWORD(2, 2), &wsaData);
    if (result != 0) {
        cerr << "ERROR: WSAStartup failed (" << result << ")\n";
        return 1;
    }

    // 2. Create socket
    SOCKET serverSock = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (serverSock == INVALID_SOCKET) {
        cerr << "ERROR: socket() failed (" << WSAGetLastError() << ")\n";
        WSACleanup();
        return 1;
    }

    // Allow address reuse
    int opt = 1;
    setsockopt(serverSock, SOL_SOCKET, SO_REUSEADDR,
        reinterpret_cast<const char*>(&opt), sizeof(opt));

    // 3. Bind
    sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_port = htons(PORT);
    addr.sin_addr.s_addr = INADDR_ANY;

    if (bind(serverSock, (sockaddr*)&addr, sizeof(addr)) == SOCKET_ERROR) {
        cerr << "ERROR: bind() failed — is port " << PORT << " already in use? ("
            << WSAGetLastError() << ")\n";
        closesocket(serverSock);
        WSACleanup();
        return 1;
    }

    // 4. Listen
    if (listen(serverSock, BACKLOG) == SOCKET_ERROR) {
        cerr << "ERROR: listen() failed (" << WSAGetLastError() << ")\n";
        closesocket(serverSock);
        WSACleanup();
        return 1;
    }

    cout << "╔══════════════════════════════════════╗\n"
         << "║   C++ Localhost Server (Windows)     ║\n"
         << "╠══════════════════════════════════════╣\n"
         << "║  http://localhost:" << PORT << "     ║\n"
         << "║  Serving files from current dir      ║\n"
         << "║  Press Ctrl+C to stop                ║\n"
         << "╚══════════════════════════════════════╝\n\n";

    // 5. Accept loop
    while (true) {
        sockaddr_in clientAddr{};
        int clientLen = sizeof(clientAddr);

        SOCKET clientSock = accept(serverSock, (sockaddr*)&clientAddr, &clientLen);
        if (clientSock == INVALID_SOCKET) {
            cerr << "WARNING: accept() failed (" << WSAGetLastError() << "), retrying...\n";
            continue;
        }

        char ipStr[INET_ADDRSTRLEN];
        inet_ntop(AF_INET, &clientAddr.sin_addr, ipStr, sizeof(ipStr));
        cout << "Connection from " << ipStr << ":" << ntohs(clientAddr.sin_port) << "\n";

        handleClient(clientSock);
    }

    closesocket(serverSock);
    WSACleanup();
    return 0;
}
