#include "main.h"


// server_chat()        More advanced client message handling
void server_chat(int sockfd) {
    // general set up
    char sendline[MAXLINE], recvline[MAXLINE], overread[BYTES_IN_LONG];
    uint32_t num_bytes = 0;
    bool get_bits = true;
    bool expect_file = false;

    // file descriptor/select() prep
    struct timeval tv;
    fd_set readfd, master;
    int maxfd;                                                  // record max fd
    FD_ZERO(&master);                                           // zero set structures
    FD_ZERO(&readfd);
    FD_SET(sockfd, &master);                                    // add sockfd to master set
    FD_SET(STDIN, &master);                                     // add standard input to master set
    maxfd = sockfd;                                             // sockfd > 0 by definition

    std::cout << "input text...\n";                             // prompt user for input
    while (true) {                                              // infinite loop to wait for socket input or I/O
        readfd = master;                                        // copy master to readfd since select updates contents
        Select(maxfd+1, &readfd, NULL, NULL, NULL);             // select() call to update fd_set struct

        if(FD_ISSET(sockfd, &readfd)) {                         // sockfd is set...
            int n = 0;
            if(!expect_file) {                                  // not expecting a file
               if ((n = read(sockfd, recvline, MAXLINE)) == 0) // read from socket
                    throw std::invalid_argument("client disconnect error...");
                recvline[n] = '\0';                             // ensure null-termination
                input_filtering(recvline, overread, &num_bytes, &expect_file, &get_bits, n);    // check for keywords
            }
            if(expect_file) {                                   // file expected from client
                std::cout << "file expected...\n";
                file_recv_handling(sockfd, recvline, DEST_FILENAME, num_bytes, get_bits);   // handle file reception
                expect_file = false;                            // reset file expected flag
            }
        }
        if(FD_ISSET(STDIN, &readfd)) {                          // standard IO fd set...
            fgets(sendline, MAXLINE-1, stdin);                  // stop input if \n or EOF. \0 terminate + check buff sz
            Write(sockfd, sendline, strlen(sendline));          // write input to socket
        }
    }
}   // END server_chat()



// server()     Deals with one client only. 1:1 connections
void server(int port_input) {
    // declare variables
    int listenfd, connfd;
    socklen_t clilen;
    struct sockaddr_in servaddr, clientaddr;

    // fill struct
    listenfd = Socket(AF_INET, SOCK_STREAM, 0);
    bzero(&servaddr, sizeof(servaddr));
    servaddr.sin_family = AF_INET;
    servaddr.sin_port = htons(port_input);
    servaddr.sin_addr.s_addr = htonl(INADDR_ANY);

    // bind and listen to socket
    Bind(listenfd, (struct sockaddr*) &servaddr, sizeof(servaddr));
    Listen(listenfd, LISTENQ);
    std::cout << "waiting for connection...\n";

    // accept a single client connection
    clilen = sizeof(clientaddr);
    connfd = Accept(listenfd, (struct sockaddr*) &clientaddr, &clilen);

    // interact with client
    std::cout << "connected...\n";
    server_chat(connfd);
}   // END server()
