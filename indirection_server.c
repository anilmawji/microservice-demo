#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <netdb.h>
#include <netinet/in.h>
#include <signal.h>
#include <arpa/inet.h>
#include <sys/select.h>
#include <sys/time.h>
#include <sys/types.h>
#include <string.h>

#define TRUE 1
#define FALSE 0

#define MAX_BUFFER_SIZE 2048

#define INDIR_SERVER_ADDR "136.159.5.25"
#define INDIR_SERVER_PORT 9043

#define TRAN_SERVER_ADDR "136.159.5.25"
#define TRAN_SERVER_PORT 9044

#define CURR_SERVER_ADDR "136.159.5.25"
#define CURR_SERVER_PORT 9045

#define VOTE_SERVER_ADDR "136.159.5.25"
#define VOTE_SERVER_PORT 9046

/**
 * Check whether a function has returned an error code and exit the program if necessary
 * Prints the relevant error to the console
 */
int check(int status, char *function_name, int can_exit) {
    if (status < 0) {
        fprintf(stderr, "[ERROR]: %s() call has failed!\n", function_name);
        perror(function_name);
        if (can_exit) {
            exit(1);
        }
    }
    return status;
}

/**
 * Prints a string buffer and its size in bytes to the console for testing
 */
void printBuffer(const char *sender, const char *buffer, int bytes) {
    printf("%s: %s (%ld bytes)\n", sender, buffer, bytes);
}

/**
 * Initializes and returns a server socket for communicating with the client
 */
int initServer(int server_port) {
	//Specify indirection server info
	struct sockaddr_in server;
	memset(&server, 0, sizeof(server));
	server.sin_family = AF_INET;
	server.sin_port = htons(server_port);
	server.sin_addr.s_addr = htonl(INADDR_ANY);
	
	int server_fd;
	
	//Create server socket and bind it to the server info
	check((server_fd = socket(PF_INET, SOCK_STREAM, 0)), "socket", TRUE);
	//Allow address to be reused
	check(setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &(int){1}, sizeof(int)), "setsockopt", TRUE);
	check(bind(server_fd, (struct sockaddr *) &server, sizeof(struct sockaddr_in)), "bind", TRUE);
	//Start listening for incoming connections
	check(listen(server_fd, 5), "listen", TRUE);
	
	return server_fd;
}

int main() {
	int server_fd = initServer(INDIR_SERVER_PORT);

	printf("[SERVER]: Listening for connections...\n");

	//Variables for dealing with a client
	int client_fd;
    struct sockaddr_in client_in;
	int sock_len = sizeof(struct sockaddr_in);
	int pid;

	while (TRUE) {
		//Found a new connection request
		check((client_fd = accept(server_fd, (struct sockaddr *) &client_in, (socklen_t *) &sock_len)), "accept", TRUE);

		pid = fork();

		if (pid == 0) {
			//Run the client connection on a new thread
			//Close the server sock since we don't need it in this thread
			close(server_fd);

			//Specify microservice server info
			struct sockaddr_in micro;
			memset(&micro, 0, sizeof(micro));
			micro.sin_family = AF_INET;

			//String for holding incoming/outgoing network data
			char buffer[MAX_BUFFER_SIZE];
			memset(&buffer, 0, MAX_BUFFER_SIZE);

			int done, bytes;

			while (!done) {
				memset(&buffer, 0, MAX_BUFFER_SIZE);

				if ((bytes = recv(client_fd, buffer, MAX_BUFFER_SIZE, 0)) > 0) {
					// Receive choice data from the client
					int choice = atoi(buffer);
					int getInput = TRUE;

					//Create socket for connecting to microserver
					int micro_fd;
					check((micro_fd = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP)), "socket", TRUE);

					//Set the microservice IP based on the service selected by the user
					if (choice == 1) {
						//User selected choice 1, the translation microservice
						micro.sin_port = htons(TRAN_SERVER_PORT);
						micro.sin_addr.s_addr = inet_addr(TRAN_SERVER_ADDR);
					} else if (choice == 2) {
						//User selected choice 2, currency microservice
						micro.sin_port = htons(CURR_SERVER_PORT);
						micro.sin_addr.s_addr = inet_addr(CURR_SERVER_ADDR);
					} else if (choice >= 3 && choice <= 5) {
						//User selected the voting microservice
						micro.sin_port = htons(VOTE_SERVER_PORT);
						micro.sin_addr.s_addr = inet_addr(VOTE_SERVER_ADDR);
						getInput = FALSE;

						if (choice == 4) {
							//User has chosen to vote for a candidate
							memset(&buffer, 0, MAX_BUFFER_SIZE);
							strcpy(buffer, "key_req");
							//Request key from voting server
							check(sendto(micro_fd, buffer, sizeof(buffer), 0, (struct sockaddr *) &micro, sock_len), "sendto", FALSE);
							memset(&buffer, 0, MAX_BUFFER_SIZE);
							//Get key from voting server
							recvfrom(micro_fd, buffer, MAX_BUFFER_SIZE, 0, (struct sockaddr *) &micro, &sock_len);
							//Forward the key over to client
							check(send(client_fd, buffer, strlen(buffer), 0), "send", FALSE);
							//Get encrypted id from client...
							memset(&buffer, 0, MAX_BUFFER_SIZE);
							recv(client_fd, buffer, MAX_BUFFER_SIZE, 0);
							//Forward encrypted id over to voting server... happens below at the next sendto call
						}
					}
					if (getInput == TRUE) {
						//The user sent additional info on top of the choice
						memset(&buffer, 0, MAX_BUFFER_SIZE);
						recv(client_fd, buffer, MAX_BUFFER_SIZE, 0);
					}
					//Send the user request to the microserver
					check(sendto(micro_fd, buffer, sizeof(buffer), 0, (struct sockaddr *) &micro, sock_len), "sendto", FALSE);

					//Get the response from the microserver
					memset(&buffer, 0, MAX_BUFFER_SIZE);
					recvfrom(micro_fd, buffer, MAX_BUFFER_SIZE, 0, (struct sockaddr *) &micro, &sock_len);

					//Forward the microserver's response back to client
					check(send(client_fd, buffer, strlen(buffer), 0), "send", FALSE);
					//We are finished communicating with the microserver
					close(micro_fd);
				} else {
					//The user is no longer sending data, so we can end this thread
					done = TRUE;
				}
			}
			//End client connection with indirection server
			close(client_fd);
		}
	}
	close(server_fd);
	
	return 0;
}
