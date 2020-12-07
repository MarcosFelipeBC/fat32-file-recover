#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <fcntl.h>

int main (){
    int usb_file = open("/dev/sdb1", O_RDONLY); //This may change to you
    if(usb_file < 0){
        printf("FATAL ERROR\n");
        return -1;
    }
    unsigned char usb_file_data[512];
    int decimal_usb_data[512];
    
    read(usb_file, usb_file_data, 512);

    for (int i=0; i<512; i++) {
        decimal_usb_data[i] = (int)usb_file_data[i];
    }

    for (int i=0; i<512; i++) {
        printf("%02X", decimal_usb_data[i]);
        if((i+1)%16 == 0) printf("\n");
        else printf(" ");
    }

    close(usb_file);
    return 0;
}