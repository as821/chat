#include "main.h"


// necessary global variables to be accessible by all threads
std::vector<Group_Message*> message_stack;                  // empty vector of messages
std::vector<pthread_t> connection_stack;                    // empty vector of all thread ids

// mutexes for altering num_connections, reading from message_stack, adding messages to
pthread_mutex_t connections_lock, message_lock;



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

        Group_Message* local_message = NULL;                // create a local struct to be filled
        pthread_t my_tid = pthread_self();
        pthread_detach(pthread_self());                     // detach thread from main thread

        // file reading vars
        uint32_t num_bytes = 0;
        char overread[BYTES_IN_LONG];
        bool get_bits = true;
        bool expect_file = false;

        // set up for select() call
        fd_set readfd, master;
        int maxfd;                                          // record max fd
        FD_ZERO(&master);                               // zero set structures
        FD_ZERO(&readfd);
        FD_SET(sockfd, &master);                        // add sockfd to master set
        maxfd = sockfd;                                    // sockfd > 0 by definition

        struct timeval tv;
        tv.tv_sec = 0;                                      // (1,000,000 u_sec = 1 sec)
        tv.tv_usec = 10000;                                 // refresh message sending every 1/10th of a second

        // infinite waiting loop
        for (;;) {
            readfd = master;                                // copy master set to readfd since select updates contents
            Select(maxfd+1, &readfd, NULL, NULL, &tv);      // select() call to update fd_set struct

            if(FD_ISSET(sockfd, &readfd)) {
                // group file sending (...now semi-functional...)
                // read from socket
                local_message = new Group_Message;          // new struct. Doesn't go out of scope
                if ((n = read(sockfd, local_message->msg, MAXLINE)) == 0)   {  // read from socket into struct
                    perror("client disconnect 1");
                    throw std::invalid_argument("client disconnect error...1"); }
                local_message->msg[n] = '\0';               // null terminate message
                local_message->msg_len = n;
                local_message->have_sent[0] = my_tid;       // add thread id to sent list
                local_message->num_sent = 1;                // increment have_sent size

                // look for keywords in message
                input_filtering_group(local_message->msg, overread, &num_bytes, &expect_file, &get_bits, n );
                if(expect_file) {   // if expecting a file, wait to send transmission until inside function
                    file_recv_handling_group(sockfd, local_message->msg, DEST_FILENAME, num_bytes, get_bits);
                    expect_file = false;                    // reset file expected flag
                    delete local_message;                   // is not going to be broadcast.  Avoid mem leak
                }
                else {  // if not expecting a file, share transmission with all connections
                    // share with other connections
                    pthread_mutex_lock(&message_lock);
                    message_stack.push_back(local_message); // push_back message onto stack
                    pthread_mutex_unlock(&message_lock);
                }
            }   // end fd_isset sockfd

            // check for group messages that haven't been sent in this thread. loop through all messages in stack
            pthread_mutex_lock(&message_lock);
            for(Group_Message* i : message_stack) {                             // range based for loop
                if( !find_pthread(i->have_sent, i->num_sent , my_tid) ) {       // search have_sent for my_tid
                    Write(sockfd, i->msg, i->msg_len);                             // if not found, write to socket
//                    std::cout << "in thread " << my_tid << " >> " << i->msg << " sent..." << std::endl;   // debug
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

        // errno determination for socket operation errors
        if(errno == EAGAIN)
            std::cout << "EAGAIN\n";
        else if(errno == EBADF)
            std::cout << "EBADF\n";
        else if(errno == EFAULT)
            std::cout << "EFAULT\n";
        else if(errno == EINTR)
                std::cout << "EINTR\n";
        else if(errno == EINVAL)
                std::cout << "EINVAL\n";
        else if(errno == EIO)
                std::cout << "EIO\n";
        else if(errno == EISDIR)
                std::cout << "EISDIR\n";
        else {
            std::cout << "unknown errno...\n";
        }
        close( *(int*)arg );                                                    // close or shutdown socket
        delete( (int*) arg );                                                   // runs for long time.  Release memory
        arg = NULL;                                                             // set connfd pointer to null

        pthread_mutex_lock(&connections_lock);
        for(int i = 0; i < connection_stack.size(); i++) {
            if(my_tid == connection_stack[i])
                connection_stack.erase(connection_stack.begin() + i);           // remove this connection from vector
        }
        pthread_mutex_unlock(&connections_lock);
    }
    catch( ... ) {
        pthread_t my_tid = pthread_self();
        std::cout << "in thread ID: " << my_tid << "... ";
        std::cout << "unknown exception in group handler.  Connection ending now" << std::endl;
        close( *(int*)arg );                                                    // close or shutdown socket
        delete( (int*) arg );                                                   // runs for long time.  Release memory
        arg = NULL;                                                             // set connfd pointer to null
        pthread_mutex_lock(&connections_lock);
        for(int i = 0; i < connection_stack.size(); i++) {
            if(my_tid == connection_stack[i])
                connection_stack.erase(connection_stack.begin() + i);           // remove this connection from vector
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
            int* extra = (int*) overread;                                   // next bytes are ints from file protocol
            *num_bytes = ntohl(*extra);                                     // convert to host byte order
            std::cout << "num_bytes(input_filter_group): " << *num_bytes << std::endl;
            *get_bits = false;                                              // do not read bytes in file_recv_handling
        }
        *expect_file = true;                                                // file prep received. file expected
    }
    else if (strstr(recvline, "//SND_TERM") != NULL) {                  // scan recvline for file prep message
        std::cout << "\nFile received. Message sending re-enabled...\n" << std::endl;
    }
    else {                                                              // pre-file message not found (normal operation)
        std::cout << "TID: " << pthread_self() << ">>  " << recvline << std::endl;
    }
}   // END input_filtering_group()



// file_recv_handling_group()
/// *** inner loops of this function are mutex locked so all file-related messages are sent back to back ***
void file_recv_handling_group(int sockfd, char* recvline, std::string filename, uint32_t num_bytes, bool get_bits) {
    // declare variables
    std::vector<Group_Message*> file_msg_vector;
    int whole, partial, n = 0;
    uint32_t *exp_data, exp_data_net;                                       // permits sending of up to 4GB file size
    exp_data = new uint32_t;
    Group_Message* local_message = NULL;
    pthread_t my_tid = pthread_self();

    // determine expected number of bits
    if( get_bits ) {                        // if num_bits not set, read expected bits from socket
        if ( read(sockfd, &exp_data_net, sizeof(exp_data_net)) < 0) {
            throw std::invalid_argument("client disconnect error...2");
        }
        *exp_data = ntohl(exp_data_net);                                    // adjust to host byte order
    }
    else {                                                                  // if num_bits set
        *exp_data = num_bytes;
    }

    // prepare for a file
    char original_msg[PROTOCOL_MSG_LEN] =  "//PRE_FILE\0";
    char *prep_message = new char [PROTOCOL_MSG_LEN];
    for(int i = 0; i < 12; i++) {
        prep_message[i] = original_msg[i];
    }

    local_message = new Group_Message;
    delete[] local_message->msg;                                          // deallocate original memory from constructor
    local_message->msg = prep_message;
    local_message->msg_len = PROTOCOL_MSG_LEN;                              // pre-file message length
    local_message->have_sent[0] = my_tid;
    local_message->num_sent = 1;
    file_msg_vector.push_back(local_message);

    // send expected file size to all
    local_message = new Group_Message;
    delete[] local_message->msg;                                         //  deallocate original memory from constructor
    *exp_data = htonl(*exp_data);                                        // convert back to network order
    local_message->msg = (char*) exp_data;
    local_message->msg_len = BYTES_IN_LONG;                                 // pre-file message length
    local_message->have_sent[0] = my_tid;
    local_message->num_sent = 1;
    file_msg_vector.push_back(local_message);                               // push_back message onto stack

    // parse num bytes into num full buffers
    whole = *exp_data/(MAXLINE-1);                                          // num whole buffers
    if( (partial = *exp_data % (MAXLINE-1)) != 0) {                         // partial buffer
        whole++;
    }
    std::cout << "total num buffers: " << whole << std::endl;               // debug output

    // read full buffers off socket and output to file
    std::cout << "File received: \n" << std::endl;
    for(int i = 0; i < whole; i++) {                                        // read each full buffer
//        std::cout << "\nwhole" << i << ": \n";
        local_message = new Group_Message;
        if(whole != 1) {
            if ((n = read(sockfd, local_message->msg, MAXLINE)) < 0) {
                throw std::invalid_argument("client disconnect error...3");
            }
            local_message->msg_len = MAXLINE;                               // pre-file message length
            recvline[n] = '\0';                                             // null terminate
        }
        else {
            local_message->msg_len = partial;                               // pre-file message length
            if ((n = read(sockfd, local_message->msg, local_message->msg_len)) < 0) {
                throw std::invalid_argument("client disconnect error...5");
            }
            recvline[n] = '\0';                                             // null terminate
        }
        local_message->have_sent[0] = my_tid;                               // sign message
        local_message->num_sent = 1;
        file_msg_vector.push_back(local_message);                           // share with other connections

        if (strstr(local_message->msg, "//SND_TERM") != NULL ) {        // network issues. File end transmission
            std::cout << "\n\nMessage issues.  file validity unknown...\n\n";
            break;                  // avoid long loops over this area due to incorrect socket I/O
        }
    }

    //SND_TERM removal
    if (strstr(local_message->msg, "//SND_TERM") != NULL ) {        // only output if file transmission is completed
        std::cout << "\nmessage sending re-enabled...\n" << std::endl;
        int index = local_message->msg_len - 12;                            // 12 == byte length of //SND_TERM
        local_message->msg[index] = '\0';
    }

    // SND_TERM addition to vector
    char* termination = new char [PROTOCOL_MSG_LEN];
    char msg[PROTOCOL_MSG_LEN] = "//SND_TERM\0";
    for(int j = 0; j < PROTOCOL_MSG_LEN; j++) {
        termination[j] = msg[j];
    }
    local_message = new Group_Message;
    delete[] local_message->msg;
    local_message->msg = termination;
    local_message->msg_len = PROTOCOL_MSG_LEN;                              // pre-file message length
    local_message->have_sent[0] = my_tid;
    local_message->num_sent = 1;
    file_msg_vector.push_back(local_message);

    // output file_msg_vector contents
//    std::cout << "\n\nfile_msg_vector: " << std::endl;                    // debug output
//    for(int i = 0; i < file_msg_vector.size(); i++) {
//        std::cout << "msg " << file_msg_vector[i]->message_id << ":\t" << file_msg_vector[i]->msg << std::endl;
//    }
    // write all of file_msg_vector to message_vector.  lock message_stack and potentially connection_stack as well
    pthread_mutex_lock(&message_lock);
    for(Group_Message* i : file_msg_vector) {
        message_stack.push_back(i);
    }
    pthread_mutex_unlock(&message_lock);

    std::cout << "expected file byte size: " << exp_data << std::endl;
}   // END file_recv_handling_group()

