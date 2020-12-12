#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <fcntl.h>

#define BUFFER_SZ 1 << 20

int bytes_per_sector;
int sectors_per_cluster;
int reserved_sectors;
int num_of_fats;
int sectors_per_fat;
int root_cluster;
int root_directory_sector;
int cluster_bytes;

int usb_file;
char buffer[BUFFER_SZ];

unsigned char toUpper(unsigned char x) {
	if(x >= 'a' && x <= 'z') {
		x += ('A' - 'a');
	}
	return x;
}

int getFileFirstCluster(char* file_entry) {
	int hi = file_entry[20] + (file_entry[21] * 256);
	int lo = file_entry[26] + (file_entry[27] * 256);
	return lo + (hi * 256 * 256);
}

int getFileSize(char* file_entry) {
	int sz = file_entry[28] + (file_entry[29] * 256) + (file_entry[30] * 256*16) + (file_entry[31] * 256*256);
	return sz;
}

int checkRecovery(char* file_entry) {
	int first_cluster = getFileFirstCluster(file_entry);
	int sz = getFileSize(file_entry);
	int used_clusters = (sz / cluster_bytes);
	if (sz % cluster_bytes != 0) used_clusters++;
	lseek(usb_file, reserved_sectors * bytes_per_sector + first_cluster * 4, SEEK_SET);
	read(usb_file, buffer, 4 * used_clusters);
	for (int i=0; i<4*used_clusters; i++) {
		if((int)buffer[i] != 0) {
			return 0;
		}
	}
	return 1;
}

void recoverFATs(char *file_entry) {
	//Recover FAT1
	int first_cluster = getFileFirstCluster(file_entry);
	int sz = getFileSize(file_entry);
	int used_clusters = (sz / cluster_bytes);
	if (sz % cluster_bytes != 0) used_clusters++;
	lseek(usb_file, reserved_sectors * bytes_per_sector + first_cluster * 4, SEEK_SET);
	unsigned char new_clusters_entry[4 * used_clusters];
	
	unsigned int val = first_cluster + 1;

	for (int i=0; i<4*(used_clusters-1); i += 4) {
		for (int j=0; j<4; j++) {
			new_clusters_entry[i+j] = val & ((1 << 8)-1);
			val >>= 8;
		}
	}
	int left = 4*(used_clusters-1);
	new_clusters_entry[left] = new_clusters_entry[left+1] = new_clusters_entry[left+1] = 0xFF;
	new_clusters_entry[left+4] = 0x0F;
	write(usb_file, new_clusters_entry, 4*used_clusters);

	//Recover FAT2
	lseek(usb_file, (reserved_sectors + sectors_per_fat) * bytes_per_sector + first_cluster * 4, SEEK_SET);
	write(usb_file, new_clusters_entry, 4*used_clusters);
}

int main (){
    //First FAT: reserved_sectors number
    //First date: reserved_sectors + (number_of_fats * sectors_per_fat) = 32768 (sector)
    //Second date: First date + 8 sectors (1 cluster) = 32808
    //Third date: Second date + 8 sectors (1 cluster) = 32816

    usb_file = open("/dev/sdb1", O_RDWR); //This may change to you
    if(usb_file < 0){
        printf("FATAL ERROR\n");
        return -1;
    }

    bytes_per_sector = buffer[0x0B] + (buffer[0x0C] * 256);
	sectors_per_cluster = buffer[0x0D];
	reserved_sectors = buffer[0x0E] + (buffer[0x0F] * 256);
	num_of_fats = buffer[0x10];
	sectors_per_fat = *((unsigned int *)(buffer+0x24));
	root_cluster = *((unsigned int *)(buffer+0x2C));

    root_directory_sector = reserved_sectors + (num_of_fats * sectors_per_fat);
	cluster_bytes = sectors_per_cluster * bytes_per_sector;

    // Reading and changing the bytes from root directory
    lseek(usb_file, root_directory_sector * bytes_per_sector, SEEK_SET); 
    read(usb_file, buffer, cluster_bytes);

    for (int i=0; i<cluster_bytes; i += 32) {
        if((int)buffer[i] == 0xE5) {
			char file_entry[32];
			strncpy(file_entry, buffer+32, 32);
			if(!checkRecovery(file_entry)) continue ;
			recoverFATs(file_entry);
			unsigned char first_char = buffer[i+1];
			buffer[i+32] = toUpper(first_char);
			buffer[i] = 'A';
		}
    }
    write(usb_file, buffer, cluster_bytes);
/*
  MetaData sector (sector 32768)    two "E5". One in the A (41 to E5), other in the first letter of the filename
                                    (54 to E5)


  First FAT
  (sector 2116, offset 0001083382)  when the file is deleted, the final of first fat turns 
                                    from 0F 00 00 00 00 FF FF FF 0F (exist)
                                    to 0F 00 00 00 00 00 00 00 00  (removed)


  Second FAT
  (sector 17442, offset 0008930289) when the file is deleted, the final of second fat turns 
                                    from 0F 00 00 00 00 FF FF FF 0F (exist)
                                    to 0F 00 00 00 00 00 00 00 00  (removed)

  Data continues the same


*/

    close(usb_file);
    return 0;
}