#include <stdio.h> 
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>
#include <signal.h>

/*Bluetooth file - rfcomm0*/
int bl_file;
/*Remote controlled module file - mycar*/
int car_file;
/*Self navigating module file - autocar*/
int auto_file;
/*Flag to indicate if we are in the self driving module*/
int auto_mode = 0;

void sighandler(int);

int main (int argc, char *argv){
	/*
	 * Take data from the rfcomm (bluetooth) file and pass them
	 * to the car_kernel module
	 * In an infinite while loop the module will try to read
	 * data from the rfcomm file (if it exists) and then copy
	 * them to the car kernel module
	 */
	
	struct sigaction action;
	
	memset(&action,0,sizeof(action));
	action.sa_flags = SA_SIGINFO;
	action.sa_handler = sighandler;
	sigemptyset(&action.sa_mask);
	sigaction(SIGINT, &action, NULL);


	char line[256];

	while (1){
		
		if( access( "/dev/rfcomm0", F_OK ) == -1 ) {
			bl_file = open("/dev/rfcomm0",O_RDONLY);
			
			if (bl_file == -1){
				printf("No bluetooth connection\n");
				continue;
			}
    			
		}
		
		if( access( "/dev/mycar", F_OK ) == -1 ) {
			car_file = open("/dev/mycar",O_RDWR);
			
			if (car_file == -1){
				printf("Something is wrong with the car kernel module\n");
				continue;
			}
    			
		}
		
		if( access( "/dev/autocar", F_OK ) == -1 ) {
			auto_file = open("/dev/autocar",O_RDWR);
			
			if (auto_file == -1){
				printf("Something is wrong with the autocar kernel module\n");
				continue;
			}
    			
		}
		
		int i;
		// Transfer data character by character
		memset(line,'\0',256);
		while(i = (read(bl_file,line,256)) != 0){
			//Initiate self driving mode
			if (line[0] == 'N'){
				write(auto_file,line,1);
				memset(line,'\0',256);
				auto_mode = 1;
				
			}
			else if (line[0] == 'B'){
				write(auto_file,line,1);
				memset(line,'\0',256);
				auto_mode = 0;
				
			}
			//Remote controlled mode
			else{
				if (auto_mode == 0){
					write(car_file,line,1);
					memset(line,'\0',256);
				}
			}
		}
	}
	
	
	close(auto_file);
	close(car_file);
	close(bl_file);
	return 0;
}
	
void sighandler(int signo){
	
	if (signo == SIGINT){
		close(bl_file);
		close(car_file);
		close(auto_file);
		exit(0);
	}
}
