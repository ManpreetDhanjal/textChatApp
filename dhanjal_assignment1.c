/**
 * @dhanjal_assignment1
 * @author  Manpreet Dhanjal <dhanjal@buffalo.edu>
 * @version 1.0
 *
 * @section LICENSE
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of
 * the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
 * General Public License for more details at
 * http://www.gnu.org/copyleft/gpl.html
 *
 * @section DESCRIPTION
 *
 * This contains the main function. Add further description here....
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <sys/time.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <unistd.h>

#include <arpa/inet.h>
#include <netdb.h>
#include <netinet/in.h>
#include <ifaddrs.h>

#include "../include/global.h"
#include "../include/logger.h"

#define STDIN 0  // file descriptor for standard input
#define BACKLOG 4 // number of clients server can connect to
#define MAXDATASIZE 1024

struct clientData{
    // use getnameinfo to get hostname
    char ip[INET_ADDRSTRLEN];
    int port; // int coz we need to sort according to port
    char hostName[128];
    int sock; // socket desc
    struct clientData* blockList;
    int status; // logged in ?
    int msgSent;
    int msgRecv;
    int blockCount;
    char messageBuffer[100][256];
    int messageCount;
    int isBlockListReady;
};

// global - ip, port, sockdesc, fdmax, masterfds, status, type
char ip[INET_ADDRSTRLEN]; // fetches ip address from sock address of current machine
char type;
char* port;
int sockDesc;
int fdMax;
fd_set masterfds; // contains all the sockets
int status;
struct clientData clientList[5]; // list of client details
int clientNum;
char blockedList[4][INET_ADDRSTRLEN];
int blockCount = 0;
struct addrinfo *myaddress; // stores address of the machine

// end - global variable declaration

// s is socket
int sendall(int s, char *buf, int *len)
{
    int total = 0;        // how many bytes we've sent
    int bytesleft = *len; // how many we have left to send
    int n;
    while(total < *len) {
        n = send(s, buf+total, bytesleft, 0);
        if (n == -1){ 
            break; 
        }
        total += n;
        bytesleft -= n;
    }
    *len = total; // return number actually sent here
    return n==-1?-1:0; // return -1 on failure, 0 on success
}

int recvall(int s, char *buf, int *len)
{
    int total = 0;        // how many bytes we've received
    int bytesleft = *len; // how many we have left to received
    int n;
    while(total < *len) {

        n = recv(s, buf+total, bytesleft, 0);
        if (n == -1){ 
            break; 
        }
        total += n;
        bytesleft -= n;
    }
    *len = total; // return number actually sent here
    buf[*len] = '\0';
    return n==-1?-1:0; // return -1 on failure, 0 on success
}

int checkMainInputError(int argc, char *argv[]){
	/////////// error checking for input format
	printf("%d\n", argc);
	printf("%c\n", *argv[1]);
	if(argc != 3){
		printf("Incorrect number of arguments\n");
		return -1;
	}
	if(*argv[1] == 's' || *argv[1] == 'c'){
		// do nothing
	}else{
		// give error
		printf("Incorrect type\n");
		return -1;
	}
	/////////// error checking for input format
	return 1;
}

char** interpretMessage(char* buf, int *count){
    int i=0;
    int num;
	int len = 0;
	char* newTok;
	char* temp;
	char* token;
    char** input = malloc(5 * sizeof(char*));
    	printf("1:%s--\n", buf);
	buf[strlen(buf)] = '\0';
	temp = malloc(strlen(buf)+1);
	strcpy(temp, buf);
    token = strtok(buf,"#");
    while(token != NULL){
	printf("token:%s\n", token);
	printf("lengh:%d\n", strlen(token));
        input[i] = malloc(strlen(token) + 1);
	if(i==1 && strcmp("MSG", input[0]) == 0){
		num = atoi(token);
		printf("num:%d\n", num);
		*count = atoi(token);
		printf("count:%d\n", *count);
	}
        strcpy(input[i],token);
        token = strtok(NULL, "#");
        i++;
	if(i==2 && ((strcmp(input[0], "SEND") == 0 && type == 's') || strcmp(input[0], "MSG") == 0)){
		len = strlen(input[0]) + strlen(input[1]) + 2;
		newTok = strtok(temp+len, "");
		printf("new token:%s\n", newTok);
		input[i] = malloc(strlen(newTok) +1);
		strcpy(input[i], newTok);
		break;
	}
    }
    return input;
}

// take user input and split
char** getUserInput(){
    int numbytes;

    int i = 0; // for index of user input

    char* token;
    char** userInput = malloc(5 * sizeof(char*)); // 3 values
    int isSend = 0;
    int isBrd = 0;
    int length = 0;

    char buf[1024]; // token changes buf
    char* temp; // copy buf into temp before changes to buf

    fgets(buf, 1024, stdin); // get input from user

    if(strlen(buf) != 1){ // user didnt just press \n
        
        numbytes = strlen(buf) - 1;
        if(buf[numbytes] == '\n'){
            buf[numbytes] = '\0';
        }
        temp = malloc(numbytes); // copy buf into temp coz buf gets modified
        strcpy(temp, buf);

        token = strtok(buf, " ");
        while(token != NULL){

            if(i==0 && strcmp("BROADCAST", token) == 0){
                isBrd = 1;
            }
	    if(i == 0 && strcmp("SEND", token) == 0){
                // dont break the message into parts
                isSend = 1;
            }
            userInput[i] = malloc(strlen(token) + 1);
            strcpy(userInput[i],token);
            if(length == 0){
                length = strlen(token);
            }else{
                length = length + strlen(token);
            }
	    i++;
	    if((isSend == 1 && i==2) || (isBrd == 1 && i==1)){
	    	token = strtok(NULL, "");
		userInput[i] = malloc(strlen(token)+1);
		strcpy(userInput[i], token);
		i++;
		break;
	    }else{
		token = strtok(NULL, " ");
	    }
        }
        free(temp);

    }
    userInput[i] = malloc(sizeof('\0'));

    return userInput;
}

int isAlreadyBlocked(char *ip){
    for(int i=0; i<blockCount; i++){
        if(strcmp(ip, blockedList[i]) == 0){
            return 1;
        }
    }
    return 0;
}

void removeFromBlockedList(char *ip){
    int i;
    for(i=0; i<blockCount; i++){
        if(strcmp(ip, blockedList[i]) == 0){
            break;
        }
    }

    for(; i<blockCount-1; i++){
        strcpy(blockedList[i], blockedList[i+1]);
    }

    blockCount--;
}

void sortClientList(){
    struct clientData clientNode;
    for(int i=1; i<clientNum; i++){
        int j=0;
        while(j<i){
            if(clientList[i].port < clientList[j].port){
                clientNode = clientList[i];
                clientList[i] = clientList[j];
                clientList[j] = clientNode;
            }
            j++;
        }
    }
	printf("sorting complete\n");
}

void sortBlockedList(struct clientData clientNode){
	struct clientData blockedNode;
	for(int i=0; i<clientNode.blockCount; i++){
		int j=0;
		while(j<i){
			if(clientNode.blockList[i].port < clientNode.blockList[j].port){
				blockedNode = clientNode.blockList[i];
				clientNode.blockList[i] = clientNode.blockList[j];
				clientNode.blockList[j] = blockedNode;
			}
			j++;
		}
	}
}

void addToClientList(int sockdesc, char* ip, int port, char* hostName){
    clientList[clientNum].sock = sockdesc;
    strcpy(clientList[clientNum].ip, ip);
    clientList[clientNum].port = port;
    clientList[clientNum].status = 1;
    clientList[clientNum].blockCount = 0;
	if(hostName == NULL){
		strcpy(clientList[clientNum].hostName, "host");
	}else{

	    strcpy(clientList[clientNum].hostName, hostName);
	}

	printf("hostname copied\n"); 
   if(clientList[clientNum].isBlockListReady == 0){
        clientList[clientNum].blockList = (struct clientData*)malloc(4*sizeof(struct clientData));
   	clientList[clientNum].isBlockListReady = 1;
	printf("assigned memory\n"); 
  }
    
    clientList[clientNum].messageCount = 0;
    clientNum++;
    sortClientList();
}

char* generateClientList(){
    int i=0;
    int port;
    int count=0;
    char ip[INET_ADDRSTRLEN];
    char* numstr = malloc(sizeof(clientNum));
    char* message = malloc(MAXDATASIZE);
    char* temp = malloc(MAXDATASIZE);
    memset(message, 0, MAXDATASIZE);
    memset(temp, 0, MAXDATASIZE);

    strcat(message, "SEND#");
    
    for(i=0; i<clientNum; i++){
        if(clientList[i].status == 0){
            continue;
        }
        count++;
        strcat(temp, "#");
        strcat(temp, clientList[i].ip);
        strcat(temp, "$");
        strcat(temp, clientList[i].hostName);
        strcat(temp, "$");
        sprintf(numstr, "%d", clientList[i].port);
        strcat(temp, numstr);
    }
    sprintf(numstr, "%d", count);
    strcat(message, numstr);
    strcat(message, temp);
    memset(numstr,0, strlen(numstr));
    free(numstr);
    return message;
}

// LIST#NUM#ip$port
void recreateClientList(char** clientMsg){

    int count = 2+atoi(clientMsg[1]);
    char* ip;
    char* port;
    char* host;
    clientNum = 0;
    for(int j=2; j<count; j++){
        ip = strtok(clientMsg[j], "$");
        host = strtok(NULL, "$");
        port = strtok(NULL,"");
        addToClientList(0, ip, atoi(port), host);
    }

}

// close all the sockets
void logOut(){
    int j;
    for(j = 1; j<= fdMax; j++){
        if(FD_ISSET(j, &masterfds)){
            close(j);
        }
    }

    // clear master fds
    // clear read fds
    FD_ZERO(&masterfds);
    fdMax = STDIN;
}

int searchClientListBySock(int sock){
    for(int i=0; i<clientNum; i++){
        if(clientList[i].sock == sock){
            return i;
        }
    }
    return -1;
}

int searchClientListByIp(char* ip){
    for(int i=0; i<clientNum; i++){
        if(strcmp(ip, clientList[i].ip) == 0){
            return i;
        }
    }
    return -1;
}

void unblockClient(int sock, char* ip){
    int i;
    int index = searchClientListBySock(sock);
    if(index < 0){
        return;
    }

    struct clientData client = clientList[index];

    for(i=0; i<clientList[index].blockCount; i++){
        if(strcmp(ip, clientList[index].blockList[i].ip) == 0){
            break;
        }
    }

    for(; i<clientList[index].blockCount-1; i++){
        clientList[index].blockList[i] = clientList[index].blockList[i+1];
    }

    clientList[index].blockCount--;
}

void removeFromList(int sock){
    int index;
    index = searchClientListBySock(sock);

    for(int i=index; i<clientNum-1; i++){
        clientList[i] = clientList[i+1];
    }

    clientNum--;
}

int connectToServer(char* ip, char* port){
    int val;
    int yes =1;
    struct addrinfo hints;
    struct addrinfo *result;
    struct addrinfo *availableResult;
    struct linger linger;

    memset(&linger, 0, sizeof linger);
    linger.l_onoff = 1;
    linger.l_linger = 0;

    memset(&hints, 0, sizeof hints);
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_STREAM;

    getaddrinfo(ip, port, &hints, &result);
    for(availableResult = myaddress; availableResult != NULL; availableResult = availableResult->ai_next){
            if((sockDesc = socket(availableResult->ai_family, availableResult->ai_socktype, availableResult->ai_protocol)) == -1){
                // give error
                continue;
            }
            if(setsockopt(sockDesc, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(int)) == -1){
                // give error
                exit(1);
            }
            setsockopt(sockDesc, SOL_SOCKET, SO_LINGER, &linger, sizeof(struct linger));
            if(bind(sockDesc, availableResult->ai_addr, availableResult->ai_addrlen) == -1){
                close(sockDesc);
                // give error
                continue;
            }
            break;
        }

        //freeaddrinfo(myaddress);

        if(availableResult == NULL){
                // give error
            exit(1);
        }
    val = connect(sockDesc, result->ai_addr, result->ai_addrlen);
    if(val == -1){
        perror("error:");
    }
    return 1;
}

// get sockaddr, IPv4 or IPv6:
void *get_in_addr(struct sockaddr *sa)
{
    if (sa->sa_family == AF_INET) {
        return &(((struct sockaddr_in*)sa)->sin_addr);
}
    return &(((struct sockaddr_in6*)sa)->sin6_addr);
}

int get_in_port(struct sockaddr *sa){
    return ntohs(((struct sockaddr_in*)sa)->sin_port);
}

void encapsulateAndSend(char* message, int sockDesc){
    int len;
    char* wholeMessage = (char*)malloc(10+strlen(message));
    sprintf(wholeMessage, "%5lu", strlen(message));
    strcat(wholeMessage, message);
    len = strlen(wholeMessage);
    sendall(sockDesc, wholeMessage, &len);
    free(wholeMessage);
}

void processCommand(){
    int i;
    int k;
    int numbytes;
    char* message;
    char* wholeMessage;
    char buf[1024];
    int index;
    char* str;
    int len;
    struct sockaddr_in sa;

    char** userInput; // input command from user
    struct clientData clientNode;

    userInput = getUserInput(); // take user input and break into parts

    if(strcmp(userInput[0],"AUTHOR") == 0){
        cse4589_print_and_log("[%s:SUCCESS]\n", userInput[0]);
        cse4589_print_and_log("I, dhanjal, have read and understood the course academic integrity policy.\n");
    }else if(strcmp(userInput[0], "IP") == 0){
        cse4589_print_and_log("[%s:SUCCESS]\n", userInput[0]);
        cse4589_print_and_log("IP:%s\n", ip);
    }else if(strcmp(userInput[0], "PORT") == 0){
        cse4589_print_and_log("[%s:SUCCESS]\n", userInput[0]);
        cse4589_print_and_log("PORT:%s\n", port);
    }else if(type == 'c' && strcmp(userInput[0], "LOGIN") == 0 && status == 0){ // LOGIN only for clients
        if(userInput[1] == NULL || userInput[2] == NULL 
            || inet_pton(AF_INET, userInput[1], &(sa.sin_addr)) != 1 || atoi(userInput[2])<1024 || atoi(userInput[2])>65535){
            cse4589_print_and_log("[%s:ERROR]\n", userInput[0]);
	    cse4589_print_and_log("[%s:END]\n", userInput[0]);
        }else{
            connectToServer(userInput[1], userInput[2]);
            if(sockDesc < 0){
                cse4589_print_and_log("[%s:ERROR]\n", userInput[0]);
            }else{
    	        // add to the master list for read and write
    	        FD_SET(sockDesc, &masterfds);
    	        fdMax = sockDesc;
		}
        }
    }else if(strcmp(userInput[0], "LIST") == 0 && (status == 1 || type == 's')){
        // assuming sorted list
        if(clientNum == 0){
        }
        cse4589_print_and_log("[%s:SUCCESS]\n", userInput[0]);
        k=0;
        for(int i=0; i<clientNum; i++){
            if(clientList[i].status == 0){
                continue;
            }
	   cse4589_print_and_log("%-5d%-35s%-20s%-8d\n", k+1, clientList[i].hostName, clientList[i].ip, clientList[i].port);
            k++;
        }
//	}
    }else if(type == 'c' && strcmp(userInput[0], "SEND") == 0 && status == 1){
        // form send message with IP address and actual message
        if(userInput[1] == NULL || inet_pton(AF_INET, userInput[1], &(sa.sin_addr)) != 1 
            || searchClientListByIp(userInput[1]) < 0){
            cse4589_print_and_log("[%s:ERROR]\n", userInput[0]);
        }else{
            cse4589_print_and_log("[%s:SUCCESS]\n", userInput[0]);
            message = (char*)malloc(strlen(userInput[0]) + strlen(userInput[1]) + strlen(userInput[2])+3); // 1 for delimiter
            strcpy(message, userInput[0]);
            strcat(message, "#");
            strcat(message, userInput[1]);
            strcat(message, "#");
            strcat(message, userInput[2]);
            encapsulateAndSend(message, sockDesc);
            free(message);
        }
        
    }else if(type == 'c' && strcmp(userInput[0], "LOGOUT") == 0 && status == 1){
        // for client close sock
        cse4589_print_and_log("[%s:SUCCESS]\n", userInput[0]);
      	status = 0;
        logOut();
    }else if(strcmp(userInput[0], "EXIT") == 0){
        // exit for server
        // close all the sockets
        if(status == 1){
            logOut();
        }
        cse4589_print_and_log("[%s:SUCCESS]\n", userInput[0]);
	cse4589_print_and_log("[%s:END]\n", userInput[0]);
        exit(0);

    }else if(type == 'c' && strcmp(userInput[0], "REFRESH") == 0 && status == 1){
        // get fresh client list;
        message = "REFRESH";
        encapsulateAndSend(message, sockDesc);
        cse4589_print_and_log("[%s:SUCCESS]\n", userInput[0]);
    }else if(type == 's' && strcmp(userInput[0], "STATISTICS") == 0){
        // print client list
        // sort list
        cse4589_print_and_log("[%s:SUCCESS]\n", userInput[0]);
        for(i=0; i<clientNum; i++){
        cse4589_print_and_log("%-5d%-35s%-8d%-8d%-8s\n", i+1, clientList[i].hostName, clientList[i].msgSent, clientList[i].msgRecv, (clientList[i].status==0)?"logged-out":"logged-in");
	}
    }else if(type == 's' && strcmp(userInput[0], "BLOCKED") == 0){
        // get the respective client from list
        if(userInput[1] == NULL || inet_pton(AF_INET, userInput[1], &(sa.sin_addr)) != 1 || searchClientListByIp(userInput[1]) < 0){
            cse4589_print_and_log("[%s:ERROR]\n", userInput[0]);
        }else{
            i=0;
            index = searchClientListByIp(userInput[1]);
            //index = 1;
            clientNode = clientList[index];
            cse4589_print_and_log("[%s:SUCCESS]\n", userInput[0]);
		sortBlockedList(clientNode);
            for(i=0; i<clientNode.blockCount; i++){
                struct clientData blockedClient = clientNode.blockList[i];
		cse4589_print_and_log("%-5d%-35s%-20s%-8d\n", i+1, blockedClient.hostName, blockedClient.ip, blockedClient.port);
            }
        
     }   
    }else if(type == 'c' && strcmp(userInput[0], "BROADCAST") == 0 && status == 1){
        // userInput[1] will contain the message
        cse4589_print_and_log("[%s:SUCCESS]\n", userInput[0]);

        message = malloc(27 + strlen(userInput[1])); // 1 for delimiter
        strcpy(message, "BROADCAST#255.255.255.255#");
        strcat(message, userInput[1]);

        encapsulateAndSend(message, sockDesc);
        
        free(message);

    }else if(type == 'c' && strcmp(userInput[0], "BLOCK") == 0 && status == 1){

        if(userInput[1] == NULL || inet_pton(AF_INET, userInput[1], &(sa.sin_addr)) != 1
            || searchClientListByIp(userInput[1]) < 0 || isAlreadyBlocked(userInput[1]) == 1){
            cse4589_print_and_log("[%s:ERROR]\n", userInput[0]);
        }else{
            strcpy(blockedList[blockCount], userInput[1]);
            blockCount++;
            cse4589_print_and_log("[%s:SUCCESS]\n", userInput[0]);

            message = malloc(7 + strlen(userInput[1]) + 1); // 1 for delimiter
            strcpy(message, "BLOCK#");
            strcat(message, userInput[1]);

            encapsulateAndSend(message, sockDesc);
            free(message);
        }
    }else if(type == 'c' && strcmp(userInput[0], "UNBLOCK") == 0 && status == 1){ 
        if(userInput[1] == NULL || inet_pton(AF_INET, userInput[1], &(sa.sin_addr)) != 1
            || searchClientListByIp(userInput[1]) < 0 || isAlreadyBlocked(userInput[1]) != 1){
            cse4589_print_and_log("[%s:ERROR]\n", userInput[0]);
        }else{
            removeFromBlockedList(userInput[1]);
            message = malloc(9 + strlen(userInput[1]) + 1); // 1 for delimiter
            strcpy(message, "UNBLOCK#");
            strcat(message, userInput[1]);

            encapsulateAndSend(message, sockDesc);
            cse4589_print_and_log("[%s:SUCCESS]\n", userInput[0]);

            free(message);
        } 
    }else{
        cse4589_print_and_log("[%s:ERROR]\n", userInput[0]);
    }
	if(strcmp("LOGIN", userInput[0])!=0){
	cse4589_print_and_log("[%s:END]\n", userInput[0]);
	}
}

int isBlocked(char* sip, int rindex){
    struct clientData clientNode = clientList[rindex];
        for(int i=0; i<clientNode.blockCount; i++){
            if(strcmp(sip, clientNode.blockList[i].ip) == 0){
                return 1;
            }
        }
    return 0;
}


/*
 * main function
 *
 * @param  argc Number of arguments
 * @param  argv The argument list
 * @return 0 EXIT_SUCCESS
 */
int main(int argc, char **argv)
{
	/*Init. Logger*/
	cse4589_init_log(argv[2]);

	/*Clear LOGFILE*/
	fclose(fopen(LOGFILE, "w"));

	/*Start Here*/
	int i; // to loop over socket descriptors
    int j;
    int k;
    char* token;
    char* tokenStr;
    int count;
    int len;
    char* str;
    char* buf;
    char* temp;
    int numbytes;
    char* message ="";
    char* wholeMessage = "";
    char** serverInput;
    char** clientMsg;
	int isValid = 0;
	struct addrinfo *availableResult;
    int numSize = 5; // size to be recieved
    char hostName[128];

    struct clientData clientNode;
    struct clientData blockedClient;
    struct clientData senderNode;

    int index;
    int blockedIndex;
    int blockCount;
	struct linger linger;
	memset(&linger, 0, sizeof linger);
    linger.l_onoff = 1;
    linger.l_linger = 0;
	int yes = 1;

	int newfd;
	socklen_t sin_size;
	struct sockaddr_storage address;

	char s[INET_ADDRSTRLEN];
    char host[128];
	char relayIp[INET_ADDRSTRLEN];

	struct addrinfo hints;

	//fd_set masterfds; // contains all the sockets
    fd_set readfds; // sockets for read
    fd_set writefds; // sockets for write
    fdMax = STDIN;
	struct ifaddrs *ifaddr, *ifa;
    int family, sss;

    clientNum = 0;
	if(checkMainInputError(argc, argv) == -1){
		printf("error\n");	
		return -1;
	}

	// set global variables
	type = *argv[1];
	port = argv[2];

	// get current machine address
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_flags = AI_PASSIVE;

    if((isValid = getaddrinfo(NULL, port, &hints, &myaddress)) != 0){
			exit(1);
	}
	
	printf("checking ifaddrs\n");
	if (getifaddrs(&ifaddr) == -1) {
        perror("getifaddrs");
        exit(EXIT_FAILURE);
    }

//	https://stackoverflow.com/questions/4139405/how-can-i-get-to-know-the-ip-address-for-interfaces-in-c
//	The belw mentioned code gets the ip address of the interface, it is similar to ifconfig command
//	Since the interface for the server is eth0, we get the ip address for the eth0 interface and display it when the user gives IP command
    for (ifa = ifaddr; ifa != NULL; ifa = ifa->ifa_next) {
        family = ifa->ifa_addr->sa_family;

        if (family == AF_INET) {
            sss = getnameinfo(ifa->ifa_addr, sizeof(struct sockaddr_in),
                                           host, NI_MAXHOST, NULL, 0, NI_NUMERICHOST);
            if (sss != 0) {
                printf("getnameinfo() failed: %s\n", gai_strerror(sss));
                exit(EXIT_FAILURE);
            }
		if(strcmp("eth0", ifa->ifa_name) == 0){
			strcpy(ip,host);
		}
        }
    }


    FD_ZERO(&masterfds);
    FD_ZERO(&readfds);
    FD_ZERO(&writefds);

    FD_SET(STDIN, &masterfds); // add standard in to read from
	printf("finding binding\n");
	if(type == 's'){ // server needs 1 listener socket, 1 for STDIN and many for other clients
		// loop through all the results and bind to the first available
		for(availableResult = myaddress; availableResult != NULL; availableResult = availableResult->ai_next){
			if((sockDesc = socket(myaddress->ai_family, availableResult->ai_socktype, availableResult->ai_protocol)) == -1){
				// give error
				continue;
			}
			if(setsockopt(sockDesc, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(int)) == -1){
				// give error
				perror("e1:");
				exit(1);
			}
			setsockopt(sockDesc, SOL_SOCKET, SO_LINGER, &linger, sizeof(struct linger));
			if(bind(sockDesc, availableResult->ai_addr, availableResult->ai_addrlen) == -1){
				close(sockDesc);
				// give error
				continue;
			}
			break;
		}

		freeaddrinfo(myaddress);

		if(availableResult == NULL){
				// give error
	perror("e2:");			
	exit(1);
		}
		if(listen(sockDesc, BACKLOG) == -1){
			perror("p3:");	
		// give error;
			exit(1);
		}

		FD_SET(sockDesc, &masterfds);
		fdMax = sockDesc; // this is the listener socket for server

	}else{ // client needs 2 sockets, 1 for server and 1 for STDIN
	}
	while(1){

    readfds = masterfds; // copy it
    FD_SET(STDIN, &readfds);    
    if (select(fdMax+1, &readfds, NULL, NULL, NULL) == -1) {
        perror("select");
        exit(4); 
    }

    count = fdMax;
    for(i=0; i<=count; i++){
        if(FD_ISSET(i, &readfds)) {
            if(i == STDIN){
                processCommand();
            }else if(type == 's' && i == sockDesc){ // server listening on this port
                sin_size = sizeof address;

                newfd = accept(sockDesc, (struct sockaddr *)&address, &sin_size);
                // search with ip and port
                inet_ntop(address.ss_family, get_in_addr((struct sockaddr *)&address), s, sizeof s);    
		
                getnameinfo((struct sockaddr*)&address, sizeof address, host, sizeof host, NULL, 0, 0);
		index = searchClientListByIp(s);
		if(index >= 0){
                   clientList[index].port = get_in_port((struct sockaddr *)&address); 
			 clientList[index].sock = newfd;
                    clientList[index].status = 1;
                }else{
                    // add to the list of clients;
			addToClientList(newfd, s, get_in_port((struct sockaddr *)&address), host);
                }

                if(newfd > fdMax){
                    fdMax = newfd;
                }
                FD_SET(newfd, &masterfds);
		message = generateClientList();
                encapsulateAndSend(message, newfd);

                if(index >= 0 && clientList[index].messageCount > 0){
                    clientNode = clientList[index];
                    free(message);
			message = malloc(12);
                    strcpy(message, "MSG#1");
                    for(k=0; k<clientNode.messageCount; k++){
                        message = realloc(message, strlen(message)+strlen(clientNode.messageBuffer[k])+1);
                        strcat(message, clientNode.messageBuffer[k]);
			memset(clientList[index].messageBuffer[k], 0, 256);
			tokenStr = strtok(clientNode.messageBuffer[k], "#");
			token = strtok(tokenStr, "$");
			strcpy(relayIp, token);
			token = strtok(NULL, "");
			encapsulateAndSend(message, newfd);
			cse4589_print_and_log("[%s:SUCCESS]\n", "RELAYED");
                        cse4589_print_and_log("msg from:%s, to:%s\n[msg]:%s\n", relayIp, clientNode.ip, token);
                        cse4589_print_and_log("[%s:END]\n", "RELAYED");
                    }
                    clientList[index].msgRecv = clientList[index].msgRecv + clientList[index].messageCount;
			clientList[index].messageCount = 0; // reset message buffer;
                }
                free(message);
            }else if(type == 's'){ 
                // logout, exit, broadcast, block, unblock
                buf = malloc(6);
                if((numbytes = recv(i, buf, 5, 0)) <= 0){
                    if(numbytes == 0){
                    }
                    
			index = searchClientListBySock(i);
                    clientList[index].status = 0;
                    close(i);
			FD_CLR(i, &masterfds);
			
                }else{
                    len = atoi(buf);
                    free(buf);
			buf = malloc(len+1);
                    recvall(i, buf, &len);

                    clientMsg = interpretMessage(buf, &count);
                    if(strcmp(clientMsg[0], "REFRESH") == 0){
                        message = generateClientList();
                        encapsulateAndSend(message, i);
                    }else if(strcmp(clientMsg[0], "SEND") == 0){
                        // look for respective socket with IP clientMsg[1]
                        // if client is logged out , save in buffer
                        // else send 
                        j = searchClientListBySock(i);
                        senderNode = clientList[j];
                        clientList[j].msgSent++;
                        index = searchClientListByIp(clientMsg[1]);

                        if(index < 0 || isBlocked(senderNode.ip,index) == 1){
                            // client has not logged in or blocked by receiver
                        }else{
                            clientNode = clientList[index]; // search by IP
                            message = malloc((25+strlen(clientMsg[2])) * sizeof(char));

                            strcpy(message, "MSG#1#");
                            strcat(message, senderNode.ip);
                            strcat(message, "$");
                            strcat(message, clientMsg[2]);

                            wholeMessage = malloc((7+strlen(message))*sizeof(char));
                            sprintf(wholeMessage, "%5lu", strlen(message));
                            strcat(wholeMessage, message); 
                            len = strlen(wholeMessage);
                            if(clientNode.status == 1){ // logged in
                                sendall(clientNode.sock, wholeMessage, &len);
                                clientList[index].msgRecv++;
				cse4589_print_and_log("[%s:SUCCESS]\n", "RELAYED");
                            	cse4589_print_and_log("msg from:%s, to:%s\n[msg]:%s\n", senderNode.ip, clientNode.ip, clientMsg[2]);
                            	cse4589_print_and_log("[%s:END]\n", "RELAYED");
                            }else{
                                // add into buffer
                                count = clientList[index].messageCount;
                                strcpy(clientList[index].messageBuffer[count],message+5);
                                clientList[index].messageCount++;
                            }
                        }
                    }else if(strcmp(clientMsg[0], "BROADCAST") == 0){
                        k = searchClientListBySock(i);
                        senderNode = clientList[k];
                        clientList[k].msgSent++;

                        for(int j=0; j<clientNum; j++){
                            if(clientList[j].sock == i || isBlocked(senderNode.ip,j) == 1){
                                continue;
                            }
                            message = malloc((24+strlen(clientMsg[2]))*sizeof(char));
                            strcpy(message, "MSG#1#");
                            strcat(message, senderNode.ip);
                            strcat(message, "$");
                            strcat(message, clientMsg[2]);
                            if(clientList[j].status == 1){
                                encapsulateAndSend(message, clientList[j].sock);
                                clientList[j].msgRecv++;
                            }else{
                                // add into buffer
                                count = clientList[index].messageCount;
                                strcpy(clientList[index].messageBuffer[count], message+5);
                                clientList[index].messageCount++;
                            }
                            
                            free(message);
                        }
                        cse4589_print_and_log("[%s:SUCCESS]\n", "RELAYED");
                        cse4589_print_and_log("msg from:%s, to:%s\n[msg]:%s\n", senderNode.ip, "255.255.255.255", clientMsg[2]);
                        cse4589_print_and_log("[%s:END]\n", "RELAYED");

                    }else if(strcmp(clientMsg[0], "BLOCK") == 0){

                        index = searchClientListBySock(i);
                        blockedIndex = searchClientListByIp(clientMsg[1]);
                        blockCount = clientList[index].blockCount;
                        strcpy(clientList[index].blockList[blockCount].ip,clientList[blockedIndex].ip);
                        clientList[index].blockList[blockCount].port = clientList[blockedIndex].port;
                        strcpy(clientList[index].blockList[blockCount].hostName, clientList[blockedIndex].hostName);
                        clientList[index].blockCount++;
                    }else if(strcmp(clientMsg[0], "UNBLOCK") == 0){
                        unblockClient(i, clientMsg[1]);                    
                    }
                    memset(buf, 0, strlen(buf));
                    free(buf);
                }
            }else{ // client receiving message
                // just recieve the length of message
                buf = malloc(6 * sizeof(char));
                if((numbytes = recv(i, buf, 5, 0)) <= 0){
                    if(numbytes == 0){
                    }
			status = 0;
			
                    FD_CLR(i, &masterfds);
                }else{
                    // it can receive list of clients or message
                    // prefix - send, msg
                    // get total size of message
                    len = atoi(buf);
			free(buf);
                    buf = malloc(len+1);

                    recvall(i, buf, &len);

                    clientMsg = interpretMessage(buf, &count);
                    k = 2+count;
		    printf("%d--\n", k);
                    if(strcmp("MSG", clientMsg[0]) == 0){
                        for(int j=2; j<k; j++){
                            token = strtok(clientMsg[j],"$");
                            cse4589_print_and_log("[%s:SUCCESS]\n", "RECEIVED");
                            cse4589_print_and_log("msg from:%s\n[msg]:%s\n", token, clientMsg[j]+strlen(token)+1);
                            cse4589_print_and_log("[%s:END]\n", "RECEIVED");
                        }
                    }else{ // assuming sorted list
                        // hostname, ip, port
                        recreateClientList(clientMsg);
			printf("recreated\n");
                    }
			if(status == 0){
			// user is freshly logging in
			cse4589_print_and_log("[%s:SUCCESS]\n", "LOGIN");
			cse4589_print_and_log("[%s:END]\n", "LOGIN");
			status = 1;
			}
                }
            }
        }

    }
}

}
