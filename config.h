#ifndef CONFIG_H
#define CONFIG_H



#define GDB_MODE                0       //prepei na einai 0 sto production  
                                        //an den einai specified to compilation flag
                                        // -DGDB_MODE einai aparaithto gia na einai energo 





#define NUM_THREADS             2       // # threads
#define NUM_CONNECTION          10      // # queued connections

#define MAX_BOOKINGS_PER_USER   5       // o megistos arithmos booking ana user 

#define PASSWORD_MAX_LENGTH     32
#define PASSWORD_MIN_LENGTH     4
#define USERNAME_MAX_LENGTH     16
#define USERNAME_MIN_LENGTH     2



#define DEBUG                   1       // debug mode: printarei to mhnuma sthn konsola 
#if DEBUG
    #define VERBOSE_DEBUG       1       
    #if VERBOSE_DEBUG
    #define VERY_VERBOSE_DEBUG  0
    #endif
#endif

#define HELP_MESSAGE_TYPE_1     0       // 1 for type 1, 0 for type 2
#define SORT_VIEW_BY_DATE       1       // kathgoriopoiei to view response me vash thn hmeromhnia kai oxi me seira krathshs


#define HIDE_PASSWORD           1       // ayto elegxei gia to an tha krubetai h tha fainetai o kwdikos pou eisagei o xrhsths 




#define DATA_FOLDER             ".data"

//to USER_FILE kai to  DATABASE apothikevontai mesa sto e DATA_FOLDER/
#define USER_FILE_NAME          "users.txt"         //edw einai to text file pou emperiexei tous users kai sxetika krutpografhmena ta passwords 
#define DATABASE_NAME           "bookings.db"



#define BUFSIZE                 2048    // buffer size: megisto length mhnumatwn 
#define BACKLOG                 10      // listen() parametros ths eksiswshs 



#define ENCRYPT_PASSWORD        1
#define RESERVATION_CODE_LENGTH 6         


// ANSI terminal colors
#define ANSI_COLOR_RED          "\x1b[31m"
#define ANSI_COLOR_GREEN        "\x1b[32m"
#define ANSI_COLOR_YELLOW       "\x1b[33m"
#define ANSI_COLOR_BLUE         "\x1b[34m"
#define ANSI_COLOR_MAGENTA      "\x1b[35m"
#define ANSI_COLOR_BMAGENTA     "\x1b[95m"
#define ANSI_COLOR_CYAN         "\x1b[36m"
#define ANSI_COLOR_RESET        "\x1b[0m"
#define ANSI_BOLD               "\033[1m"


#endif