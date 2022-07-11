#ifndef ADDRESS_H
#define ADDRESS_H



typedef struct address {
    #if 0
        char*   ip;
    #else
        char    ip[16];
    #endif
    int     port;
} Address;




void repr_addr(Address* a);



void
repr_addr(Address* addr)
{
    printf("%s:%d\n", addr->ip, addr->port);
}


#endif