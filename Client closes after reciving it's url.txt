#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>

// #define DEFAULT_PORT 2012               // replace port number with one you specified for the server
// #define DEFAULT_SERVER_IP "127.0.0.1"   // replace with IP of server. 
#define BUFFER_SIZE 1024

void print_usage(const char *program_name) {
    printf("Usage: %s <server_ip> <port> <QR_img>\n", program_name);
}

int main(int argc, char *argv[]) {
    if (argc != 4) {
        print_usage(argv[0]);
        exit(1);
    }

    const char *server_ip = argv[1];
    int port = atoi(argv[2]);
    const char *file_name = argv[3];
    int size;

    // Open the QR file
    FILE *image = fopen(file_name, "rb");
    if (!image) {
        perror("Error opening QR image");
        exit(1);
    }

    fseek(image, 0, SEEK_END);
    size = ftell(image);
    fseek(image, 0, SEEK_SET);


    // Create socket
    int sockfd;
    struct sockaddr_in serv_addr;
    char buffer[BUFFER_SIZE];
    ssize_t bytes_read;

    // Create socket
    sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if (sockfd < 0) {
        perror("ERROR opening socket");
        fclose(image); // Close the image file before exiting
        exit(1);
    }

    // Initialize socket structure
    bzero((char *) &serv_addr, sizeof(serv_addr));
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_port = htons(port);
    inet_pton(AF_INET, server_ip, &serv_addr.sin_addr);

    // Connect to server
    if (connect(sockfd, (struct sockaddr *) &serv_addr, sizeof(serv_addr)) < 0) {
        perror("ERROR connecting");
        fclose(image); // Close the image file before exiting
        close(sockfd);
        exit(1);
    }

    //Send Picture Size from: https://stackoverflow.com/questions/13097375/sending-images-over-sockets
    printf("Sending Picture Size %d\n", size);
    write(sockfd, &size, sizeof(size));

    // Send message to server
    // Read from image file and send over socket
    while ((bytes_read = fread(buffer, 1, BUFFER_SIZE, image)) > 0) {
        if (send(sockfd, buffer, bytes_read, 0) != bytes_read) {
            perror("Error sending data");
            fclose(image); // Close the image file before exiting
            close(sockfd);
            return 1;
        }
    }

    // Read response from server In real case, url depicted from QR code. 
    bzero(buffer, BUFFER_SIZE);
    ssize_t n = recv(sockfd, buffer, BUFFER_SIZE - 1, 0);
    if (n < 0) {
        perror("ERROR receiving response");
        fclose(image); // Close the image file before exiting
        close(sockfd);
        exit(1);
    }

    printf("Server response: %s\n", buffer);

    // Close socket and file
    fclose(image);
    close(sockfd);

    return 0;
}