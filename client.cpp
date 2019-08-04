#include "main.h"

// client_chat()            More advanced message handling
void client_chat(FILE *fp, int sockfd) {
    // set up variables
    int maxfdp1, stdineof;                                      // max file descriptor +1
    fd_set rset, master;                                        // descriptor set to hold read descriptor bits
    char    sendline[MAXLINE], recvline[MAXLINE];
    int n = 0;

    // file descriptor/select() prep
    FD_ZERO(&rset);                                             // zero read descriptor set structures
    FD_ZERO(&master);
    FD_SET(sockfd, &master);
    FD_SET(fileno(fp), &master);
    maxfdp1 = std::max(fileno(fp), sockfd);


    for ( ; ; ) {                                               // infinite loop for select() calls
        rset = master;                                          // reset struct to original (select calls alter set)
        select(maxfdp1 + 1, &rset, NULL, NULL, NULL);           // select()
        if(FD_ISSET(sockfd, &rset)) {                           // socket readable
            if((n = read(sockfd, recvline, MAXLINE)) == 0) {    // read line off socket
                throw std::invalid_argument("server disconnect error...");
            }
            recvline[n] = '\0';                                 // NULL terminate c-string
            std::cout << "CONNECTED:\t" << recvline;            // print received line
        }
        if(FD_ISSET(fileno(fp), &rset)) {                       // input readable
            fgets(sendline, MAXLINE-1, stdin);                  // input if '\n' or EOF. \0 terminate + check buff sz
            if(strstr(sendline, "//SND_FILE") != NULL) {        // check if input contains file send prompt
                std::cout << "Input full path of file to send: " << std::endl;
                std::string file;                               // get file to send
                std::cin >> file;
                std::cout << "\nfile being sent.  Please wait...\n";
                send_file(file, sockfd);    // specify file to send
                std::cout << "\nfile sent!\n";

                char term_not[12] = "//file end\0";
                Write( sockfd, term_not, 12 );
            }
            else {
                Write(sockfd, sendline, strlen(sendline));      // write input to socket
            }
        }
    }   // end infinite loop
}   // END client_chat()



// client()         Deals with any server (1:1 or group) the same
void client(int port_input, std::string ip_input) {
    // declare variables
    int sockfd;
    struct sockaddr_in servaddr;

    // fill struct
    sockfd = Socket(AF_INET, SOCK_STREAM, 0);
    bzero(&servaddr, sizeof(servaddr));
    servaddr.sin_family = AF_INET;
    servaddr.sin_port = htons(port_input);
    Inet_pton(AF_INET, ip_input.c_str(), &servaddr.sin_addr);

    // connect to server
    std::cout << "finding connection...\n";
    Connect(sockfd, (struct sockaddr*) &servaddr, sizeof(servaddr));

    // interact with server
    std::cout << "connected...\n";
    client_chat(stdin, sockfd);     // stdin must be used since is FILE*
}   // END client()