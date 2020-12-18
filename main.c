#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <fcntl.h>
#include <stdlib.h>

#define BUFFER_SZ 1 << 20

int bytes_per_sector;
int sectors_per_cluster;
int reserved_sectors;
int num_of_fats;
int sectors_per_fat;
int root_cluster;
int root_directory_sector;
int cluster_bytes;
int files_selected_amount;
char *file_list[105];

int usb_file;
unsigned char buffer[BUFFER_SZ];

unsigned char toUpper(unsigned char x) {
    if(x >= 'a' && x <= 'z') {
        x += ('A' - 'a');
    }
    return x;
}

unsigned int getFileFirstCluster(unsigned char* file_entry) {
    unsigned int hi = (unsigned int)file_entry[20] + ((unsigned int)file_entry[21] * 256);
    unsigned int lo = (unsigned int)file_entry[26] + ((unsigned int)file_entry[27] * 256);
    return lo + (hi * 256 * 256);
}

unsigned int getFileSize(unsigned char* file_entry) {
    unsigned int sz = (unsigned int)file_entry[28] + ((unsigned int)file_entry[29] * 256) + ((unsigned int)file_entry[30] * 256*256) + ((unsigned int)file_entry[31] * 256*256*256);
    return sz;
}

int checkRecovery(unsigned char* file_entry) {
    int first_cluster = getFileFirstCluster(file_entry); //quantidade de clusters antes do primeiro cluster que contém o arquivo
    int sz = getFileSize(file_entry); //quantidade de bytes do arquivo
    int used_clusters = (sz / cluster_bytes); //baseado no tamanho de arquivos, este valor representa a quantidade de clusters que o arquivo deve ocupar
    if (sz % cluster_bytes != 0) used_clusters++;
	unsigned char aux_buffer[4];
    lseek(usb_file, reserved_sectors * bytes_per_sector + first_cluster * 4, SEEK_SET);
    for (int i=0; i<used_clusters; i++) {
		read(usb_file, aux_buffer, 4);
        for (int j=0; j<4; j++) {
			if((int)aux_buffer[j] != 0) {
				return 0;
			}
		}
	}
    
    lseek(usb_file, (reserved_sectors + sectors_per_fat) * bytes_per_sector + first_cluster * 4, SEEK_SET);
    for (int i=0; i<used_clusters; i++) {
		read(usb_file, aux_buffer, 4);
        for (int j=0; j<4; j++) {
			if((int)aux_buffer[j] != 0) {
				return 0;
			}
		}
	}
    return 1;
}

void recoverFATs(unsigned char *file_entry) {
    //Recover file at FAT#1
    int first_cluster = getFileFirstCluster(file_entry);
    int sz = getFileSize(file_entry);
    int used_clusters = (sz / cluster_bytes);
    if (sz % cluster_bytes != 0) used_clusters++;
    lseek(usb_file, reserved_sectors * bytes_per_sector + first_cluster * 4, SEEK_SET);
    unsigned char new_clusters_entry[4];

    for (int i=0, cluster = first_cluster; i<4*(used_clusters-1); i += 4, cluster++) {
        unsigned int val = cluster + 1;
        for (int j=0; j<4; j++) {
            new_clusters_entry[j] = val & ((1 << 8)-1);
            val >>= 8;
        }
		write(usb_file, new_clusters_entry, 4);
    }
    new_clusters_entry[0] = new_clusters_entry[1] = new_clusters_entry[2] = 0xFF;
    new_clusters_entry[3] = 0x0F;
    write(usb_file, new_clusters_entry, 4);

    //Recover file at FAT#2
    lseek(usb_file, (reserved_sectors + sectors_per_fat) * bytes_per_sector + first_cluster * 4, SEEK_SET);
	for (int i=0, cluster = first_cluster; i<4*(used_clusters-1); i += 4, cluster++) {
        unsigned int val = cluster + 1;
        for (int j=0; j<4; j++) {
            new_clusters_entry[j] = val & ((1 << 8)-1);
            val >>= 8;
        }
		write(usb_file, new_clusters_entry, 4);
    }
	new_clusters_entry[0] = new_clusters_entry[1] = new_clusters_entry[2] = 0xFF;
    new_clusters_entry[3] = 0x0F;
    write(usb_file, new_clusters_entry, 4);
}

int isAtFileList(unsigned char *file_entry, char firstLetter) {
    char *filename = (char *)malloc(sizeof(char)*9);
    filename[0] = toUpper(firstLetter);
    int end_pos = 8;
    for (int i=1; i<8; i++){
        if(file_entry[i] == 0x20) {
            end_pos = i;
            break;
        }
        filename[i] = file_entry[i];
    }
    filename[end_pos] = '\0';

    char *extension = (char *)malloc(sizeof(char)*4);
    end_pos = 3;
    for (int i=8; i<11; i++) {
        if(file_entry[i] == 0x20) {
            end_pos = i;
            break;
        }
        extension[i-8] = file_entry[i];
    }
    extension[end_pos] = '\0';
    strcat(filename, ".");
    strcat(filename, extension);
    for (int i=0; i<files_selected_amount; i++) {
        printf("%s %s\n", filename, file_list[i]);
        if(strcmp(filename, file_list[i]) == 0) {
            return 1;
        }
    }
    return 0;
}

int main (){
    char device_filename[10];
    printf("Enter with your device filename: ");
    scanf("%s", device_filename);
    char device_path[20];
    sprintf(device_path, "/dev/%s", device_filename);
    usb_file = open(device_path, O_RDWR);
    if(usb_file < 0){
        printf("FATAL ERROR\n");
        return -1;
    }
    read(usb_file, buffer, 512);

    bytes_per_sector = buffer[0x0B] + (buffer[0x0C] * 256); //quantidade de bytes por setor
    sectors_per_cluster = buffer[0x0D]; //quantidade de setores por cluster
    reserved_sectors = buffer[0x0E] + (buffer[0x0F] * 256); //quantidade de setores reservados
    num_of_fats = buffer[0x10]; //número de FATs
    sectors_per_fat = *((unsigned int *)(buffer+0x24)); //quantidade de setores por FAT
    root_cluster = *((unsigned int *)(buffer+0x2C)); //índice do root cluster
    root_directory_sector = reserved_sectors + (num_of_fats * sectors_per_fat); //quantidade de setores antes do root directory
    cluster_bytes = sectors_per_cluster * bytes_per_sector; //quantidade de bytes por cluster

    printf("Recovery options:\n");
    printf("1 - Recover all possible files\n");
    printf("2 - Recover some given files if possible\n");
    printf("3 - Recover all possible files except the given ones\n\n");
    printf("Type the number of your recovery option (type an invalid number to quit): ");
    int option;
    scanf("%d", &option);
    if(option < 1 || option > 3) return 0;
    if(option != 1) {
        printf("Type how many files you want to select (up to 100): ");
        scanf("%d", &files_selected_amount);
        printf("\nType the name of each file with extension one per line:\n(Note: The name of the files are not case sensitive)\n");
        for (int i=0; i<files_selected_amount; i++) {
            file_list[i] = (char *)malloc(sizeof(char)*20);
            scanf("%s", file_list[i]);
            for (int j=0; j<strlen(file_list[i]); j++) {
                char c = file_list[i][j];
                if(c >= 'a' && c <= 'z') file_list[i][j] = toUpper(c);
            }
        }
    }

    lseek(usb_file, root_directory_sector * bytes_per_sector, SEEK_SET);
    read(usb_file, buffer, cluster_bytes);
    int files_recovered = 0;
    for (int i=0; i<cluster_bytes; i += 32) {
        if(buffer[i] == 0xE5) {
            unsigned char *file_entry;
            file_entry = (unsigned char *)malloc(sizeof(unsigned char)*32);
            for (int j=i+32; j<i+64; j++) file_entry[j-(i+32)] = buffer[j];
            
            if(option == 2 && !isAtFileList(file_entry, buffer[i+1])) {
                i += 32;
                continue ;
            }
            if(option == 3 && isAtFileList(file_entry, buffer[i+1])) {
                i += 32;
                continue ;
            }
            if(!checkRecovery(file_entry)){
                i += 32;
                continue ;
            }
            recoverFATs(file_entry);
            unsigned char first_char = buffer[i+1];
            buffer[i+32] = toUpper(first_char);
            buffer[i] = 'A';
            files_recovered++;
			i += 32;
        }
    }
    lseek(usb_file, root_directory_sector * bytes_per_sector, SEEK_SET);
    write(usb_file, buffer, cluster_bytes);

    close(usb_file);
    printf("Recovered a total of %d files\n", files_recovered);
    return 0;
}