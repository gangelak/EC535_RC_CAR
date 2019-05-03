/*  
 * Authors: Stefan Gvozdenovic, Ioannis Angelakopoulos
 */

#include <linux/module.h>
#include <linux/init.h>
#include <linux/proc_fs.h>
#include <linux/string.h>
#include <linux/ctype.h>

#include <asm/hardware.h>
#include <asm/arch/pxa-regs.h>
#include <asm/uaccess.h>
#include <asm-arm/arch-pxa/gpio.h>

#include <linux/init.h>
#include <linux/module.h>
#include <linux/kernel.h> /* printk() */
#include <linux/slab.h> /* kmalloc() */
#include <linux/fs.h> /* everything... */
#include <linux/errno.h> /* error codes */
#include <linux/types.h> /* size_t */
#include <linux/proc_fs.h>
#include <linux/fcntl.h> /* O_ACCMODE */
#include <linux/jiffies.h> /* jiffies */
#include <asm/system.h> /* cli(), *_flags */
#include <asm/uaccess.h> /* copy_from/to_user */

#include <linux/interrupt.h>
#include <linux/delay.h>

/* Declaration of memory.c functions */
static int mycar_open(struct inode *inode, struct file *filp);
static int mycar_release(struct inode *inode, struct file *filp);
static ssize_t mycar_read(struct file *filp,
		char *buf, size_t count, loff_t *f_pos);
static ssize_t mycar_write(struct file *filp,
		const char *buf, size_t count, loff_t *f_pos);
/*
 * Define the GPIO pin numbers and the GPIO max and half levels
 */ 


#define PIN0 28
#define PIN1 29
#define PIN2 30
#define PIN3 31
#define IR_R 118
#define IR_L 101
#define BT2 117
#define BT1 101
#define BT0 118
#define PWM0 GPIO16_PWM0_MD
#define PWM1 GPIO17_PWM0_MD
#define PWM_CR_MASK 0x3F           //0x3F is the largest value allowed for the PWM_CTR register
#define PWM_DCR_MASK 0xD7          //0x3FF is the largest value allowed for the PWM_PWDUTY register
#define PWM_PCR_MASK    0x3FF      //0x3FF is the largest value allowed for the PWM_PERVAL register 
#define LEVEL 500
#define HALF 200

#define STEP (100)
#define ABS(x)  ((x<0)?-x:x)
// C = A % B is equivalent to C = A – B * (A / B)
#define MODULO(A,B) (A-B*(A/B))

#if (MODULO(LEVEL,STEP)!=0 || MODULO(HALF,STEP)!=0)
  #error "level/step must be multiple of level"
#endif
#if (LEVEL==0 || HALF==0)
  #error "LEVEL/HALF cant be zero"
#endif


int motor_l = 0; //Current PWM level of left motor
int motor_r = 0; //Current PWM level of right motor
int dir = 1;     //Direction of the two motors

/* Structure that declares the usual file */
/* access functions */
struct file_operations mycar_fops = {
	read: mycar_read,
	write: mycar_write,
	open: mycar_open,
	release: mycar_release
};

/* Major number */
static int mycar_major = 61;
static char *buffer;

/*Arrays to easily access the GPIO pins and PWM levels*/

int pins[4] = {28,29,30,31};
short duty[3] = {32,128,1024};



// B
void back(int * ref_motor){
	
	int motor = *ref_motor;
	// If motor -> fwd spin then reduce its PWM level
	if (motor > STEP){
		motor -= STEP;
		dir = 1;
	}
	// If motor is stopped change its direction to
	// go backwards
	else if(motor == 0){
		dir = 0;	
		motor -= STEP;
	}
	// Increase the PWM Level to bwd direction until it
	// reaches the maximum level
	else if(motor > -LEVEL && motor <= STEP){
		motor -= STEP;
		dir = 0;
	}
	
	*ref_motor = motor;
}

// F
void front(int * ref_motor){
	int motor = *ref_motor;
	// If motor -> bwd spin reduce its PWM level
	if (motor < 0){
		dir = 0;
		motor += STEP;
	} 
	//If motor is stopped change its direction to 
	//go forwards
	else if (motor==0){
		dir = 1;
		motor += STEP;
	}
	// Increase the PWM level to fwd direction until it
	// reaches the mazimum level
	else if (motor < LEVEL && motor>0){
		motor += STEP;
		dir = 1;
	}
	
	*ref_motor = motor;
}

// L,R
void turn(int *ref_turn_motr, int *ref_stop_motr){
	int turn_motr = *ref_turn_motr;
	int stop_motr = *ref_stop_motr;
	
	// Stop the motor on the side of turn gracefully
	if(stop_motr < 0){
		stop_motr += STEP;
	}
	
	else if (stop_motr > 0){
		stop_motr -= STEP;
	}
	
	// Increase the level of the opposite motor to turn
	if (turn_motr < LEVEL){
		turn_motr += STEP;
	}
	// If the direction of the turning motor was bwd 
	// change it to fwd and increase the PWM level
	else if(turn_motr ==0){
		dir = 1;
		turn_motr += STEP;
	}

	*ref_turn_motr = turn_motr;
	*ref_stop_motr = stop_motr;
}

// Q,E
void half_turn_front(int *ref_turn_motr, int *ref_half_motr){
	
	//Direction is fwd
	int turn_motr = *ref_turn_motr;
	int half_motr = *ref_half_motr;
	
	// Bring the level of the motor to the side of turn 
	// to half the max PWM level
	if (half_motr < HALF){
		half_motr += STEP;
	}

	else if(half_motr ==0){
		dir = 1;
		half_motr += STEP;
	}

	else if(half_motr >HALF){
		half_motr -= STEP;
	}
	
	// Bring the level of the opposite motor to the max
	// PWM level
	if (turn_motr < LEVEL){
		turn_motr += STEP;
	}
	
	else if(turn_motr ==0){
		dir = 1;
		turn_motr += STEP;
	}

	*ref_turn_motr = turn_motr;
	*ref_half_motr = half_motr;
}
// A,D
void half_turn_back(int *ref_turn_motr, int *ref_half_motr){
	
	// Direction is bwd
	int turn_motr = *ref_turn_motr;
	int half_motr = *ref_half_motr;
	
	// Bring the level of the motor to the side of turn
	// to half the max PWM level
	if (half_motr > -HALF){
		half_motr -= STEP;
	}
	
	else if(half_motr ==0){
		dir = 0;
		half_motr -= STEP;
	}
	
	else if(half_motr < -HALF){
		half_motr += STEP;
	}
	
	// Bring the level of the opposite motor to the max
	// PWM level
	if (turn_motr > -LEVEL){
		turn_motr -= STEP;
	}

	else if(turn_motr == 0){
		dir = 0;
		turn_motr -= STEP;
	}

	*ref_turn_motr = turn_motr;
	*ref_half_motr = half_motr;
}

static int mycar_open(struct inode *inode, struct file *filp)
{
	/* Success */
	return 0;
}

static int mycar_release(struct inode *inode, struct file *filp)
{
	/* Success */
	return 0;
}
static ssize_t mycar_write(struct file *filp, const char *buf,
							size_t count, loff_t *f_pos)
{
	
	// Prevent buffer overflow
	if (count > 256)
		count = 256;

	memset(buffer, '\0', 256);

	if (copy_from_user(buffer, buf, count))
	{
		printk(KERN_INFO "Copy from user failed\n");
		return -EFAULT;
	}
	
	printk(KERN_INFO "write %s\n",buffer);
	
	/*
	 * Based on the command the module got, call the correct function
	 */

        if (buffer[0] == 'F') {
		printk(KERN_INFO "F-\n");
		front(&m0);
		front(&m1);
	} 
     	else if (buffer[0] == 'Q') {
                printk(KERN_INFO "Q-\n");
		half_turn_front(&m1,&m0);
        } 
        else if (buffer[0] == 'E') {
                printk(KERN_INFO "E-\n");
		half_turn_front(&m0,&m1);
        }
	else if (buffer[0] == 'B') {
		printk(KERN_INFO "B-\n"); 
		back(&m0);
		back(&m1);
        }
        else if (buffer[0] == 'A') {
                printk(KERN_INFO "A-\n");
		half_turn_back(&m1,&m0);
        }
        else if (buffer[0] == 'D') {
                printk(KERN_INFO "D-\n");
		half_turn_back(&m0,&m1);
        }
	else if (buffer[0] == 'L'){
		printk(KERN_INFO "L-\n");
		turn(&m1,&m0);
      	}
	
	else if (buffer[0] == 'R'){
        	printk(KERN_INFO "R-\n");  
		turn(&m0,&m1);
	}

	else if (buffer[0] == 'S'){
		printk(KERN_INFO "S-\n"); 
		if (m0>0){ 
			m0 -= STEP;
		}
		else if(m0<0){
			m0 += STEP;
		}
		if (m1>0){
			m1 -= STEP;
		}
		else if(m1<0){
			m1 += STEP;
		}
	}
	
	else {
          	printk(KERN_INFO "Incorrect usage\n");
        }

	/*
	 * Set the motor GPIO pins based on the direction
	 */

	// Forward direction
	if (dir) {
          	pxa_gpio_set_value(pins[0],0);
          	pxa_gpio_set_value(pins[1],1);
 		pxa_gpio_set_value(pins[2],0);
                pxa_gpio_set_value(pins[3],1);
	} 
	// Backward direction
	else {
                pxa_gpio_set_value(pins[0],1);
                pxa_gpio_set_value(pins[1],0);
		pxa_gpio_set_value(pins[2],1);
                pxa_gpio_set_value(pins[3],0);
	}

	printk(KERN_ALERT "m0: %d\n",m0);
	printk(KERN_ALERT "m1: %d\n",m1);
        // Set the PWM level for both motors
	PWM_PWDUTY1 = ABS(m1);
        PWM_PWDUTY0 = ABS(m0);

	return count;
}

static ssize_t mycar_read(struct file *filp, char *buf, 
							size_t count, loff_t *f_pos)
{
	return count;
}

static void mycar_exit(void)
{
	printk(KERN_INFO "Exiting kernel module\n");
	
	/* Freeing buffer memory */
	// fancy dynamic buffer here to demonstration
	if (buffer) // but not needed as such
	{
		kfree(buffer);
	}
	
	/* Freeing the major number */
	unregister_chrdev(mycar_major, "mycar");
	
	PWM_PWDUTY0 = 0;
	PWM_PWDUTY1 = 0;
}
static int __init mycar_init(void)
{
	int result;
	printk(KERN_INFO "Inserting module\n");
	
	/* Allocating mytimer for the buffer */
	buffer = kmalloc(256, GFP_KERNEL); 
	if (!buffer)
	{ 
		printk(KERN_ALERT "Insufficient kernel memory\n"); 
		result = -ENOMEM;
		goto fail; 
	} 
	memset(buffer, 0, 256);

	/* Registering device */
	result = register_chrdev(mycar_major, "mycar", &mycar_fops);
	if (result < 0)
	{
		printk(KERN_ALERT
			"my_car: cannot obtain major number %d\n", mycar_major);
		return result;
	}
	
	/*Setting up the PWD pins for the two motors*/
	pxa_gpio_mode(GPIO16_PWM0_MD);
	pxa_set_cken(CKEN0_PWM0, 1);
	pxa_gpio_mode(GPIO17_PWM1_MD);
	pxa_set_cken(CKEN1_PWM1, 1);
	
	/*
	 * Setting up the GPIO pins that drive the two motors
	 * as output pins
	 * Each motor requires two pins that indicate the
	 * spin direction
	 * Left motor: PIN0, PIN1
	 * Right motor: PIN2, PIN3
	 */
	
	gpio_direction_output(PIN0, 1);
        gpio_direction_output(PIN1, 1);
	gpio_direction_output(PIN2, 1);
        gpio_direction_output(PIN3, 1);
        pxa_gpio_set_value(PIN0, 1);
        pxa_gpio_set_value(PIN1, 1);
        pxa_gpio_set_value(PIN2, 1);
        pxa_gpio_set_value(PIN3, 1);
	
	//Set the PWM0 register 
	PWM_CTRL0    = PWM_CR_MASK;               // Set Scaled Counter Clock
	PWM_PWDUTY0 =  0;                         // Set Duty Cycle
	PWM_PERVAL0 = PWM_PCR_MASK;               // Set Period 
	
	// Set the PWM1 register  
	PWM_CTRL1    = PWM_CR_MASK;               // Set Scaled Counter Clock
	PWM_PWDUTY1 =  0;                         // Set Duty Cycle
	PWM_PERVAL1 = PWM_PCR_MASK;               // Set Period 

	/*
	 * Set the PWM briefly to max for both motors 
	 * to show to the user that the module is 
	 * loaded and the pins are configured properly
	 */ 

	PWM_PWDUTY0 = LEVEL;
	PWM_PWDUTY1 = LEVEL;
                pxa_gpio_set_value(pins[0],0);
                pxa_gpio_set_value(pins[1],1);
                pxa_gpio_set_value(pins[2],1);
                pxa_gpio_set_value(pins[3],0);
	
	// Reset the PWM to 0 for both motors
	msleep(500);
	PWM_PWDUTY0 = 0;
        PWM_PWDUTY1 = 0;

	return 0;
fail: 
	mycar_exit();
	return 0;
}

module_init(mycar_init);
module_exit(mycar_exit);
MODULE_LICENSE("GPL");
