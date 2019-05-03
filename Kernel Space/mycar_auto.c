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

// Define the pins used for controlling the motors and their
// PWM levels
#define PIN0 28
#define PIN1 29
#define PIN2 30
#define PIN3 31
#define IR_R 118
#define IR_L 101
#define PWM0 GPIO16_PWM0_MD
#define PWM1 GPIO17_PWM0_MD
#define PWM_CR_MASK 	0x3F           //0x3F is the largest value allowed for the PWM_CTR register
#define PWM_DCR_MASK 	0xD7           //0x3FF is the largest value allowed for the PWM_PWDUTY register
#define PWM_PCR_MASK    0x3FF          //0x3FF is the largest value allowed for the PWM_PERVAL register 
#define LEVEL 400                      //MAX
#define HALF 200


/* Declaration of memory.c functions */
static int mycar_auto_open(struct inode *inode, struct file *filp);
static int mycar_auto_release(struct inode *inode, struct file *filp);
static ssize_t mycar_auto_read(struct file *filp,
		char *buf, size_t count, loff_t *f_pos);
static ssize_t mycar_auto_write(struct file *filp,
		const char *buf, size_t count, loff_t *f_pos);

void del_func(unsigned long);

/* Structure that declares the usual file */
/* access functions */
struct file_operations mycar_auto_fops = {
	read: mycar_auto_read,
	write: mycar_auto_write,
	open: mycar_auto_open,
	release: mycar_auto_release
};

/* Major number */
static int mycar_auto_major = 62;
static char *buffer;

/*
 * Variables for keeping a simple history of previous corrections
 * and set the delay for the currect connection accordingly
 */

int l=0,r=0;   
int ll = 0, lll = 0, rr = 0, rrr = 0;

// timer used to gradually slow down/speed up
//static struct timer_list * acceleration_timer;



static struct timer_list * ir_sensor_timer;
static struct timer_list * delay_timer;

// Array to access easily
int pins[4] = {28,29,30,31};

short duty[3] = {32,128,1024};


static int mycar_auto_open(struct inode *inode, struct file *filp)
{
	/* Success */
	return 0;
}


static int mycar_auto_release(struct inode *inode, struct file *filp)
{
	/* Success */
	return 0;
}

int active = 0;

static ssize_t mycar_auto_write(struct file *filp, const char *buf,
							size_t count, loff_t *f_pos)
{
	
	// Buffer overflow mitigation
	if (count > 256)
		count = 256;

	memset(buffer, '\0', 256);
	
	if (copy_from_user(buffer, buf, count))
	{
		printk(KERN_INFO "Copy from user failed\n");
		return -EFAULT;
	}

	printk(KERN_INFO "write_auto %s %d\n",buffer,active);
	
	// Received command to go into self driving mode
        if (buffer[0] == 'N') {
		printk(KERN_INFO "N-\n");
		active = 1;
	// Received command to go back into the remote controlled mode
	}else if (buffer[0] == 'Z'){
		printk(KERN_INFO "Z-\n");
		active = 0;
                PWM_PWDUTY0 = 0;   
                PWM_PWDUTY1 = 0;
	} 

	int l,r;l=0,r=0;

	return count;
}
static ssize_t mycar_auto_read(struct file *filp, char *buf, 
							size_t count, loff_t *f_pos)
{
	return count;
}

static void mycar_auto_exit(void)
{
	printk(KERN_INFO "Exiting kernel module\n");
	
	/* Freeing buffer memory */
	// fancy dynamic buffer here to demonstration
	if (buffer) // but not needed as such
	{
		kfree(buffer);
	}
	
	/* Freeing the major number */
	unregister_chrdev(mycar_auto_major, "autocar");
	
	PWM_PWDUTY0 = 0;
	PWM_PWDUTY1 = 0;
	
	free_irq(IRQ_GPIO(IR_L),NULL);
	free_irq(IRQ_GPIO(IR_R),NULL);
	
	del_timer(ir_sensor_timer);
	if(ir_sensor_timer)
		kfree(ir_sensor_timer);
	
	del_timer(delay_timer);
	if(delay_timer)
		kfree(delay_timer);

}

void ir_sensing_func(unsigned long unused){
	
	del_timer(ir_sensor_timer);
	
	r = pxa_gpio_get_value(IR_R);  // Get right sensor data
	l = pxa_gpio_get_value(IR_L);  // Get left sensor data
	
	int delay = 1;

	if(active){
		// Both sensor "see" white
		// Just go forward
		if(!l && !r){
			lll=ll;ll=0; 
			rrr=rr;rr=0; 
			pxa_gpio_set_value(pins[0],0);
			pxa_gpio_set_value(pins[1],1);
			pxa_gpio_set_value(pins[2],0);
			pxa_gpio_set_value(pins[3],1);
			PWM_PWDUTY1 = LEVEL;PWM_PWDUTY0 = LEVEL; 
			delay = 50;
		} 
		// Need to do a correction to the left
		else if(!l){
			pxa_gpio_set_value(pins[0],0);
			pxa_gpio_set_value(pins[1],1);
			pxa_gpio_set_value(pins[2],1);
			pxa_gpio_set_value(pins[3],0);
			PWM_PWDUTY0 = LEVEL;PWM_PWDUTY1 = LEVEL;
			delay = 42+ ll*42 + lll*42;    //set delay based on history
			lll = ll; ll = 1;              //set history variables for left correction
			rrr=rr;rr=0;
		}
		// Need to do a correction to the right
		else if(!r){
			pxa_gpio_set_value(pins[0],1);
			pxa_gpio_set_value(pins[1],0);
			pxa_gpio_set_value(pins[2],0);
			pxa_gpio_set_value(pins[3],1);
			PWM_PWDUTY1 = LEVEL;PWM_PWDUTY0 = LEVEL;
			delay = 42+ rr*42 + rrr*42; 	//set delay based on history
			lll=ll;ll=0;
			rrr=rr;rr=1;                    //set history variables for right correction
		}
		// Both sensors see black -> stop
		else{	
			lll=ll;ll=0;rrr=rr;rr=0;        //stop the motors
			PWM_PWDUTY1 = 0;PWM_PWDUTY0 = 0;
		}
	}
                setup_timer(delay_timer, del_func, 0);
                mod_timer(delay_timer, jiffies + msecs_to_jiffies(delay));
}

void del_func(unsigned long unused){
	del_timer(delay_timer);
	
	if(active){
		PWM_PWDUTY1 = 0;PWM_PWDUTY0 = 0;
	}
	
	setup_timer(ir_sensor_timer, ir_sensing_func, 0);
	mod_timer(ir_sensor_timer, jiffies + msecs_to_jiffies(200));
}

static int __init mycar_auto_init(void)
{
	int result;
	printk(KERN_INFO "Inserting kernel module\n");

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
	result = register_chrdev(mycar_auto_major, "autocar", &mycar_auto_fops);
	if (result < 0)
	{
		printk(KERN_ALERT
			"autocar: cannot obtain major number %d\n", mycar_auto_major);
		return result;
	}
	
	// Set the PWM pins for both motors
	pxa_gpio_mode(GPIO16_PWM0_MD);
	pxa_set_cken(CKEN0_PWM0, 1);
	pxa_gpio_mode(GPIO17_PWM1_MD);
	pxa_set_cken(CKEN1_PWM1, 1);
	
	//Set the direction of the GPIO pins that control the motors to ouput
	gpio_direction_output(PIN0, 1);
        gpio_direction_output(PIN1, 1);
	gpio_direction_output(PIN2, 1);
        gpio_direction_output(PIN3, 1);
        pxa_gpio_set_value(PIN0, 1);
        pxa_gpio_set_value(PIN1, 1);
        pxa_gpio_set_value(PIN2, 1);
        pxa_gpio_set_value(PIN3, 1);

	//Set the direction of the GPIO pins that get data from the IR sensors to input
        gpio_direction_input(IR_L);
        gpio_direction_input(IR_R);
        pxa_gpio_mode(IR_L | GPIO_IN);
        pxa_gpio_mode(IR_R | GPIO_IN);
	
	// Set the PWM registers for both motors
	PWM_CTRL0    = PWM_CR_MASK;               //Set Scaled Counter Clock
	PWM_PWDUTY0 =  0;                         // Set Duty Cycle
	PWM_PERVAL0 = PWM_PCR_MASK;               // Set Period 
	
	PWM_CTRL1    = PWM_CR_MASK;               //Set Scaled Counter Clock
	PWM_PWDUTY1 =  0;                         // Set Duty Cycle
	PWM_PERVAL1 = PWM_PCR_MASK;               // Set Period 

	//Drive the motors briefly to indicate that the module is inserted to the user
	PWM_PWDUTY0 = LEVEL;
	PWM_PWDUTY1 = LEVEL;
        pxa_gpio_set_value(pins[0],0);
        pxa_gpio_set_value(pins[1],1);
	pxa_gpio_set_value(pins[2],1);
        pxa_gpio_set_value(pins[3],0);
	
	// Stop the motors
	msleep(500);
	PWM_PWDUTY0 = 0;
        PWM_PWDUTY1 = 0;
	
	// Set the two timers: One for sampling the data from sensors and one for the duration of the 
	// path correction
	ir_sensor_timer = (struct timer_list *) kmalloc(sizeof(struct timer_list), GFP_KERNEL);
	delay_timer = (struct timer_list *) kmalloc(sizeof(struct timer_list), GFP_KERNEL);
	
	if (!ir_sensor_timer || !delay_timer)
        {
                printk(KERN_ALERT "Insufficient kernel memory\n");
                result = -ENOMEM;
                goto fail;
        }
        
	setup_timer(ir_sensor_timer, ir_sensing_func, 0);
        mod_timer(ir_sensor_timer, jiffies + msecs_to_jiffies(500));
	
	return 0;
fail: 
	mycar_auto_exit();
	return 0;
}

module_init(mycar_auto_init);
module_exit(mycar_auto_exit);
MODULE_LICENSE("GPL");
