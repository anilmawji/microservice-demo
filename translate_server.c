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

#define PORT 9044
#define NUM_WORDS 5

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
 * Translates a given English word to French
 * 
 * @param word:          a word in English, will become the translated word in French
 * @param english_words: list of english words
 * @param french_words:  list of french words
 */
int translate(char *word, char *english_words[NUM_WORDS], char *french_words[NUM_WORDS]) {
    for (int i = 0; i < NUM_WORDS; i++) {
        //English word was found in the list
        if (strcmp(word, english_words[i]) == 0) {
            memset(word, 0, strlen(word));
            //Write the corresponding french word to the word string
            strcpy(word, french_words[i]);
            return 0;
        }
    }
    return -1;
}

/*
 * Prints useful info about the microserver, including the conversion rates
 */
void printStartup(char *words[NUM_WORDS], char *translated[NUM_WORDS]) {
    printf("English\t\tFrench\n-------\t\t------\n");

    for (int i = 0; i < NUM_WORDS; i++) {
        printf("%s\t\t%s\n", words[i], translated[i]);
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
    char *english_words[NUM_WORDS] = {"hello", "school", "book", "boy", "girl"};
    char *french_words[NUM_WORDS] = {"bonjour", "ecole", "livre", "garcon", "fille"};

    //String for holding incoming/outgoing network data
    char buffer[MAX_BUFFER_SIZE];
	memset(&buffer, 0, MAX_BUFFER_SIZE);

    printStartup(english_words, french_words);
    
    //Placeholder info for communicating with indirection server
    struct sockaddr_in server;

    int sock_len = sizeof(struct sockaddr_in);
    int done = FALSE;
    int bytes;

	while (!done) {
        memset(buffer, 0, MAX_BUFFER_SIZE);

        if (bytes = recvfrom(server_fd, buffer, MAX_BUFFER_SIZE, 0, (struct sockaddr *) &server, &sock_len) > 0) {
            //Attempt to translate the data received from indirection server
            if (translate(buffer, english_words, french_words) == -1) {
                memset(buffer, 0, MAX_BUFFER_SIZE);
                strcpy(buffer, "Invalid word, please try again.");
            }
            //Send result message back to indirection server
            sendto(server_fd, buffer, sizeof(buffer), 0, (struct sockaddr *) &server, sock_len);
        }
	}
	close(server_fd);
	
	return 0;
}
