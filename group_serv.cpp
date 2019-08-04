#include "main.h"


// necessary global variables to be accessible by all threads
std::vector<Group_Message*> message_stack;                  // empty vector of messages
std::vector<pthread_t> connection_stack;                    // empty vector of all thread ids
int message_id_counter;                                     // must be global so Group_Message constructor has access

// mutexes for altering num_connections, reading from message_stack, adding messages to
pthread_mutex_t connections_lock, message_lock/*, std_output_lock*/;



// Group_Message constructor
Group_Message::Group_Message() {
    if(++message_id_counter == INT_MAX) {                                      // wrap around max int value (2147483647)
        message_id_counter -= INT_MAX;
    }
    message_id = message_id_counter;
}   // END constructor



// group_serv()
void group_serv(int port_input) {
    // declare networking variables
    int listenfd, *connfd = NULL;
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
    std::cout << "waiting for connections...\n";
    std::cout << "listenfd: " << listenfd << std::endl;

    // set up for select() call
    fd_set readfd, master;
    int maxfd;                                              // record max fd
    FD_ZERO(&master);                                       // zero set structures
    FD_ZERO(&readfd);
    FD_SET(listenfd, &master);                              // add sockfd to master set
    maxfd = listenfd;                                       // sockfd > 0 by definition

    struct timeval tv;
    tv.tv_sec = 5;                                          // Clean message_list of old messages every 5 seconds
    tv.tv_usec = 0;

    // threading variables
    pthread_t tid;



    // initialize mutexes and check for errors
    if (pthread_mutex_init(&connections_lock, NULL) != 0) {
        throw std::invalid_argument("num_connections_lock error");
    }
    if (pthread_mutex_init(&message_lock, NULL) != 0) {
        throw std::invalid_argument("message_add_lock error");
    }



    for (;;) {  // infinite wait for client connections

        // select set up and reset
        readfd = master;                                    // copy master set to readfd set. Select updates contents
        Select(maxfd+1, &readfd, NULL, NULL, &tv);          // select() call to update fd_set struct
        if(FD_ISSET(listenfd, &readfd)) {
            // setup
            clilen = sizeof(clientaddr);

            // handle accept() interrupted by signal handling (EINTR)
            connfd = new int;                               // make reentrant through dynamic allocation
            if ( (*connfd = accept(listenfd, (struct sockaddr*) &clientaddr, &clilen)) < 0 ) {  // if accept error
                if (errno == EINTR)                         // if accept() interrupted by signal handling
                    continue;                               // restart infinite loop at next iteration
                else
                    throw std::invalid_argument("accept error (main)");
            }
            std::cout << "...NEW CONNECTION... connfd: " << *connfd << "\n";

            // make new thread for client
            if ( pthread_create(&tid, NULL, group_handler, (void*) connfd) != 0 )
                throw std::invalid_argument("pthread_create error...");

            // add thread id of ne connection to proper vector
            pthread_mutex_lock(&connections_lock);
            connection_stack.push_back(tid);
            pthread_mutex_unlock(&connections_lock);
        }

        // set up to remove old messages
        std::vector<int> all_sent_vector;
        int index = 0;
        bool all_sent = true;

        // clear messages that have been sent to all
        pthread_mutex_lock(&message_lock);
        for(Group_Message* message : message_stack) {       // loop through all messages
            for (pthread_t conn : connection_stack) {       // loop through all connections
                // if a connection has not been sent a message, move to next message
                if (!find_pthread(message->have_sent, message->num_sent, conn)) {
                    all_sent = false;                       // set flag
                    break;
                }
            }
            // push_back message ID of message all have read onto vector
            if (all_sent) {
                all_sent_vector.push_back(message->message_id);
            }
            all_sent = true;                                // reset for next message_stack iteration
            index++;                                        // increment index offset
        }

        // search algorithm to find/delete specific message (avoid issues with change in array size)
        int offset = 0;
        for(int i = (all_sent_vector.size() - 1); i >= 0; i--) {    // loop backwards so "+ offset" works
            Group_Message* message_to_delete = message_id_find(all_sent_vector[i], &message_stack, &offset);
            std::vector<Group_Message*>::iterator it = message_stack.begin() + offset;
            delete message_to_delete;                       // remove message and free memory
            message_stack.erase(it);
        }
        pthread_mutex_unlock(&message_lock);
    }   // end infinite loop

    // code will never execute, but on principle...  Is also handled in group_handler() exception handling
    // close sockets
    close(*connfd);
    close(listenfd);

    // destroy mutexes
    pthread_mutex_destroy(&connections_lock);
    pthread_mutex_destroy(&message_lock);
}   // END group_serv()



// group_handler()
void* group_handler(void* arg) {
    try {
        std::cout << "in new thread: " << pthread_self() << std::endl;
        int sockfd = *(int *) arg;
        int n = 0;  // num bytes read off socket

        Group_Message* local_message = NULL;      // create a local struct to be filled
        pthread_t my_tid = pthread_self();
        pthread_detach(pthread_self());             // detach thread from main thread

        // make socket non-blocking (select() makes this not necessary)
        if( fcntl(sockfd, F_SETFL, fcntl(sockfd, F_GETFL, 0) | O_NONBLOCK) == -1)
            throw std::invalid_argument("fnctl error");

        // file reading vars
        uint32_t num_bytes = 0;
        char overread[BYTES_IN_LONG];
        bool get_bits = true;
        bool expect_file = false;

        // set up for select() call
        fd_set readfd, master;
        int maxfd;                                          // record max fd
        FD_ZERO(&master);                                   // zero set structures
        FD_ZERO(&readfd);
        FD_SET(sockfd, &master);                            // add sockfd to master set
        maxfd = sockfd;                                     // sockfd > 0 by definition

        struct timeval tv;
        tv.tv_sec = 0;                                      // (1,000,000 u_sec = 1 sec)
        tv.tv_usec = 10000;                                 // refresh message sending every 1/10th of a second

        // infinite waiting loop
        for (;;) {
            readfd = master;                                // copy master set to readfd since select updates contents
            Select(maxfd+1, &readfd, NULL, NULL, &tv);      // select() call to update fd_set struct

            if(FD_ISSET(sockfd, &readfd)) {
                // standard group message sending
                // prepare struct
                local_message = new Group_Message;                              // new struct. Doesn't go out of scope
                local_message->num_sent = 0;

                // read from socket
                if ((n = read(sockfd, &(local_message->msg), MAXLINE)) == 0)    // read from socket into struct
                    throw std::invalid_argument("client disconnect error...");
                local_message->msg[n] = '\0';                                   // null terminate message


                std::cout << "TID: " << pthread_self() << ">>  " << local_message->msg << std::endl;

                // prepare for group sharing
                local_message->have_sent[0] = my_tid;                           // add thread id to sent list
                local_message->num_sent++;                                      // increment have_sent size

                // share with other connections
                pthread_mutex_lock(&message_lock);
                message_stack.push_back(local_message);                         // push_back message onto stack
                pthread_mutex_unlock(&message_lock);



                // group file sending (...under construction...)
//                if(!expect_file) {                          // not expecting a file
//                    // prepare struct
//                    local_message = new Group_Message;      // new struct. Doesn't go out of scope
//
//                    // read from socket
//                    if ((n = read(sockfd, &(local_message->msg), MAXLINE)) == 0)    // read from socket into struct
//                        throw std::invalid_argument("client disconnect error...");
//                    local_message->msg[n] = '\0';           // null terminate message
//
//                    // prepare for group sharing
//                    local_message->have_sent[0] = my_tid;   // add thread id to sent list
//                    local_message->num_sent = 1;            // increment have_sent size
//
//                    // share with other connections
//                    pthread_mutex_lock(&message_lock);
//                    message_stack.push_back(local_message); // push_back message onto stack
//                    pthread_mutex_unlock(&message_lock);
//
//                    // look for keywords in message
//                    input_filtering_group(local_message->msg, overread, &num_bytes, &expect_file, &get_bits, n );
//
//                }
//                else {
//                    // handle file reception
//                    std::cout << "file expected...\n";
//                    file_recv_handling_group(sockfd, local_message->msg, DEST_FILENAME, num_bytes, get_bits);
//                    expect_file = false;                    // reset file expected flag
//                }
            }


            // check for group messages that haven't been sent in this thread. loop through all messages in stack
            pthread_mutex_lock(&message_lock);
            for(Group_Message* i : message_stack) {                             // range based for loop
                if( !find_pthread(i->have_sent, i->num_sent , my_tid) ) {       // search have_sent for my_tid
                    Write(sockfd, &(i->msg), MAXLINE);                          // if not found, write to socket
                    i->have_sent[i->num_sent] = my_tid;                         // have thread sign sent list (at end)
                    i->num_sent++;                                              // increment have_sent size
                }
            }
            pthread_mutex_unlock(&message_lock);
        }                                                                       // close infinite loop
    }
    catch( std::invalid_argument &e ) {
        pthread_t my_tid = pthread_self();
        std::cout << "in thread ID: " << my_tid << "... " << e.what() << std::endl;

        close( *(int*)arg );                                                    // close or shutdown socket
        delete( (int*) arg );                                                   // runs for long time.  Release memory
        arg = NULL;                                                             // set connfd pointer to null


        pthread_mutex_lock(&connections_lock);
        for(int i = 0; i < connection_stack.size(); i++) {
            if(my_tid == connection_stack[i])
                connection_stack.erase(connection_stack.begin() + i);           // remove this connection from vector
            else {
                std::cout << "my_tid not found in connection_stack" << std::endl;
            }
        }
        pthread_mutex_unlock(&connections_lock);
    }
    catch( ... ) {
        pthread_t my_tid = pthread_self();
        std::cout << "in thread ID: " << my_tid << "... ";
        std::cout << "unknown exception in group_handler.  Connection ending now" << std::endl;

        close( *(int*)arg );                                                    // close or shutdown socket
        delete( (int*) arg );                                                   // runs for long time.  Release memory
        arg = NULL;                                                             // set connfd pointer to null


        pthread_mutex_lock(&connections_lock);
        for(int i = 0; i < connection_stack.size(); i++) {
            if(my_tid == connection_stack[i])
                connection_stack.erase(connection_stack.begin() + i);           // remove this connection from vector
            else {
                std::cout << "my_tid not found in connection_stack" << std::endl;
            }
        }
        pthread_mutex_unlock(&connections_lock);
    }

    return (void*) 1;                                                           // must return something
}   // END group_handler()



// message_id_find()
Group_Message* message_id_find(int message_id, std::vector<Group_Message*>* message_stack_ptr, int* offset ) {
    Group_Message* message_ptr;

    // linear search
    for(int i = 0; i < message_stack_ptr->size(); i++) {
        message_ptr = *(message_stack_ptr->begin() + i );
        if(message_ptr->message_id == message_id) {
            *offset = i;
            return message_ptr;
        }
    }

    // if not found, throw exception
    throw std::logic_error("message_id not found");
}   // END message_id_find()














// input_filtering_group()
void input_filtering_group(char* recvline, char* overread, uint32_t* num_bytes, bool* expect_file, bool* get_bits, int n) {
    if (strstr(recvline, "//PRE_FILE") != NULL) {   // scan recvline for file prep message
        std::cout << "\nPrepare for file reception.  Message sending disabled...\n";
        if(n > 12) {    // check number of bits read above.  If > 12, then have read num_bits to recv too
            // rearrange and cast to extract get num bits to read
            overread[0] = recvline[12];
            overread[1] = recvline[13];
            overread[2] = recvline[14];
            overread[3] = recvline[15];
            int* extra = (int*) overread;       // know next 2 bytes are ints from file protocol set with program
            *num_bytes = ntohl(*extra);          // convert to host byte order
            *get_bits = false;                   // do not read 2 bytes in file_recv_handling
        }
        *expect_file = true;                     // file prep received. file expected
    }
    else if (strstr(recvline, "//SND_TERM") != NULL) {   // scan recvline for file prep message
        std::cout << "\nFile received. Message sending re-enabled...\n" << std::endl;
    }
    else {                                      // pre-file message not found (normal operation)
        std::cout << "TID: " << pthread_self() << ">>  " << recvline << std::endl;
    }
}   // END input_filtering_group()







/*
 *  TODO push new message to message_stack and create/initialize specific objects
 *  TODO make sure to add mutexes where needed
 */

// file_recv_handling_group()
void file_recv_handling_group(int sockfd, char* recvline, std::string filename, uint32_t num_bytes, bool get_bits) {
    // declare variables
    int whole, partial, n = 0;
    uint32_t exp_data, exp_data_net;        // permits sending of up to 4GB file size
    bool end_process = false;

    // open file for writing
    std::ofstream file;
    file.open(filename);

    // determine expected number of bits
    if( get_bits ) {                        // if num_bits not set, read expected bits from socket
        if ( read(sockfd, &exp_data_net, sizeof(exp_data_net)) < 0)
            throw std::invalid_argument("client disconnect error...");
        exp_data = ntohl(exp_data_net);     // adjust to host byte order
    }
    else {                                  // if num_bits set
        exp_data = num_bytes;
    }

    // parse num bytes into num full buffers
    whole = exp_data/(MAXLINE-1);           // num whole buffers
    std::cout << "whole buffers: " << whole << std::endl;   // TODO debug
    if( whole > 0) {
        partial = exp_data % (MAXLINE-1);   // num of bits in partial buffer
    }
    else {
        partial = exp_data;
    }
    std::cout << "partial buffers: " << partial << std::endl;           // TODO debug


    // read full buffers off socket and output to file
    Group_Message* local_message = NULL;
    std::cout << "File received: \n" << std::endl;
    for(int i = 0; i < whole; i++) {                                    // read each full buffer
        std::cout << "\nwhole" << i << ": \n";
        local_message = new Group_Message;
        if ((n = read(sockfd, local_message->msg, MAXLINE)) < 0)
            throw std::invalid_argument("client disconnect error...");
        recvline[MAXLINE] = '\0';           // null terminate

        local_message->have_sent[0] = pthread_self();                   // sign message
        local_message->num_sent = 1;

        // share with other connections
        pthread_mutex_lock(&message_lock);
        message_stack.push_back(local_message);                         // push_back message onto stack
        pthread_mutex_unlock(&message_lock);


        // output to file
        file << recvline;                                               // output to file
        std::cout << recvline;

        if (strstr(recvline, "//SND_TERM") != NULL ) {  // deal with network irregularity. Search for file end transmission
            std::cout << "ERROR: network issues.  end_process set to true...\n";    // TODO debug-ish
            end_process = true;     // network issues.  Whole buffers should not contain "//SND_TERM"
            break;                  // avoid long loops over this area due to incorrect socket I/O
        }
    }
    if(!end_process) {              // if //SND_TERM not found yet...
        local_message = new Group_Message;
        if ((n = read(sockfd, local_message->msg, partial-1)) < 0)  // read final partial buffer from socket
            throw std::invalid_argument("client disconnect error...");
        local_message->msg[n] = '\0';                                   // null terminate

        local_message->have_sent[0] = pthread_self();                   // sign message
        local_message->num_sent = 1;
        pthread_mutex_lock(&message_lock);                              // share with other connections
        message_stack.push_back(local_message);                         // push_back message onto stack
        pthread_mutex_unlock(&message_lock);

        file << recvline;                                               // output to file
        std::cout << recvline << std::endl;
    }


    // TODO debug output
    std::cout << "expected num bits: " << exp_data << std::endl;

    file.close();                                       // close file
    if (strstr(recvline, "//SND_TERM") != NULL ) {      // only output if file transmission is completed
        std::cout << "\nmessage sending re-enabled...\n" << std::endl;
    }
}   // END file_recv_handling_group()

