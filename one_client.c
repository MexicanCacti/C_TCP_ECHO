#define _GNU_SOURCE

#include <sys/types.h>
#include <sys/socket.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <errno.h>
#include <netdb.h>


int main(int argc, char** argv)
{
    int status, returnCode;
    struct addrinfo hints;
    struct addrinfo *serverInfo;
    char ipstr[INET_ADDRSTRLEN];

    memset(&hints, 0, sizeof hints);    // ensure empty
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_STREAM;


    /*
        gai_strerror translates networking error codes to human readable string

        int getaddrinfo(const char *node,           // e.g. "www.example.com" or IP
                        const char *service,        // e.g. "http" or port number
                        const struct addrinfo *hints,   // point to struct addrinfo filled with relevent info
                        struct addrinfo **res);         // point to struct to hold results of getaddrinfo

        addrinfo structre contains the following:
        struct addrinfo {
               int              ai_flags;       // AI_PASSIVE, AI_CANNONNAME
               int              ai_family;      // desired addr family for returned address... like AF_INET, AF_INET6, AP_UNSPEC
               int              ai_socktype;    // preferred socket type ... like SOCK_STREAM or SOCK_DGRAM
               int              ai_protocol;    // 0 for any
               size_t           ai_addrlen;     // size of ai_addr in bytes
               struct sockaddr *ai_addr;        // struct addr_in or _in6
               char            *ai_canonname;   // full canonical hostname
               struct addrinfo *ai_next;        // linked list, next node 
        };                


        NOTE: Can cast btwn sockaddr & sockaddr_in ... both ways!

        struct sockaddr {
            unsigned short  sa_family,          // address_family, AF_xxx
            char            sa_data[14]         // 14 bytes of protocol address
        };

        struct sockaddr_in {
            short int sin_family;               // Address family, AF_INET
            unsigned short int sin_port;        // Port number
            struct in_addr sin_addr;            // Internet address
            unsigned char sin_zero[8];           // Same size as struct sockaddr
        };

        struct in_addr {
            uint32_t s_addr;                    // that's a 32-bit int (4 bytes)
        };

        Remember to call freeaddinfo when done!
    */
    if( (status = getaddrinfo("127.0.0.1", "4567", &hints, &serverInfo)) != 0) // Note if we wanted to connect to a specific IP, replace NULL with "IP.Goes.Here.!"
    {
        fprintf(stderr, "gai error: %s\n", gai_strerror(status)); 
        exit(1);
    }

    // Convet IP to string
    void* addr = NULL;
    struct sockaddr_in *ipv4;
    ipv4 = (struct sockaddr_in *)serverInfo->ai_addr;

    //inet_ntop(p->ai_family, addr, ipstr, sizeof ipstr);

    /*
        const char *inet_ntop(  int af, const void *restrict src, char dst[restrict .size], socklen_t size);
    */
    inet_ntop(serverInfo->ai_family, &(ipv4->sin_addr), ipstr, sizeof ipstr);

    //inet_ntop(serverInfo->ai_family, &serverInfo->ai_addr.s_addr, ipstr, sizeof ipstr);
    printf("    %s: %s\n", "IPV4", ipstr);

    int serverFD = socket(serverInfo->ai_family, serverInfo->ai_socktype, serverInfo->ai_protocol);
    if(serverFD < 0)
    {
        returnCode = errno;
        perror("socket creating error");
        goto cleanup;
    }

    status = connect(serverFD, serverInfo->ai_addr, serverInfo->ai_addrlen);
    if(status == -1)
    {
        returnCode = errno;
        perror("connect error");
        goto cleanup;
    }

    char readBuffer[1024];
    int bytes_recv = recv(serverFD, &readBuffer, sizeof(readBuffer) - 1, 0);
    if(bytes_recv < 0)
    {
        returnCode = errno;
        perror("Error recv msg");
        close(serverFD);
        goto cleanup;
    }

    readBuffer[bytes_recv] = '\0';

    printf("Received %d bytes: %s\n", bytes_recv, readBuffer);

    char* msg = "Hello Server!";
    int len, bytes_sent;
    len = strlen(msg);
    bytes_sent = send(serverFD, msg, len, 0);
    if(bytes_sent < 0)
    {
        returnCode = errno;
        perror("Error sending msg");
        close(serverFD);
        goto cleanup;
    }

    cleanup:
    shutdown(serverFD, SHUT_RDWR);
    close(serverFD);
    freeaddrinfo(serverInfo); // free the linked-list
    return returnCode;
}