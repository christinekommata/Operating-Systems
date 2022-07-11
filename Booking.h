#ifndef BOOKING_H
#define BOOKING_H

typedef struct booking {
    char    date[6];    // h imeromhnia einai maximum 6 xarakthres 
                       
    char    room[4];   // kai to room maximum 4 xarakthres  
    char    code[RESERVATION_CODE_LENGTH];    // einai alfarithmitika stoixeia kai prokuptoun apo autogenerated
} Booking;




void printBooking(Booking* b){
    printf("Booking on date %s, in room %s, with code %s\n", 
            b->date, b->room, b->code
        );
    return;
}




#endif 
