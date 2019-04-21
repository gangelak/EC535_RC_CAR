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
static int mygpio_open(struct inode *inode, struct file *filp);
static int mygpio_release(struct inode *inode, struct file *filp);
static ssize_t mygpio_read(struct file *filp,
		char *buf, size_t count, loff_t *f_pos);
static ssize_t mygpio_write(struct file *filp,
		const char *buf, size_t count, loff_t *f_pos);
/* Structure that declares the usual file */
/* access functions */
struct file_operations mygpio_fops = {
	read: mygpio_read,
	write: mygpio_write,
	open: mygpio_open,
	release: mygpio_release
};

/* Major number */
static int mygpio_major = 61;
static char *buffer;

int times_read = 0;
char seconds[5];
//static struct timer_list * fasync_timer;
//static struct timer_list * irq_timer;
char speed = 'M';

int counter = 1;
int counter_period = 1000;
#define LED0 28
#define LED1 29
#define LED2 30
#define LED3 31
#define BT2 117
#define BT1 101
#define BT0 118
#define PWM0 GPIO16_PWM0_MD
#define PWM1 GPIO17_PWM0_MD
#define PWM_CR_MASK 0x3F               //0x3F is the largest value allowed for the PWM_CTR register
#define PWM_DCR_MASK 0xD7          //0x3FF is the largest value allowed for the PWM_PWDUTY register
#define PWM_PCR_MASK    0x3FF       //0x3FF is the largest value allowed for the PWM_PERVAL register 
int leds[4] = {28,29,30,31};

//#define MYGPIO 101
volatile int pwm0 = 0;
volatile int pwm1 = 0;

short duty[3] = {32,128,1024};
char brightness[3] = {'L','M','H'};
char cnt, dir = 0;
int irq_global = 0;
int timer_running = 0;

/*
irqreturn_t btn0_irq(int irq, void *dev_id, struct pt_regs *regs)
{
	return IRQ_HANDLED;
}

irqreturn_t btn1_irq(int irq, void *dev_id, struct pt_regs *regs)
{
	return IRQ_HANDLED;
}
*/
static int mygpio_open(struct inode *inode, struct file *filp)
{
	/* Success */
	return 0;
}

static int mygpio_release(struct inode *inode, struct file *filp)
{
	/* Success */
	return 0;
}

static ssize_t mygpio_write(struct file *filp, const char *buf,
							size_t count, loff_t *f_pos)
{
	int len;
	memset(buffer, 0, 256);
	if (copy_from_user(buffer, buf, count))
	{
		printk(KERN_INFO "Copy from user failed\n");
		return -EFAULT;
	}
printk(KERN_INFO "write\n");
        if (buffer[0] == 'F') {
printk(KERN_INFO "F-\n");
          pxa_gpio_set_value(leds[0],0);
          pxa_gpio_set_value(leds[1],0);
        }
        else if (buffer[0] == 'B') {
printk(KERN_INFO "B-\n"); 
	 pxa_gpio_set_value(leds[0],1);
          pxa_gpio_set_value(leds[1],1);
        }else if (buffer[0] == 'L'){
	printk(KERN_INFO "L-\n");
	  PWM_PWDUTY1 = 50;
        }else if (buffer[0] == 'R'){
        printk(KERN_INFO "R-\n");  
	PWM_PWDUTY0 = 50;
        }else if (buffer[0] == 'S'){
	printk(KERN_INFO "S-\n"); 
	 PWM_PWDUTY0 = 0;
	  PWM_PWDUTY1 = 0;
	}else {
          printk(KERN_INFO "Incorrect usage\n");
        }

	return count;
}
static ssize_t mygpio_read(struct file *filp, char *buf, 
							size_t count, loff_t *f_pos)
{
	char tbuf[256], *tbptr;
	tbptr = tbuf;
	memset(tbptr,'\0',256);
/*	if (times_read == 0){
		tbptr += sprintf(tbptr,"%d %5c %8s %5s %6c\n", counter, speed, cnt ? "Count" : "Hold", dir ? "Up" : "Down", brightness[global] );

		times_read++;
	}		
	else{
		times_read = 0;	
	}
	if (copy_to_user(buf, tbuf, sizeof(tbuf)))
	{
		printk(KERN_INFO "fault transfering data to user 216\n");
		return -EFAULT;
	}*/
	count = strlen(tbuf);
	return count;
}

static void gpio_exit(void)
{
	/* Freeing buffer memory */
	// fancy dynamic buffer here to demonstration
	if (buffer) // but not needed as such
	{
		kfree(buffer);
	}
	/* Freeing the major number */
	unregister_chrdev(mygpio_major, "my_car");


/*	free_irq(IRQ_GPIO(BT0), NULL);
	free_irq(IRQ_GPIO(BT1), NULL);
*/
}

/*static void timer_handler(unsigned long data) {

//struct file *f;
//char buf[128];
//f = filp_open("/dev/rfcomm0", O_RDONLY, 0);

       deltime(fasync_timer);
       setup_timer(fasync_timer, timer_handler, 0);
       mod_timer(fasync_timer, jiffies + msecs_to_jiffies(10000));
}*/

static int __init gpio_init(void)
{
	int result;
	/* Allocating mytimer for the buffer */
	buffer = kmalloc(256, GFP_KERNEL); 
	if (!buffer)
	{ 
		printk(KERN_ALERT "Insufficient kernel memory\n"); 
		result = -ENOMEM;
		goto fail; 
	} 
	memset(buffer, 0, 256);
	/* Allocating buffers */
//	fasync_timer = (struct timer_list *) kmalloc(sizeof(struct timer_list), GFP_KERNEL);
//	irq_timer = (struct timer_list *) kmalloc(sizeof(struct timer_list), GFP_KERNEL);

	/* Registering device */
	result = register_chrdev(mygpio_major, "my_car", &mygpio_fops);
	if (result < 0)
	{
		printk(KERN_ALERT
			"my_car: cannot obtain major number %d\n", mygpio_major);
		return result;
	}
/*	gpio_direction_input(BT0);
	gpio_direction_input(BT1);
*/	//gpio_direction_output(LED0, 1);
	pxa_gpio_mode(GPIO16_PWM0_MD);
	pxa_set_cken(CKEN0_PWM0, 1);
	pxa_gpio_mode(GPIO17_PWM1_MD);
	pxa_set_cken(CKEN1_PWM1, 1);
	
	//setze PWM register  
	PWM_CTRL0    = PWM_CR_MASK;                                 //Set Scaled Counter Clock
	PWM_PWDUTY0 =  100;                         // Set Duty Cycle
	PWM_PERVAL0 = PWM_PCR_MASK;                            // Set Period 
	//setze PWM register  
	PWM_CTRL1    = PWM_CR_MASK;                                 //Set Scaled Counter Clock
	PWM_PWDUTY1 =  100;                         // Set Duty Cycle
	PWM_PERVAL1 = PWM_PCR_MASK;                            // Set Period 

/*	gpio_direction_output(LED1, 1);
	gpio_direction_output(LED2, 1);
	gpio_direction_output(LED3, 1);
	pxa_gpio_set_value(LED0, 1);
	pxa_gpio_set_value(LED1, 0);
	pxa_gpio_set_value(LED2, 0);
	pxa_gpio_set_value(LED3, 0);*/

/*	pxa_gpio_mode(BT0 | GPIO_IN);
	pxa_gpio_mode(BT1 | GPIO_IN);
	int irq0 = IRQ_GPIO(BT0);
	int irq1 = IRQ_GPIO(BT1);
*/
  /*      if (!fasync_timer)
        { 
                printk(KERN_ALERT "Insufficient kernel memory\n"); 
                result = -ENOMEM;
                goto fail;
        }
        setup_timer(fasync_timer, timer_handler, 0);
        mod_timer(fasync_timer, jiffies + msecs_to_jiffies(10000);
  */
/*	if (request_irq(irq0, &btn0_irq, SA_INTERRUPT | SA_TRIGGER_RISING,
				"mygpio", NULL) != 0 ) {
                printk ( "irq not acquired \n" );
                return -1;
        }
	if (request_irq(irq1, &btn1_irq, SA_INTERRUPT | SA_TRIGGER_RISING,
				"mygpio", NULL) != 0 ) {
                printk ( "irq not acquired \n" );
                return -1;
        }
*/
	return 0;
fail: 
	gpio_exit();
	return 0;
}

module_init(gpio_init);
module_exit(gpio_exit);
MODULE_LICENSE("GPL");
