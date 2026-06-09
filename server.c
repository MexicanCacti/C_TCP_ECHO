#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <arpa/inet.h>
#include <errno.h>

#define BACKLOG 10
#define LISTEN_PORT "3490"

int main(int argc, char** argv)
{
    int status, socketFD, returnCode = 0;
    struct addrinfo hints;
    struct addrinfo *serverSetupInfo;
    struct sockaddr_in* ipInfo;
    char ipstr[INET_ADDRSTRLEN];

    memset(&hints, 0 , sizeof hints);
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_STREAM;  
    hints.ai_flags = AI_PASSIVE;    // fill in ip for me, so listening on host's IP addr

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
    
    if( (status = getaddrinfo(NULL, LISTEN_PORT, &hints, &serverSetupInfo) ) != 0)
    {
        fprintf(stderr, "gai error: %s\n", gai_strerror(status));
        goto cleanup;
    }

    // const char *inet_ntop(int af, const void* restrict src, char dst[restrict .size], socketlen_t size);
    ipInfo = (struct sockaddr_in *)serverSetupInfo->ai_addr;
    if( inet_ntop(serverSetupInfo->ai_family, &(ipInfo->sin_addr), ipstr, INET_ADDRSTRLEN) == NULL)
    {
        perror("ipv4 conversion error");
        returnCode = 1;
        goto cleanup;
    }
    
    // int socket(int domain, int type, int protocol);

    socketFD = socket(serverSetupInfo->ai_family, serverSetupInfo->ai_socktype, serverSetupInfo->ai_protocol);
    if(socketFD < 0)
    {
        returnCode = errno;
        perror("socket creating error");
        goto cleanup;
    }
    
    // int bind(int sockfd, struct sockaddr* myaddr, int addrlen);
    /* NOTE: if address is in use, can reuse like so: 
            int yes=1;
            setsockopt(listener, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof yes);

        Also if you don't care what local port it is, can just call connect() & will check if socket is unbound and bind() to unused port
    */
    status = bind(socketFD, serverSetupInfo->ai_addr, serverSetupInfo->ai_addrlen);

    if(status != 0)
    {
        returnCode = errno;
        perror("socket binding error");
        goto cleanup;
    }
    
    printf("Server listening on IP: %s and Port: %hu\n", ipstr, ntohs(ipInfo->sin_port));

    /*
        int listen(int sockfd, int backlog);
            backlog: # of connections allowed on incoming queue, basically connections wait in queue until it accept.
    */

    status = listen(socketFD, BACKLOG);
    if(status == -1)
    {
        returnCode = errno;
        perror("listen error");
        goto cleanup;
    }

    /*
        int accept(int sockfd, struct sockaddr *addr, socklen_t *addrlen);
            Accept call returns a NEW socket file descriptor for THIS SINGLE connection. OG one is still listening for connections

            addr -> pointer to local struct where info about incoming connections will go
            addrlen -> local int var set to sizeof(struct sockaddr_storage)... will place <= sizeof bytes. will change value of addrlen to reflect that
    */


    struct sockaddr_storage connectionAddr;

    // So the portion below would be called in a loop, listen only needs to be called once!
    socklen_t addr_size;

    addr_size = sizeof connectionAddr;

    int connectionFD = accept(socketFD, (struct sockaddr *addr) &connectionAddr, &addr_size);

    // can close listen if no other connections will be used!

    // send & recv are BLOCKING calls, so program will stop until something received or until all sent

    /* int send(int sockfd, const void* msg, int len, int flags)

        returns # of bytes sent... so just like read/write same I/O rules apply
    */

    char* msg = "Connected :)";
    int len, bytes_sent;

    len = strlen(msg);
    bytes_sent = send(connectionFD, &msg, len, 0);

    if(bytes_sent < 0)
    {
        returnCode = errno;
        perror("Error sending msg");
        close(connectionFD);
        goto cleanup;
    }

    /*  int recv(int sockfd, void *buf, int len, int flags);

            Returns # of bytes read into the buffer
    */


cleanup:
    /*
        close(sockFD); ... prevents write/reads to socket... anyone attempting will receive an error

        Could also do...
            int shutdown(int sockfd, int how);
            how
            0   -   Further receives are disallowed
            1   -   Futher sends are disallowed
            2   -   Further sends & receives are disallowed [ THIS IS CLOSE!! ]

            Ret 0 on success, 1 on error... shutdown doesn't close file descriptor!!!   Just changes its usability
    
    */
    close(socketFD);
    freeaddrinfo(serverSetupInfo);
    exit(returnCode);
}