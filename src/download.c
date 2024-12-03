#include "../include/download.h"

//Função que é responsável para dar parse ao input do utilizador e preencher a estrutura URL
int parse(char *input, struct URL *url) {

    regex_t regex;  //estrutura que guarda a expressão regular
    regcomp(&regex, BAR, 0);    //compila a expressão regular defeinida pela barra
    
    if (regexec(&regex, input, 0, NULL, 0)){
        //A URL não contém '/' logo não tem recurso
        printf("Resource not found\n");
        return -1;
    } 

    regcomp(&regex, AT, 0); //compila a expressão regular definida pelo arroba
    if (regexec(&regex, input, 0, NULL, 0) != 0) { //ftp://<host>/<url-path>
        
        //A URL não contém '@' logo não tem user nem password

        sscanf(input, HOST_REGEX, url->host);
        strcpy(url->user, DEFAULT_USER); //'anonymous'
        strcpy(url->password, DEFAULT_PASSWORD); //'password'

    } else { // ftp://[<user>:<password>@]<host>/<url-path>

        //A URL contém '@' logo tem user e password

        sscanf(input, HOST_AT_REGEX, url->host);
        sscanf(input, USER_REGEX, url->user);
        sscanf(input, PASS_REGEX, url->password);
    }

    //Utiliza uma expressão regular para obter o recurso da URL
    sscanf(input, RESOURCE_REGEX, url->resource);
    
    //extrai o nome do ficheiro da URL apratir da ultima barra
    strcpy(url->file, strrchr(input, '/') + 1);

    //estrutura que guarda os dados do servidor
    struct hostent *h;
    
    //verifica se o host foi preenchido
    if (strlen(url->host) == 0){
        printf("Host not found\n");
        return -1;
    } 

    //Função que retorna um ponteiro para a estrutura hostent contendo informações sobre o host especificado incluindo o endereço IP
    h = gethostbyname(url->host);
    if (h == NULL) {
        printf("Invalid hostname '%s'\n", url->host);
        exit(-1);
    }
    
    //Converte o endereço IP retornado por gethostbyname para uma string legível usando inet_ntoa(O IP fica com este formato "xxx.xxx.xxx.xxx") e copia essa string para o campo ip da estrutura url.
    strcpy(url->ip, inet_ntoa(*((struct in_addr *) h->h_addr)));

    //Verifica se algum dos campos da estrutura URL está vazio e retorna -1 se estiver
    
    
    if (strlen(url->resource) == 0){
        printf("Resource not found\n");
        return -1;
    }
    else if (strlen(url->file) == 0){
        printf("File not found\n");
        return -1;
    }

    
    
    return 0;
}

//função que cria uma socket que conecta ao servidor com o ip e porto passados por argumento
int createSocket(char *ip, int port) {

    int sockfd; //file descriptor da socket
    struct sockaddr_in server_addr; //estrutura que guarda o endereço do servidor

    //inicialização da estrutura server_addr
    bzero((char *) &server_addr, sizeof(server_addr)); //zera a estrutura
    server_addr.sin_family = AF_INET;   //família de endereços psra IPv4 16 bits (2 bytes)
    server_addr.sin_addr.s_addr = inet_addr(ip);  //converter o endereço IP para o formato de 32 bits (4 bytes)
    server_addr.sin_port = htons(port); //converte a porta do formato de host (little-endian) para o formato de rede (big-endian) de 16 bits (2 bytes)

    sockfd = socket(AF_INET, SOCK_STREAM, 0); //cria a socket do tipo TCP (SOCK_STREAM) e da família de endereços IPv4 (AF_INET)
    if (sockfd < 0) {
        perror("socket()");
        exit(-1);
    }

    //tenta estabelecer uma conexão com o servidor usando a socket criada e a estrutura server_addr com o endereço do servidor
    if (connect(sockfd, (struct sockaddr *) &server_addr, sizeof(server_addr)) < 0) {
        perror("connect()");
        exit(-1);
    }
    
    return sockfd;
}

//função que autentica a conexão com o servidor FTP com o username e password dados pelo utilizador
int authConn(const int socket, const char* user, const char* pass) {

    //"user <username>\n" = 5 + strlen(username) + 1
    char userCommand[5+strlen(user)+1];  
    sprintf(userCommand, "user %s\n", user);
    
    //"pass <password>\n" = 5 + strlen(password) + 1
    char passCommand[5+strlen(pass)+1];
    sprintf(passCommand, "pass %s\n", pass);
    
    char answer[MAX_LENGTH];
    
    write(socket, userCommand, strlen(userCommand));        //envia através da socket o comando user <username>
    if (readResponse(socket, answer) != SV_READY4PASS) {    //lê a resposta do servidor e verifica se o código de resposta é 331
        printf("Unknown user '%s'. Abort.\n", user);
        exit(-1);
    }
    
    //envia através da socket o comando pass <password>
    write(socket, passCommand, strlen(passCommand));
    return readResponse(socket, answer);
}

int passiveMode(const int socket, char *ip, int *port) {

    char answer[MAX_LENGTH];
    int ip1, ip2, ip3, ip4, port1, port2;
    write(socket, "pasv\n", 5);
    if (readResponse(socket, answer) != SV_PASSIVE) return -1;

    sscanf(answer, PASSIVE_REGEX, &ip1, &ip2, &ip3, &ip4, &port1, &port2);
    *port = port1 * 256 + port2;
    sprintf(ip, "%d.%d.%d.%d", ip1, ip2, ip3, ip4);

    return SV_PASSIVE;
}

int readResponse(const int socket, char* buffer) {

    char byte;
    int index = 0, responseCode;
    ResponseState state = START;
    memset(buffer, 0, MAX_LENGTH);

    while (state != END) {
        
        read(socket, &byte, 1);
        switch (state) {
            case START:
                if (byte == ' ') state = SINGLE;
                else if (byte == '-') state = MULTIPLE;
                else if (byte == '\n') state = END;
                else buffer[index++] = byte;
                break;
            case SINGLE:
                if (byte == '\n') state = END;
                else buffer[index++] = byte;
                break;
            case MULTIPLE:
                if (byte == '\n') {
                    memset(buffer, 0, MAX_LENGTH);
                    state = START;
                    index = 0;
                }
                else buffer[index++] = byte;
                break;
            case END:
                break;
            default:
                break;
        }
    }

    sscanf(buffer, RESPCODE_REGEX, &responseCode);
    return responseCode;
}

int requestResource(const int socket, char *resource) {

    char fileCommand[5+strlen(resource)+1], answer[MAX_LENGTH];
    sprintf(fileCommand, "retr %s\n", resource);
    write(socket, fileCommand, sizeof(fileCommand));
    return readResponse(socket, answer);
}

int getResource(const int socketA, const int socketB, char *filename) {

    FILE *fd = fopen(filename, "wb");
    if (fd == NULL) {
        printf("Error opening or creating file '%s'\n", filename);
        exit(-1);
    }

    char buffer[MAX_LENGTH];
    int bytes;
    do {
        bytes = read(socketB, buffer, MAX_LENGTH);
        if (fwrite(buffer, bytes, 1, fd) < 0) return -1;
    } while (bytes);
    fclose(fd);

    return readResponse(socketA, buffer);
}

int closeConnection(const int socketA, const int socketB) {
    
    char answer[MAX_LENGTH];
    write(socketA, "quit\n", 5);
    if(readResponse(socketA, answer) != SV_GOODBYE) return -1;
    return close(socketA) || close(socketB);
}

int main(int argc, char *argv[]) {

    if (argc != 2) {
        printf("Usage: ./download ftp://[<user>:<password>@]<host>/<url-path>\n");
        exit(-1);
    } 

    struct URL url;
    memset(&url, 0, sizeof(url));
    if (parse(argv[1], &url) != 0) {
        printf("Parse error. Usage: ./download ftp://[<user>:<password>@]<host>/<url-path>\n");
        exit(-1);
    }
    
    printf("Host: %s\nResource: %s\nFile: %s\nUser: %s\nPassword: %s\nIP Address: %s\n", url.host, url.resource, url.file, url.user, url.password, url.ip);

    char answer[MAX_LENGTH];

    //cria uma socket para o servidor com o ip dado pelo utilizador e o porto 21
    int socketA = createSocket(url.ip, FTP_PORT); //21 -> porta padrão para FTP
    if (socketA < 0 || readResponse(socketA, answer) != SV_READY4AUTH) {
        printf("Socket to '%s' and port %d failed\n", url.ip, FTP_PORT);
        exit(-1);
    }
    
    printf("\nConnected to %s:%d\n", url.ip, FTP_PORT);
    
    //autentica a conexão com o servidor com o username e password dados pelo utilizador
    if (authConn(socketA, url.user, url.password) != SV_LOGINSUCCESS) {
        printf("Authentication failed with username = '%s' and password = '%s'.\n", url.user, url.password);
        exit(-1);
    }

    printf("\nAuthentication success with username = '%s' and password = '%s'.\n", url.user);
    
    int port;
    char ip[MAX_LENGTH];
    if (passiveMode(socketA, ip, &port) != SV_PASSIVE) {
        printf("Passive mode failed\n");
        exit(-1);
    }

    int socketB = createSocket(ip, port);
    if (socketB < 0) {
        printf("Socket to '%s:%d' failed\n", ip, port);
        exit(-1);
    }

    if (requestResource(socketA, url.resource) != SV_READY4TRANSFER) {
        printf("Unknown resouce '%s' in '%s:%d'\n", url.resource, ip, port);
        exit(-1);
    }

    if (getResource(socketA, socketB, url.file) != SV_TRANSFER_COMPLETE) {
        printf("Error transfering file '%s' from '%s:%d'\n", url.file, ip, port);
        exit(-1);
    }

    if (closeConnection(socketA, socketB) != 0) {
        printf("Sockets close error\n");
        exit(-1);
    }

    return 0;
}
