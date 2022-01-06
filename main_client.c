#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <netdb.h>
#include <netinet/in.h>
#include <signal.h>
#include <arpa/inet.h>
#include <string.h>

#define TRUE 1
#define FALSE 0

#define MAX_BUFFER_SIZE 2048

#define INDIR_SERVER_ADDR "136.159.5.25"
#define INDIR_SERVER_PORT 9043

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
 * Cycles through extraneous characters left in the input buffer
 */
int clearInputStream() {
	while (getchar() != '\n');
    
	return TRUE;
}

/**
 * Repeatedly asks the user for an int until a valid integer is provided
 */
int userInt(const char *message) {
	int input;
	char c;

	printf("%s", message);

    // Cycle through characters in the input buffer, parsing them as an int
    while (((scanf("%d%c", &input, &c) == 0 || c != '\n') && clearInputStream())) {
        printf("Invalid input. Please enter an integer.\n%s", message);
    }
	return input;
}

/**
 * Repeatedly asks the user for an int until a valid integer is provided
 * The int must also be within the bounds of min and max
 */
int userIntRange(const char *message, int min, int max) {
	int input;
	char c;

	printf("%s", message);

    // Cycle through characters in the input buffer, parsing them as an int
    while (((scanf("%d%c", &input, &c) == 0 || c != '\n') && clearInputStream()) || input < min || input > max) {
        printf("Invalid input. Please enter an integer (%d-%d).\n%s", min, max, message);
    }
	return input;
}

/**
 * Prints the correct usage of executing the program
 */
void usageError(const char *message, const char *invoke) {
	printf("%s\n", message);
    fprintf(stderr, "Usage: %s <Server IP> <Server Port>\n", invoke);
	exit(1);
}

/**
 * Prints the options for the various microservices to choose from
 */
void printMenu() {
    printf("\nMicroservice Menu\n");
    printf("-----------------\n");
    printf("(1) Translator\n");
    printf("(2) Currency converter\n");
    printf("(3) Show candidates\n");
    printf("(4) Vote for candidate\n");
    printf("(5) Voting summary\n");
    printf("(6) End connection\n");
}

/**
 * Initializes and returns a client socket for communicating with the indirection server
 */
int initClient(const char *indir_server_addr, int indir_server_port) {
    //Specify indirection server info
    struct sockaddr_in server;
    memset(&server, 0, sizeof(server));
    server.sin_family = AF_INET;
    server.sin_port = htons(indir_server_port);
    server.sin_addr.s_addr = inet_addr(indir_server_addr);

    //Create timeval for a recv timeout of 5 seconds
    struct timeval timeout;      
    timeout.tv_sec = 5;
    timeout.tv_usec = 0;

    int client_fd;

    //Create client socket and connect to the server
    check((client_fd = socket(PF_INET, SOCK_STREAM, 0)), "socket", TRUE);
    //Enable socket timeout on the recv function
    setsockopt(client_fd, SOL_SOCKET, SO_RCVTIMEO, (char *) &timeout, sizeof(timeout));
    //Connect to indirection server
    check(connect(client_fd, (struct sockaddr*) &server, sizeof(server)), "connect", TRUE);

    return client_fd;
}

int main(int argc, const char *argv[]) {
    //Check if the cmd line args are of the proper format
    if (argc != 3) usageError("Invalid number of arguments!", argv[0]);
    if (strcmp(argv[1], INDIR_SERVER_ADDR) != 0) usageError("Invalid server IP!", argv[0]);
    if (atoi(argv[2]) != INDIR_SERVER_PORT) usageError("Invalid server port!", argv[0]);

    int client_fd = initClient(INDIR_SERVER_ADDR, INDIR_SERVER_PORT);

    //String for holding incoming/outgoing network data
	char buffer[MAX_BUFFER_SIZE];
	memset(&buffer, 0, MAX_BUFFER_SIZE);

    int done = FALSE;
    int canShowResults = FALSE;
    int bytes, choice, id, sendInput, key;
    char input[10];
    
    while (!done) {
        //Display the menu to the user
        printMenu();
        choice = userIntRange("Enter the corresponding number of an option:\n", 1, 6);
        sendInput = FALSE;

        if (choice == 1) {
            //User chose the translation microservice
            printf("Enter an English word:\n");
            sendInput = TRUE;
        } else if (choice == 2) {
            //User chose the currency microservice
            printf("Enter a currency conversion (INT|CUR|CUR):\n");
            sendInput = TRUE;
        } else if (choice == 4) {
            //User chose the voting microservice
            id = userInt("Enter the ID of the candidate you want to vote for:\n");
        } else if (choice == 5 && canShowResults == FALSE) {
            //User chose to show voting results
            printf("You must vote before you view the results!\n");
            continue;
        } else if (choice == 6) {
            //User chose to quit the program
            printf("Closing connection...\n");
            break;
        }

        //Send input choice to the indirection server
        memset(&buffer, 0, MAX_BUFFER_SIZE);
        sprintf(buffer, "%d", choice);
        check(send(client_fd, buffer, strlen(buffer), 0), "send", FALSE);

        if (sendInput == TRUE) {
            //The microservice chosen by the user requires additional data to be sent to indirection server
            scanf("%s", input);
            check(send(client_fd, input, strlen(input), 0), "send", FALSE);
        } else if (choice == 4) {
            //Get encryption key from indirection server
            memset(&buffer, 0, MAX_BUFFER_SIZE);
            recv(client_fd, buffer, MAX_BUFFER_SIZE, 0);
            key = atoi(buffer);
            
            memset(&buffer, 0, MAX_BUFFER_SIZE);
            //Send encrypted id to indirection server
            //encrypted_key = id * key
            sprintf(buffer, "%d", id * key);
            check(send(client_fd, buffer, strlen(buffer), 0), "send", FALSE);
        }
        //Get response from indirection server
        memset(&buffer, 0, MAX_BUFFER_SIZE);
        bytes = recv(client_fd, buffer, MAX_BUFFER_SIZE, 0);

        if (bytes <= 0) {
            //No bytes were received back from indirection server (the microserver is most likely not running)
            strcpy(buffer, "Connection timed out: requested microserver is not responding. Please try again later!");
            //Reset TCP connection to recover from timeout
            close(client_fd);
            client_fd = initClient(INDIR_SERVER_ADDR, INDIR_SERVER_PORT);
        }
        //The user voted successfully, and is now allowed to show voting results
        if (canShowResults == FALSE && strstr(buffer, "vote")) {
            canShowResults = TRUE;
        }
        //Print response from indirection server
        printf("Server response:\n%s\n", buffer);
        memset(&buffer, 0, MAX_BUFFER_SIZE);
    }
    close(client_fd);

    return 0;
}
