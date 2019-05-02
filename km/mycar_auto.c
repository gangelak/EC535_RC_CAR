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
/* Structure that declares the usual file */
/* access functions */
struct file_operations mycar_fops = {
	read: mycar_read,
	write: mycar_write,
	open: mycar_open,
	release: mycar_release
};

/* Major number */
static int mycar_major = 62;
static char *buffer;
int ll=0;int lll=0;int rr=0;int rrr=0;
void del_func(unsigned long);
// timer used to gradually slow down/speed up
//static struct timer_list * acceleration_timer;
int times_read = 0;

char seconds[5];

char speed = 'M';

#include <linux/delay.h>
#define LEVEL_LINE 300
int isLeft = 0;
int isRight = 0;
int cache_right = 0;
int cache_left = 0;
int timer_running = 0;
int l=0,r=0;
static struct timer_list * irq_timer;
static struct timer_list * delay_timer;
#define LED0 28
#define LED1 29
#define LED2 30
#define LED3 31
#define IR_R 118
#define IR_L 101
#define BT2 117
#define BT1 101
#define BT0 118
#define PWM0 GPIO16_PWM0_MD
#define PWM1 GPIO17_PWM0_MD
#define PWM_CR_MASK 0x3F               //0x3F is the largest value allowed for the PWM_CTR register
#define PWM_DCR_MASK 0xD7          //0x3FF is the largest value allowed for the PWM_PWDUTY register
#define PWM_PCR_MASK    0x3FF       //0x3FF is the largest value allowed for the PWM_PERVAL register 
#define LEVEL 400
#define HALF 200
int leds[4] = {28,29,30,31};

short duty[3] = {32,128,1024};

static int mycar_open(struct inode *inode, struct file *filp)
{
printk(KERN_INFO "OPEN\n");
	/* Success */
	return 0;
}


static int mycar_release(struct inode *inode, struct file *filp)
{
	/* Success */
	return 0;
}

int active = 0;
static ssize_t mycar_write(struct file *filp, const char *buf,
							size_t count, loff_t *f_pos)
{
	printk(KERN_ALERT "Inside write\n");
	memset(buffer, '\0', 256);
	if (copy_from_user(buffer, buf, count))
	{
		printk(KERN_INFO "Copy from user failed\n");
		return -EFAULT;
	}

	printk(KERN_INFO "write_auto %s %d\n",buffer,active);

        if (buffer[0] == 'N') {
		printk(KERN_INFO "N-\n");
		active = 1;
	}else if (buffer[0] == 'Z'){
		printk(KERN_INFO "Z-\n");
		active = 0;
                PWM_PWDUTY0 = 0;   
                PWM_PWDUTY1 = 0;
	} 

	int l,r;l=0,r=0;

	return count;
}
static ssize_t mycar_read(struct file *filp, char *buf, 
							size_t count, loff_t *f_pos)
{
	return count;
}

static void mycar_exit(void)
{
printk(KERN_INFO "EXIT\n");
	/* Freeing buffer memory */
	// fancy dynamic buffer here to demonstration
	if (buffer) // but not needed as such
	{
		kfree(buffer);
	}
	/* Freeing the major number */
	unregister_chrdev(mycar_major, "autocar");
	PWM_PWDUTY0 = 0;
	PWM_PWDUTY1 = 0;
	free_irq(IRQ_GPIO(IR_L),NULL);
	free_irq(IRQ_GPIO(IR_R),NULL);
	del_timer(irq_timer);
	if(irq_timer)
		kfree(irq_timer);
	
	del_timer(delay_timer);
	if(delay_timer)
		kfree(delay_timer);

}

void btn_func(unsigned long unused){
	del_timer(irq_timer);
	r = pxa_gpio_get_value(IR_R);
	l = pxa_gpio_get_value(IR_L);
	int delay = 1;
	if(active){
	if(!l && !r){
	lll=ll;ll=0;rrr=rr;rr=0;
                pxa_gpio_set_value(leds[0],0);
                pxa_gpio_set_value(leds[1],1);
                pxa_gpio_set_value(leds[2],0);
                pxa_gpio_set_value(leds[3],1);
		PWM_PWDUTY1 = LEVEL;PWM_PWDUTY0 = LEVEL;
	delay = 50;
	} else if(!l){
                pxa_gpio_set_value(leds[0],0);
                pxa_gpio_set_value(leds[1],1);
                pxa_gpio_set_value(leds[2],1);
                pxa_gpio_set_value(leds[3],0);
		PWM_PWDUTY0 = LEVEL;PWM_PWDUTY1 = LEVEL;
		delay = 42+ ll*42 + lll*42;
		lll = ll; ll = 1;
		rrr=rr;rr=0;
	}else if(!r){
                pxa_gpio_set_value(leds[0],1);
                pxa_gpio_set_value(leds[1],0);
                pxa_gpio_set_value(leds[2],0);
                pxa_gpio_set_value(leds[3],1);
		lll=ll;ll=0;
		rrr=rr;rr=1;
		PWM_PWDUTY1 = LEVEL;PWM_PWDUTY0 = LEVEL;
		delay = 42+ rr*42 + rrr*42;
	}else{	
		lll=ll;ll=0;rrr=rr;rr=0;
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
                setup_timer(irq_timer, btn_func, 0);
                mod_timer(irq_timer, jiffies + msecs_to_jiffies(200));
}

static int __init mycar_init(void)
{
	int result;
	printk(KERN_INFO "INIT\n");
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
	result = register_chrdev(mycar_major, "autocar", &mycar_fops);
	if (result < 0)
	{
		printk(KERN_ALERT
			"autocar: cannot obtain major number %d\n", mycar_major);
		return result;
	}
	
	pxa_gpio_mode(GPIO16_PWM0_MD);
	pxa_set_cken(CKEN0_PWM0, 1);
	pxa_gpio_mode(GPIO17_PWM1_MD);
	pxa_set_cken(CKEN1_PWM1, 1);
	
	//setze PWM register  
	gpio_direction_output(LED0, 1);
        gpio_direction_output(LED1, 1);
	gpio_direction_output(LED2, 1);
        gpio_direction_output(LED3, 1);
        pxa_gpio_set_value(LED0, 1);
        pxa_gpio_set_value(LED1, 1);
        pxa_gpio_set_value(LED2, 1);
        pxa_gpio_set_value(LED3, 1);
        gpio_direction_input(IR_L);
        gpio_direction_input(IR_R);
        pxa_gpio_mode(IR_L | GPIO_IN);
        pxa_gpio_mode(IR_R | GPIO_IN);

	PWM_CTRL0    = PWM_CR_MASK;                                 //Set Scaled Counter Clock
	PWM_PWDUTY0 =  0;                         // Set Duty Cycle
	PWM_PERVAL0 = PWM_PCR_MASK;                            // Set Period 
	//setze PWM register  
	PWM_CTRL1    = PWM_CR_MASK;                                 //Set Scaled Counter Clock
	PWM_PWDUTY1 =  0;                         // Set Duty Cycle
	PWM_PERVAL1 = PWM_PCR_MASK;                            // Set Period 

	//indicate to user that module was loaded
	PWM_PWDUTY0 = LEVEL;
	PWM_PWDUTY1 = LEVEL;
                pxa_gpio_set_value(leds[0],0);
                pxa_gpio_set_value(leds[1],1);
                pxa_gpio_set_value(leds[2],1);
                pxa_gpio_set_value(leds[3],0);
	msleep(500);
	PWM_PWDUTY0 = 0;
        PWM_PWDUTY1 = 0;

	irq_timer = (struct timer_list *) kmalloc(sizeof(struct timer_list), GFP_KERNEL);
	delay_timer = (struct timer_list *) kmalloc(sizeof(struct timer_list), GFP_KERNEL);
	if (!irq_timer || !delay_timer)
        {
                printk(KERN_ALERT "Insufficient kernel memory\n");
                result = -ENOMEM;
                goto fail;
        }
                setup_timer(irq_timer, btn_func, 0);
                mod_timer(irq_timer, jiffies + msecs_to_jiffies(500));
	return 0;
fail: 
	mycar_exit();
	return 0;
}

module_init(mycar_init);
module_exit(mycar_exit);
MODULE_LICENSE("GPL");
