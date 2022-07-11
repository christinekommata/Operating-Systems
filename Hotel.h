#ifndef HOTEL_H
#define HOTEL_H



typedef struct hotel {
    int     available_rooms;
    int     booked_rooms[12][31];   // ta dwmatia pou einai diathesima kathe mera toy xronou 
} Hotel;


void    initializeHotel(Hotel* h);
int     bookRoom(Hotel* h, int day, int month);



// orismos methodologias
void initializeHotel(Hotel* h){
    for (int i = 0; i < 12; i++) {
        for (int j = 0; j < 31; j++) {
            h->booked_rooms[i][j] = 0;
        }
    }
    return;
}


/**
 * epistrefei 0 an h krathsh einai epityxhs,alliws epistrefei to -1
 */
int bookRoom(Hotel* h, int day, int month){
    if (h->booked_rooms[day][month] < h->available_rooms){
        h->booked_rooms[day][month]++;
        return 0;   // success
    }
    else {
        return -1;  // failure
    }
}



#endif