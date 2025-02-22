#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef _WIN32
    #include <winsock2.h>
    #include <ws2tcpip.h>
    #include <windows.h>
    #pragma comment(lib, "ws2_32.lib")
    #define CLOSESOCKET closesocket
    #define SHUT_WR SD_SEND
    #define SLEEP(ms) Sleep(ms)  // Windows takes milliseconds
#else
    #include <arpa/inet.h>
    #include <unistd.h>
    #define CLOSESOCKET close
    #define SLEEP(ms) usleep((ms) * 1000)  // Convert ms to µs
#endif

#define PORT 8080
#define BUFFER_SIZE 4096

void serve_client(int client_socket, const char *filename) {
    char buffer[BUFFER_SIZE];
    FILE *html_file = fopen(filename, "r");

    if (!html_file) {
        const char *error_response =
            "HTTP/1.1 404 Not Found\r\n"
            "Content-Type: text/html\r\n"
            "Content-Length: 47\r\n"
            "Connection: close\r\n"
            "\r\n"
            "<html><body><h1>404 Not Found</h1></body></html>";
        send(client_socket, error_response, strlen(error_response), 0);
        shutdown(client_socket, SHUT_WR);
        SLEEP(1);
        CLOSESOCKET(client_socket);
        return;
    }

    // Read HTML file contents
    fseek(html_file, 0, SEEK_END);
    long file_size = ftell(html_file);
    rewind(html_file);

    char *html_content = malloc(file_size + 1);
    fread(html_content, 1, file_size, html_file);
    html_content[file_size] = '\0';
    fclose(html_file);

    // Send HTTP headers first
    snprintf(buffer, sizeof(buffer),
             "HTTP/1.1 200 OK\r\n"
             "Content-Type: text/html\r\n"
             "Content-Length: %ld\r\n"
             "Connection: close\r\n"
             "\r\n", file_size);

    if (send(client_socket, buffer, strlen(buffer), 0) == -1) {
        perror("Send headers failed");
        free(html_content);
        CLOSESOCKET(client_socket);
        return;
    }

    // Send HTML content
    if (send(client_socket, html_content, file_size, 0) == -1) {
        perror("Send body failed");
    }

    free(html_content);

    // Ensure data is fully sent before closing
    shutdown(client_socket, SD_SEND);
    SLEEP(1);
    CLOSESOCKET(client_socket);
}

int main(int argc, char *argv[]) {
    if (argc < 3 || (strcmp(argv[1], "-s") != 0 && strcmp(argv[1], "-serve") != 0)) {
        printf("Usage: %s -s <file_to_serve>\n", argv[0]);
        return EXIT_FAILURE;
    }

    const char *filename = argv[2];

#ifdef _WIN32
    WSADATA wsa;
    if (WSAStartup(MAKEWORD(2, 2), &wsa) != 0) {
        printf("WSAStartup failed.\n");
        return EXIT_FAILURE;
    }
#endif

    int server_fd, client_socket;
    struct sockaddr_in server_addr, client_addr;
    socklen_t addr_len = sizeof(client_addr);

    // Create socket
    server_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (server_fd == -1) {
        perror("Socket creation failed");
        exit(EXIT_FAILURE);
    }

    // Configure server address
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = INADDR_ANY;
    server_addr.sin_port = htons(PORT);

    // Bind socket to port
    if (bind(server_fd, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0) {
        perror("Bind failed");
        CLOSESOCKET(server_fd);
#ifdef _WIN32
        WSACleanup();
#endif
        exit(EXIT_FAILURE);
    }

    // Start listening for connections
    if (listen(server_fd, 10) < 0) {
        perror("Listen failed");
        CLOSESOCKET(server_fd);
#ifdef _WIN32
        WSACleanup();
#endif
        exit(EXIT_FAILURE);
    }

    printf("Serving %s at http://localhost:%d\n", filename, PORT);

    // Accept and handle client connections
    while ((client_socket = accept(server_fd, (struct sockaddr *)&client_addr, &addr_len)) >= 0) {
        serve_client(client_socket, filename);
    }

    CLOSESOCKET(server_fd);
#ifdef _WIN32
    WSACleanup();
#endif

    return 0;
}
