#include "main.h"

/*    wrapper functions   */
// Socket
int Socket (int family, int type, int protocol) {
    int n;
    if( (n = socket(family, type, protocol)) < 0)
        throw std::invalid_argument( "socket error");
    return n;
}   // END socket()



// Bind
void Bind(int sockfd, const struct sockaddr *addr, socklen_t addrlen) {
    if ( ::bind( sockfd, addr, addrlen) < 0 )
        throw std::invalid_argument( "bind error" );
}   // END Bind()



// Listen
void Listen(int listenfd, int backlog) {
    if ( listen(listenfd, backlog) < 0 )
        throw std::invalid_argument( "listen error" );
}   // END Listen()



// Accept
int Accept (int sockfd, struct sockaddr *addr, socklen_t *addrlen) {
    int n;
    if( (n = accept(sockfd, addr, addrlen)) < 0)
        throw std::invalid_argument( "socket error");
    return n;
}   // END Accept()



// Write
int Write ( int fd, const void *buf, size_t count) {
    int n;
    if( (n = write(fd, buf, count)) < 0) {
        throw std::invalid_argument( "socket error");
    }
    return n;
}   // END Write()



// Connect
void Connect(int sockfd, struct sockaddr* address, int addr_len) {
    if (connect(sockfd, address, addr_len) < 0)
        throw std::invalid_argument("connect error");
}   // END Connect()



// Inet_pton
void Inet_pton(int family, const char* strptr, void* addrptr ) {
    if (inet_pton(family, strptr, addrptr) <= 0)
        throw std::invalid_argument("inet_pton error");
}   // END Inet_pton()



// Select
void Select(int numfds, fd_set* readfds, fd_set* writefds, fd_set* exceptfds, struct timeval* timeout) {
    if (select(numfds, readfds, writefds, exceptfds, timeout) == -1) {
        std::cout << errno << std::endl;
        throw std::invalid_argument("select error");
    }
}   // END Select()



// send_file
void send_file(std::string file_path, int sockfd) {
    // declare variables
    char* store_buffer, *out_buffer;
    int start_sub, end_sub;
    uint32_t length = 0;

    // open file
    std::ifstream source_file;
    source_file.open(file_path.c_str());

    // put source file into memory buffer
    source_file.seekg(0, std::ios::end);                    // go to the end of file
    length = source_file.tellg();                           // report location (this is the length)
    source_file.seekg(0, std::ios::beg);                    // go back to the beginning
    store_buffer = new char[length];                        // allocate memory for a buffer of appropriate dimension
    source_file.read(store_buffer, length);                 // read the whole file into the buffer
    source_file.close();                                    // close file handle

    // send prep notifications (notification then num whole buffers)
    char prep_message[12] = "//PRE_FILE\0";
    Write( sockfd, prep_message, sizeof(prep_message) );    // output file prep to socket

    // alert to num bytes to expect
    std::string s = std::to_string(length);
    const char *len_data = s.c_str();
//    std::cout << "amt. data to send: " << len_data << std::endl;    // debug output
    uint32_t len_out = htonl(length);                       // set to network byte order
    Write(sockfd, &len_out, sizeof(len_out));               // output to socket


    // calculate number of buffers to fill
    int whole = length / (MAXLINE-1);                       // num full buffers to fill. leave room for null termination
    int partial = length % (MAXLINE-1);                     // final bits to send
//    std::cout << "whole buffers: " << whole << std::endl;   // debug output
//    std::cout << "partial buffers: " << partial << std::endl;   // debug output

    // determine subscripts to read from store_buffer
    start_sub = 0;
    if(whole >= 1) {
        end_sub = MAXLINE-1;
    }
    else {
        end_sub = length;
    }

    // TODO accommodate partial sends
    // fill buffers and output to socket
    out_buffer = new char[MAXLINE];                         // allocate memory
    for(int i = 0; i < whole; i++) {                        // full buffer output
        for(int j = 0; j < MAXLINE; j++) {                  // fill output buffer (note < not <=)
            out_buffer[j] = store_buffer[start_sub+j];      // start_sub+j avoids resending portions of file
        }
        out_buffer[MAXLINE] = '\0';                         // null terminate transmission

        // debug output
//        std::cout << "\nwhole" << i <<": \n";
//        for( int y = 0; y < MAXLINE; y++) {
//            std::cout <<  out_buffer[y];
//        }
//        std::cout << '\n';

        Write(sockfd, out_buffer, MAXLINE);                 // output to socket
        start_sub = end_sub + 1;                            // reset reading subscripts for next buffer
        end_sub += MAXLINE;
    }

//    std::cout << "\npartial: \n";                         // debug output
    for(int x = 0; x < partial; x++) {                      // send partial buffer ( < not <= )
        out_buffer[x] = store_buffer[start_sub+x];          // start_sub+j avoids resending portions of file
    }
    out_buffer[partial] = '\0';                             // null terminate buffer

//    // debug output
//    for( int y = 0; y < partial; y++) {
//        std::cout <<  out_buffer[y];
//    }

    Write(sockfd, out_buffer, partial);                     // size(out_buffer) takes size of element 0 of array
    delete[] out_buffer;                                    // deallocate memory

    // termination notification
    char term_not[12] = "//SND_TERM\0";
    Write( sockfd, term_not, 12 );                          // send file termination notification
}   // END send_file()



// file_recv_handling()
void file_recv_handling(int sockfd, char* recvline, std::string filename, uint32_t num_bytes, bool get_bits) {
    // declare variables
    int whole, partial, n = 0;
    uint32_t exp_data, exp_data_net;                        // permits sending of up to 4GB file size
    bool end_process = false;

    // open file for writing
    std::ofstream file;
    file.open(filename);

    // determine expected number of bits
    if( get_bits ) {                                        // if num_bits not set, read expected bits from socket
        if ( read(sockfd, &exp_data_net, sizeof(exp_data_net)) < 0)
            throw std::invalid_argument("client disconnect error...");
        exp_data = ntohl(exp_data_net);                     // adjust to host byte order
    }
    else {                                                  // if num_bits has been set
        exp_data = num_bytes;
    }

    // parse num bytes into num full buffers
    whole = exp_data/(MAXLINE-1);                           // num whole buffers
//    std::cout << "whole buffers: " << whole << std::endl;   // debug output
    if( whole > 0) {
        partial = exp_data % (MAXLINE-1);   // num of bits in partial buffer
    }
    else {
        partial = exp_data;
    }
//    std::cout << "partial buffers: " << partial << std::endl;   // debug output


    // read full buffers off socket and output to file
    std::cout << "File received: \n" << std::endl;
    for(int i = 0; i < whole; i++) {                        // read each full buffer
        std::cout << "\nwhole" << i << ": \n";
        if ((n = read(sockfd, recvline, MAXLINE)) < 0)
            throw std::invalid_argument("client disconnect error...");
        recvline[MAXLINE] = '\0';                           // null terminate
        file << recvline;                                   // output to file
        std::cout << recvline;

        if (strstr(recvline, "//SND_TERM") != NULL ) {  // deal with network issues. Search for file end transmission
            std::cout << "ERROR: network issues.  end_process set to true...\n";    // TODO debug-ish
            end_process = true;     // network issues.  Whole buffers should not contain "//SND_TERM"
            break;                  // avoid long loops over this area due to incorrect socket I/O
        }
    }
    if(!end_process) {                                      // if //SND_TERM not found yet...
        if ((n = read(sockfd, recvline, partial-1)) < 0)    // read final partial buffer from socket
            throw std::invalid_argument("client disconnect error...");
        file << recvline;                                   // output to file
        std::cout << recvline << std::endl;
    }


    // TODO debug output
    std::cout << "expected num bits: " << exp_data << std::endl;

    file.close();                                           // close file
    if (strstr(recvline, "//SND_TERM") != NULL ) {          // only output to socket if file transmission is completed
        std::cout << "\nmessage sending re-enabled...\n" << std::endl;
    }
}   // END file_recv_handling()



// find_pthread()
bool find_pthread(pthread_t* src, int size, pthread_t search_val ) {
    bool found = false;
    for(int i = 0; i < size; i++) {
        if(src[i] == search_val) {
            found = true;
            break;
        }
    }
    return found;
}   // END find_pthread()



// print_machine_ip()
void print_machine_ip() {
    char hostbuffer[256];
    char *IPbuffer;
    struct hostent *host_entry;
    int hostname;

    // retrieve hostname
    hostname = gethostname(hostbuffer, sizeof(hostbuffer));
    if( hostname < 0 )
        throw std::invalid_argument("gethostname error");

    // retrieve host information
    host_entry = gethostbyname(hostbuffer);
    if( host_entry == NULL )
        throw std::invalid_argument("gethostbyname error");

    // convert an Internet network address into ASCII string
    IPbuffer = inet_ntoa( *((struct in_addr*) host_entry->h_addr_list[0]) );
    if( IPbuffer == NULL )
        throw std::invalid_argument("inet_ntoa error");

    std::cout << "Host IP: " << IPbuffer << std::endl;
}   // END print_machine_ip()



// input_filtering()
void input_filtering(char* recvline, char* overread, uint32_t* num_bytes, bool* expect_file, bool* get_bits, int n) {
    if (strstr(recvline, "//PRE_FILE") != NULL) {   // scan recvline for file prep message
        std::cout << "\nPrepare for file reception.  Message sending disabled...\n";
        if(n > 12) {    // check number of bits read above.  If > 12, then have read 2 messages
            // rearrange and cast to extract get num bits to read
            overread[0] = recvline[12];
            overread[1] = recvline[13];
            overread[2] = recvline[14];
            overread[3] = recvline[15];
            int* extra = (int*) overread;           // know next 2 bytes are ints from file protocol set with program
            *num_bytes = ntohl(*extra);             // convert to host byte order
            *get_bits = false;                      // do not read 2 bytes in file_recv_handling
        }
        *expect_file = true;                        // file prep received. file expected
    }
    else if (strstr(recvline, "//SND_TERM") != NULL) {   // scan recvline for file prep message
        std::cout << "\nFile received. Message sending re-enabled...\n" << std::endl;
    }
    else {                                          // pre-file message not found (normal messaging/operation)
        std::cout << "CONNECTED:\t" << recvline << std::endl;       // print received line
    }
}   // END input_filtering()