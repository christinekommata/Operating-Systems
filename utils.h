#ifndef UTILS_H
#define UTILS_H


#include <netinet/in.h>     
#include <arpa/inet.h>      
#include <stdarg.h>
#include <ctype.h>         
#include <termios.h>
#include <regex.h>


#include "xp_getpass.h"
#include "config.h"
#include "messages.h"

#include "Address.h"

typedef struct query {
    int     rv;             
    void*   query_result;   // apotelesma tou query
} query_t;


typedef enum {  
    //INITIALIZE
    INIT,
    HELP_UNLOGGED,      //HELP
    HELP_LOGGED_IN,

    
    // REGISTER
    REGISTER,
    PICK_USERNAME,
    PICK_PASSWORD,   
    
    SAVE_CREDENTIAL, 
    
    // LOGIN
    LOGIN_REQUEST,
    CHECK_USERNAME,
    CHECK_PASSWORD,
    GRANT_ACCESS,
    
    LOGIN,                  //LOGIN

    
    // RESERVE
    CHECK_DATE_VALIDITY,
    CHECK_AVAILABILITY,
    RESERVE_CONFIRMATION,

    //VIEW
    VIEW,

    //RELEASE
    RELEASE,

    //QUIT
    QUIT                    //kleisimo syndeshs me client
    
} server_fsm_state_t;




typedef enum {
    // CLIENT INITIALIZE
    CL_INIT,
    
    // HELP
    SEND_HELP,
    SEND_HELP_LOGGED,

    READ_HELP_RESP,
    READ_HELP_LOGGED_RESP,

    // QUIT
    SEND_QUIT,

    // REGISTER
    SEND_REGISTER,
    READ_REGISTER_RESP,

    SEND_USERNAME,
    READ_USERNAME_RESP,
    
    SEND_PASSWORD,
    READ_PASSWORD_RESP,

    // LOGOUT
    SEND_LOGOUT,

    // LOGIN
    SEND_LOGIN,
    READ_LOGIN_RESP,          
    SEND_LOGIN_USERNAME,       
    READ_LOGIN_USERNAME_RESP,  
    SEND_LOGIN_PASSWORD,       
    READ_LOGIN_PASSWORD_RESP,
    

    CL_LOGIN,

    // RESERVE
    INVALID_DATE,
    SEND_RESERVE,
    READ_RESERVE_RESP,

    // VIEW
    SEND_VIEW,
    READ_VIEW_RESP,

    // RELEASE
    INVALID_RELEASE,
    SEND_RELEASE,
    READ_RELEASE_RESP,
    
    // INVALID
    INVALID_UNLOGGED,
    INVALID_LOGGED_IN

} client_fsm_state_t;


// eidopoiei gia sfalma kai kanei exit
void        perror_die(const char* msg);

// diavazei arguments kai epistrefei IP kai port(an yparxoun)

Address     readArguments(int argc, char** argv);

// diaxeirish exwterikhs epikoinwnias me socket
void        writeSocket(int sockfd, char* msg);

// diaxeirish eswterikhs epkoinwnias socket
void        readSocket(int sockfd, char* msg);

// setup tou socket sth meria tou server
int         setupServer(Address* address);

// setupt tou socket sth meria tou client
int         setupClient(Address* address);


void        printServerFSMState(server_fsm_state_t* s, int* tid);


void        printClientFSMState(client_fsm_state_t* s);


// metatroph se peza
void        lower(char* str);

// metatroph se kefalaia
void        upper(char* str);


// diavase password apo user
void        readPassword(char* password);

// tsekare an to string tou password tairiazei sto pattern
int         regexMatch(const char* string, const char* pattern);

void 
perror_die(const char* msg)
{
    perror(msg);
    exit(-1);
}


Address
readArguments(int argc, char** argv)
{
    Address addr;

    if (argc < 3) {
        printf("\x1b[31mWrong number of parameters!\x1b[0m\n");
        printf("Usage: %s <ip> <port>\n", argv[0]);
        exit(-1);
    }

    #if 0 
        addr.ip = argv[1];
    #else
        strcpy(addr.ip, argv[1]);
    #endif
    addr.port = atoi(argv[2]);

    return addr;
}



void
writeSocket(int sockfd, char* msg)
{
    static int ret;

    int dim;

    dim = htonl(strlen(msg));

    // printf("dim = %d\n", dim);

    ret = send(sockfd, (void*) &dim, sizeof(dim), 0);

    if (ret < 0 || ret < sizeof(dim)){
        perror_die("send(dim)");
    }

    ret = send(sockfd, (void*) msg, strlen(msg), 0);

    if (ret < 0 || ret < strlen(msg)) {
        perror_die("send(msg)");
    }

    return;
}



void
readSocket(int sockfd, char* msg)
{
    int ret;   
    int dim;  


    ret = recv(sockfd, (void*) &dim, sizeof(dim), MSG_WAITALL); // blocking function

    // check return value
    if (ret < 0 || ret < sizeof(dim)) {
        perror_die("recv(dim)");
    }

    // translation: network2host long
    dim = ntohl(dim);

    #if 0
        // katharise th domh
        memset(msg, '\0', sizeof(msg));
    #endif

    // pairnei data apo socket
    ret = recv(sockfd, (void*) msg, dim, MSG_WAITALL);

    //check return value
    if (ret < 0 || ret < dim) {
        perror_die("recv(msg)");
    }

    return;
}






int 
setupServer(Address* address)
{
    
    int sockfd;   
    int ret;

    struct sockaddr_in server_addr;     // server address
    

    // dhmiourgia socket & return value
    sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if (sockfd == -1) {
        printf("socket creation failed...\n");
        exit(-1);
    }
    else {
        printf(ANSI_COLOR_GREEN "[+] Socket successfully created.\n" ANSI_COLOR_RESET);
    }
    memset(&server_addr, '\0', sizeof(server_addr));

    // ip and port
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = htonl(INADDR_ANY);    // syndesh me opoiodhpote port number
    server_addr.sin_port = htons(address->port);

    // sysndesh IP me socket 

    ret = (bind(sockfd, (struct sockaddr*) &server_addr, sizeof(server_addr)));
    if (ret != 0) {
        perror_die("socket bind failed...\n");
    }

    // else
    //     printf("Socket successfully binded..\n");

  
    ret = listen(sockfd, BACKLOG);
    if (ret != 0) {
        perror_die("Listen()");
    }
    else
        printf(ANSI_COLOR_GREEN "[+] Server listening on %s:%d" ANSI_COLOR_RESET "\n", address->ip, address->port);

    return sockfd;
}


int 
setupClient(Address* address)
{
   // syndesh me socket
    int sockfd;          // socket file descriptor
    int ret;


    struct sockaddr_in server_addr;
  

    // dhmiourgia socket
    sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if (sockfd == -1) {
        perror_die("socket()");
        exit(0);
    }
    #if DEBUG
        else
            printf(ANSI_COLOR_GREEN "[+] Socket successfully created.\n" ANSI_COLOR_RESET);
    #endif

    // katharise mnhmh
    #if 0
        bzero(&server_addr, sizeof(server_addr)); 
    #else
        memset(&server_addr, '\0', sizeof(server_addr));
    #endif

    // syndesh IP, PORT
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = inet_addr(address->ip);
    server_addr.sin_port = htons(address->port);

    
    ret = connect(sockfd, (struct sockaddr*) &server_addr, sizeof(server_addr));
    if (ret != 0) {
        perror_die("connect()");
    }
    #if DEBUG
        else
            printf(ANSI_COLOR_GREEN "[+] Connected to the server.\n" ANSI_COLOR_RESET);
    #endif


    return sockfd;
}




void 
printServerFSMState(server_fsm_state_t* s, int* tid)
{
    char* rv;

    switch (*s)
    {
        case INIT:                          rv = "INIT";                        break;

        case HELP_UNLOGGED:                 rv = "HELP_UNLOGGED";               break;
        case HELP_LOGGED_IN:                rv = "HELP_LOGGED_IN";              break;

        case REGISTER:                      rv = "REGISTER";                    break;
        case PICK_USERNAME:                 rv = "PICK_USERNAME";               break;
        case PICK_PASSWORD:                 rv = "PICK_PASSWORD";               break;

        case SAVE_CREDENTIAL:               rv = "SAVE_CREDENTIAL";             break;
        case LOGIN:                         rv = "LOGIN";                       break;

        case QUIT:                          rv = "QUIT";                        break;
        
        case LOGIN_REQUEST:                 rv = "LOGIN_REQUEST";               break;
                
        case CHECK_USERNAME:                rv = "CHECK_USERNAME";              break;
                
        case CHECK_PASSWORD:                rv = "CHECK_PASSWORD";              break;
                
        case GRANT_ACCESS:                  rv = "GRANT_ACCESS";                break;

        case CHECK_DATE_VALIDITY:           rv = "CHECK_DATE_VALIDITY";         break;
        case CHECK_AVAILABILITY:            rv = "CHECK_AVAILABILITY";          break;
        case RESERVE_CONFIRMATION:          rv = "RESERVE_CONFIRMATION";        break;

        case VIEW:                          rv = "VIEW";                        break;

        case RELEASE:                       rv = "RELEASE";                     break;
    }

    printf("THREAD #%d: state: %s\n", *tid, rv);
    return;
}


void 
printClientFSMState(client_fsm_state_t* s)
{
    char* rv;

    switch (*s) 
    {
        case CL_INIT:                       rv = "CL_INIT";                     break;

        case SEND_HELP:                     rv = "SEND_HELP";                   break;
        case SEND_HELP_LOGGED:              rv = "SEND_HELP_LOGGED";            break;

        case READ_HELP_RESP:                rv = "READ_HELP_RESP";              break;
        case READ_HELP_LOGGED_RESP:         rv = "READ_HELP_LOGGED_RESP";       break;

        case SEND_QUIT:                     rv = "SEND_QUIT";                   break;

        case SEND_REGISTER:                 rv = "SEND_REGISTER";               break;
        case READ_REGISTER_RESP:            rv = "READ_REGISTER_RESP";          break;

        case SEND_USERNAME:                 rv = "SEND_USERNAME";               break;
        case READ_USERNAME_RESP:            rv = "READ_USERNAME_RESP";          break;

        case SEND_PASSWORD:                 rv = "SEND_PASSWORD";               break;
        case READ_PASSWORD_RESP:            rv = "READ_PASSWORD_RESP";          break;

        case CL_LOGIN:                      rv = "CL_LOGIN";                    break;

        case INVALID_UNLOGGED:              rv = "INVALID_UNLOGGED";            break;
        case INVALID_LOGGED_IN:             rv = "INVALID_LOGGED_IN";           break;

        case SEND_LOGOUT:                   rv = "SEND_LOGOUT";                 break;
        case SEND_LOGIN:                    rv = "SEND_LOGIN";                  break;
        case READ_LOGIN_RESP:               rv = "READ_LOGIN_RESP";             break;          
        case SEND_LOGIN_USERNAME:           rv = "SEND_LOGIN_USERNAME";         break;       
        case READ_LOGIN_USERNAME_RESP:      rv = "READ_LOGIN_USERNAME_RESP";    break;  
        case SEND_LOGIN_PASSWORD:           rv = "SEND_LOGIN_PASSWORD";         break;       
        case READ_LOGIN_PASSWORD_RESP:      rv = "READ_LOGIN_PASSWORD_RESP";    break;

        case INVALID_DATE:                  rv = "INVALID_DATE";                break;
        case SEND_RESERVE:                  rv = "SEND_RESERVE";                break;
        case READ_RESERVE_RESP:             rv = "READ_RESERVE_RESP";           break;

        case SEND_VIEW:                     rv = "SEND_VIEW";                   break;
        case READ_VIEW_RESP:                rv = "READ_VIEW_RESP";              break;

        case INVALID_RELEASE:               rv = "INVALID_RELEASE";             break;
        case SEND_RELEASE:                  rv = "SEND_RELEASE";                break;
        case READ_RELEASE_RESP:             rv = "READ_RELEASE_RESP";           break;
    }

    printf("State: %s\n", rv);
    return;
}




void lower(char* str)
{
    for (int i = 0; str[i]; i++){
        str[i] = tolower(str[i]);
    }
}

void upper(char* str)
{
    for (int i = 0; str[i]; i++){
        str[i] = toupper(str[i]);
    }
}




void 
readPassword(char* password)
{
    #if HIDE_PASSWORD
        strcpy(password, xp_getpass(PASSWORD_PROMPT_MSG));
    #else
        printf(PASSWORD_PROMPT_MSG);
        fgets(password, PASSWORD_MAX_LENGTH, stdin);
        password[strlen(password)-1] = '\0';
    #endif
}


int 
regexMatch(const char* string, const char* pattern)
{
    regex_t re;
    if (regcomp(&re, pattern, REG_EXTENDED|REG_NOSUB) != 0) {
        return 0;
    }
    int status = regexec(&re, string, 0, NULL, 0);
    regfree(&re);
    if (status != 0) {
        return 0;
    }
    return 1;
}




#endif