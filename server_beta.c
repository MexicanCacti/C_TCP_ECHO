/*
    Create socket -> bind socket -> listen -> accept

    int socket(int domain, int type, int protocol)
        domain -> Communication protocol family for comms, AF_INET for IPv4
        type -> comm semantics, SOCK_STREAM (sequenced, reliable, two-way, connection-based byte stream)
        protocol -> particular protocol socket type within given protocol family, protocl can be specified as 0 if only singly protocl exists to supp socket type

        for SOCK_STREAM, once connected data transf using read & write calls... or some varient of send and recv calls
        can close with a close call

        Sucessful ret: fd returned, error: -1 & errno set

    int bind(int sockfd, const struct sockaddr * addr, socklen_t addrlen)
        Binds a name to the socket
        bind assigns address specified by addr to socket referred to by fd sockfd, addrlen specifies size of address strcuct pointed to by addr
        Must bind before accept
        General structure of sockaddr:
        struct sockaddr
        {
            sa_family_t sa_family
            char        sa_data[14];
        }

        struct sockaddr_in          serverConnectionInfo;
        Success ret: 0, error: -1 & errno set


    int listen(int sockfd, int backlog);
        Listens for connections on a socket
        Marks the socket ref by sockfd as passive socket, will be used to accept incoming connection requests w/ accept
        backlog defines max length which queue of pending connections for sockfd may grow
        Success ret: 0, error: -1 & errno set

    int accept(int sockfd, struct sockaddr *_Nullable restrict addr, socklen_t *_Nullable restrict addrlen);
        Accepts a connection on a socket, used with connection-based socket types like SOCK_STREAM and SOCK_SEQPACKET
        Extracts connection request on queue of pending connections for listening socket, sockfd
        Creates a new connected socket & returns a new file descriptor referring to that socket
        Newly created socket is NOT in the listening state, original socket sockfd unaffected by call

        addr -> pointer to sockaddr structure, filled in w/ address of peer socket, can be NULL
        addrlen -> caller must init to contain size (in bytes) of structure pointed to by addr, on ret will contain actual size of peer address
        If no pending connections, appect blocks caller until a connection is present
        To notify of incoming connections, can use select, poll, or epoll. Could also set socket to deliver SIGIO when activity occurs on a socket

        Sucess: ret fd for accepted socket, error: -1 , errno set, addrlen unchanged

    int connect(int sockfd, const struct sockaddr *addr, socklen_t addrlen)
        Connects socket ref by sockfd to addr specified by addr. addrlen specifeis size of addr
        
        Success: ret 0, error: -1 & errno set

    ssize_t send(int sockfd, const void buf[.size], size_t size, int flags);
        Sends message on a socket
        Send can only be used when socket is in connected state.
        sockfd is fd of sending socket
        msg is found in buf & has size

        If msg doesn't fit in send buffer, send normally blocks unless in nonblocking I/O mode.

        Success: ret # of bytes send, error: -1 & errno setq

    ssize_t recv(int sockfd, void buf[.size], size_t size);
        Receives msg from socket, if no msgs avaialbe, waits for msg to arrive unless socket is nonblocking, in which case -1 ret & errno set
        Success: ret size of msg, error: -1 & errno set

    int getaddrinfo(const char *restrict node,      // e.g. "www.example.com" or IP
                    const char *restrict service,   // e.g. "http" or port number
                    const struct addrinfo *restrict hints,     // point to struct addrinfo filled with relevent info
                    struct addrinfo **restrict res);
        
        Given node & service, which identify internet host & service, returns 1+ addrinfo structres. Each contain internet address which can be used in bind or connect
        Combines gethostbyname & getservbyname into 1 single interface
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

        so init the addrinfo skeleton, call getaddrinfo, use the ai_addr to init socket... can do this instead of manual init!
        

    int setsockopt(int sockfd, int level, int optnname, const void optval[.optlen], socklen_t optlen);
        Manifpulate options for the socket referred to by the file descriptor sockfd

        Success: 0, error: -1 & errno set


        Useful functions:
        h - host  to - to  n - network  s - short   l - long
        
        htons()
        h ost to n etwork s hort
        htonl()
        h ost to n etwork l ong
        ntohs()
        n etwork to h ost s hort
        ntohl()
        n etwork to h ost l ong
*/

#include <netdb.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>
#include <errno.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#define BUF_SIZE 1024


/*
    Input:
        argv[1] = IPv4 Addr
        argv[2] = Port

*/


int main(int argc, char** argv)
{
    int                         sfd, s;
    char                        buf[BUF_SIZE];
    char                        addr[INET_ADDRSTRLEN];
    ssize_t                     nread;
    socklen_t                   peer_addrlen;
    struct addrinfo             *hints;
    struct addrinfo             *result, *rp;
    struct sockaddr_storage     peer_addr;

    int socketFD, returnValue, errorCheck = 0;
    struct sockaddr_in          serverConnectionInfo;

    char* throwaway;

    if(argc < 3)
    {
        fprintf(stderr, "Invalid Args, format like so...\n <program name> <ip address> <port>");
        return -1;
    }

    errorCheck = inet_pton(AF_INET, argv[1], &serverConnectionInfo.sin_addr.s_addr);
    if(errorCheck <= 0)
    {
        fprintf(stderr, "Error converting ip address\n");
        return -1;
    }

    // inet_aton(argv[1], (struct in_addr *)&serverConnectionInfo.sin_addr.s_addr); this is an old way of doing things, doesn't work with ipv6

    //throwaway = inet_ntop(AF_INET, &serverConnectionInfo.sin_addr.s_addr, addr, ADDR_INFO_SIZE); //inet_ntop ... network to presentation
    throwaway = inet_ntop(AF_INET, &serverConnectionInfo.sin_addr.s_addr, addr, INET_ADDRSTRLEN);   // INET_ADDRSTRLEN is a macro to hold size of largest IPV4 string
    if(throwaway == NULL)
    {
        fprintf(stderr, "Error parsing ip address\n");
        return -1;
    }

    serverConnectionInfo.sin_family = AF_INET;

    // implicitly converts to unsigned short
    serverConnectionInfo.sin_port = atoi(argv[2]);

    if(serverConnectionInfo.sin_port <= 0)
    {
        fprintf(stderr, "Error parsing port\n");
        return -1;
    }

    /*
        BIG NOTE
            instead of the manual stepped way like above could just do the following...
            errorCheck = getaddrinfo(argv[1], argv[2], NULL, &serverConnectionInfo); // NULL could be replaced with a DIFFERENT addrinfo struct semi filled out

            The NULL hints would look like the following for the TCP...
                hints.ai_family = AF_UNSPEC; // use IPv4 or IPv6, whichever
                hints.ai_socktype = SOCK_STREAM;
                hints.ai_flags = AI_PASSIVE; // fill in my IP for me, if want manual, drop AI_PASSIVE & put fill in the struct received in getaddrinfo

            Should then do some error checking & walk through the ai_next linked list until getting a valid entry, then use that.
    
    */

    printf("Starting server on Addr {%s} and Port {%u}...\n", throwaway, serverConnectionInfo.sin_port);


    // Create a socket

    /*
        BIG NOTE
            Could instead just do
                socketFD = socket(serverConnectionInfo->ai_family, serverConnectionInfo->ai_socktype, serverConnectionInfo->ai_protocol);
    
    */
    socketFD = socket(AF_INET, SOCK_STREAM, 0);
    
    if(socketFD < 0)
    {
        returnValue = errno;
        perror("Socket creation failed");
        goto cleanup;
    }

    // bind to address

    //errorCheck = bind(socketFD, (struct sockaddr *)&serverConnectionInfo, sizeof(serverConnectionInfo));
    errorCheck = bind(socketFD, serverConnectionInfo.ai_addr, serverConnectionInfo.ai_addrlen);
    if(errorCheck == -1)
    {
        returnValue = errno;
        perror("Socket binding failed");
        goto cleanup;
    }

    // listen in a loop


    // get connection -> read socket buffer
    
    // echo socket buffer



    // clean up
    cleanup:
    errorCheck = close(socketFD);
    if(errorCheck != 0)
    {
        returnValue = errno;
        perror("Error closing server socket");
    }

    printf("Closing successful!\n");
    return returnValue;
}