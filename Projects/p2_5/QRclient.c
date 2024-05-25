#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>

#define BUFFER_SIZE 20000

void print_usage(const char *program_name) {
    printf("Usage: %s <server_ip> <port>\n", program_name);
}

int main(int argc, char *argv[]) {
    if (argc != 3) {
        print_usage(argv[0]);
        exit(1);
    }

    const char *server_ip = argv[1];
    int port = atoi(argv[2]);
    char file_name[256]; // buffer for storing file name
    int size;

    // create socket
    int sockfd;
    struct sockaddr_in serv_addr;
    char buffer[BUFFER_SIZE];
    ssize_t bytes_read;

    // create socket
    sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if (sockfd < 0) {
        perror("ERROR opening socket");
        exit(1);
    }

    // initialize socket structure
    bzero((char *) &serv_addr, sizeof(serv_addr));
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_port = htons(port);
    inet_pton(AF_INET, server_ip, &serv_addr.sin_addr);

    // connect to server
    if (connect(sockfd, (struct sockaddr *) &serv_addr, sizeof(serv_addr)) < 0) {
        perror("ERROR connecting");
        close(sockfd);
        exit(1);
    }

    // Read response from server
    bzero(buffer, BUFFER_SIZE);
    ssize_t n = recv(sockfd, buffer, BUFFER_SIZE - 1, 0);
    if (n < 0) {
        perror("ERROR receiving response");
        close(sockfd);
        exit(1);
    }

    // Check for error message from server
    if (strncmp(buffer, "Error:", 6) == 0) {
        fprintf(stderr, "Error: Connection to server unsuccessful - %s\n", buffer + 7);
        close(sockfd);
        exit(1);
    }

    /////////////////////////////////////////////////////////////////"ACK"//////////////////////////////////////////////////////////////
    printf("Received from Server: %s\n", buffer);

    while (1) {
        // Prompt for user input
        printf("Enter 'close' to disconnect, 'shutdown' to turn off server, or a qr code filename to get the URL: \n");
        
        if (!fgets(file_name, sizeof(file_name), stdin)) {
            perror("Error reading input");
            close(sockfd);
            exit(1);
        }
        file_name[strcspn(file_name, "\n")] = 0; // remove newline character

        // Check user input
        if (strcmp(file_name, "close") == 0) {
            printf("Disconnecting from server...\n");
            if (send(sockfd, "close", strlen("close"), 0) < 0) {
                perror("Error sending client disconnect command");
            }
            break; // Exit the loop to disconnect from the server
        } else if (strcmp(file_name, "shutdown") == 0) {
            printf("Shutting down server...\n");
            // Send shutdown command to server
            if (send(sockfd, "shutdown", strlen("shutdown"), 0) < 0) {
                perror("Error sending shutdown command");
            }
            close(sockfd);
            printf("Connection closed.\n");
            exit(0);
        }

        // open the QR file
        FILE *image = fopen(file_name, "rb");
        if (!image) {
            perror("Error opening QR image");
            continue; // Skip to next iteration
        }

        fseek(image, 0, SEEK_END);
        size = ftell(image);
        fseek(image, 0, SEEK_SET);

        // Send Picture Size
        printf("Sending Picture Size %d\n", size);
        write(sockfd, &size, sizeof(size));

        // Send image file
        while ((bytes_read = fread(buffer, 1, BUFFER_SIZE, image)) > 0) {
            if (send(sockfd, buffer, bytes_read, 0) != bytes_read) {
                perror("Error sending data");
                fclose(image); // Close the image file before exiting
                close(sockfd);
                return 1;
            }
        }
        printf("Sent Picture Data \n");

        // Read response from server In real case, url depicted from QR code.
        bzero(buffer, BUFFER_SIZE);
        n = recv(sockfd, buffer, BUFFER_SIZE - 1, 0);
        if (n < 0) {
            perror("ERROR receiving response");
            fclose(image); // Close the image file before exiting
            close(sockfd);
            exit(1);
        }

        printf("Server response: %s\n", buffer);

        // Check if server response indicates an error
        if (strncmp(buffer, "Error:", 6) == 0) {
            fprintf(stderr, "Error from server: %s\n", buffer + 7);
            fclose(image); // Close the image file before exiting
            close(sockfd);
            exit(1);
        }

        // Close the image file
        fclose(image);
    }

    // Close socket
    close(sockfd);

    return 0;
}
