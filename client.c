/**
 * *compilation      `gcc client.c -o client`
 */

/*      diathesimes entoles:
 *      help                
 *      register            
 *      login               
 *      view                
 *      quit                
 *      logout
 *      reserve    [date]
 *      release    [date] [room] [code]
 */


#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>         

// networking
#include <sys/socket.h>
#include <netdb.h>
#include <netinet/in.h>
#include <arpa/inet.h>

// miscellaneous
#include <regex.h>
#include <signal.h>        





// config definition and declarations
#include "config.h"

#include "utils.h"
#include "messages.h"

#include "Address.h"
#include "Booking.h"
#include "Hotel.h"
#include "User.h"



#define REGEX_HELP          "help"
#define REGEX_LOGIN         "login"
#define REGEX_REGISTER      "register"
#define REGEX_VIEW          "view"
#define REGEX_QUIT          "quit"
#define REGEX_LOGOUT        "logout"
#define REGEX_RESERVE       "reserve"
#define REGEX_RELEASE       "release"

#define REGEX_ROOM          "^[1-9]{1,3}$"          // ta noumera twn dwmatiwn kymainontai apo 1-999 
#define REGEX_CODE          "^([a-zA-Z0-9]{5})$"    // 5 xarakthres, alfarithmhtika


#define REGEX_DATE_FORMAT   "[0-9]{2}/[0-9]{2}$"
#define REGEX_DATE_VALID    "(((0[1-9]|[12][0-9]|3[01])/(0[13578]|1[02]))|((0[1-9]|[12][0-9]|30)/(0[13456789]|1[012]))|((0[1-9]|1[0-9]|2[0-8])/02)|(29/02))$"



int 
main(int argc, char** argv) 
{

    // katharismso terminal
    system("clear");

    #if GDB_MODE
        char port[5];
        printf("Insert port number: ");
        scanf("%s", port);

        Address address = (Address){
            .ip   = "127.0.0.1",
            .port = atoi(port)
        };
        
    #else
        // diavasma apo stdin
        Address address = readArguments(argc, argv);
    #endif     

    // kanei setup ton fd tou client & return socket
    int sockfd = setupClient(&address);



    #ifdef MANAGE_CNTRL_C
        signal(SIGINT, closeConnection);
    #endif



    char command[BUFSIZE];
    char cmd[BUFSIZE];
    char response[BUFSIZE];


    
    char username[USERNAME_MAX_LENGTH];
    char password[PASSWORD_MAX_LENGTH];




    User* user = (User*) malloc(sizeof(User));
    memset(user, '\0', sizeof(User));


    Booking* booking = (Booking*) malloc(sizeof(Booking));
    memset(booking, '\0', sizeof(Booking));


    
    //arxikopoihsh
    client_fsm_state_t state = CL_INIT;    


    // emfanish diathesimwn entolwn sto cmd
    printf("%s\n", HELP_UNLOGGED_MESSAGE);    
    

    while (1) 
    {
        
        switch (state)
        {
            case CL_INIT:
                printf("> ");
                
                // diavase input
                memset(command, '\0', BUFSIZE);
                fgets(command, 20, stdin);

                //delete new line apo to telos tou string
                command[strlen(command)-1] = '\0';
                
                //kanei ta grammata ths entolhs peza
                lower(command);



                if      (regexMatch(command, REGEX_HELP)){
                    state = SEND_HELP;
                }
                else if (regexMatch(command, REGEX_LOGIN)){ 
                    state = SEND_LOGIN;
                }
                else if (regexMatch(command, REGEX_REGISTER)) {
                    state = SEND_REGISTER;
                }
                else if (regexMatch(command, REGEX_QUIT)){
                    state = SEND_QUIT;
                }
                else{
                    state = INVALID_UNLOGGED;
                }
                
                break;



            case INVALID_UNLOGGED:
				//den stelnei tipota sto socket afou einai invalid
                printf(INVALID_COMMAND_MESSAGE);
                state = CL_INIT;
                break;
            

            case SEND_HELP:
                writeSocket(sockfd, HELP_MSG);  
                state = READ_HELP_RESP;
                break;

            case READ_HELP_RESP:
                //katharisma buffer
                memset(response, '\0', BUFSIZE);

                readSocket(sockfd, response);
                if (strcmp(response, "H") == 0){
                    printf("%s\n", HELP_UNLOGGED_MESSAGE);    
                }
                state = CL_INIT;
                break;

            case SEND_HELP_LOGGED:
                writeSocket(sockfd, HELP_MSG);
                state = READ_HELP_LOGGED_RESP;
                break;

            case READ_HELP_LOGGED_RESP:
                memset(response, '\0', BUFSIZE);
                readSocket(sockfd, response);
                if (strcmp(response, "H") == 0){
                    printf("%s\n", HELP_LOGGED_IN_MESSAGE);    
                }
                state = CL_LOGIN;
                break;

            case SEND_QUIT:
                writeSocket(sockfd, QUIT_MSG);
                printf("Quitting...\n");
                
                memset(command, '\0', sizeof(command));
                strcpy(command, "abort");
                break;

            case SEND_REGISTER:
                writeSocket(sockfd, REGISTER_MSG);
                state = READ_REGISTER_RESP;
                break;

            case READ_REGISTER_RESP:
                memset(command, '\0', sizeof(command));
                readSocket(sockfd, command);
                state = SEND_USERNAME;
                break;

            case SEND_USERNAME:
                printf("Choose username: ");
                fgets(username, USERNAME_MAX_LENGTH, stdin);          // allagh tou `\n`  me `\0` sto telos tou string
                username[strlen(username)-1] = '\0';

                
                lower(username);

                if (strlen(username) < USERNAME_MIN_LENGTH || strlen(username) > USERNAME_MAX_LENGTH-1){
                    printf("Sorry, username length needs to be [%d-%d]\n", USERNAME_MIN_LENGTH, USERNAME_MAX_LENGTH);
                    state = SEND_USERNAME;
                    break;
                }
                
                writeSocket(sockfd, username); 

                
                strcpy(user->username, username); 

                state = READ_USERNAME_RESP;
                break;

            case READ_USERNAME_RESP:
                memset(command, '\0', sizeof(command));
                readSocket(sockfd, command);
                if (strcmp(command, "Y") == 0){
                    printf("Username OK.\n");
                }
                else {
                    printf(USERNAME_TAKEN_MSG);
                }

                if (strcmp(command, "Y") == 0){
                    state = SEND_PASSWORD;
                }
                else {
                    state = SEND_USERNAME;
                }
                break;

            case SEND_PASSWORD:

                readPassword(password);

                // check megethos password 
                if (strlen(password) < PASSWORD_MIN_LENGTH || strlen(password) > PASSWORD_MAX_LENGTH-1){
                    printf("Sorry, password length needs to be [%d-%d]\n", PASSWORD_MIN_LENGTH, PASSWORD_MAX_LENGTH);
                    state = SEND_PASSWORD;
                    break;
                }

                writeSocket(sockfd, password);

                state = READ_PASSWORD_RESP;
                break;
            
            case READ_PASSWORD_RESP:
                memset(command, '\0', sizeof(command));
                readSocket(sockfd, command); 
                printf("%s\n", command);

                memset(command, '\0', sizeof(command));
                readSocket(sockfd, command);
                printf("%s\n", command);

                state = CL_LOGIN;
                break;

            case SEND_LOGIN:
                writeSocket(sockfd, LOGIN_MSG);         
                state = READ_LOGIN_RESP;
                break;

            case READ_LOGIN_RESP:
                memset(command, '\0', sizeof(command));
                readSocket(sockfd, command);
                state = SEND_LOGIN_USERNAME;
                break;

            case SEND_LOGIN_USERNAME:
                printf(USERNAME_PROMPT_MSG);
                fgets(username, USERNAME_MAX_LENGTH, stdin);          
                username[strlen(username)-1] = '\0';

                lower(username);
                writeSocket(sockfd, username);

                
                strcpy(user->username, username); 
                
                state = READ_LOGIN_USERNAME_RESP;
                break;

            case READ_LOGIN_USERNAME_RESP:
                memset(command, '\0', sizeof(command));
                readSocket(sockfd, command);
                if (strcmp(command, "Y") == 0){
                    printf("OK.\n");
                    state = SEND_LOGIN_PASSWORD;
                }
                else {
                    printf(UNREGISTERED_USERNAME_ERR_MSG);
                    state = CL_INIT;
                }
                break;

            case SEND_LOGIN_PASSWORD:

                readPassword(password);


                writeSocket(sockfd, password);
                state = READ_LOGIN_PASSWORD_RESP;
                break;

            case READ_LOGIN_PASSWORD_RESP:
                memset(command, '\0', sizeof(command));
                readSocket(sockfd, command);
                if (strcmp(command, "Y") == 0){
                    printf(ACCESS_GRANTED_MSG);
                    state = CL_LOGIN;
                }
                else {
                    printf(WRONG_PASSWORD_MSG);
                    state = CL_INIT;
                }
                break;

            case CL_LOGIN:
                
                printf(ANSI_COLOR_YELLOW ANSI_BOLD "(%s)" ANSI_COLOR_RESET "> ", user->username);

                //diavasma input
                memset(command, '\0', BUFSIZE);
                fgets(command, 30, stdin);

            
                //allagh tou \n me \0
                command[strlen(command)-1] = '\0';


                if      (regexMatch(command, REGEX_HELP)){
                    state = SEND_HELP_LOGGED;
                }
                else if (regexMatch(command, REGEX_VIEW)) {
                    state = SEND_VIEW;  
                }
                else if (regexMatch(command, REGEX_QUIT)){
                    state = SEND_QUIT;
                }
                else if (regexMatch(command, REGEX_LOGOUT)){
                    state = SEND_LOGOUT;
                }
                else {
                    //xwrizei to input se kommatia

                    memset(cmd,           '\0', sizeof cmd);
                    memset(booking->date, '\0', sizeof booking->date);
                    memset(booking->room, '\0', sizeof booking->room);
                    memset(booking->code, '\0', sizeof booking->code);

                    sscanf(command, "%s %s %s %s", 
                        cmd, booking->date, booking->room, booking->code
                    ); 


                    if (regexMatch(cmd, REGEX_RESERVE)){
                        if (regexMatch(booking->date, REGEX_DATE_FORMAT)){
                            if (regexMatch(booking->date, REGEX_DATE_VALID)){
                                state = SEND_RESERVE;        
                            } else {
                                printf(INVALID_DATE_MSG);
                                state = CL_LOGIN;
                            }
                            
                        }
                        else{
                            state = INVALID_DATE;
                        }
                    }
                    else if (regexMatch(cmd, REGEX_RELEASE)){

                        if (regexMatch(booking->date, REGEX_DATE_FORMAT) && 
                            regexMatch(booking->room, REGEX_ROOM) &&
                            regexMatch(booking->code, REGEX_CODE))
                        {
                            if (regexMatch(booking->date, REGEX_DATE_VALID)){
                                state = SEND_RELEASE;
                            }
                            else {
                                printf(INVALID_DATE_MSG);
                                state = CL_LOGIN;
                            }
                            
                        }
                        else {
                            state = INVALID_RELEASE;
                        }
                    
                    }
                    else {
                        state = INVALID_LOGGED_IN;
                    }
                }
                break;

            case INVALID_LOGGED_IN:
                printf(INVALID_COMMAND_MESSAGE);
                state = CL_LOGIN;
                break;

            case INVALID_DATE:
                printf(INVALID_FORMAT_RESERVE_MSG);
                state = CL_LOGIN;
                break;

            case INVALID_RELEASE:
                printf(INVALID_FORMAT_RELEASE_MSG);
                state = CL_LOGIN;
                break;


            case SEND_LOGOUT:
                writeSocket(sockfd, LOGOUT_MSG);
                state = CL_INIT;
				
                memset(user, '\0', sizeof(User));

                break;

            case SEND_RESERVE:
                writeSocket(sockfd, RESERVE_MSG);
                writeSocket(sockfd, booking->date);
                state = READ_RESERVE_RESP;
                break;
            
            case READ_RESERVE_RESP:
                memset(command, '\0', sizeof(command));
                readSocket(sockfd, command);


                if (strcmp(command, "BADDATE") == 0){
                    printf("%s\n", "Wrong date, please change format");
                }
                else if (strcmp(command, "NOTAVAL") == 0){
                    printf("No room available on %s/2022\n", booking->date);
                }
                else if (strcmp(command, "RESOK") == 0){
                    memset(booking->room, '\0', sizeof(booking->room));
                    memset(booking->code, '\0', sizeof(booking->code));

                    readSocket(sockfd, booking->room);
                    readSocket(sockfd, booking->code);

                    printf("Your reservation is successful: room %s & code %s\n", booking->room, booking->code);
                }
                state = CL_LOGIN;
                break;


            case SEND_VIEW:
                writeSocket(sockfd, VIEW_MSG);
                state = READ_VIEW_RESP;
                break;

            case READ_VIEW_RESP:
                memset(response, '\0', BUFSIZE);
                readSocket(sockfd, response);
                printf("%s\n", response);
                state = CL_LOGIN;
                break;

            case SEND_RELEASE:
                writeSocket(sockfd, RELEASE_MSG);
                writeSocket(sockfd, booking->date);
                writeSocket(sockfd, booking->room);
                writeSocket(sockfd, booking->code);

                state = READ_RELEASE_RESP;
                break;

            case READ_RELEASE_RESP:
                memset(response, '\0', BUFSIZE);
                readSocket(sockfd, response);

                printf("%s\n", response);
                state = CL_LOGIN;

                break;
            
        } // telos switch
        

        #if DEBUG
            printClientFSMState(&state);
        #endif

        // to avoid infinite loop, client writes quit
        if (strcmp(command, "abort") == 0){ 
            break;
        }

    }

    
    free(user);
    free(booking);

    close(sockfd);

    return 0;
}
