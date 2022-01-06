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

#define PORT 9045
#define NUM_CURRENCIES 5

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
 * Converts a source currency to the equivalent amount in a destination currency
 * 
 * @param amount:      quantity of the source currency
 * @param source:      name of the source currency
 * @param dest:        name of the destination currency
 * @param currencies:  list of all currency names
 * @param conversions: list of currency conversion rates relative to CAD
 */
float convert(int amount, char *source, char *dest, char *currencies[NUM_CURRENCIES], float conversions[NUM_CURRENCIES]) {
    //Handle edge cases... 0 of any currency is always 0
    if (amount == 0) return 0;
    //Both a source and dest currency must be specified
    if (source == NULL || dest == NULL) return -1;
    
    float res = -1;
    //Booleans indicating whether valid currency names were given
    int validSource = FALSE;
    int validDest = FALSE;

    if (amount > 0) {
        // If it isn't already, convert the source currency to the default currency (CAD)
        if (strcmp(source, currencies[0]) != 0) {
            for (int i = 0; i < NUM_CURRENCIES; i++) {
                //Find the conversion factor for converting the source currency to CAD
                if (strcmp(source, currencies[i]) == 0) {
                    //To convert to CAD, we use the reciprocal of the value provided in conversions
                    res = ((float) amount) * (1.0 / conversions[i]);
                    validSource = TRUE;
                    break;
                }
            }
        } else {
            validSource = TRUE;
        }
        //Covert the source currency (now in CAD), to the dest currency
        for (int i = 0; i < NUM_CURRENCIES; i++) {
            //Find the conversion factor for converting to the dest currency
            if (strcmp(dest, currencies[i]) == 0) {
                // If the original source currency was CAD, use amount
                //Otherwise, use the new value of res from the previous step
                res = (res == -1 ? amount : res) * conversions[i];
                validDest = TRUE;
                break;
            }
        }
    }
    //Return error if a valid source/dest currency was not provided
    return validSource && validDest ? res : -1;
}

/**
 * Splits a string into an array of strings about a given delimitor
 * Eg. "a|b|c" split about '|' becomes 3 separate strings a, b, c
 * 
 * @param source:      the string to split
 * @param dest:        array to hold the split strings
 * @param delim:       the delimitor character
 */
int split(char *source, char dest[5][10], char delim) {
	int i = 0, j = 0, n = 0;
	
	do {
        //Keep iterating until we find an occurrence of delim
		if (source[i] != delim) {
			dest[n][j++] = source[i];
		} else {
            //We found a delim character, so insert the string into dest
			dest[n++][j] = '\0';
            //Reset counter
			j = 0;
		}
	} while (source[i++] != '\0');
    
	return n+1;
}

/*
 * Prints useful info about the microserver, including the conversion rates
 */
void printStartup(char *currencies[NUM_CURRENCIES], float conversions[NUM_CURRENCIES]) {
    printf("Currency\tMult Factor (CAD)\n--------\t-----------------\n");

    for (int i = 0; i < NUM_CURRENCIES; i++) {
        printf("%s\t\t%.2f\n", currencies[i], conversions[i]);
    }
}

/**
 * Prints a string buffer and its size in bytes to the console for testing
 */
void printBuffer(const char *sender, const char *buffer, int bytes) {
    printf("%s: %s (%ld bytes)\n", sender, buffer, bytes);
}

/**
 * Initializes and returns a socket for communicating with the indirection server
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
    check((server_fd = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP)), "socket", TRUE);
    //Allow address to be reused
    check(setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &(int){1}, sizeof(int)), "setsockopt", TRUE);
    check(bind(server_fd, (struct sockaddr*) &server, sizeof(server)), "bind", TRUE);

    return server_fd;
}

int main() {
	int server_fd = initServer(PORT);

    //Important microservice info
    char *currencies[NUM_CURRENCIES] = {"CAD", "USD", "EUR", "GBP", "BTC"};
    float conversions[NUM_CURRENCIES] = {1, 0.81, 0.70, 0.59, 0.00001277};

    //String for holding incoming/outgoing network data
    char buffer[MAX_BUFFER_SIZE];
	memset(&buffer, 0, MAX_BUFFER_SIZE);

    //Print info about the microservice
    printStartup(currencies, conversions);

    //Placeholder info for communicating with indirection server
    struct sockaddr_in server;

    int sock_len = sizeof(struct sockaddr_in);
    int done = FALSE;
    int bytes;

	while (!done) {
        memset(buffer, 0, MAX_BUFFER_SIZE);

        if (bytes = recvfrom(server_fd, buffer, MAX_BUFFER_SIZE, 0, (struct sockaddr *) &server, &sock_len) > 0) {
            //Get conversion string from the buffer and split it about the '|' char
            //Store the 3 individual strings in input
            char input[5][10];
            int n = split(buffer, input, '|');

            memset(buffer, 0, MAX_BUFFER_SIZE);

            if (n == 3) {
                //We received the 3 desired separate strings (amount, source, dest)
                //Convert the currency
                float amount = convert(atoi(input[0]), input[1], input[2], currencies, conversions);

                if (amount >= 0) {
                    sprintf(buffer, "%.2f", amount);
                } else {
                    //convert() returned error code -1
                    strcpy(buffer, "Invalid input, please try again.");
                }
            } else {
                //User input did not split into 3 strings
                strcpy(buffer, "Invalid input, please try again.");
            }
            //Send result message back to indirection server
            sendto(server_fd, buffer, sizeof(buffer), 0, (struct sockaddr *) &server, sock_len);
        }
	}
	close(server_fd);
	
	return 0;
}
