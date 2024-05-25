#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <pthread.h>
#include <stdbool.h>
#include <time.h>

#define DEFAULT_PORT 2012
#define DEFAULT_RATE_MSGS 3
#define DEFAULT_RATE_TIME 60
#define DEFAULT_MAX_USERS 3
#define DEFAULT_TIMEOUT 80
#define BUFFER_SIZE 10000
#define MAX_URL_LENGTH 256
#define MAX_IMG_SIZE 104857600

// Flags for different conditions
bool timeout_flag = false;
bool rate_limit_exceeded_flag = false;
bool max_users_exceeded_flag = false;
bool shutdown_requested = false; // Flag to indicate if shutdown is requested

// Structure for the server message
struct ServerMessage
{
    uint32_t return_code;
    uint32_t url_length;
    char url[MAX_URL_LENGTH];
};

int current_users = 0;
time_t last_activity_time = 0;
pthread_mutex_t current_users_mutex = PTHREAD_MUTEX_INITIALIZER; 
pthread_mutex_t shutdown_mutex = PTHREAD_MUTEX_INITIALIZER;

// Rate limiting variables
static int request_count = 0;
static time_t last_request_time = 0;

void write_log_entry(const char *log_message)
{
    FILE *log_file = fopen("admin_log.txt", "a");
    if (log_file == NULL)
    {
        perror("Error opening log file\n");
        exit(1);
    }

    time_t current_time;
    char formatted_time[40];
    struct tm *time_info;

    time(&current_time);                                                              // Get current time
    time_info = localtime(&current_time);                                             // convert to local time
    strftime(formatted_time, sizeof(formatted_time), "%Y-%m-%d %H:%M:%S", time_info); // Format the time portion of the log.

    // Write the formatted time in the log entry
    fprintf(log_file, "[%s] %s\n", formatted_time, log_message);
    fclose(log_file);
}

void print_usage()
{
    printf("Usage: ./QRServer [options]\n");
    printf("Options:\n");
    printf("  -p, --port <port number>\n");
    printf("  -r, --rate <number requests> <number seconds>\n");
    printf("  -m, --max-users <number of users>\n");
    printf("  -t, --timeout <number of seconds>\n");
}

char *decode_qr_code(const char *image_path)
{
    // Command to run
    char command[256];
    sprintf(command, "java -cp javase.jar:core.jar com.google.zxing.client.j2se.CommandLineRunner %s", image_path);

    // Open a pipe to the command
    FILE *pipe = popen(command, "r");
    if (!pipe)
    {
        perror("Error opening pipe");
        return NULL;
    }

    // Read the output from the command
    char *url = NULL;
    char *line = NULL;
    size_t len = 0;
    ssize_t read;

    // Read each line from the pipe
    while ((read = getline(&line, &len, pipe)) != -1)
    {
        // Check if the line contains "http" indicating it's a URL
        if (strstr(line, "http") != NULL)
        {
            // Allocate memory for the URL and copy it
            url = strdup(line);
            break; // Stop reading after the first URL is found
        }
    }

    // Close the pipe
    pclose(pipe);
    if (line)
    {
        free(line);
    }

    return url;
}

void *handleTCPClient(void *arg)
{
    int *clientSocket = (int *)arg;

    // Extract client IP address
    struct sockaddr_in cli_addr;
    socklen_t clilen = sizeof(cli_addr);
    getpeername(*clientSocket, (struct sockaddr *)&cli_addr, &clilen);

    while (1)
    {
        // Timeout handling
        time_t current_time;
        time(&current_time);
        double elapsed_time = difftime(current_time, last_activity_time);
        if (elapsed_time > DEFAULT_TIMEOUT)
        {
            char timeout_message[100];
            sprintf(timeout_message, "timeout occured. closing connection %s", inet_ntoa(cli_addr.sin_addr));
            write_log_entry(timeout_message);
            printf("%s\n", timeout_message);
            timeout_flag = true; // Set timeout flag
            break;
        }

        // Rate Limiting
        // time(&current_time);
        // double elapsed = difftime(current_time, last_request_time);
        // if (elapsed > DEFAULT_RATE_TIME)
        // {
        //     request_count = 0;
        // }
        // if (request_count >= DEFAULT_RATE_MSGS)
        // {
        //     printf("Rate limit exceeded\n");
        //     // Send error message to client
        //     char error_message[100];
        //     snprintf(error_message, sizeof(error_message), "Error: Rate limit exceeded. Maximum %d requests allowed per %d seconds.\n", DEFAULT_RATE_MSGS, DEFAULT_RATE_TIME);
        //     if (write(*clientSocket, error_message, strlen(error_message) + 1) < 0)
        //     {
        //         perror("ERROR writing to socket");
        //         exit(1);
        //     }
        //     // Log the occurrence of rate limit exceeded
        //     char log_message[100];
        //     sprintf(log_message, "Error: Rate limit exceeded for client %s", inet_ntoa(cli_addr.sin_addr));
        //     write_log_entry(log_message);
        //     rate_limit_exceeded_flag = true; // Set rate limit exceeded flag
        //     break;
        // }
        request_count++;
        last_request_time = current_time;

        // Check maximum concurrent users
        pthread_mutex_lock(&current_users_mutex);
        if (current_users > DEFAULT_MAX_USERS)
        {
            pthread_mutex_unlock(&current_users_mutex);
            //printf("current users %d\n", current_users);
           //printf("Max users %d\n", DEFAULT_MAX_USERS);

            char max_users_msg[100];
            sprintf(max_users_msg, "Error: Maximum users limit exceeded. Closing connection. %s\n", inet_ntoa(cli_addr.sin_addr));
            write_log_entry(max_users_msg);

            if (write(*clientSocket, max_users_msg, strlen(max_users_msg) + 1) < 0)
            {
                perror("ERROR writing to socket");
                exit(1);
            }

            pthread_mutex_lock(&current_users_mutex);
            current_users--;
            pthread_mutex_unlock(&current_users_mutex);
            max_users_exceeded_flag = true; // Set max users exceeded flag
            break;
        }
        pthread_mutex_unlock(&current_users_mutex);

        char success_connect[100];
        snprintf(success_connect, sizeof(success_connect), "Successfully Connected");
        if (write(*clientSocket, success_connect, strlen(success_connect) + 1) < 0)
        {
            perror("ERROR writing to socket");
            exit(1);
        }

        // Update last activity time
        last_activity_time = current_time;

        // Read PNG file size from the client
        u_int32_t size;
        read(*clientSocket, &size, sizeof(u_int32_t));
        printf("%u bytes\n", size);

        // Check if the received size exceeds the maximum allowed size
        if (size > MAX_IMG_SIZE)
        {
            // Log the rejection of the request due to excessive size
            char log_rejection[100];
            sprintf(log_rejection, "Rejected request from %s: Excessive file size", inet_ntoa(cli_addr.sin_addr));
            write_log_entry(log_rejection);

            // Return a failure code to the client indicating excessive file size
            u_int32_t failure_code = htonl(1); // Assuming failure code 1 indicates excessive file size
            send(*clientSocket, &failure_code, sizeof(u_int32_t), 0);
            close(*clientSocket); // Close the connection
            break;               // Exit the function or handle the rejection accordingly
        }

        // Log received size
        char log_received_size[100];
        sprintf(log_received_size, "Server received file size from: %s", inet_ntoa(cli_addr.sin_addr));
        write_log_entry(log_received_size);

        // Clear and reset the socket buffer
        char buffer[size];
        memset(buffer, 0, sizeof(buffer));

        // Read PNG file data from the client
        u_int32_t bytesReceivedSoFar = 0;

        while (bytesReceivedSoFar < size)
        {
            int bytesActuallyReceived = read(*clientSocket, buffer + bytesReceivedSoFar, size - bytesReceivedSoFar);
            if (bytesActuallyReceived < 0)
            {
                perror("ERROR reading from socket");
                exit(1);
            }
            bytesReceivedSoFar += bytesActuallyReceived;
        }

        printf("Received PNG file data from client\n");
        time(&last_activity_time);

        // Log received file data
        char log_received_data[100];
        sprintf(log_received_data, "Server received file data from: %s", inet_ntoa(cli_addr.sin_addr));
        write_log_entry(log_received_data);

        // Save the PNG file received from the client
        FILE *imageFile = fopen("received_image.png", "wb");
        if (imageFile == NULL)
        {
            perror("ERROR opening image file");
            exit(1);
        }
        fwrite(buffer, 1, size, imageFile);
        fclose(imageFile);

        // Decode QR code from the saved image file
        char *url = decode_qr_code("received_image.png");
        int url_length = strlen(url);

        // Prepare the server message based on success
        struct ServerMessage serverMsg;
        serverMsg.url_length = url_length;
        strncpy(serverMsg.url, url, MAX_URL_LENGTH);

        if (timeout_flag)
        {
            // Timeout
            serverMsg.return_code = 2;
            serverMsg.url_length = 39;
        }
        else if (rate_limit_exceeded_flag)
        {
            // Rate Limit Exceeded
            serverMsg.return_code = 3;
            serverMsg.url_length = 66; // No URL
        }
        else if (max_users_exceeded_flag)
        {
            // Maximum Users Exceeded
            serverMsg.return_code = 1;
            serverMsg.url_length = 0; // No URL
        }
        else
        {
            // Success
            serverMsg.return_code = 0;
            serverMsg.url_length = url_length;
            strncpy(serverMsg.url, url, MAX_URL_LENGTH);
        }

        // Prepare response message with URL
        char response[MAX_URL_LENGTH + 20];                                                                                       // Allocate enough space for "Link: " prefix and URL
        snprintf(response, sizeof(response), "Return Code: %d Link length: %d Link: %s", serverMsg.return_code, url_length, url); // Using snprintf to prevent buffer overflow

        // Send response back to client
        int n = write(*clientSocket, response, strlen(response) + 1); // +1 for the null terminator
        if (n < 0)
        {
            perror("ERROR writing to socket");
            exit(1);
        }
        printf("Responded to client\n");

        // Log sent URL to client
        char log_send_url[100];
        sprintf(log_send_url, "Server Sent URL to client %s", inet_ntoa(cli_addr.sin_addr));
        write_log_entry(log_send_url);

        // Free dynamically allocated URL
        free(url);

        // Read command from client
        char command[10];
        int bytes_read = read(*clientSocket, command, sizeof(command));
        if (bytes_read <= 0)
        {
            perror("ERROR reading from socket");
            exit(1);
        }

        // Process client command
        if (strncmp(command, "close", 5) == 0)
        {
            // Close command received, log client disconnect and close socket
            char log_disconnect[100];
            sprintf(log_disconnect, "Client disconnected: %s", inet_ntoa(cli_addr.sin_addr));
            write_log_entry(log_disconnect);
            pthread_mutex_lock(&current_users_mutex);
            current_users--;
            pthread_mutex_unlock(&current_users_mutex);
            printf("Current users %d\n", current_users);
            close(*clientSocket);
            break; // Exit the loop
        }
        else if (strncmp(command, "shutdown", 8) == 0)
        {
            // Shutdown command received, set shutdown flag and break the loop
            char log_shutdown[100];
            sprintf(log_shutdown, "Server Shutdown initiated by: %s", inet_ntoa(cli_addr.sin_addr));
            write_log_entry(log_shutdown);

            pthread_mutex_lock(&shutdown_mutex);
            shutdown_requested = true;
            pthread_mutex_unlock(&shutdown_mutex);
            break;
        }
    }

    close(*clientSocket);
    free(clientSocket);
    return NULL;
}

int main(int argc, char *argv[])
{
    int sockfd, newsockfd;
    struct sockaddr_in serv_addr, cli_addr;
    socklen_t clilen;

    int port = DEFAULT_PORT;
    int rate_msgs = DEFAULT_RATE_MSGS;
    int rate_time = DEFAULT_RATE_TIME;
    int max_users = DEFAULT_MAX_USERS;
    int timeout = DEFAULT_TIMEOUT;

    // Parse command-line arguments
    int opt;
    while ((opt = getopt(argc, argv, "p:r:t:m:o:")) != -1)
    {
        switch (opt)
        {
        case 'p':
            port = atoi(optarg);
            break;
        case 'r':
            rate_msgs = atoi(optarg);
            break;
        case 't':
            rate_time = atoi(optarg);
            break;
        case 'm':
            max_users = atoi(optarg);
            break;
        case 'o':
            timeout = atoi(optarg);
            break;
        default:
            print_usage();
            return 1;
        }
    }

    printf("Port: %d | ", port);
    printf("Rate Messages: %d | ", rate_msgs);
    printf("Rate Time: %d | ", rate_time);
    printf("Max Users: %d | ", max_users);
    printf("Timeout: %d\n", timeout);

    // Create socket
    sockfd = socket(PF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (sockfd < 0)
    {
        perror("ERROR opening socket");
        exit(1);
    }

    // Initialize socket structure
    memset(&serv_addr, 0, sizeof(serv_addr));
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_addr.s_addr = INADDR_ANY;
    serv_addr.sin_port = htons(port);

    // Bind the host address
    if (bind(sockfd, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) < 0)
    {
        perror("ERROR on binding");
        exit(1);
    }

    // Start listening for the clients
    if ((listen(sockfd, 5)) != 0)
    {
        printf("Listen failed...\n");
        exit(0);
    }
    else
        printf("Server started. Waiting for clients on port %d...\n", port);

    write_log_entry("Server Started");

    while (1)
    {
        clilen = sizeof(cli_addr);
        newsockfd = accept(sockfd, (struct sockaddr *)&cli_addr, &clilen);
        if (newsockfd < 0)
        {
            perror("ERROR on accept");
            exit(1);
        }

        // Increment current user count
        pthread_mutex_lock(&current_users_mutex);
        current_users++;
        pthread_mutex_unlock(&current_users_mutex);

        // update last activity time
        time(&last_activity_time);

        // Log client connection
        char log_message[100];
        sprintf(log_message, "Client connected: %s", inet_ntoa(cli_addr.sin_addr));
        write_log_entry(log_message);

        // Create a new thread to handle the client
        pthread_t thread_id;
        int *new_sock = malloc(sizeof(int));
        *new_sock = newsockfd;

        if (pthread_create(&thread_id, NULL, handleTCPClient, (void *)new_sock) < 0)
        {
            perror("could not create thread");
            return 1;
        }

        // check for shutdown request
        pthread_mutex_lock(&shutdown_mutex);
        if(shutdown_requested){
            pthread_mutex_unlock(&shutdown_mutex);
            break;
        }
        pthread_mutex_unlock(&shutdown_mutex);
    }

    // Close the server socket
    close(sockfd);
    pthread_exit(NULL);

    return 0;
}
