# chat
TCP client/server in C/C++

**Client:<br/>**
A generic TCP client.  Allows message sending and sending of files up to 4GB to a running server program (type "//SND_FILE" once
connected to server).   Uses non-blocking IO and non-blocking sockets.  Default IP is 127.0.0.1 (self) with port 40000.  Both 
can be set by user when program runs.

**Server:<br/>**
Single connection server. Meant for 1:1 connection with a client.  Can receive files from client program ( input full 
destination file path into DEST_FILENAME in main.h before compiling.  Accepts connections and messages from any TCP client 
(netcat, client provided here, etc.)  Outputs machine IP address to screen at start.  Listens to port 40000.

**Group server:<br/>**
Multi-threaded server that handles up to 100 ongoing connections.  Each message or file sent by a client is sent to every other connected
client (like a chat room).  This program acts as a facilitator between clients and does not participate in message sending.  This
allows any TCP client to have access to group message sending/receiving.  Uses non-blocking IO and sockets. Listens at port 40000.

**TODO:<br/>**
  Fix appropriate socket function wrappers to safely support incomplete client and server side transmissions.<br/>
  Plan to add encryption to file and message sending and improve runtime efficiency (improved search/storage functions).<br/>
  Improve messaging protocol to improve reliability and decrease overhead.
