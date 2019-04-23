#include <stdio.h> 
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>
#include <signal.h>

int bl_file;
int car_file;
void sighandler(int);

int main (int argc, char *argv){
	/*
	 * Take data from the rfcomm (bluetooth) file and pass them
	 * to the car_kernel module
	 * In an infinite while loop the module will try to read
	 * data from the rfcomm file (if it exists) and then copy
	 * them to the car kernel module
	 *
	 *
	 * */
	
	struct sigaction action;
	
	memset(&action,0,sizeof(action));
	action.sa_flags = SA_SIGINFO;
	action.sa_handler = sighandler;
	sigemptyset(&action.sa_mask);
	sigaction(SIGINT, &action, NULL);


	char line[256];

	while (1){
		
		// Open the rfcomm file. Continue if not there
		bl_file = open("/dev/rfcomm0",O_RDONLY);
		
		if (!bl_file){
			printf("No bluetooth connection\n");
			continue;
		}
		
		// We can receive data from bluetooth. Pass them to the car kernel module
		car_file = open("/dev/mycar",O_RDWR);
		
		if (!car_file){
			printf("Something is wrong with the car kernel module\n");
			return 1;
		}
		
		int i;
		// Transfer data character by character
		memset(line,'\0',256);
		while(i = (read(bl_file,line,256)) != 0){
			//printf("%c\n",line[0]);
			write(car_file,line,1);
			memset(line,'\0',256);
		}

		close(bl_file);
		close(car_file);
	
	}
	return 0;
}
	
void sighandler(int signo){
	
	if (signo == SIGINT){
		close(bl_file);
		close(car_file);
		exit(0);
	}
}
