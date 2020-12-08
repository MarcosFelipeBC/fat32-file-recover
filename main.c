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
    unsigned char fat_volume_id[512];
    
    read(usb_file, fat_volume_id, 512);

    // for (int i=0; i<512; i++) {
    //     printf("%02X", fat_volume_id[i]);
    //     if((i+1)%16 == 0) printf("\n");
    //     else printf(" ");
    // }

    int bytes_per_sector = fat_volume_id[0x0B] + (fat_volume_id[0x0C] * 256);
	int sectors_per_cluster = fat_volume_id[0x0D];
	int reserved_sectors = fat_volume_id[0x0E] + (fat_volume_id[0x0F] * 256);
	int num_of_fats = fat_volume_id[0x10];
	int sectors_per_fat = *((unsigned int *)(fat_volume_id+0x24));
	int root_cluster = *((unsigned int *)(fat_volume_id+0x2C));

    printf("Bytes per sector: %d\n", bytes_per_sector);
    printf("Sectors per cluster: %d\n", sectors_per_cluster);
    printf("Reserved sectors: %d\n", reserved_sectors);
    printf("Number of FATs: %d\n", num_of_fats);
    printf("Sectors per FAT: %u\n", sectors_per_fat);
    printf("Root Cluster: %u\n", root_cluster);

    close(usb_file);
    return 0;
}