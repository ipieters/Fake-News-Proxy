/* 
 * Igor Pieters
 * UCID: 30061116
 * CPSC 441 - Winter 2020
 * Assignment 1 - Fake News Web Proxy
 * 
 * Used tutorial client-server proxy code given as reference
 * 
 * 
 * Program Details: Client Server Proxy that receives the content of a webpage and changes the words "Floppy" and "Italy" 
 * something else, in this case to "Trolly" and "Germany" respectively. It also looks for picture containing the word "floppy" and changes
 * them to another image, in this case an image in Professor's Carey website of a trollface. 
 * 
 */

/* standard libraries*/
#include <iostream>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

/* libraries for socket programming */
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <cstdlib>

/*libraries for parsing strings*/
#include <string>
#include <regex>

/*h_addr address*/
#include <netdb.h>

/*clean exit*/
#include <signal.h>

using namespace std;

/* port numbers */
#define HTTP_PORT 80

/* string sizes */
#define MESSAGE_SIZE 2048
const regex r("[fF]loppy[a-zA-Z]*");
const regex r1(" Italy");
const regex img("<img[^>]+src=\"([^>]+Floppy+[^>\"]+[^>]+[>])");
const regex ahref("<a[^>]+href=\"([^>]+Floppy+[^>\"]+[^>]+[>])");
const regex content("Content-Length:( +[0-9]*)");

int lstn_sock, data_sock, web_sock;

void cleanExit(int sig)
{
	close(lstn_sock);
	close(data_sock);
	close(web_sock);
	exit(0);
}

/**
 * Receives proxy port number as parameter and initializes socket client socket 
 * connection. Starts listening for a server. 
 ***/

void initializeConnection(int proxyPort)
{
	/* to handle ungraceful exits */
	signal(SIGTERM, cleanExit);
	signal(SIGINT, cleanExit);

	//initialize proxy address
	printf("Initializing proxy address...\n");
	struct sockaddr_in proxy_addr;
	proxy_addr.sin_family = AF_INET;
	proxy_addr.sin_port = htons(proxyPort);
	proxy_addr.sin_addr.s_addr = htonl(INADDR_ANY);

	//create listening socket
	printf("Creating lstn_sock...\n");
	lstn_sock = socket(AF_INET, SOCK_STREAM, 0);
	if (lstn_sock < 0)
	{
		perror("socket() call failed");
		exit(-1);
	}

	//bind listening socket
	printf("Binding lstn_sock...\n");
	if (bind(lstn_sock, (struct sockaddr *)&proxy_addr, sizeof(struct sockaddr_in)) < 0)
	{
		perror("bind() call failed");
		exit(-1);
	}

	//listen for client connection requests
	printf("Listening on lstn_sock...\n");
	if (listen(lstn_sock, 20) < 0)
	{
		perror("listen() call failed...\n");
		exit(-1);
	}
}

/**
 *  Sends clients requests to the server. Stores client request bytes 
 * 
 ***/
void processClientRequest(char client_request[], char server_request[], char host[], char url[], char path[])
{
	int i;
	printf("%s\n", client_request);

	//copy to server_request to preserve contents (client_request will be damaged in strtok())
	strcpy(server_request, client_request);

	//tokenize to get a line at a time
	char *line = strtok(client_request, "\r\n");

	//extract url
	sscanf(line, "GET http://%s", url);

	//separate host from path
	for (i = 0; i < strlen(url); i++)
	{
		if (url[i] == '/')
		{
			strncpy(host, url, i);
			host[i] = '\0';
			break;
		}
	}
	bzero(path, MESSAGE_SIZE);
	strcat(path, &url[i]);

	printf("Host: %s, Path: %s\n", host, path);

	//initialize server address
	printf("Initializing server address...\n");
	struct sockaddr_in server_addr;
	struct hostent *server;
	server = gethostbyname(host);

	bzero((char *)&server_addr, sizeof(struct sockaddr_in));
	server_addr.sin_family = AF_INET;
	server_addr.sin_port = htons(HTTP_PORT);
	bcopy((char *)server->h_addr, (char *)&server_addr.sin_addr.s_addr, server->h_length);

	//create web_socket to communicate with web_server
	web_sock = socket(AF_INET, SOCK_STREAM, 0);
	if (web_sock < 0)
	{
		perror("socket() call failed\n");
	}

	//send connection request to web server
	if (connect(web_sock, (struct sockaddr *)&server_addr, sizeof(struct sockaddr_in)) < 0)
	{
		perror("connect() call failed\n");
	}

	//send http request to web server
	if (send(web_sock, server_request, MESSAGE_SIZE, 0) < 0)
	{
		perror("send(0 call failed\n");
	}
}

/*****
 *  Changes the contents of the server response and sends it to the client with the 
 *  update content and server bytes size
 *****/

void processServerResponse(char client_response[], char server_response[], int serverBytes)
{
	string server_resp_str = server_response;
	int preChange = server_resp_str.length();
	server_resp_str = regex_replace(server_resp_str, ahref, "<a href=\"./trollface.jpg\">");
	server_resp_str = regex_replace(server_resp_str, img, "<img src=\"./trollface.jpg\" width=\"200px\" height=\"400px\">");
	server_resp_str = regex_replace(server_resp_str, r, "Trolly");
	server_resp_str = regex_replace(server_resp_str, r1, " Germany");
	int differenceInChange = server_resp_str.length() - preChange;

	int pos = server_resp_str.find("UTF-8\r\n\r\n");
	if ((pos != -1) || differenceInChange != 0)
	{
		string sub = server_resp_str.substr(pos + 9);
		int len = sub.length();
		//  printf("%s\n", server_resp_str.c_str());
		server_resp_str = regex_replace(server_resp_str, content, "Content-Length: " + to_string(len));
		// printf("%s\n", server_resp_str.c_str()); //Uncomment this to
		bcopy(server_resp_str.c_str(), client_response, serverBytes + differenceInChange);

		//send http response to client
		if (send(data_sock, client_response, serverBytes + differenceInChange, 0) < 0)
		{
			perror("send() call failed...\n");
		}
	}
	else
	{
		//we are not modifying here, just passing the response along
		bcopy(server_response, client_response, serverBytes);

		//send http response to client
		if (send(data_sock, client_response, serverBytes, 0) < 0)
		{
			perror("send() call failed...\n");
		}
	}
	bzero(client_response, MESSAGE_SIZE);
	bzero(server_response, MESSAGE_SIZE);
}

/*****
 *  After socket initialization has been completed, this function processes the client request and server response 
 * 
 *****/

void processClientServer()
{
	char client_request[MESSAGE_SIZE], server_request[MESSAGE_SIZE], server_response[10 * MESSAGE_SIZE], client_response[10 * MESSAGE_SIZE];
	char url[MESSAGE_SIZE], host[MESSAGE_SIZE], path[MESSAGE_SIZE];
	int clientBytes, serverBytes;
	//while loop to receive client requests
	while ((clientBytes = recv(data_sock, client_request, MESSAGE_SIZE, 0)) > 0)
	{
		processClientRequest(client_request, server_request, host, url, path);
		//receive http response from server
		serverBytes = 0;
		while ((serverBytes = recv(web_sock, server_response, MESSAGE_SIZE, 0)) > 0)
		{
			processServerResponse(client_response, server_response, serverBytes);

		} //while recv() from server
	}	 //while recv() from client
}

int main(int argc, char *argv[])
{
	int pid;
	int proxyPort = atoi(argv[1]);

	initializeConnection(proxyPort);

	//infinite while loop for listening
	while (1)
	{
		printf("Accepting connections...\n");

		//accept client connection request
		data_sock = accept(lstn_sock, NULL, NULL);
		if (data_sock < 0)
		{
			puts("accept() call failed\n");
			exit(-1);
		}
		pid = fork();
		if (pid < 0)
			puts("error on fork\n");
		if (pid == 0)
		{
			close(lstn_sock);
			processClientServer();
			exit(0);
		}
		else
			close(data_sock);
	} //infinite loop
	close(lstn_sock);
	return 0;
}