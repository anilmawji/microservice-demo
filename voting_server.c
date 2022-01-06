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

#define PORT 9046
#define NUM_CANDIDATES 4
#define ENCRYPT_KEY "9"

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
 * Adds 1 to the vote count of a candidate based given their id
 */ 
int addVote(int id, char *ids[NUM_CANDIDATES], int votes[NUM_CANDIDATES]) {
    for (int i = 0; i < NUM_CANDIDATES; i++) {
        //Found a matching id
        if (atoi(ids[i]) == id)  {
            votes[i]++;
            return i;
        }
    }
    return -1;
}

/**
 * Writes the candidate info (name and id) to a given string
 * 
 * @param dest:       string to hold the results
 * @param candidates: list of candidate names
 * @param ids:        list of candidate ids
 */
void sprintCandidates(char *dest, char *candidates[NUM_CANDIDATES], char *ids[NUM_CANDIDATES]) {
    strcpy(dest, "ID\tName\n--\t----\n");

    //Loop through all candidates, writing the info for each into dest
    for (int i = 0; i < NUM_CANDIDATES; i++) {
        strcat(dest, ids[i]);
        strcat(dest, "\t");
        strcat(dest, candidates[i]);
        strcat(dest, "\n");
    }
}

/**
 * Writes the voting results to a given string
 * 
 * @param dest:       string to hold the results
 * @param candidates: list of candidate names
 * @param ids:        list of candidate ids
 * @param votes:      list of candidate vote counts
 */
void sprintResults(char *dest, char *candidates[NUM_CANDIDATES], char *ids[NUM_CANDIDATES], int votes[NUM_CANDIDATES]) {
    strcpy(dest, "Votes\tID\tName\n-----\t--\t----\n");

    char buffer[10];
    memset(&buffer, 0, 10);

    //Loop through all candidates, writing the info for each into dest
    for (int i = 0; i < NUM_CANDIDATES; i++) {
        sprintf(buffer, "%d", votes[i]);
        strcat(dest, buffer);
        strcat(dest, "\t");
        strcat(dest, ids[i]);
        strcat(dest, "\t");
        strcat(dest, candidates[i]);
        strcat(dest, "\n");
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
    char *candidates[NUM_CANDIDATES] = {"Dennis Ritchie", "Linus Torvalds", "Bill Gates", "Gordon Moore"};
    char *ids[NUM_CANDIDATES] = {"101", "202", "303", "404"};
    int votes[NUM_CANDIDATES] = {89, 62, 70, 50};

    //String for holding incoming/outgoing network data
    char buffer[MAX_BUFFER_SIZE];
    memset(buffer, 0, MAX_BUFFER_SIZE);

    //Print info about the microservice
    sprintCandidates(buffer, candidates, ids);
    printf("%s\n", buffer);

    //Placeholder info for communicating with indirection server
    struct sockaddr_in server;

    int sock_len = sizeof(struct sockaddr_in);
    int done = FALSE;
    int bytes;

	while (!done) {
        memset(buffer, 0, MAX_BUFFER_SIZE);

        if (bytes = recvfrom(server_fd, buffer, MAX_BUFFER_SIZE, 0, (struct sockaddr *) &server, &sock_len) > 0) {
            if (strcmp(buffer, "key_req") == 0) {
                //Indirection server has requested the encryption key
                memset(buffer, 0, MAX_BUFFER_SIZE);
                strcpy(buffer, ENCRYPT_KEY);
            } else {
                //Get the input choice entered by the client
                int input = atoi(buffer);
                memset(buffer, 0, MAX_BUFFER_SIZE);

                if (input == 3) {
                    //Show candidate info
                    sprintCandidates(buffer, candidates, ids);
                } else if (input == 5) {
                    //Show voting results
                    sprintResults(buffer, candidates, ids, votes);
                } else {
                    //Unknown number received... we will assume it is an encrypted id!
                    //Decrypt the id retrieved from indirection server
                    int id = input / atoi(ENCRYPT_KEY), i;

                    //Add 1 to the vote count of the corresponding candidate
                    if ((i = addVote(id, ids, votes)) == -1) {
                        //The id provided was invalid
                        strcpy(buffer, "Invalid candidate ID, please try again.");
                    } else {
                        sprintf(buffer, "Your vote for %s has been added!", candidates[i]);
                    }
                }
            }
            //Send result message back to indirection server
            sendto(server_fd, buffer, sizeof(buffer), 0, (struct sockaddr *) &server, sock_len);
        }
	}
	close(server_fd);
	
	return 0;
}
