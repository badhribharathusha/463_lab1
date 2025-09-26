/* The code is subject to Purdue University copyright policies.
 * Do not share, distribute, or post online.
 */
#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <sys/wait.h>
#include <netdb.h>
#include <arpa/inet.h>
#include <fcntl.h>
#include <sys/stat.h>

int main(int argc, char *argv[])
{
    if (argc != 4)
    {
        fprintf(stderr, "usage: ./http_client [host] [port number] [filepath]\n");
        exit(1);
    }

    // printf("Parsing host, port, filepath...\n");
    char *host = argv[1];
    int port = atoi(argv[2]);
    char *filepath = argv[3];

    // printf("Validating port and filepath...\n");
    if ((port <= 0) || (port > 65535) || filepath[0] != '/')
    {
        exit(1);
    }

    // Extracting filename
    char *filename;
    if (strcmp(filepath, "/") == 0)
    {
        filename = "index.html";
    }
    else
    {
        char *final_slash = strrchr(filepath, '/');
        if (final_slash == NULL)
        {
            filename = "index.html";
        }
        else if (*(final_slash + 1) != '\0')
        {
            filename = final_slash + 1;
        }
        else
        {
            filename = "index.html";
        }
    }

    // printf("Host: %s\n", host);
    // printf("Port: %d\n", port);
    // printf("Filepath: %s\n", filepath);
    // printf("Filename: %s\n", filename);

    struct sockaddr_in server_address;
    server_address.sin_family = AF_INET;
    server_address.sin_port = htons(port);

    struct hostent *host_info = gethostbyname(host);
    if ((host_info == NULL) || (host_info->h_addr_list[0] == NULL)) 
    {
        exit(1); // Can't find IP address case or failed DNS case
    }

    struct in_addr *ip_address = (struct in_addr *)host_info->h_addr_list[0];
    server_address.sin_addr = *ip_address;

    // printf("Resolved %s -> %s\n", host, inet_ntoa(*ip_address));

    int sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if (sockfd < 0) 
    {
        exit(1);
    }

    if (connect(sockfd, (struct sockaddr *)&server_address, sizeof(server_address)) < 0) 
    { 
        close(sockfd);
        exit(1);
    }

    // printf("Connected to %s (%s) on port %d\n", host, inet_ntoa(*addr), port);

    char get_request[1024];
    int req_len = snprintf(get_request, sizeof(get_request), "GET %s HTTP/1.0\r\nHost: %s:%d\r\n\r\n", filepath, host, port);

    if (send(sockfd, get_request, req_len, 0) < 0) {
        close(sockfd);
        exit(1);
    }

    char buffer[4096];
    char header_buffer[8192];
    int header_length = 0;
    char *header_end = NULL;
    int bytes_read = read(sockfd, buffer, sizeof(buffer));

    while (bytes_read > 0) {
        if (header_length + bytes_read >= sizeof(header_buffer)) {
            close(sockfd);
            exit(1);
        }
        memcpy(header_buffer + header_length, buffer, bytes_read);
        header_length += bytes_read;
        header_buffer[header_length] = '\0';

        header_end = strstr(header_buffer, "\r\n\r\n");
        if (header_end != NULL)
        {
            break;
        } 
        bytes_read = read(sockfd, buffer, sizeof(buffer));
    }

    if (header_end == NULL) {
        close(sockfd);
        exit(1);
    }

    int status_code;
    if (sscanf(header_buffer, "HTTP/%*s %d", &status_code) != 1) {
        close(sockfd);
        exit(1);
    }
    if (status_code != 200) {
        char *line_end = strstr(header_buffer, "\r\n");
        if (line_end) *line_end = '\0';
        printf("%s\n", header_buffer);
        close(sockfd);
        exit(0);
    }
  
    long content_length = -1;
    char *header_pointer = strstr(header_buffer, "Content-Length:");
    if (header_pointer)
    {
        sscanf(header_pointer, "Content-Length: %ld", &content_length); // Getting content length
    } 
    else
    {
        printf("Could not download the requested file (content length unknown)\n"); // content length errror
        close(sockfd);
        exit(1);
    }

    FILE* output_file = fopen(filename, "w"); // Creating output file
    if (output_file == NULL) {
        close(sockfd);
        exit(1);
    }

    int header_bytes = (header_end + 4) - header_buffer;
    int preread_bytes = header_length - header_bytes;
    if (preread_bytes > 0) {
        int written = 0;
        while (written < preread_bytes) {
            int n = fwrite(header_buffer + header_bytes + written, 1, preread_bytes - written, output_file);
            if (n <= 0) 
            { 
                fclose(output_file); 
                close(sockfd); 
                exit(1); 
            }
            written += n;
        }
    }
    long remaining = content_length - preread_bytes;

    while (remaining > 0) {
        bytes_read = read(sockfd, buffer, sizeof(buffer));
        if (bytes_read <= 0)
        {
             break;
        }
        if (bytes_read > remaining) 
        {
            bytes_read = remaining;
        }

        int written = 0;
        while (written < bytes_read) {
            int n = fwrite(buffer + written, 1, bytes_read - written, output_file);
            if (n <= 0) 
            { 
                fclose(output_file); 
                close(sockfd); 
                exit(1); 
            }
            written += n;
        }

        remaining -= bytes_read;
    }

    fclose(output_file);
    close(sockfd);
}