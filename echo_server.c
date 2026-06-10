#define _GNU_SOURCE

#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>
#include <errno.h>
#include <arpa/inet.h>
#include <string.h>
#include <unistd.h>
#include <sys/wait.h>
#include <signal.h>
#include <netinet/in.h>

#define BACKLOG 10
#define LISTEN_PORT "4567"
#define LISTEN_IP "127.0.0.1"

void sigchld_handler(int s)
{
    (void)s;

    int saved_errno = errno;

    while(waitpid(-1, NULL, WNOHANG) > 0);

    errno = saved_errno;
}

int get_readable_ip(struct addrinfo** addrInfo, struct sockaddr_in** ipInfo, char* ipStringBuffer, socklen_t len);

int create_and_bind_socket(struct addrinfo** addrInfo, int* socketFD);

int main(int argc, char** argv)
{
    int status, returnCode = 0;
    int socketFD = -1;
    
    struct addrinfo     hints;
    struct addrinfo     *serverInfo = NULL;
    struct sockaddr_in  *ipInfo = NULL;

    char                ipstr[INET_ADDRSTRLEN];

    memset(&hints, 0, sizeof hints);
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_STREAM;
    //hints.ai_flags = AI_PASSIVE;    // fill in IP for me

    if ( (status = getaddrinfo(LISTEN_IP, LISTEN_PORT, &hints, &serverInfo) ) != 0)
    {
        returnCode = status;
        fprintf(stderr, "gai error: %s\n", gai_strerror(status));
        goto free;
    }

    // Convert & store IP address for display after setup
    returnCode = get_readable_ip(&serverInfo, &ipInfo, ipstr, sizeof ipstr);
    if(returnCode != 0) goto free;

    returnCode = create_and_bind_socket(&serverInfo, &socketFD);
    if(returnCode != 0) goto close;

    status = listen(socketFD, BACKLOG);
    if(status == -1)
    {
        returnCode = errno;
        perror("listen error");
        goto close;
    }

    printf("Server listening on IP: %s and Port: %hu\n", ipstr, ntohs(ipInfo->sin_port));

    // Loop and accept
    int connectionFD;
    socklen_t addr_size;
    struct sockaddr_storage connection_addr;
    struct sigaction sa;

    sa.sa_handler = sigchld_handler;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = SA_RESTART;

    if(sigaction(SIGCHLD, &sa, NULL) == -1)
    {
        returnCode = errno;
        perror("sigaction");
        goto close;
    }

    while(1)
    {
        addr_size = sizeof connection_addr;
        connectionFD = accept(socketFD, (struct sockaddr *)&connection_addr, &addr_size);
        if(connectionFD == -1)
        {
            perror("accept");
            continue;
        }


        // Alternative but easier way of doing this
        char connectionIP[INET_ADDRSTRLEN];
        char connectionPort[NI_MAXSERV];

        status = getnameinfo( (struct sockaddr *)&connection_addr, addr_size, connectionIP, sizeof(connectionIP), connectionPort, sizeof(connectionPort), NI_NUMERICHOST | NI_NUMERICSERV); // Flag return numeric vals

        if(status != 0) continue;

        printf("Connection from IP: %s and Port: %s\n", connectionIP, connectionPort);

        // Fork child process to read then echo back until connection is over
        if(!fork())
        {
            close(socketFD); // child doesn't need listener
            ssize_t recv_bytes = 0;
            char readBuffer[1024];

            do
            {
                recv_bytes = recv(connectionFD, readBuffer, sizeof(readBuffer) - 1, 0);
                if(recv_bytes < 0)
                {
                    perror("read");
                    goto cleanthread;
                }
                readBuffer[recv_bytes] = '\0';

                printf("[%s]: %s\n", connectionIP, readBuffer);
                ssize_t writtenBytes = 0;
                while(writtenBytes < recv_bytes)
                {
                    ssize_t bytes = send(connectionFD, readBuffer + writtenBytes, recv_bytes - writtenBytes, 0);
                    if(bytes < 0)
                    {
                        perror("send");
                        goto cleanthread;
                    }
                    writtenBytes += bytes;
                }
                
            } while (recv_bytes != 0);

            cleanthread:
            close(connectionFD);
            exit(0);
        }

        close(connectionFD);    // parent doesn't need this
    }

close:
    close(socketFD);

free:
    freeaddrinfo(serverInfo);

    return returnCode;
}

int get_readable_ip(struct addrinfo** addrInfo, struct sockaddr_in** ipInfo, char* ipStringBuffer, socklen_t len)
{
    int returnVal = 0;
    *ipInfo = (struct sockaddr_in *)(*addrInfo)->ai_addr;
    if(inet_ntop((*addrInfo)->ai_family, &((*ipInfo)->sin_addr), ipStringBuffer, len) == NULL)
    {
        returnVal = errno;
        perror("ipv4 conversion error");
    }

    return returnVal;
}

int create_and_bind_socket(struct addrinfo** addrInfo, int* socketFD)
{
    int returnVal = 0;

    *socketFD = socket((*addrInfo)->ai_family, (*addrInfo)->ai_socktype, (*addrInfo)->ai_protocol);
    if(*socketFD == -1)
    {
        returnVal = errno;
        perror("socket creation error");
        goto cleanCreate;
    }

    int yes = 1;
    setsockopt(*socketFD, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(int));

    returnVal = bind(*socketFD, (*addrInfo)->ai_addr, (*addrInfo)->ai_addrlen);
    if(returnVal != 0)
    {
        perror("socket binding error");
    }

cleanCreate:
    return returnVal;
}