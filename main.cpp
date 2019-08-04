#include "main.h"





/*
 *
 *  Main function.  Recieves user input for messaging mode. Group server currently cannot send messages.  It acts as
 *  a facilitator for the connected clients.  This allows any TCP-based client to access this group messaging.  Server
 *  works with any TCP client (netcat, the client provided here, etc.). To receive files, enter full destination
 *  path in DEST_FILENAME in main.h.
 *
 */

int main() {
    try{
        char selection;
        std::cout << "s: server\nc: client\ng: group server\n";
        std::cin >> selection;
        std::cin.ignore(1000, '\n');


        if(selection == 'c') {          // client
            std::string ip_addr;
            std::cout << "\nInput IP address to connect to: (Press 'd' to use default 127.0.0.1, port 40000)\n";
            std::cin >> ip_addr;
            if(ip_addr == "d") {
                client(40000, "127.0.0.1");
            }
            else {
                int port;
                std::cout << "\nInput port to connect to: (default is 40000)\n";
                std::cin >> port;
                client(port, ip_addr);
            }
        }
        else if (selection == 's') {    // server
            print_machine_ip();
            server(40000);
        }
        else if (selection == 'g') {    // group server
            group_serv(40000);
        }
        else {
            std::cout << "invalid.  Program ending now...\n";

//          Create variable file sizes for testing network file sending with multiple packets.  For debugging
//            to fill a test file
//            std::ofstream file_fill;
//            file_fill.open(SRC_FILENAME);
//            for(int i = 0; i < 5000; i++) {
//                file_fill << i;
//                if(i%2 == 0)
//                    file_fill << '\n';
//            }
//            file_fill.close();
        }


    }

    // exception handling
    catch (std::invalid_argument &e) {
        std::cout << e.what() << std::endl;
    }
    catch (...) {
        std::cout << "unknown error caught in main.  Ending now...\n";
    }
}