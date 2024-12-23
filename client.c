// client.c
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <pthread.h>
#include <arpa/inet.h>
#include <dirent.h>
#include <sys/socket.h>
#include <netinet/in.h>

#define SERVER_PORT 12345
#define BUFFER_SIZE 4096
#define MAX_PATH_LENGTH 1024
#define MAX_FILENAME_LENGTH 256

char response_buffer[BUFFER_SIZE];
pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;
pthread_cond_t cond = PTHREAD_COND_INITIALIZER;
int response_ready = 0;
int running = 1;
char sharing_folder[MAX_PATH_LENGTH]; // Global variable to hold sharing folder path

void register_with_server(int sock, struct sockaddr_in server_addr, const char* username, int tcp_port);
void announce_resource(int sock, struct sockaddr_in server_addr, const char* resource_name, const char* username);
void announce_resources(int sock, struct sockaddr_in server_addr, const char* username, const char* sharing_folder);
void query_resources(int sock, struct sockaddr_in server_addr);
void query_users(int sock, struct sockaddr_in server_addr);
void respond_to_hello(int sock, struct sockaddr_in server_addr);
void* listener_thread(void* arg);
void* tcp_server_thread(void* arg);
void display_menu(int sock, struct sockaddr_in server_addr, const char* username);
void* handle_tcp_client(void* arg);
void download_resource(int sock, struct sockaddr_in server_addr);

void register_with_server(int sock, struct sockaddr_in server_addr, const char* username, int tcp_port) {
    char message[BUFFER_SIZE];
    snprintf(message, BUFFER_SIZE, "register %s %d", username, tcp_port);
    sendto(sock, message, strlen(message), 0, (struct sockaddr*)&server_addr, sizeof(server_addr));

    // Wait for acknowledgment
    pthread_mutex_lock(&mutex);
    while (!response_ready) {
        pthread_cond_wait(&cond, &mutex);
    }
    response_ready = 0;
    pthread_mutex_unlock(&mutex);

    if (strcmp(response_buffer, "Registration successful") == 0) {
        printf("Registered with server as %s.\n", username);
    } else {
        printf("Registration failed.\n");
        running = 0;
    }
}

void announce_resource(int sock, struct sockaddr_in server_addr, const char* resource_name, const char* username) {
    char message[BUFFER_SIZE];
    snprintf(message, BUFFER_SIZE, "announce %s %s", resource_name, username);
    sendto(sock, message, strlen(message), 0, (struct sockaddr*)&server_addr, sizeof(server_addr));

    // Wait for acknowledgment
    pthread_mutex_lock(&mutex);
    while (!response_ready) {
        pthread_cond_wait(&cond, &mutex);
    }
    response_ready = 0;
    pthread_mutex_unlock(&mutex);

    if (strcmp(response_buffer, "Resource announced successfully") == 0) {
        printf("Announced resource: %s\n", resource_name);
    } else {
        printf("Failed to announce resource.\n");
    }
}

void announce_resources(int sock, struct sockaddr_in server_addr, const char* username, const char* sharing_folder) {
    DIR* dir = opendir(sharing_folder);
    if (dir == NULL) {
        perror("Failed to open sharing folder");
        return;
    }
    struct dirent* entry;
    while ((entry = readdir(dir)) != NULL) {
        // Skip . and ..
        if (strcmp(entry->d_name, ".") != 0 && strcmp(entry->d_name, "..") != 0) {
            // Announce resource
            announce_resource(sock, server_addr, entry->d_name, username);
        }
    }
    closedir(dir);
}

void query_resources(int sock, struct sockaddr_in server_addr) {
    char message[BUFFER_SIZE] = "query resources";
    sendto(sock, message, strlen(message), 0, (struct sockaddr*)&server_addr, sizeof(server_addr));

    // Wait for response
    pthread_mutex_lock(&mutex);
    while (!response_ready) {
        pthread_cond_wait(&cond, &mutex);
    }
    response_ready = 0;
    pthread_mutex_unlock(&mutex);

    printf("%s\n", response_buffer);
}

void query_users(int sock, struct sockaddr_in server_addr) {
    char message[BUFFER_SIZE] = "query users";
    sendto(sock, message, strlen(message), 0, (struct sockaddr*)&server_addr, sizeof(server_addr));

    // Wait for response
    pthread_mutex_lock(&mutex);
    while (!response_ready) {
        pthread_cond_wait(&cond, &mutex);
    }
    response_ready = 0;
    pthread_mutex_unlock(&mutex);

    printf("%s\n", response_buffer);
}

void respond_to_hello(int sock, struct sockaddr_in server_addr) {
    char message[] = "hello response";
    sendto(sock, message, strlen(message), 0, (struct sockaddr*)&server_addr, sizeof(server_addr));
}

void* listener_thread(void* arg) {
    int sock = *(int*)arg;
    char message[BUFFER_SIZE];
    struct sockaddr_in from_addr;
    socklen_t from_len = sizeof(from_addr);
    while (running) {
        int bytes_received = recvfrom(sock, message, BUFFER_SIZE, 0, (struct sockaddr*)&from_addr, &from_len);
        if (bytes_received > 0) {
            message[bytes_received] = '\0';
            if (strcmp(message, "hello") == 0) {
                respond_to_hello(sock, from_addr);
            } else {
                pthread_mutex_lock(&mutex);
                strcpy(response_buffer, message);
                response_ready = 1;
                pthread_cond_signal(&cond);
                pthread_mutex_unlock(&mutex);
            }
        }
    }
    return NULL;
}

void* handle_tcp_client(void* arg) {
    int client_sock = *(int*)arg;
    free(arg);
    char buffer[BUFFER_SIZE];
    int bytes_received = recv(client_sock, buffer, BUFFER_SIZE, 0);
    if (bytes_received > 0) {
        buffer[bytes_received] = '\0';
        // Assume the request is "get filename"
        if (strncmp(buffer, "get ", 4) == 0) {
            char filename[MAX_FILENAME_LENGTH];
            sscanf(buffer, "get %255s", filename);
            // Send the file
            char filepath[MAX_PATH_LENGTH + MAX_FILENAME_LENGTH];
            snprintf(filepath, sizeof(filepath), "%s/%s", sharing_folder, filename);
            FILE* fp = fopen(filepath, "rb");
            if (fp != NULL) {
                // Send the file contents
                char file_buffer[BUFFER_SIZE];
                int bytes_read;
                while ((bytes_read = fread(file_buffer, 1, BUFFER_SIZE, fp)) > 0) {
                    send(client_sock, file_buffer, bytes_read, 0);
                }
                fclose(fp);
            } else {
                // File not found
                char error_message[] = "Error: File not found.\n";
                send(client_sock, error_message, strlen(error_message), 0);
            }
        }
    }
    close(client_sock);
    return NULL;
}

void* tcp_server_thread(void* arg) {
    int server_sock = *(int*)arg;
    while (running) {
        struct sockaddr_in client_addr;
        socklen_t client_len = sizeof(client_addr);
        int client_sock = accept(server_sock, (struct sockaddr*)&client_addr, &client_len);
        if (client_sock >= 0) {
            // Handle the connection in a separate thread
            pthread_t client_thread;
            int* client_sock_ptr = malloc(sizeof(int));
            *client_sock_ptr = client_sock;
            pthread_create(&client_thread, NULL, handle_tcp_client, client_sock_ptr);
            pthread_detach(client_thread);
        }
    }
    return NULL;
}

void download_resource(int sock, struct sockaddr_in server_addr) {
    // Query resources first
    query_resources(sock, server_addr);

    // Ask the user to enter the resource name
    char resource_name[MAX_FILENAME_LENGTH];
    printf("Enter the name of the resource to download: ");
    fgets(resource_name, sizeof(resource_name), stdin);
    resource_name[strcspn(resource_name, "\n")] = '\0';

    // Request owner info from the server
    char message[BUFFER_SIZE];
    snprintf(message, BUFFER_SIZE, "get resource_info %s", resource_name);
    sendto(sock, message, strlen(message), 0, (struct sockaddr*)&server_addr, sizeof(server_addr));

    // Wait for response
    pthread_mutex_lock(&mutex);
    while (!response_ready) {
        pthread_cond_wait(&cond, &mutex);
    }
    response_ready = 0;
    char resource_info[BUFFER_SIZE];
    strcpy(resource_info, response_buffer);
    pthread_mutex_unlock(&mutex);

    if (strncmp(resource_info, "Error", 5) == 0) {
        printf("%s\n", resource_info);
        return;
    }

    // Parse the list of owners
    char* lines[10];
    int owner_count = 0;
    char* line = strtok(resource_info, "\n");
    while (line != NULL && owner_count < 10) {
        lines[owner_count++] = line;
        line = strtok(NULL, "\n");
    }

    if (owner_count == 0) {
        printf("No active owners found for resource '%s'.\n", resource_name);
        return;
    }

    // Display the list of owners
    printf("Available owners for resource '%s':\n", resource_name);
    for (int i = 0; i < owner_count; i++) {
        printf("%d. %s\n", i + 1, lines[i]);
    }

    // Ask the user to select an owner
    int choice;
    printf("Select an owner (1-%d): ", owner_count);
    scanf("%d", &choice);
    getchar(); // consume newline

    if (choice < 1 || choice > owner_count) {
        printf("Invalid choice.\n");
        return;
    }

    // Extract owner info
    char owner[50], owner_ip[INET_ADDRSTRLEN];
    int owner_tcp_port;
    sscanf(lines[choice - 1], "%s %s %d", owner, owner_ip, &owner_tcp_port);

    printf("Downloading resource '%s' from %s (%s:%d)\n", resource_name, owner, owner_ip, owner_tcp_port);

    // Connect to the owner's TCP server and request the file
    int client_sock = socket(AF_INET, SOCK_STREAM, 0);
    if (client_sock < 0) {
        perror("Socket creation failed");
        return;
    }
    struct sockaddr_in owner_addr;
    owner_addr.sin_family = AF_INET;
    owner_addr.sin_port = htons(owner_tcp_port);
    inet_pton(AF_INET, owner_ip, &owner_addr.sin_addr);

    if (connect(client_sock, (struct sockaddr*)&owner_addr, sizeof(owner_addr)) < 0) {
        perror("Connection to owner failed");
        close(client_sock);
        return;
    }

    // Send "get filename" request
    snprintf(message, BUFFER_SIZE, "get %s", resource_name);
    send(client_sock, message, strlen(message), 0);

    // Receive file contents and save to local file
    char filepath[MAX_PATH_LENGTH + MAX_FILENAME_LENGTH];
    snprintf(filepath, sizeof(filepath), "downloaded_%s_%s", owner, resource_name);
    FILE* fp = fopen(filepath, "wb");
    if (fp == NULL) {
        perror("Failed to open file for writing");
        close(client_sock);
        return;
    }
    char file_buffer[BUFFER_SIZE];
    int bytes_received;
    while ((bytes_received = recv(client_sock, file_buffer, BUFFER_SIZE, 0)) > 0) {
        fwrite(file_buffer, 1, bytes_received, fp);
    }
    fclose(fp);
    close(client_sock);

    printf("Resource '%s' downloaded and saved as '%s'\n", resource_name, filepath);
}

void display_menu(int sock, struct sockaddr_in server_addr, const char* username) {
    while (running) {
        int choice;
        char resource_name[MAX_FILENAME_LENGTH];

        printf("\n--- MENU ---\n");
        printf("1. Announce a resource\n");
        printf("2. Query available resources\n");
        printf("3. Query active users\n");
        printf("4. Download a resource\n");
        printf("5. Exit\n");
        printf("Select an option: ");
        scanf("%d", &choice);
        getchar();

        switch (choice) {
            case 1:
                printf("Enter resource name: ");
                fgets(resource_name, sizeof(resource_name), stdin);
                resource_name[strcspn(resource_name, "\n")] = 0;
                announce_resource(sock, server_addr, resource_name, username);
                break;
            case 2:
                query_resources(sock, server_addr);
                break;
            case 3:
                query_users(sock, server_addr);
                break;
            case 4:
                download_resource(sock, server_addr);
                break;
            case 5:
                running = 0;
                break;
            default:
                printf("Invalid choice. Please try again.\n");
        }
    }
}

int main(int argc, char* argv[]) {
    if (argc < 3) {
        printf("Usage: %s <server_ip> <username>\n", argv[0]);
        return 1;
    }

    const char* server_ip = argv[1];
    const char* username = argv[2];

    int sock;
    struct sockaddr_in server_addr;

    sock = socket(AF_INET, SOCK_DGRAM, 0);
    if (sock < 0) {
        perror("Socket creation failed");
        exit(EXIT_FAILURE);
    }
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(SERVER_PORT);
    inet_pton(AF_INET, server_ip, &server_addr.sin_addr);

    // TCP server setup
    int tcp_server_sock;
    struct sockaddr_in tcp_server_addr;

    tcp_server_sock = socket(AF_INET, SOCK_STREAM, 0);
    if (tcp_server_sock < 0) {
        perror("TCP socket creation failed");
        exit(EXIT_FAILURE);
    }

    tcp_server_addr.sin_family = AF_INET;
    tcp_server_addr.sin_addr.s_addr = INADDR_ANY;
    tcp_server_addr.sin_port = 0; // Use any available port

    if (bind(tcp_server_sock, (struct sockaddr*)&tcp_server_addr, sizeof(tcp_server_addr)) < 0) {
        perror("TCP bind failed");
        close(tcp_server_sock);
        exit(EXIT_FAILURE);
    }

    if (listen(tcp_server_sock, 5) < 0) {
        perror("TCP listen failed");
        close(tcp_server_sock);
        exit(EXIT_FAILURE);
    }

    // Get the assigned port
    socklen_t addrlen = sizeof(tcp_server_addr);
    getsockname(tcp_server_sock, (struct sockaddr*)&tcp_server_addr, &addrlen);
    int tcp_port = ntohs(tcp_server_addr.sin_port);

    // Prompt for sharing folder
    printf("Enter the path to the sharing folder: ");
    fgets(sharing_folder, sizeof(sharing_folder), stdin);
    sharing_folder[strcspn(sharing_folder, "\n")] = '\0';

    // Create listener thread
    pthread_t listener;
    pthread_create(&listener, NULL, listener_thread, &sock);

    // Start TCP server thread
    pthread_t tcp_server_thread_id;
    pthread_create(&tcp_server_thread_id, NULL, tcp_server_thread, &tcp_server_sock);

    register_with_server(sock, server_addr, username, tcp_port);

    if (running) {
        // Announce resources upon registration
        announce_resources(sock, server_addr, username, sharing_folder);
        display_menu(sock, server_addr, username);
    }

    // Clean up
    running = 0;
    pthread_cancel(listener);
    pthread_cancel(tcp_server_thread_id);
    pthread_join(listener, NULL);
    pthread_join(tcp_server_thread_id, NULL);
    close(sock);
    close(tcp_server_sock);
    return 0;
}
