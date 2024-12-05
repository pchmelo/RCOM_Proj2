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

//mete o cliente FTP em modo passivo, o que significa que o servidor vai abrir uma porta e o cliente vai ter de se conectar a essa porta para transferir dados
int passiveMode(const int socket, char *ip, int *port) {

    char answer[MAX_LENGTH];

    int ip1, ip2, ip3, ip4; //variáveis para guardar os 4 bytes do endereço IP
    int port1, port2;   //variáveis para guardar os 2 bytes da porta de transferência
    
    write(socket, "pasv\n", 5); //envia o comando pasv para o servidor, que vai responder com o endereço IP e a porta de transferência
    
    if (readResponse(socket, answer) != SV_PASSIVE){
        printf("Passive mode failed\n");
        return -1;
    } 

    sscanf(answer, PASSIVE_REGEX, &ip1, &ip2, &ip3, &ip4, &port1, &port2);  //227 Entering Passive Mode (h1,h2,h3,h4,p1,p2).
    
    *port = port1 * 256 + port2;    //calcula a porta de transferência a partir dos 2 bytes recebidos
    
    sprintf(ip, "%d.%d.%d.%d", ip1, ip2, ip3, ip4); //Formata o endereço IP como uma string no formato "xxx.xxx.xxx.xxx"

    return 0;
}

/*
START: Estado inicial, onde a resposta começa a ser lida.
SINGLE: Estado para uma linha de resposta única.
MULTIPLE: Estado para uma resposta de múltiplas linhas.
END: Estado final, onde a leitura da resposta termina.

esposta de Linha Única:
220 Welcome to the FTP server.
331 User name okay, need password.
230 User logged in, proceed.

Resposta de Múltiplas Linhas:
150-File status okay; about to open data connection.
150 Opening data connection.
*/



//função que lê a resposta do servidor e retorna o código de resposta
int readResponse(const int socket, char* buffer) {

    char byte; //variável para guardar qualquer byte lido da socket
    int index = 0;  //índice para a posição do buffer
    int responseCode;   //variável para guardar o código de resposta extraído da resposta do servidor
    
    ResponseState state = START; //estado inicial da máquina de estados que lê a resposta do servidor
    memset(buffer, 0, MAX_LENGTH); //inicializa o buffer com 0s 

    printf("\n<----------------- Server response ----------------->\n");

    while (state != END) {  //enquanto o estado não for END
        
        read(socket, &byte, 1); //lê um byte da socket
        printf("%c", byte);
        switch (state) {
            case START:
                if (byte == ' '){
                    state = SINGLE; 
                }    
                else if (byte == '-'){
                    state = MULTIPLE;
                }
                else if (byte == '\n'){
                    state = END;
                }
                else{
                    buffer[index] = byte;
                    index++;
                } 
                break;
            case SINGLE:
                if (byte == '\n'){
                    state = END;
                }
                else{
                    buffer[index] = byte;
                    index++;
                } 
                break;
            case MULTIPLE:
                if (byte == '\n') {
                    memset(buffer, 0, MAX_LENGTH);
                    state = START;
                    index = 0;
                }
                else{
                    buffer[index] = byte;
                    index++;
                }
                break;
            case END:
                break;
            default:
                break;
        }
    }

    printf("<--------------------------------------->\n");


    sscanf(buffer, RESPCODE_REGEX, &responseCode); //lê o codigo de resposta da resposta do servidor com uso de uma expressão regular
    return responseCode;
}

//função que pede ao servidor para solicitar a transferência do arquivo do servidor FTP 
int requestResource(const int socket, char *resource) {
    //retr <filename>\n = 5 + strlen(filename) + 1
    char fileCommand[5+strlen(resource)+1], answer[MAX_LENGTH];
    sprintf(fileCommand, "retr %s\n", resource);
    write(socket, fileCommand, sizeof(fileCommand));
    return readResponse(socket, answer);
}

//função que recebe o arquivo do servidor FTP e guarda-o localmente
//utilizando a socket de transferência (socketB) para receber os dados.
//a soket A é utilizada como controlo para verificar se a transferência foi bem sucedida
int getResource(const int socketA, const int socketB, char *filename) {

    FILE *fd = fopen(filename, "wb");
    if (fd == NULL) {
        printf("Error opening or creating file '%s'\n", filename);
        exit(-1);
    }

    char buffer[MAX_LENGTH];
    int bytes;  //variável para guardar o número de bytes lidos da socket de transferência
    
    do {
        bytes = read(socketB, buffer, MAX_LENGTH); //lê os dados da socket de transferência
        if (fwrite(buffer, bytes, 1, fd) < 0){ //escreve os dados lidos no ficheiro
            printf("Error writing to file '%s'\n", filename);
            return -1;
        }
    } while (bytes);
    
    fclose(fd);

    return readResponse(socketA, buffer);
}

//função que fecha as duas sockets de controlo e de transferência
int closeConnection(const int socketA, const int socketB) {
    
    char answer[MAX_LENGTH];
    write(socketA, "quit\n", 5);

    //recebe 221 do servidor, o que significa que a conexão foi fechada
    if(readResponse(socketA, answer) != SV_GOODBYE){
        printf("Error closing connection\n");
        return -1;
    }

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
    if (passiveMode(socketA, ip, &port) != 0) {
        printf("Passive mode failed\n");
        exit(-1);
    }

    printf("\nPassive mode success with IP = '%s' and port = '%d'.\n", ip, port);

    int socketB = createSocket(ip, port);  //cria uma socket para o servidor com o ip e porto de transferência dados pelo servidor 
    if (socketB < 0) {
        printf("Socket to '%s:%d' failed\n", ip, port);
        exit(-1);
    }

    printf("\nConnected  the transfer socket to %s:%d\n", ip, port);

    if (requestResource(socketA, url.resource) != SV_READY4TRANSFER) {  //se receber a resposta 150 do servidor, então o servidor está pronto para transferir o arquivo
        printf("Unknown resouce '%s' in '%s:%d'\n", url.resource, ip, port);
        exit(-1);
    }

    printf("\nResource '%s' ready for transfer from '%s:%d'\n", url.resource, ip, port);

    //recebe 226 do servidor, o que significa que a transferência foi bem sucedida
    if (getResource(socketA, socketB, url.file) != SV_TRANSFER_COMPLETE) {
        printf("Error transfering file '%s' from '%s:%d'\n", url.file, ip, port);
        exit(-1);
    }

     printf("\nFile '%s' transfered successfully from '%s:%d'\n", url.file, ip, port);

    if (closeConnection(socketA, socketB) != 0) {
        printf("Sockets close error\n");
        exit(-1);
    }

    printf("\nConnection closed\n");

    return 0;
}
