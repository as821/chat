#ifndef MAIN_H
#define MAIN_H

// include statements
#include <sys/types.h>          // socket struct headers
#include <sys/socket.h>
#include <netdb.h>

#include <arpa/inet.h>          // internet headers
#include <netinet/in.h>
#include <unistd.h>

#include <stdexcept>            // additional headers
#include <iostream>
#include <stdio.h>
#include <stdint.h>
#include <string>
#include <sys/time.h>
#include <fstream>
#include <fcntl.h>
#include <pthread.h>
#include <vector>
#include <errno.h>


// constants
#define MAXLINE             4096            // max buffer length
#define BYTES_IN_LONG       4               // num bytes in a uint32_t
#define LISTENQ             1024            // size of listen queue
#define MAX_CONNECTION      100             // max number of connections
#define STDIN               0               // std input fd is 0
#define PROTOCOL_MSG_LEN    12              // protocol message length (arbitrary)
#define DEST_FILENAME   ""                  // default destination file
#define SRC_FILENAME    ""                  // default source file


extern int message_id_counter;              // must be global so Group_Message constructor has access



// Group_Message struct definition
struct Group_Message {
    char* msg;                              // message text
    size_t msg_len;
    pthread_t have_sent [MAX_CONNECTION];   // store thread ids of all threads that have sent message
    int num_sent;                           // have_sent size
    int message_id;                         // ID number
    bool msg_dynamic_alloc;                 // true if msg has been dynamically allocated (if needs to be deleted)

    Group_Message();                        // constructor (assign message_id and allocate for msg)
    ~Group_Message();                       // destructor
};



//  top-level functions
void server(int port_input);
void client(int port_input, std::string ip_input);
void group_serv(int port_input);

// file sending/reception
void send_file(std::string file_path, int sockfd);
void file_recv_handling(int sockfd, char* recvline, std::string filename, uint32_t num_bytes, bool get_bits);
void input_filtering(char* recvline, char* overread, uint32_t* num_bytes, bool* expect_file, bool* get_bits, int n);

// group messaging functions
void* group_handler(void* sockfd);
bool find_pthread(pthread_t* src, int size, pthread_t search_val );
Group_Message* message_id_find(int message_id, std::vector<Group_Message*>* message_stack_ptr, int* offset );
void input_filtering_group(char* recvline, char* overread, uint32_t* num_bytes, bool* expect_file, bool* get_bits, int n);
void file_recv_handling_group(int sockfd, char* recvline, std::string filename, uint32_t num_bytes, bool get_bits);

// system call wrappers (error checking)
int Socket (int family, int type, int protocol);
void Bind(int sockfd, const struct sockaddr *addr, socklen_t addrlen);
void Listen(int listenfd, int backlog);
int Accept (int sockfd, struct sockaddr *addr, socklen_t *addrlen);
int Write (int fd, const void *buf, size_t count);
void Connect(int sockfd, struct sockaddr* address, int addr_len);
void Inet_pton(int family, const char* strptr, void* addrptr );
void Select(int numfds, fd_set* readfds, fd_set* writefds, fd_set* exceptfds, struct timeval* timeout);

// other
void print_machine_ip();


#endif // MAIN_H
