#include "main.h"

// client_chat()            More advanced message handling
void client_chat(FILE *fp, int sockfd) {
    // set up variables
    int maxfdp1, stdineof;                                              // max file descriptor +1
    fd_set rset, master;                                                // descriptor set to hold read descriptor bits
    char sendline[MAXLINE], recvline[MAXLINE], overread[BYTES_IN_LONG];
    uint32_t num_bytes = 0;
    bool get_bits = true, expect_file = false;
    int n = 0;
    uint32_t exp_data, *exp_data_net;

    // file descriptor/select() prep
    FD_ZERO(&rset);                                                 // zero read descriptor set structures
    FD_ZERO(&master);
    FD_SET(sockfd, &master);
    FD_SET(fileno(fp), &master);
    maxfdp1 = std::max(fileno(fp), sockfd);

    // parsing
    std::ofstream file;
    file.open(DEST_FILENAME);

    for ( ; ; ) {                                                       // infinite loop for select() calls
        rset = master;                                                  // reset struct to original (select calls alter set)
        select(maxfdp1 + 1, &rset, NULL, NULL, NULL);                   // select()
        if(FD_ISSET(sockfd, &rset)) {                               // socket readable
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
        if(FD_ISSET(fileno(fp), &rset)) {                           // input readable
            fgets(sendline, MAXLINE-1, stdin);              // input if '\n' or EOF found. \0 terminate + check buff sz
            if(strstr(sendline, "//SND_FILE") != NULL) {            // check if input contains file send prompt
                std::cout << "Input full path of file to send ('d' to use default source file): " << std::endl;
                std::string file;                                       // get file to send
                std::cin >> file;
                if(file == "d") {
                    file = SRC_FILENAME;
                }
                send_file(file, sockfd);                                // specify file to send
            }
            else {
                Write(sockfd, sendline, size_t(strlen(sendline)));      // write input to socket
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
    std::cin.ignore(1000, '\n');                                    // flush buffer before next round of input
    client_chat(stdin, sockfd);                                     // stdin must be used since is FILE*
}   // END client()
