#ifndef USER_H
#define USER_H

#include "config.h"

typedef struct user {
    char        username[USERNAME_MAX_LENGTH];
    char        actual_password[PASSWORD_MAX_LENGTH];
} User;


#endif

