// server.c
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <pthread.h>
#include <arpa/inet.h>
#include <sys/time.h>
#include <time.h>

#define PORT 12345
#define BUFFER_SIZE 4096
#define MAX_CLIENTS 100
#define HELLO_INTERVAL 5  // Interval for hello messages in seconds
#define HELLO_TIMEOUT 15  // Timeout for client response in seconds

// Structure for user directory entry
typedef struct {
    char username[50];
    struct sockaddr_in addr;
    int status;  // 1 = active, 0 = inactive
    time_t last_response;
    int tcp_port; // New field for client's TCP server port
} UserDirectoryEntry;

// Structure for resource directory entry
typedef struct {
    char resource_name[100];
    char owner[50];
} ResourceDirectoryEntry;

UserDirectoryEntry user_directory[MAX_CLIENTS];
ResourceDirectoryEntry resource_directory[MAX_CLIENTS * 10]; // Increased size to hold more resources
int user_count = 0, resource_count = 0;

// Mutexes for thread synchronization
pthread_mutex_t user_mutex = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t resource_mutex = PTHREAD_MUTEX_INITIALIZER;

// Function to add user to directory
void add_user(const char* username, struct sockaddr_in addr, int tcp_port) {
    pthread_mutex_lock(&user_mutex);
    strcpy(user_directory[user_count].username, username);
    user_directory[user_count].addr = addr;
    user_directory[user_count].status = 1;  // active
    user_directory[user_count].last_response = time(NULL);  // initial time
    user_directory[user_count].tcp_port = tcp_port; // store tcp port
    user_count++;
    pthread_mutex_unlock(&user_mutex);
}

// Function to add resource to directory
void add_resource(const char* resource_name, const char* owner) {
    pthread_mutex_lock(&resource_mutex);
    strcpy(resource_directory[resource_count].resource_name, resource_name);
    strcpy(resource_directory[resource_count].owner, owner);
    resource_count++;
    pthread_mutex_unlock(&resource_mutex);
}

// Function to send hello messages to clients
void send_hello_messages(int sockfd) {
    char hello_message[] = "hello";
    pthread_mutex_lock(&user_mutex);
    for (int i = 0; i < user_count; i++) {
        if (user_directory[i].status == 1) {
            sendto(sockfd, hello_message, strlen(hello_message), 0,
                   (struct sockaddr*)&user_directory[i].addr, sizeof(user_directory[i].addr));
        }
    }
    pthread_mutex_unlock(&user_mutex);
}

// Function to check client statuses based on hello response timeout
void check_client_statuses() {
    time_t current_time = time(NULL);
    pthread_mutex_lock(&user_mutex);
    for (int i = 0; i < user_count; i++) {
        if (user_directory[i].status == 1 && (current_time - user_directory[i].last_response) > HELLO_TIMEOUT) {
            printf("User %s has disconnected.\n", user_directory[i].username);
            user_directory[i].status = 0;  // mark as inactive
            // Remove user's resources
            pthread_mutex_lock(&resource_mutex);
            for (int j = 0; j < resource_count; ) {
                if (strcmp(resource_directory[j].owner, user_directory[i].username) == 0) {
                    // Shift resources
                    for (int k = j; k < resource_count - 1; k++) {
                        resource_directory[k] = resource_directory[k + 1];
                    }
                    resource_count--;
                } else {
                    j++;
                }
            }
            pthread_mutex_unlock(&resource_mutex);
        }
    }
    pthread_mutex_unlock(&user_mutex);
}

// Function to handle client requests
void handle_client(int sockfd, struct sockaddr_in client_addr, char* buffer) {
    if (strncmp(buffer, "register", 8) == 0) {
        char username[50];
        int tcp_port;
        sscanf(buffer, "register %s %d", username, &tcp_port);
        add_user(username, client_addr, tcp_port);
        printf("User %s registered with TCP port %d.\n", username, tcp_port);
        // Send acknowledgment
        char ack_message[] = "Registration successful";
        sendto(sockfd, ack_message, strlen(ack_message), 0, (struct sockaddr*)&client_addr, sizeof(client_addr));
    } else if (strncmp(buffer, "announce", 8) == 0) {
        char resource_name[100], owner[50];
        sscanf(buffer, "announce %s %s", resource_name, owner);
        add_resource(resource_name, owner);
        printf("Resource %s announced by %s\n", resource_name, owner);
        // Send acknowledgment
        char ack_message[] = "Resource announced successfully";
        sendto(sockfd, ack_message, strlen(ack_message), 0, (struct sockaddr*)&client_addr, sizeof(client_addr));
    } else if (strncmp(buffer, "query resources", 15) == 0) {
        char resource_list[BUFFER_SIZE] = "";
        pthread_mutex_lock(&resource_mutex);
        if (resource_count == 0) {
            strcpy(resource_list, "No resources available.");
        } else {
            strcpy(resource_list, "Resources:\n");
            for (int i = 0; i < resource_count; i++) {
                char resource_entry[300];
                // Find the owner's IP address and TCP port
                char owner_ip[INET_ADDRSTRLEN];
                int owner_tcp_port = 0;
                int owner_active = 0;
                pthread_mutex_lock(&user_mutex);
                for (int j = 0; j < user_count; j++) {
                    if (strcmp(user_directory[j].username, resource_directory[i].owner) == 0) {
                        inet_ntop(AF_INET, &(user_directory[j].addr.sin_addr), owner_ip, INET_ADDRSTRLEN);
                        owner_tcp_port = user_directory[j].tcp_port;
                        owner_active = user_directory[j].status;
                        break;
                    }
                }
                pthread_mutex_unlock(&user_mutex);

                if (owner_active == 1) {
                    snprintf(resource_entry, sizeof(resource_entry), "%s (Owner: %s, IP: %s, TCP Port: %d)\n",
                             resource_directory[i].resource_name, resource_directory[i].owner, owner_ip, owner_tcp_port);
                    strcat(resource_list, resource_entry);
                }
            }
            if (strlen(resource_list) == strlen("Resources:\n")) {
                strcpy(resource_list, "No resources available.");
            }
        }
        pthread_mutex_unlock(&resource_mutex);
        sendto(sockfd, resource_list, strlen(resource_list), 0,
               (struct sockaddr*)&client_addr, sizeof(client_addr));
    } else if (strncmp(buffer, "query users", 11) == 0) {
        char user_list[BUFFER_SIZE] = "";
        pthread_mutex_lock(&user_mutex);
        int active_users = 0;
        for (int i = 0; i < user_count; i++) {
            if (user_directory[i].status == 1) {
                active_users++;
                strcat(user_list, user_directory[i].username);
                strcat(user_list, "\n");
            }
        }
        if (active_users == 0) {
            strcpy(user_list, "No active users.");
        } else {
            char header[] = "Active users:\n";
            memmove(user_list + strlen(header), user_list, strlen(user_list) + 1);
            memcpy(user_list, header, strlen(header));
        }
        pthread_mutex_unlock(&user_mutex);
        sendto(sockfd, user_list, strlen(user_list), 0,
               (struct sockaddr*)&client_addr, sizeof(client_addr));
    } else if (strcmp(buffer, "hello response") == 0) {
        pthread_mutex_lock(&user_mutex);
        for (int i = 0; i < user_count; i++) {
            if (user_directory[i].addr.sin_addr.s_addr == client_addr.sin_addr.s_addr &&
                user_directory[i].addr.sin_port == client_addr.sin_port) {
                user_directory[i].last_response = time(NULL);
                user_directory[i].status = 1;  // mark as active
                break;
            }
        }
        pthread_mutex_unlock(&user_mutex);
    } else if (strncmp(buffer, "get resource_info", 17) == 0) {
        char resource_name[100];
        sscanf(buffer, "get resource_info %s", resource_name);
        // Find all resources in the resource directory with the given name
        pthread_mutex_lock(&resource_mutex);
        int found = 0;
        char owners_list[BUFFER_SIZE] = "";
        for (int i = 0; i < resource_count; i++) {
            if (strcmp(resource_directory[i].resource_name, resource_name) == 0) {
                // Get the owner's IP and TCP port
                char owner_ip[INET_ADDRSTRLEN];
                int owner_tcp_port = 0;
                int owner_active = 0;
                pthread_mutex_lock(&user_mutex);
                for (int j = 0; j < user_count; j++) {
                    if (strcmp(user_directory[j].username, resource_directory[i].owner) == 0) {
                        inet_ntop(AF_INET, &(user_directory[j].addr.sin_addr), owner_ip, INET_ADDRSTRLEN);
                        owner_tcp_port = user_directory[j].tcp_port;
                        owner_active = user_directory[j].status;
                        break;
                    }
                }
                pthread_mutex_unlock(&user_mutex);
                if (owner_active == 1) {
                    char owner_info[200];
                    snprintf(owner_info, sizeof(owner_info), "%s %s %d\n",
                             resource_directory[i].owner, owner_ip, owner_tcp_port);
                    strcat(owners_list, owner_info);
                    found = 1;
                }
            }
        }
        pthread_mutex_unlock(&resource_mutex);
        if (!found) {
            char error_message[BUFFER_SIZE];
            snprintf(error_message, BUFFER_SIZE, "Error: Resource '%s' not found.", resource_name);
            sendto(sockfd, error_message, strlen(error_message), 0, (struct sockaddr*)&client_addr, sizeof(client_addr));
        } else {
            sendto(sockfd, owners_list, strlen(owners_list), 0, (struct sockaddr*)&client_addr, sizeof(client_addr));
        }
    }
}

// Thread function to handle client messages
void* client_handler_thread(void* arg) {
    int sockfd = *(int*)arg;
    struct sockaddr_in client_addr;
    socklen_t addr_len = sizeof(client_addr);
    char buffer[BUFFER_SIZE];

    while (1) {
        int bytes_received = recvfrom(sockfd, buffer, BUFFER_SIZE, 0,
                                      (struct sockaddr*)&client_addr, &addr_len);
        if (bytes_received > 0) {
            buffer[bytes_received] = '\0';
            handle_client(sockfd, client_addr, buffer);
        }
    }
    return NULL;
}

// Thread function to send hello messages and check client statuses
void* hello_thread(void* arg) {
    int sockfd = *(int*)arg;
    while (1) {
        send_hello_messages(sockfd);
        check_client_statuses();
        sleep(HELLO_INTERVAL);
    }
    return NULL;
}

int main() {
    int sockfd;
    struct sockaddr_in server_addr;

    sockfd = socket(AF_INET, SOCK_DGRAM, 0);
    if (sockfd < 0) {
        perror("Socket creation failed");
        exit(EXIT_FAILURE);
    }
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = INADDR_ANY;
    server_addr.sin_port = htons(PORT);

    if (bind(sockfd, (struct sockaddr*)&server_addr, sizeof(server_addr)) < 0) {
        perror("Bind failed");
        close(sockfd);
        exit(EXIT_FAILURE);
    }

    pthread_t client_thread_id, hello_thread_id;

    pthread_create(&client_thread_id, NULL, client_handler_thread, &sockfd);
    pthread_create(&hello_thread_id, NULL, hello_thread, &sockfd);

    pthread_join(client_thread_id, NULL);
    pthread_join(hello_thread_id, NULL);

    close(sockfd);
    return 0;
}
