/**
 *compilation     `make server` or `gcc server.c -o server [-lcrypt -lpthread] -lsqlite3`
 */


#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

// POSIX threading
#include <pthread.h>    // thelei gcc -lpthread flag 
#include "xp_sem.h"


#include <sys/socket.h>
#include <sys/types.h>
#include <netdb.h>
#include <netinet/in.h>

// security
#ifdef __linux__
    #include <crypt.h>  // gcc requires -crypt flag 
    // crypt() is part of `unistd.h` on __APPLE__
#endif

// miscellaneous
#include <sqlite3.h>    // gcc requires -lsqlite3 flag 


//headers

#include "config.h"
#include "utils.h"


#include "Address.h"
#include "Booking.h"
#include "Hotel.h"
#include "User.h"


#define QUOTE(...)          #__VA_ARGS__     


static pthread_mutex_t  lock_g;                     
static pthread_mutex_t  users_lock_g;               // prosvash sto arxeio `users.txt`


static xp_sem_t         free_threads;              
static xp_sem_t         evsem[NUM_THREADS];         

static int              fds[NUM_THREADS];           // pinakas me ta fds


static int              busy[NUM_THREADS];          
static int              tid[NUM_THREADS];           // pinakas me xrhsimopoihmena ids
static pthread_t        threads[NUM_THREADS];       // pinakas me pre-allocated threads



static char             query_result_g[BUFSIZE];    
static char             rooms_busy_g[4];            
static int              entry_id_g;                 


static int              hotel_max_available_rooms;  

static char             USER_FILE[30];              // user file path
static char             DATABASE[30];               // database  path 



// Thread body for the request handlers, perimeneis shma apo to kyrio thread, mazeyei ta fds kai ta client requests
void*       threadHandler(void* opaque);

// ektelei ta request tou client
void        dispatcher(int sockfd, int thread_index);

// checking an sto users.txt yparxei to username sou
int         usernameIsRegistered(char* u);


//anoigei to file me tous users kai kanei update ta data tou user
int         updateUsersRecordFile(char* username, char* password);

// kanei to password encrypted
char*       encryptPassword(int thread_index, char* password);


// checking an to password tairiazei me to server / return 0 is ok
int         checkIfPasswordMatches(User* user);


// save user reservation to database
int         saveReservation(int thread_index, User* user, Booking* booking);


// bale thn entolh sto database
int         commitToDatabase(int thread_index, const char* sql_command);

// query the database
query_t*    queryDatabase(int thread_index, const int query_id, const char* sql_command);

// xrhsh apo queryDatabase
int         viewCallback(void* NotUsed, int argc, char** argv, char** azColName);


// Used by queryDatabase()
int         busy_roomsCallback(void* NotUsed, int argc, char** argv, char** azColName);


// Used by queryDatabase()
int         validEntryCallback(void* NotUsed, int argc, char** argv, char** azColName);

// ftiaxnei to booking table
int         setupDatabase();

// dinei dwmatio ston client meta apo request
char*       assignRoom(int thread_index, char* date);

// ftiaxnei random string me generator
void        generateRandomString(char* str, size_t size);

// anoigei to database kai epistrefei to reservation tou user
char*       fetchUserReservations(int thread_index, User* user);

// diagrafh tou reservation
int         releaseReservation(int thread_index, User* user, Booking* booking);

int 
main(int argc, char** argv)
{
    // clear terminal
    system("clear");

    
    
    strcat(USER_FILE, DATA_FOLDER);
    strcat(USER_FILE, "/");
    strcat(USER_FILE, USER_FILE_NAME);

    strcat(DATABASE, DATA_FOLDER);
    strcat(DATABASE, "/");
    strcat(DATABASE, DATABASE_NAME);



    int conn_sockfd;    // file descriptor tou syndedemenou socket


    #if GDB_MODE
        char port[5];
        printf("Insert port number: ");
        scanf("%s", port);

        Address address = (Address){
            .ip   = "127.0.0.1",
            .port = atoi(port)
        };

        hotel_max_available_rooms = 3;
        
    #else
        // diavase IP &  port apo stdin
        Address address = readArguments(argc, argv); 

        // diavase ton arithmo dwmatiou apo to stdin
        if (argc < 4){
            printf("\x1b[31mWrong number of parameters!\x1b[0m\n");
            printf("Usage: %s <ip> <port> <hotel rooms>\n", argv[0]);
            exit(-1);
        }
        else {
            hotel_max_available_rooms = atoi(argv[3]);
            if (hotel_max_available_rooms <= 0){
                printf("Usage: %s <ip> <port> <hotel rooms>\n", argv[0]);
                printf("\x1b[31mhotel rooms has to be >= 1\x1b[0m\n");
                exit(-1);
            }
        }

    #endif
    


    // ssetup ton server & epestrepse ton fd tou socket
    int sockfd = setupServer(&address);    


    // setup ta flags
    pthread_mutex_init(&lock_g, 0);
    pthread_mutex_init(&users_lock_g, 0);
    xp_sem_init(&free_threads, 0, NUM_THREADS);


    char ip_client[INET_ADDRSTRLEN];
    

    // setup database
    char mkdir_command[7 + sizeof(DATA_FOLDER)] = "mkdir ";
    strcat(mkdir_command, DATA_FOLDER); // DATA_FOLDER set inside `config.h`
    system(mkdir_command);

    if (setupDatabase() != 0){
        perror_die("Database error.");
    }
    #if DEBUG
        printf(ANSI_COLOR_GREEN "[+] Database setup OK.\n" ANSI_COLOR_RESET );
    #endif




    //  pool-thread
    for (int i = 0; i < NUM_THREADS; i++) {
        int rv;

        tid[i] = i;
        
        rv = pthread_create(&threads[i], NULL, threadHandler, (void*) &tid[i]);
        if (rv) {
            printf("ERROR: #%d\n", rv);
            exit(-1);
        }
        busy[i] = 0;                    // arxikopoihsh twn busy threads kai twn flags
        xp_sem_init(&evsem[i], 0, 0); 
    }






    while(1) 
    {
        int thread_index;


        xp_sem_wait(&free_threads);             // wait until a thread is free

            struct sockaddr_in client_addr;         // client address
            socklen_t addrlen = sizeof(client_addr);

            conn_sockfd = accept(sockfd, (struct sockaddr*) &client_addr, &addrlen);
            if (conn_sockfd < 0) {
                perror_die("accept()");
            }

            //metatroph tou network se present
            inet_ntop(AF_INET, &client_addr.sin_addr, ip_client, INET_ADDRSTRLEN);

            printf("%s: connection established  with client @ %s:%d\n",
                                                            "MAIN",
                                                            ip_client, 
                                                            address.port 
                                                        );


            // looking for free thread

            /* critical section */
            pthread_mutex_lock(&lock_g);

                for (thread_index = 0; thread_index < NUM_THREADS; thread_index++){  
                    // thread assignment
                    if (busy[thread_index] == 0) {
                        break;
                    }
                }
                printf("MAIN: Thread #%d has been selected.\n", thread_index);

                fds[thread_index] = conn_sockfd;    // vazei socket fd sto file descriptors array
                busy[thread_index] = 1;             // enhmerwnei to busy thread

            pthread_mutex_unlock(&lock_g);

        
        xp_sem_post(&evsem[thread_index]);        

    }

    

    close(conn_sockfd);


    return 0;

} // end main

//edw ksekinane ta functions


void* 
threadHandler(void* indx)
{
    int thread_index = *(int*) indx;       
    int conn_sockfd;                       

    
    printf("THREAD #%d ready.\n", thread_index);



    while(1)
    {
        
        //perimenoume to request apo to main thread 
        xp_sem_wait(&evsem[thread_index]);
        
        
            
            pthread_mutex_lock(&lock_g);
                conn_sockfd = fds[thread_index];
            pthread_mutex_unlock(&lock_g);
           


            // ekshphrethsh tou aithmatos 
            dispatcher(conn_sockfd, thread_index);
            
            // emfanizei to parakatw mhnuma otan o client exei aposundethei 
            printf ("Thread #%d closed session, client disconnected.\n", thread_index);
        


        
            // ginetai enhmerwsh tou main thread pws auto to thread einai eleuthero kai mporei na dextei nea aitimata
        
            
            pthread_mutex_lock(&lock_g);
                busy[thread_index] = 0;
                printf("MAIN: Thread #%d has been freed.\n", thread_index);
            pthread_mutex_unlock(&lock_g);
           
    
    
        xp_sem_post(&free_threads);

    }

    pthread_exit(NULL);
    
}


void 
dispatcher (int conn_sockfd, int thread_index)
{

    char command[BUFSIZE];
    Booking booking;        // metavliti gia na xeirizetai ta functions  
                            // krathseis kai akurwseis pelatwn 



    //dhmiourgeia tis metavliths pou krataei to current state toy FSM ston server 
    server_fsm_state_t state = INIT;

    

    // dhmiourgia tou entikeimenou user (pelath)
    User* user = (User*) malloc(sizeof(User));

    memset(user->username, '\0', sizeof(user->username));
    memset(user->actual_password, '\0', sizeof(user->actual_password));


    memset(booking.date, '\0', sizeof(booking.date));
    memset(booking.code, '\0', sizeof(booking.code));


    //xrhsimopoieitai otan ginetai h diadikasia 'view' request kai stelnei mhnuma pisw ston pelath 
    char reservation_response[BUFSIZE];
    char view_response[BUFSIZE];

    

    while (1) 
    {
    
    
        // apothikevei thn timh value pou xrhsimopoeitai sthn epanalhpsh 
        int rv;


        switch (state)
        {
            case INIT:
                memset(command, '\0', BUFSIZE);
                readSocket(conn_sockfd, command);  

                if      (strcmp(command, HELP_MSG) == 0){
                    printf("THREAD #%d: command received: \033[1m%s\x1b[0m\n", thread_index, "help");
                    state = HELP_UNLOGGED;
                }
                else if (strcmp(command, REGISTER_MSG) == 0){
                    printf("THREAD #%d: command received: \033[1m%s\x1b[0m\n", thread_index, "register");
                    state = REGISTER;
                }
                else if (strcmp(command, LOGIN_MSG) == 0){
                    printf("THREAD #%d: command received: \033[1m%s\x1b[0m\n", thread_index, "login");
                    state = LOGIN_REQUEST;
                }
                else if (strcmp(command, QUIT_MSG) == 0){
                    printf("THREAD #%d: command received: \033[1m%s\x1b[0m\n", thread_index, "quit");
                    state = QUIT;
                }
                else {
                    state = INIT;
                }
                break;
    

            case HELP_UNLOGGED:
                writeSocket(conn_sockfd, "H");
                state = INIT;
                break;



            case REGISTER:
                writeSocket(conn_sockfd, "Choose username: ");

                state = PICK_USERNAME;
                break;

            case PICK_USERNAME:
                memset(command, '\0', BUFSIZE);
                readSocket(conn_sockfd, command);
                strcpy(user->username, command);

                #if VERBOSE_DEBUG
                    printf("Username inserted: \033[1m%s\x1b[0m\n", user->username);
                #endif

                rv = usernameIsRegistered(user->username);

                if (rv == 0){
                    state = PICK_PASSWORD;
                    writeSocket(conn_sockfd, "Y");  // Y : "username OK.\nChoose password: "
                }
                else {
                    state = PICK_USERNAME;
                    writeSocket(conn_sockfd, "N");  // N : "username already taken, pick another one: "
                }

                break;

            case PICK_PASSWORD:
                memset(command, '\0', BUFSIZE);
                readSocket(conn_sockfd, command);
                strcpy(user->actual_password, command);

                #if VERBOSE_DEBUG
                    printf("Thread #%d: Plain text password inserted: \033[1m%s\x1b[0m\n", thread_index, user->actual_password);
                #endif

                state = SAVE_CREDENTIAL;
                break;

            case SAVE_CREDENTIAL:
                
                #if ENCRYPT_PASSWORD 
                    updateUsersRecordFile(user->username, encryptPassword(thread_index, user->actual_password));
                #else
                    updateUsersRecordFile(user->username, user->actual_password);
                #endif

                writeSocket(conn_sockfd, "password OK.");
                writeSocket(conn_sockfd, "Successfully registerd, you are now logged-in.");

                state = LOGIN;
                break;

            case LOGIN_REQUEST:
                writeSocket(conn_sockfd, "OK"); 
                state = CHECK_USERNAME;
                break;

            case CHECK_USERNAME:
                memset(command, '\0', BUFSIZE);
                readSocket(conn_sockfd, command);

                rv = usernameIsRegistered(command);

                if (rv == 1){
                    state = CHECK_PASSWORD;

                    // edw elegxei an o kwdikos pou exei dwthei einai o idios me ton kwdiko pou exei dwthei se prohgoymenh xrhsh tou pelath          
                    strcpy(user->username, command);

                    writeSocket(conn_sockfd, "Y");  // Y -> OK
                }
                else {
                    state = INIT;
                    writeSocket(conn_sockfd, "N");  // N ->  NOT OK
                }
                
                break;

            case CHECK_PASSWORD:
                memset(command, '\0', BUFSIZE);
                readSocket(conn_sockfd, command);

                strcpy(user->actual_password, command);

                #if VERBOSE_DEBUG
                    printf("Thread #%d: Plain text password received \033[1m%s\x1b[0m\n", thread_index, user->actual_password);
                #endif

                #if DEBUG
                    printf("Thread #%d: checking whether \033[1m%s\x1b[0m is in user.txt\n", thread_index, user->username);
                #endif


                rv = checkIfPasswordMatches(user);


                if (rv == 0){
                    state = GRANT_ACCESS;
                }
                else {
                    state = INIT;
                    writeSocket(conn_sockfd, "N");  // N -> NOT OK
                }
                
                break;

            case GRANT_ACCESS:
                writeSocket(conn_sockfd, "Y");  // Y -> OK
                state = LOGIN;
                break;

            case LOGIN:
                
                memset(command, '\0', BUFSIZE);
                readSocket(conn_sockfd, command);  

                if      (strcmp(command, HELP_MSG) == 0){ 
                    printf("THREAD #%d: command received: \033[1m%s\x1b[0m\n", thread_index, "help");
                    state = HELP_LOGGED_IN;
                }
                else if (strcmp(command, QUIT_MSG) == 0){  
                    printf("THREAD #%d: command received: \033[1m%s\x1b[0m\n", thread_index, "quit");
                    state = QUIT;
                }
                else if (strcmp(command, LOGOUT_MSG) == 0) {
                    printf("THREAD #%d: command received: \033[1m%s\x1b[0m\n", thread_index, "logout");
                    state = INIT;
                }
                else if (strcmp(command, VIEW_MSG) == 0){  
                    printf("THREAD #%d: command received: \033[1m%s\x1b[0m\n", thread_index, "view");
                    state = VIEW;
                }
                else if (strcmp(command, RESERVE_MSG) == 0){
                    printf("THREAD #%d: command received: \033[1m%s\x1b[0m\n", thread_index, "reserve");
                    state = CHECK_DATE_VALIDITY;
                }
                else if (strcmp(command, RELEASE_MSG) == 0){
                    printf("THREAD #%d: command received: \033[1m%s\x1b[0m\n", thread_index, "release");
                    state = RELEASE;
                }
                else {
                    state = LOGIN;        
                }

                break;

            case HELP_LOGGED_IN:
                writeSocket(conn_sockfd, "H");
                state = LOGIN;
                break;

            
            case CHECK_DATE_VALIDITY:
                memset(command, '\0', BUFSIZE);
                readSocket(conn_sockfd, command); 

                // ginetai validate o pelaths 
                strcpy(booking.date, command);
                rv = 0; 
                if (rv == 0){
                    state = CHECK_AVAILABILITY;
                }
                else {
                    state = LOGIN;
                    writeSocket(conn_sockfd, "BADDATE");
                }
                break;

            // elegxei thn diathesimotita kai thn epikurwsh twn data 
            case CHECK_AVAILABILITY:

                if (strcmp(assignRoom(thread_index, booking.date), "FULL") != 0){
                    state = RESERVE_CONFIRMATION;
                }
                else {
                    writeSocket(conn_sockfd, "NOAVAL");
                    state = LOGIN;
                }
                break;

            case RESERVE_CONFIRMATION:
                strcpy(booking.room, assignRoom(thread_index, booking.date));
                
                
                generateRandomString(booking.code, RESERVATION_CODE_LENGTH);
                upper(booking.code);


                saveReservation(thread_index, user, &booking);

                writeSocket(conn_sockfd, "RESOK");
                writeSocket(conn_sockfd, booking.room);
                writeSocket(conn_sockfd, booking.code);
                state = LOGIN;
                break;

            case VIEW:

                memset(reservation_response, '\0', sizeof(reservation_response));
                memset(query_result_g, '\0', sizeof(query_result_g));
                
                strcpy(reservation_response, fetchUserReservations(thread_index, user));


                if (strcmp(reservation_response, "") == 0){
                    writeSocket(conn_sockfd, "You have 0 active reservations.");
                }
                else {

                    // can be moved to server side except reservation_response...
                    memset(view_response, '\0', sizeof(view_response));
                    strcat(view_response, "Your active reservations in 2020");
                    #if SORT_VIEW_BY_DATE
                        strcat(view_response, " sorted by DATE");
                    #else

                    #endif
                    strcat(view_response, ":\n");
                    strcat(view_response, "-----+------+-------+\n");
                    strcat(view_response, "date | room | code  |\n");
                    strcat(view_response, "-----+------+-------+\n");
                    strcat(view_response, reservation_response);
                    writeSocket(conn_sockfd, view_response);
                }
                
                state = LOGIN;
                break;

            case RELEASE:

                // arxikopoiei prwtou diavasei 
                memset(&booking, '\0', sizeof booking);

                readSocket(conn_sockfd, booking.date);
                readSocket(conn_sockfd, booking.room);
                readSocket(conn_sockfd, booking.code);

                // anagkazei ton kwdika na einai se kefalaia grammata alliws den tairiazei 
                upper(booking.code);


                rv = releaseReservation(thread_index, user, &booking);

                if (rv == 0){
                    writeSocket(conn_sockfd, "\033[92mOK.\x1b[0m Reservation deleted successfully.");
                }
                else {
                    writeSocket(conn_sockfd, "\x1b[31mFailed. \x1b[0mYou have no such reservation.");
                }

                state = LOGIN;

                break;

            case QUIT:
                strcpy(command, "abort");
                
                free(user);
    
                printf("THREAD #%d: quitting\n", thread_index);
                break;

        }

        #if DEBUG
            printServerFSMState(&state, &thread_index);
        #endif
        

        // elegxei an h monh entolh pou mporei na klhsei to programma apo to while loop yparxei
        if (strcmp(command, "abort") == 0){
            return;
        }

    
    }

}


int 
commitToDatabase(int thread_index, const char* sql_command)
{
    sqlite3* db;
    char* err_msg = 0;
    
    int rc = sqlite3_open(DATABASE, &db);
    
    if (rc != SQLITE_OK) {
        fprintf(stderr, "Cannot open database: %s\n", sqlite3_errmsg(db));
        sqlite3_close(db);
        return -1;
    }
    

    #if VERBOSE_DEBUG
        
        if (thread_index >= 0){
            printf("Thread #%d: Committing to database:\n   %s\n", thread_index, sql_command);
        }
    #endif


    rc = sqlite3_exec(db, sql_command, 0, 0, &err_msg);
    
    if (rc != SQLITE_OK ) {
        fprintf(stderr, "SQL error: %s\n", err_msg);
        
        sqlite3_free(err_msg);        
        sqlite3_close(db);
        
        return 1;
    } 
    
    sqlite3_close(db);
    return 0;
}



query_t* 
queryDatabase(int thread_index, const int query_id, const char* sql_command) 
{
    
    query_t* query = (query_t*) malloc(sizeof(query_t)); // metavlhth pou epistrefetai kai ginetai arxikopoihsh 


    sqlite3* db;
    char* err_msg = 0;
    
    int rc = sqlite3_open(DATABASE, &db);

    
    if (rc != SQLITE_OK) {
        fprintf(stderr, "Cannot open database:\n%s\n", sqlite3_errmsg(db));
        sqlite3_close(db);
        
        query->rv = -1;
        query->query_result = (void*)"";
        return query;
    }
    


    switch (query_id){
        case 0:
            rc = sqlite3_exec(db, sql_command, viewCallback, 0, &err_msg);
            break;
        
        case 1:
            rc = sqlite3_exec(db, sql_command, busy_roomsCallback, 0, &err_msg);
            break;

        case 2:
            rc = sqlite3_exec(db, sql_command, validEntryCallback, 0, &err_msg); 
            break;
    }

    #if VERBOSE_DEBUG
        printf("Thread #%d: Querying from database:\n   %s\n", thread_index, sql_command);
    #endif


    if (rc != SQLITE_OK ) {
        fprintf(stderr, "Failed to select data\n");
        fprintf(stderr, "SQL error: %s\n", err_msg);

        sqlite3_free(err_msg);
        sqlite3_close(db);
        
        query->rv = -1;
        query->query_result = (void*)"";
        return query;
    } 

    sqlite3_close(db);


    // analoga me to query epistrefetai h katallhlh metavlhth 
    

    switch (query_id){
        case 0:

            
            query_result_g[strlen(query_result_g)-1] = '\0';

            query->rv = 0;
            query->query_result = (void*) query_result_g;
            break;
        
        case 1:
            query->rv = 0;
            query->query_result = (void*) rooms_busy_g;
            break;
        
        case 2:
            query->rv = 0;
            query->query_result = (void*) &entry_id_g;
            break;
    }

    return query;
}


int 
viewCallback (void* NotUsed, int argc, char** argv, char** azColName) 
{

    
    char tmp_str[64];
    memset(tmp_str, '\0', sizeof(tmp_str));


    for (int i = 0; i < argc; i++)
    {
        if (i==0){
            snprintf(tmp_str, sizeof(tmp_str), "%s   ", argv[i]);
        }
        else if (i == 1){
            snprintf(tmp_str, sizeof(tmp_str), "%s     ", argv[i]);   
        }
        else if (i == 2){
            snprintf(tmp_str, sizeof(tmp_str), "%s", argv[i]);   
        }
        strcat(query_result_g, tmp_str);

    }
    
    strcat(query_result_g, "\n");
        

    return 0;
}




int 
busy_roomsCallback(void* NotUsed, int argc, char** argv, char** azColName) 
{
    
    // rooms_busy_g einai to payload "to be returned". 
    // rooms_busy_g einai global metavlhth 

    //katharizei to payload prin to ksanagemisei ap thn arxh 
    memset(rooms_busy_g, '\0', sizeof(rooms_busy_g));
    
    strcpy(rooms_busy_g, argv[0]);  // h metavlhth pou epistrefetai einai mono enas arithmos kai ena mono argument 
                                    
    return 0;
}


int 
validEntryCallback(void* NotUsed, int argc, char** argv, char** azColName) 
{   
    entry_id_g = atoi(argv[0]);

    #if VERY_VERBOSE_DEBUG
        printf("Releasing entry with id %d\n", entry_id_g);
    #endif
    
    return 0;
}



int 
setupDatabase()
{

    char* sql_command = QUOTE(
            CREATE TABLE IF NOT EXISTS Bookings(
                `id`            INTEGER     PRIMARY KEY,
                `user`          TEXT        DEFAULT NULL,
                `date`          TEXT        DEFAULT NULL,
                `date_yyyymmdd` TEXT        DEFAULT NULL,
                `room`          TEXT        DEFAULT NULL,
                `code`          TEXT        DEFAULT NULL,

                UNIQUE(user, date, room)
            );
    );

    int rv;
    rv = commitToDatabase(-1, sql_command);
    
    return rv;  // 0 -> ok, -1 -> not ok
}


int 
usernameIsRegistered(char* u)
{
    char username[30];
    char enc_pass[30];
    char line[50];

    // kleidwnei to shared resource
    pthread_mutex_lock(&users_lock_g);

    FILE* users_file;

    users_file = fopen(USER_FILE, "r");
    if(users_file == NULL) {
        pthread_mutex_unlock(&users_lock_g);
        return 0;       // den uparxei to file ,kai gia auto den yparxei to username 
    }

    //elegxei an o user uparxei hdh sto arxeio 
    while(fgets(line, sizeof(line), users_file)) {
        sscanf(line, "%s %s\n", username, enc_pass);
        
        // elegxei an to username tairiazei me to username pou uparxei sto file tou server  
        if (strcmp(u, username) == 0){
            fclose(users_file);
            
            // ksekleidwnei koina resources prin to epistrepsei ston caller 
            pthread_mutex_unlock(&users_lock_g);
            return 1;   // vriskei ton xrhsth sto arxeio 
        }
    }

    fclose(users_file);

    
    pthread_mutex_unlock(&users_lock_g);
    
    return 0;           // to username `u` einai kainourio sto susthma kai gia ayto proxwrame sto registration .
}



int 
updateUsersRecordFile(char* username, char* encrypted_password)
{
    // o buffer tha apothikeusei to line sto arxeio 
    char buffer[USERNAME_MAX_LENGTH + PASSWORD_MAX_LENGTH + 2];
    memset(buffer, '\0', sizeof(buffer));

    // dhmiourgia tou "payload"
    strcat(buffer, username);
    strcat(buffer, " ");
    strcat(buffer, encrypted_password);

    // prostateuei ta shared resources me semaphores 
    pthread_mutex_lock(&users_lock_g);

    FILE* users_file;
    users_file = fopen(USER_FILE, "a+");
    if (users_file == NULL) {
        perror_die("fopen()");
    }

    // add line
    fprintf(users_file, "%s\n", buffer);

    // kleinei to connection me to ffile
    fclose(users_file);

    pthread_mutex_unlock(&users_lock_g);

    return 0;
}

char* 
encryptPassword(int thread_index, char* password)
{
    // epistrefetai o kruptografhmmenos kwdikos  
    static char res[512];   
    
    char salt[2];           

    generateRandomString(salt, 3);  // 3:   2 gia salt, 1 for '\0'.
    

    #if VERBOSE_DEBUG
        printf("Thread #%d: Salt: %c%c\n", thread_index, salt[0], salt[1]);
    #endif

    
    // antigrafei ton kryptograffhmeno kwdiko sto res kai to epistrefei 
    strncpy(res, crypt(password, salt), sizeof(res)); 

    return res;
}
    

int 
checkIfPasswordMatches(User* user) 
{
    char stored_username[USERNAME_MAX_LENGTH];
    char stored_enc_psswd[PASSWORD_MAX_LENGTH];
    char line[USERNAME_MAX_LENGTH + PASSWORD_MAX_LENGTH + 2];

    #if ENCRYPT_PASSWORD 
        char salt[2];
    #else
    #endif
    

    char res[512]; // to apotelesma ths apokruptografhshs

    // prostasia me thn xrhsh semaphore
    pthread_mutex_lock(&users_lock_g);

    FILE* users_file;

    users_file = fopen(USER_FILE, "r");     // opening file in read mode
    if (users_file == NULL) {
        perror("fopen(USER_FILE)");

        pthread_mutex_unlock(&users_lock_g);
        return -1;       // file is missing...
    }


    // kanei scroll sta lines tou arxeiou twn users 
    while(fgets(line, sizeof(line), users_file)){


        // pairnei ton kwdiko kai to arxeio apo to line 
        sscanf(line, "%s %s", stored_username, stored_enc_psswd);

        #if ENCRYPT_PASSWORD 
            // retrieve salt
            salt[0] = stored_enc_psswd[0];
            salt[1] = stored_enc_psswd[1];

            
            // antigrafh tou kruptografhmenou kwdikou sto res kai epistrofh toy 
            #if 1
                strncpy(res, crypt(user->actual_password, salt), sizeof(res)); 
            #else
                memset(res, '\0', sizeof(res));
                strcpy(res, crypt(user->actual_password, salt)); 
            #endif
            
        #else
            strcpy(res, user->actual_password);
        
        #endif
        // an to username kai o kwdikos tairiazoyn tote epityxanetai to login 
        if (strcmp(user->username, stored_username) == 0 && strcmp(res, stored_enc_psswd) == 0){
            fclose(users_file);
            pthread_mutex_unlock(&users_lock_g);
            return 0;
        }
    }

    fclose(users_file);
    pthread_mutex_unlock(&users_lock_g);
    return 1;

}


int 
saveReservation(int thread_index, User* u, Booking* b)
{
    int rv;


    // orismos ths hmeromhnias ws  <yyyymmdd> pou einai pio eukolo na ginei sort 
    char date_yyyymmdd[9] = {'2','0','2','0',b->date[3],b->date[4],b->date[0],b->date[1]};


    char sql_command[256];

    memset(sql_command, '\0', sizeof(sql_command));

   
    strcat(sql_command, "INSERT or IGNORE INTO Bookings(user, date, date_yyyymmdd, room, code) VALUES('");
    strcat(sql_command, u->username);
    strcat(sql_command, "', '");
    strcat(sql_command, b->date);
    strcat(sql_command, "', '");
    strcat(sql_command, date_yyyymmdd);
    strcat(sql_command, "', '");
    strcat(sql_command, b->room);
    strcat(sql_command, "', '");
    strcat(sql_command, b->code);
    strcat(sql_command, "');");
    

    rv = commitToDatabase(thread_index, sql_command);

    return rv;  // 0 -> OK, -1 -> not.
}



char* 
assignRoom(int thread_index, char* date)
{

    char sql_command[512];
    memset(sql_command, '\0', sizeof(sql_command));

    strcat(sql_command, "SELECT count(id) from Bookings WHERE date = '");
    strcat(sql_command, date);
    strcat(sql_command, "'");


    // Querying to datavase me thn entolh pou molis dhmiourgithike 
    query_t* query = (query_t*) malloc(sizeof(query_t));
    // memset(&query, 0, sizeof query );


    // arxikopoihsh ths metablhths auths 
    query = queryDatabase(thread_index, 1, sql_command);



    // elegxei ta apotelesmata tou query 
    if (query->rv == 0){     // an den epustrepsei 0 tote kati phge lathos 

        if (atoi(query->query_result) == 0){
            return "1";
        }
        else {

            char hotel_max_available_rooms_string[4];
            sprintf(hotel_max_available_rooms_string, "%d", hotel_max_available_rooms);


            // eisagwgh tou query sto `sql_command` string.

            memset(sql_command, '\0', sizeof(sql_command));

            strcat(sql_command, "SELECT MIN ( ( SELECT ifnull ( ( SELECT (room + 1) FROM Bookings WHERE ( (date = '");
            strcat(sql_command, date);
            strcat(sql_command, "') AND room+1 <= ");
            strcat(sql_command, hotel_max_available_rooms_string);
            strcat(sql_command, " AND (room + 1 NOT IN (SELECT DISTINCT room FROM Bookings WHERE date = '");
            strcat(sql_command, date);
            strcat(sql_command, "') ) ) ), 999 ) ) , ( SELECT ifnull ( ( SELECT (room -1) FROM Bookings WHERE ( (date = '");
            strcat(sql_command, date);
            strcat(sql_command, "') AND room-1 > 0 AND (room - 1 NOT IN (SELECT DISTINCT room FROM Bookings WHERE date = '");
            strcat(sql_command, date);
            strcat(sql_command, "') ) ) ), 999 ) ) )");

            query = queryDatabase(thread_index, 1, sql_command);

            // elegxei ta apotelesmata tou query 
            if (query->rv == 0){     // an den epistrepsei 0 tote kati phge lathos 

                if (atoi(query->query_result) == 999){
                    //to ksenodoxeio einai gemato 
                    free(query);
                    return "FULL";
                }
                else {
                    // free(query); ... to be done
                    return query->query_result; // NUMBER as string
                }
            }
            else {
                printf("%s\n", "Error querying the database!");
                free(query);
                return "";
            }

        }
    }
    else {
        printf("%s\n", "Error querying the database!");
        free(query);
        return "";
    }
}   





void
generateRandomString(char* str, size_t size)  // size_t: ikano type na kanei represent to megethos opoioudhpote antikeimeno se bytes 
{

    const char charset[] =  "ABCDEFGHIJKLMNOPQRSTUVWXYZ"
                            "abcdefghijklmnopqrstuvwxyz"
                            "0123456789";                       

    // init random number generator
    srand(time(NULL));
    

    if (size) {
        --size;
        for (size_t n = 0; n < size; n++) {
            int key = rand() % (int) (sizeof charset - 1);
            str[n] = charset[key];
        }
        // str[size] = '\0';
    }
    return;
}



char* 
fetchUserReservations(int thread_index, User* user)
{
    query_t* query = (query_t*) malloc(sizeof(query_t));
 
    char sql_command[1024];
    memset(sql_command, '\0', sizeof(sql_command));

    strcat(sql_command, "SELECT date, room, code FROM Bookings WHERE user = '");
    strcat(sql_command, user->username);
    #if SORT_VIEW_BY_DATE
        strcat(sql_command, "' ORDER BY date_yyyymmdd, room");
    #else
        strcat(sql_command, "' ORDER BY id");
    #endif
    

    query = queryDatabase(thread_index, 0, sql_command);


    if (query->rv == 0){
        return (char*) query->query_result;
    }
    else {
        printf("%s\n", "Error querying the database!");

        free(query);
        return "";
    }

}


int 
releaseReservation(int thread_index, User* user, Booking* booking)
{

    // proetimasia tou payload
    char sql_command[1024];
    memset(sql_command, '\0', sizeof(sql_command));
    strcat(sql_command, "SELECT COUNT(id) FROM Bookings WHERE user = '");
    strcat(sql_command, user->username);
    strcat(sql_command, "' and date = '");
    strcat(sql_command, booking->date);
    strcat(sql_command, "' and room = '");
    strcat(sql_command, booking->room);
    strcat(sql_command, "' and code = '");
    strcat(sql_command, booking->code);
    strcat(sql_command, "'");
    

    // allocating thn dynamikh metavlhth opou tha apothikeftoun ta apotelesmata toyu query 
    query_t* query = (query_t*) malloc(sizeof(query_t));
    query = queryDatabase(thread_index, 2, sql_command);
    

    if (query->rv == 0){
        
        if ( *((int*) query->query_result) == 0){  // 1 an einai sto database , 0 an den einai 
            
            free(query);
            return -1;
        }

        else {

            //proetimasia tou payload gia na svhsei to entry apo to table 

            memset(sql_command, '\0', sizeof(sql_command));
            strcat(sql_command, "DELETE FROM Bookings WHERE user = '");
            strcat(sql_command, user->username);
            strcat(sql_command, "' and date = '");
            strcat(sql_command, booking->date);
            strcat(sql_command, "' and room = '");
            strcat(sql_command, booking->room);
            strcat(sql_command, "' and code = '");
            strcat(sql_command, booking->code);
            strcat(sql_command, "'");

            // apeleftherwsei ths mnhmhs prin to epistrepsei 
            free(query);
            return commitToDatabase(thread_index, sql_command); // to 0 einai ok.

        }
    
    }
    else {
        printf("%s\n", "Error querying the database!");
        free(query);
        return -1;
    }

}
