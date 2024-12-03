#include <stdio.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <stdlib.h>
#include <netdb.h>
#include <unistd.h>
#include <string.h>
#include <regex.h>
#include <termios.h>

#define MAX_LENGTH  500
#define FTP_PORT    21

/* Server responses */
#define SV_READY4AUTH           220
#define SV_READY4PASS           331
#define SV_LOGINSUCCESS         230
#define SV_PASSIVE              227
#define SV_READY4TRANSFER       150
#define SV_TRANSFER_COMPLETE    226
#define SV_GOODBYE              221

/* Parser regular expressions */
#define AT              "@"
#define BAR             "/"
#define HOST_REGEX      "%*[^/]//%[^/]"
#define HOST_AT_REGEX   "%*[^/]//%*[^@]@%[^/]"
#define RESOURCE_REGEX  "%*[^/]//%*[^/]/%s"
#define USER_REGEX      "%*[^/]//%[^:/]"
#define PASS_REGEX      "%*[^/]//%*[^:]:%[^@\n$]"
#define RESPCODE_REGEX  "%d"
#define PASSIVE_REGEX   "%*[^(](%d,%d,%d,%d,%d,%d)%*[^\n$)]"

/* Default login for case 'ftp://<host>/<url-path>' */
#define DEFAULT_USER        "anonymous"
#define DEFAULT_PASSWORD    "password"
/*
Esta struct é usada para guardar os dados da URL
Host -> Representa o nome do servidor ex 'ftp.up.pt'
Resource -> Representa o caminho para o ficheiro ex 'parrot/misc/canary/warrant-canary-0.txt'
File -> Representa o nome do ficheiro ex 'warrant-canary-0.txt'
User -> Representa o nome do utilizador ex 'username'
Password -> Representa a password do utilizador ex 'password'
IP -> Representa o endereço IP do servidor ex '193.137.29.15'
*/
struct URL {
    char host[MAX_LENGTH];      
    char resource[MAX_LENGTH];  
    char file[MAX_LENGTH];      
    char user[MAX_LENGTH];      
    char password[MAX_LENGTH];  
    char ip[MAX_LENGTH];       
};

/* Machine states that receives the response from the server */
typedef enum {
    START,
    SINGLE,
    MULTIPLE,
    END
} ResponseState;

/* 
* Parser that transforms user input in url parameters
* @param input, a string containing the user input
* @param url, a struct that will be filled with the url parameters
* @return 0 if there is no parse error or -1 otherwise
*/
int parse(char *input, struct URL *url);

/* 
* Create socket file descriptor based on given server ip and port
* @param ip, a string containing the server ip
* @param port, an integer value containing the server port
* @return socket file descriptor if there is no error or -1 otherwise
*/
int createSocket(char *ip, int port);

/* 
* Authenticate connection
* @param socket, server connection file descriptor
* @param user, a string containing the username
* @param pass, a string containing the password
* @return server response code obtained by the operation
*/
int authConn(const int socket, const char *user, const char *pass);

/* 
* Read server response
* @param socket, server connection file descriptor
* @param buffer, string that will be filled with server response
* @return server response code obtained by the operation
*/
int readResponse(const int socket, char *buffer);

/* 
* Enter in passive mode
* @param socket, server connection file descriptor
* @param ip, string that will be filled with data connection ip
* @param port, string that will be filled with data connection port
* @return server response code obtained by the operation
*/
int passiveMode(const int socket, char* ip, int *port);

/* 
* Request resource
* @param socket, server connection file descriptor
* @param resource, string that contains the desired resource
* @return server response code obtained by the operation
*/
int requestResource(const int socket, char *resource);

/* 
* Get resource from server and download it in current directory
* @param socketA, server connection file descriptor
* @param socketB, server connection file descriptor
* @param filename, string that contains the desired file name
* @return server response code obtained by the operation
*/
int getResource(const int socketA, const int socketB, char *filename);

/* 
* Closes the server connection and the socket itself
* @param socketA, server connection file descriptor
* @param socketB, server connection file descriptor
* @return 0 if there is no close error or -1 otherwise
*/
int closeConnection(const int socketA, const int socketB);
