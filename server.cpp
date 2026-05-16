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
//crea un host sul tuo PC con port PORT, BACKLOG indica il numero di persone che possono essere collegate al sito contemporaneamente, ROOT_DIR indica probabilmente il path di github a cui accedere e DEFAULT_DOC indica il nome del file
const int    PORT = 8080;
const int    BACKLOG = 10;
const string ROOT_DIR = "./public/";
const string DEFAULT_DOC = "index.html";

// ── MIME TYPES ────────────────────────────────────────────────────────────────
// tutti i tipi di file che possono essere caricati sul sito
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
// string& path significa classe stringa simil-python
string getMime(const string& path) {
    size_t dot = path.rfind('.');
    if (dot != string::npos) {                                               //string::npos indica che la posizione non è trovata, quindi se non esiste un punto nel file path in linea 42                                            
        string ext = path.substr(dot);                                       //trova l'estensione del file (.jpg o simili) controllando tutto cio che sta dopo l'ultimo punto del file di path (line 42)
        auto it = MIME_TYPES.find(ext);                                      //la variabile it contiene le informazioni che definiscono un file .ext, cioè è in grado di distinguere un .ext, se non riesce a trovare la definizione di .ext allora prende un valore rilevabile dalla linea successiva
        if (it != MIME_TYPES.end()) return it->second;                       //se it codifica qualcosa di non definito allora prende il valore contenuto in MIME_TYPES.end() e quindi il fatto dice "se la ricerca non è andata male (ergo è andata bene) fai"
    }
    return "application/octet-stream";
}

string readFile(const string& path, bool& ok) {
    ifstream f(path, ios::binary);                                           //leggi tramite ifstream il path come binario e mettilo nella variabile f, l' ios::binary è un tipo di stream buffer (fatto che legge o scrive un file) ed è generalmente diviso in linee quando legge
    if (!f) { ok = false; return ""; }                                       //se il file risulta falso NON siamo ok (ok = false)
    ok = true;
    return string(istreambuf_iterator<char>(f), {});                         //istreambuf_iterator è in grado di leggere un buffer (leggi linea 52) e legge singolarmente tutte le linee, il <char> le trasforma da binario a char e string() le unisce insieme
}

string buildResponse(int code, const string& status,
    const string& contentType,
    const string& body) {
    ostringstream oss;                                                        //l'oggetto oss è in grado di scrivere velocemente in un file partendo da uno stream buffer (linea 52)
    oss << "HTTP/1.1 " << code << " " << status << "\r\n"                     //da quello che ci capisco questo è semplicemente il text da ficcare nella pagina, da rifare quando conosco l'html
        << "Content-Type: " << contentType << "\r\n"
        << "Content-Length: " << body.size() << "\r\n"
        << "Connection: close\r\n"
        << "\r\n"
        << body;
    return oss.str();
}

string parsePath(const string& request) {
    istringstream iss(request);                                               //è un input stream che usa uno string buffer (linea 52) per leggere un file come una string
    string method, path, version;
    iss >> method >> path >> version;                                         //leggi le prime righe di iss e mettile nelle rispettive variabili, non so se iss legga da un json o cosa ma è comunque un file per trasportare da html a cpp

    size_t q = path.find('?');                                                //size_t penso sia un template, cerca il ? più vicino in path e mette in q tutto quello che sta prima del ?, almeno penso
    if (q != string::npos) path = path.substr(0, q);                          //se esiste "?" in q dimentica crea una string (detta substring da substr()) che contenga tutto quello ceh contiene path dall'inizio (0) al "?" (q)

    if (path == "/") path = "/" + DEFAULT_DOC;                                //fai in modo che il path sia leggibile correttamente e aggiungici il file in html che definisce il server
    return path;
}

// ── HANDLE ONE CLIENT ─────────────────────────────────────────────────────────
void handleClient(SOCKET clientSock) {                                        //socket è il fatto che ti connette a internet e al server in generale, questa funzione controlla tutti gli errori possibili (per non fare casino con i file) e connettersi al socket, poi passa il lavoro all'html
    char buf[8192] = {};                                                      //il buf (buffer) funge da lista d'attesa per tutti i dati che devono essere processati, principalmente usato nelle funzioni di winsock
    int received = recv(clientSock, buf, sizeof(buf) - 1, 0);                 //recv() riceve dati dal socket, ha bisogno di un buffer per contenerli e devi specificare la size del buffer, 0 è il numero e codice delle flag, ovvero nessuna
    if (received <= 0) { closesocket(clientSock); return; }                   //se il socket non dà informazioni chiudi il socket e finisci la funzione

    string request(buf, received);                                            //fa una richiesta a internet di connettersi usando il buffer come ram e il socket per indicare chi e a cosa ci si deve connettere
    string path = parsePath(request);                                         //dal buffer e le informazioni ricevute ottiene un path, la funzione 100% made in carletto è scritta in linea 72

    // Block directory traversal
    if (path.find("..") != string::npos) {                                    //se esistono ".." nel path (che se non ricordo male significa torna indietro nei file) dai un messaggio di crash
        string resp = buildResponse(403, "Forbidden", "text/plain", "403 Forbidden"); //crea il messaggio di crash "403 forbidden" significa che il server non ti permette di fare l'azione perchè non è in grado di andare indietro nei file
        send(clientSock, resp.c_str(), (int)resp.size(), 0);                  //manda al server l'errore
        closesocket(clientSock);                                              //chiudi il socket e finisci la funzione
        return;
    }

    // Convert forward slashes to backslashes for Windows file paths
    string filePath = ROOT_DIR + path;                                        //file_path indica il path da seguire per ottenere
    for (char& c : filePath)                                                  //è un fatto detto "for each loop", per ogni "/" (che viene storato in c) in filePath ripeti il loop
        if (c == '/') c = '\\';                                               //se c = "/" allora cambialo in c = "\\", questo serve per standarizzare il path per windows, il path viene modificato in automatico quando ridefinisci c

    bool ok;
    string body = readFile(filePath, ok);                                     //leggi filepath e mettilo in body, se riesci a leggerlo ok = true

    string response;
    if (ok) {                                                                 //se riesci a leggere il file costruisci la risposta
        response = buildResponse(200, "OK", getMime(path), body);             //costruisci la risposta usando buildresponse in linea 59
        cout << "[200] " << path << "\n";                                     //stampa nel terminale, visibile solo al dev e non al sito
    }
    else {                                                                    //se NON riesci a leggere il file manda errore 404
        string notFound = "<html><body><h1>404 Not Found</h1><p>" + path + "</p></body></html>"; //costrisci il messaggio di errore e scrivilo (come comando in html) sul sito
        response = buildResponse(404, "Not Found", "text/html; charset=utf-8", notFound); //costruisci la risposta
        cout << "[404] " << path << "\n";                                     //stampa nel terminale, visibile solo al dev e non al sito
    }

    send(clientSock, response.c_str(), (int)response.size(), 0);              //manda al server la risposta di successo o l'eventuale errore
    closesocket(clientSock);                                                  //chiudi il collegamento con il socket e quindi con il server
}

// ── MAIN ──────────────────────────────────────────────────────────────────────
int main() {
    // 1. Initialise Winsock
    WSADATA wsaData;                                                          //struct che permette di inizializzare winsock2
    int result = WSAStartup(MAKEWORD(2, 2), &wsaData);                        //result dice se lo startup ha funzionato o meno, makeword indica la versione di socket da far partire, e wsdata contiene informazioni necessarie allo startup
    if (result != 0) {                                                        //se lo startup fallisce dimmerlo
        cerr << "ERROR: WSAStartup failed (" << result << ")\n";              //non può comunicarlo al sito perchè non si è connesso ma dice che c'è stato un errore, cerr singnifica cout error
        return 1;
    }

    // 2. Create socket
    SOCKET serverSock = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);            //stiamo usando un server hostato sul pc per far partire il programma dal pc, il server di default non va in internet ma potrebbe. per più informazioni vai a https://www.geeksforgeeks.org/cpp/socket-programming-in-cpp/
    if (serverSock == INVALID_SOCKET) {                                       //se non funziona il socket dai un errore
        cerr << "ERROR: socket() failed (" << WSAGetLastError() << ")\n";     //commento uguale a linea 130
        WSACleanup();                                                         //de inizializza winsock2 e finisci il programma
        return 1;
    }

    // Allow address reuse
    int opt = 1;                                                              
    setsockopt(serverSock, SOL_SOCKET, SO_REUSEADDR,                          //definsci le opzioni del socket, il tipo (serverSock), SOL_SOCKET definisce le opzioni a cui dobbiamo accedere, SO_REUSEADDR Consente a un socket di associare un indirizzo e una porta già in uso,
        reinterpret_cast<const char*>(&opt), sizeof(opt));                    //cambia opt in character (al posto di int 1 diventa char "1"), e serve per storare le opzioni

    // 3. Bind, binda il socket all'address del sito
    sockaddr_in addr{};                                                       // è uno struct che dà il port per il tipo di socket che stiamo usando, ovvero AF_INET
    addr.sin_family = AF_INET;                                                //secondo il sito di microsoft sin_family è una Famiglia di indirizzi per l'indirizzo di trasporto. Questo membro deve essere sempre impostato su AF_INET
    addr.sin_port = htons(PORT);                                              //sin_port indica il port legato ad adress, e tramite htons() lo trasforma in un fatto mandabile per sistema TCP/IP
    addr.sin_addr.s_addr = INADDR_ANY;                                        //il .s_addr indica come accedere alle informazioni di sin_addr, che a sua volta contiene un indirizzo internet, e INADDR_ANY permette di fare il bind, ovvero legare il server ad un address o un port tipo

    if (bind(serverSock, (sockaddr*)&addr, sizeof(addr)) == SOCKET_ERROR) {   //runna il bind(), e se dà errore fai come in linea 129
        cerr << "ERROR: bind() failed — is port " << PORT << " already in use? ("
            << WSAGetLastError() << ")\n";
        closesocket(serverSock);
        WSACleanup();
        return 1;
    }

    // 4. Listen, ergo vedi se c'è qualcuno che tenta di connettersi
    if (listen(serverSock, BACKLOG) == SOCKET_ERROR) {                        //runna il listen() sul socket e vede la quantità di gente che si connette, se dà errore fai come in linea 129
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

    // 5. Accept loop, ergo accetta il fatto che qualcuno si collega e fai partire l'html con tutte le varie funzioni
    while (true) {
        sockaddr_in clientAddr{};                                             //idem che in linea 148 ma con l'addres dello user
        int clientLen = sizeof(clientAddr);

        SOCKET clientSock = accept(serverSock, (sockaddr*)&clientAddr, &clientLen); //accetta la connessione di qualcuno
        if (clientSock == INVALID_SOCKET) {                                   //se non ti puoi connettere al socket dello user dai errore, e riprova
            cerr << "WARNING: accept() failed (" << WSAGetLastError() << "), retrying...\n";
            continue;
        }

        char ipStr[INET_ADDRSTRLEN];                                          //dovrebbe essere un buffer per processare l'indirizzo dello user (info ottenuta dalla definizione della funzione successiva)
        inet_ntop(AF_INET, &clientAddr.sin_addr, ipStr, sizeof(ipStr));       //converte l'addres dello user in un tipo di stringa, più precisamente un tipo di stringa utilizzabile con AF_INET (un tipo di indirizzo), &clientAddr.sin_addr gli dà l'indirizzo dello user, e gli passi il buffer per processare queste informazioni
        cout << "Connection from " << ipStr << ":" << ntohs(clientAddr.sin_port) << "\n"; //ntohs() converte un intero a 16 bit (short) dall'ordine dei byte di rete (Big-Endian) all'ordine dei byte dell'host (Little-Endian o Big-Endian a seconda dell'architettura)

        handleClient(clientSock);                                             //tutte le funzioni di prima solo per questa singola riga, mi viene da piangere
    }

    closesocket(serverSock);                                                  //finalmente abbiamo finito di inizializzare il server, la connessione è definita bene e senza errori e adesso se la smazza il file html caricato sulla ram del pc
    WSACleanup();                                                             //evita di usare memoria inutile e de inizializza winsock2, per fortuna perchè non volevo avere più nulla a che fare con lui
    return 0;
}
