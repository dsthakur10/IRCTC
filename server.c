
// MT2021146 - Thakur Devendrasingh Premsingh


#include <string.h>
#include <sys/socket.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <stdio.h> 
#include <stdlib.h> 
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

#include "serverUtils.h"

#define PORT 8000
#define TRAIN "./IRCTC/Train"
#define BOOKING "./IRCTC/Booking"
#define USER "./IRCTC/User"
  

int main() 
{ 
    
    int socket_desc, client_sock, c; 
    struct sockaddr_in server, client; 
    char buf[100]; 
    
    //return socket descriptor(uses tcp)
    socket_desc = socket(AF_INET, SOCK_STREAM, 0); 
    if (socket_desc == -1) { 
        printf("Could not create socket"); 
    } 
    
    /*The htons function takes a 16-bit number in host byte order and returns a 16-bit number in network byte order used in TCP/IP networks (the AF_INET or AF_INET6 address family). The htons   	function can be used to convert an IP port number in host byte order to the IP port number in network byte order.*/
    
    server.sin_family = AF_INET; 
    server.sin_addr.s_addr = INADDR_ANY; //can recieve at all interfaces
    server.sin_port = htons(PORT); 
    
    //bind the socket descriptor with port number and address family
    if (bind(socket_desc, (struct sockaddr*)&server, sizeof(server)) < 0) 
        perror("bind failed. Error"); 
   
    //can listen atmost 10 clients at a time
    listen(socket_desc, 10);  						// backlog queue size = 10
    c = sizeof(struct sockaddr_in); 
    
    printf("Server started\n");
    //multi client structure
    while (1)
    {
    	    printf("\nListening....\n");
	    
	    //accept incoming connection request from client
	    client_sock = accept(socket_desc, (struct sockaddr*)&client, (socklen_t*)&c); 
	    
	    //client servicing, parent child created for synchronization, and child services the client
	    /*When a connection is established, accept returns, the server calls fork, and the child process services the client (on the connected socket connfd). The parent process waits for a		nother connection (on the listening socket listenfd. The parent closes the connected socket since the child handles the new client.*/
	    
	    //child === service, parent === wait for accepting new connections
	    
	    if (!fork())
	    {
		    close(socket_desc);
		    //servicing client
		    service_cli(client_sock);								// Service client, once done client exits
		    exit(0);
	    }
	    else
	    	close(client_sock);
	    //closed because child is servicing the client
    }
    return 0;
}


