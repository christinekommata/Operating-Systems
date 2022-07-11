#ifndef XP_GETPASS  // XP einai gia cross-platform
#define XP_GETPASS


/* Xrisimopoioume auto to variable gia na thymomaste ta original terminal attributes. */
struct termios saved_attributes;

void
reset_input_mode(void)
{
    tcsetattr(STDIN_FILENO, TCSANOW, &saved_attributes);
}


void
set_input_mode(const char* prompt)
{
    struct termios tattr;
    // char* name;

    /* Prepei to stdin na einai terminal. */
    if (!isatty(STDIN_FILENO)){
        fprintf(stderr, "Not a terminal.\n");
        exit(EXIT_FAILURE);
    }

    //swzei ta attributes gia epanaklhsh argotera
    tcgetattr(STDIN_FILENO, &saved_attributes);
    
    #if 1
        atexit(reset_input_mode);
    #endif

    
    
    tcgetattr(STDIN_FILENO, &tattr);
    printf("%s", prompt);
    fflush(stdout);
    tattr.c_lflag &= ~(ICANON | ECHO);    // katharisma ICANON and ECHO
    tattr.c_cc[VMIN] = 1;
    tattr.c_cc[VTIME] = 0;
    tcsetattr(STDIN_FILENO, TCSAFLUSH, &tattr);
}


char*
term_getpass(const char* prompt)
{
    static char pass[100];
    char c;

    char asterisk = '*';

    set_input_mode(prompt);

    int i = 0;
    while (read (STDIN_FILENO, &c, 1) && (isalnum (c) || ispunct (c)) && i < sizeof (pass) - 2){
        pass[i++] = c;
        write (STDOUT_FILENO, &asterisk, 1);
    }

    pass[i] = '\0';

    printf("\n");
    return pass;

}


static inline char* 
xp_getpass(const char* prompt)
{
    #ifdef __APPLE__
        return getpass(prompt);
    #else

        #if 0
            return term_getpass(prompt);
        #else
            return getpass(prompt);
        #endif

    #endif
}


#endif