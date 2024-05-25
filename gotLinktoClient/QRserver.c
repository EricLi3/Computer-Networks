#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/wait.h>
#include <netinet/in.h>
#include <netdb.h>
#include <getopt.h>
#include <ctype.h>

#define DEFAULT_PORT 2012
#define DEFAULT_RATE_LIMIT 3
#define DEFAULT_MAX_USERS 3
#define DEFAULT_TIMEOUT 80
#define MAX_URL_LENGTH 256
// #define BUFFER_SIZE 1024


void print_usage() {
    printf("Usage: ./QRServer [options]\n");
    printf("Options:\n");
    printf("  -p, --port <port number>\n");
    printf("  -r, --rate <number requests> <number seconds>\n");
    printf("  -m, --max-users <number of users>\n");
    printf("  -t, --timeout <number of seconds>\n");
}

char* decode_qr_code(const char *image_path) {
    // Command to run
    char command[256];
    sprintf(command, "java -cp javase.jar:core.jar com.google.zxing.client.j2se.CommandLineRunner %s", image_path);

    // Open a pipe to the command
    FILE *pipe = popen(command, "r");
    if (!pipe) {
        perror("Error opening pipe");
        return NULL;
    }

    // Read the output from the command
    char buffer[MAX_URL_LENGTH];
    char *url = NULL;
    char *line = NULL;
    size_t len = 0;
    ssize_t read;

    // Read each line from the pipe
    while ((read = getline(&line, &len, pipe)) != -1) {
        // Check if the line contains "http" indicating it's a URL
        if (strstr(line, "http") != NULL) {
            // Allocate memory for the URL and copy it
            url = strdup(line);
            break; // Stop reading after the first URL is found
        }
    }

    // Close the pipe
    pclose(pipe);
    if (line) {
        free(line);
    }

    return url;
}

void handleTCPClient(int clientSocket) {
    printf("Reading Picture Size\n");
    int size;
    read(clientSocket, &size, sizeof(int));

    char buffer[size];
    int n;
    // Read PNG file data from client
    bzero(buffer, size);
    n = read(clientSocket, buffer, size);
    if (n < 0) {
        perror("ERROR reading from socket");
        exit(1);
    }

    printf("Received PNG file data from client\n");

    // Save the PNG file received from the client
    FILE *imageFile = fopen("received_image.png", "wb");
    if (imageFile == NULL) {
        perror("ERROR opening image file");
        exit(1);
    }
    fwrite(buffer, 1, n, imageFile);
    fclose(imageFile);

    // Decode QR code from the saved image file
    char* url = decode_qr_code("received_image.png");
    int url_length = strlen(url);
    
    // Prepare response message with URL
    char response[MAX_URL_LENGTH + 20];  // Allocate enough space for "Link: " prefix and URL
    snprintf(response, sizeof(response), "Link length: %d Link: %s", url_length, url); // Using snprintf to prevent buffer overflow


    // Send response back to client
    n = write(clientSocket, response, strlen(response) + 1); // +1 for the null terminator
    if (n < 0) {
        perror("ERROR writing to socket");
        exit(1);
    }

    // Free dynamically allocated URL
    free(url);

    // Close the client socket
    close(clientSocket);
}


int main(int argc, char *argv[]) {
    int sockfd, newsockfd;
    struct sockaddr_in serv_addr, cli_addr;
    socklen_t clilen;
    FILE *image;
    //char buffer[BUFFER_SIZE];
    ssize_t bytes_received;
    
    int port = DEFAULT_PORT;
    int rate_limit = DEFAULT_RATE_LIMIT;
    int max_users = DEFAULT_MAX_USERS;
    int timeout = DEFAULT_TIMEOUT;

    // Parse command-line arguments using getopt
    int opt;
    while ((opt = getopt(argc, argv, "p:r:m:t:")) != -1) {
        switch (opt) {
            case 'p':
                port = atoi(optarg);
                break;
            case 'r':
                rate_limit = atoi(optarg);
                // If next argument is available and starts with a digit, parse it as the second part of rate limit
                if (optind < argc && isdigit(*argv[optind])) {
                    rate_limit = rate_limit * 1000 + atoi(argv[optind]);
                    optind++;
                } else {
                    printf("Error: Missing second argument for RATE\n");
                    return 1;
                }
                break;
            case 'm':
                max_users = atoi(optarg);
                break;
            case 't':
                timeout = atoi(optarg);
                break;
            default:
                print_usage();
                return 1;
        }
    }

    // Create socket
    sockfd = socket(PF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (sockfd < 0) {
        perror("ERROR opening socket");
        exit(1);
    }

    // Initialize socket structure
    memset(&serv_addr, 0, sizeof(serv_addr));
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_addr.s_addr = INADDR_ANY;
    serv_addr.sin_port = htons(port);

    // Bind the host address
    if (bind(sockfd, (struct sockaddr *) &serv_addr, sizeof(serv_addr)) < 0) {
        perror("ERROR on binding");
        exit(1);
    }

    // Start listening for the clients
    if ((listen(sockfd, 5)) != 0) { 
        printf("Listen failed...\n"); 
        exit(0); 
    }
    else 
        printf("Server started. Waiting for clients on port %d...\n", port);

    while (1) {
        // Accept actual connection from the client
        clilen = sizeof(cli_addr);
        newsockfd = accept(sockfd, (struct sockaddr *) &cli_addr, &clilen);
        if (newsockfd < 0) {
            perror("ERROR on accept");
            exit(1);
        }

        // Fork a new process for this client
        pid_t pid = fork();
        if (pid < 0) {
            perror("ERROR on fork");
            exit(1);
        }

        if (pid == 0) {  // Child process
            close(sockfd);  // Close listening socket in child process
            handleTCPClient(newsockfd);
            exit(0);  // Exit child process after handling client
        } 
        else {  // Parent process
            close(newsockfd);  // Close client socket in parent process
            // Wait for any terminated child processes to prevent zombie processes
            while (waitpid(-1, NULL, WNOHANG) > 0);
        }
    }

    // Close the server socket (never reached in this loop)
    close(sockfd);

    return 0;
}