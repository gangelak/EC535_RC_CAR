#include <stdio.h> 
#include <stdlib.h>



int main (){
	int bl_file,car_file;
	/*
	 * Take data from the rfcomm (bluetooth) file and pass them
	 * to the car_kernel module
	 * In an infinite while loop the module will try to read
	 * data from the rfcomm file (if it exists) and then copy
	 * them to the car kernel module
	 *
	 *
	 * */
	
	char *c;

	while (1){
		
		// Open the rfcomm file. Continue if not there
		bl_file = open("/dev/rfcomm0","r");
		
		if (!bl_file)
			continue;
		
		// We can receive data from bluetooth. Pass them to the car kernel module
		car_file = open("/dev/my_car","w");
		
		if (!car_file){
			printf("Something is wrong with the car kernel module\n");
			return 1;
		}
		
		int i;
		// Transfer data character by character
		while(i = (read(bl_file,c,1)) != 0){
			write(car_file,c,1);
		}

		close(bl_file);
		close(car_file);
	
	}

}
