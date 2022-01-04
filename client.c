
// MT2021146 - Thakur Devendrasingh Premsingh


#include <string.h>
#include <sys/socket.h> 
#include <unistd.h> 
#include <arpa/inet.h> 
#include <stdio.h>
#include <stdlib.h> 

#include "clientUtils.h"
	 
#define PORT 8000


int main(void) 
{ 
	int sock; 
    	struct sockaddr_in server; 
    	char server_reply[50],*server_ip;
	server_ip = "127.0.0.1"; 		//loop back address
        
        //client socket descriptor is made
    	sock = socket(AF_INET, SOCK_STREAM, 0); 
    	if (sock == -1) { 
       	printf("Could not create socket"); 
    	} 
        
        //put server ip address in sockaddr_in(server)
    	server.sin_addr.s_addr = inet_addr(server_ip); 
    	server.sin_family = AF_INET; 
    	server.sin_port = htons(PORT); 
        
        //client sends a connection request to server
    	if (connect(sock, (struct sockaddr*)&server, sizeof(server)) < 0)
       	perror("connect failed. Error"); 
    	
    	//run loop till user gives 3 i.e. exit
	while(client(sock)!=3);
	
    	close(sock); //close client socket when user wants to exit(gives 3)
	return 0; 
} 

