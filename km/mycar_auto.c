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
irqreturn_t btnr_irq(int irq, void *dev_id, struct pt_regs *regs);
irqreturn_t btnl_irq(int irq, void *dev_id, struct pt_regs *regs);
void btn_func(unsigned long);
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

//#define MYGPIO 101
//volatile int pwm0 = 0;
//volatile int pwm1 = 0;
//int accel_period = 5;
//int accel_steps = 5;
//volatile int remaining_steps = 0;

short duty[3] = {32,128,1024};

static int mycar_open(struct inode *inode, struct file *filp)
{
printk(KERN_INFO "OPEN\n");
	/* Success */
	return 0;
}
/*
static void timer_handler(unsigned long data) {
	if (remaining_steps > 0){
        PWM_PWDUTY0 -= pwm0;
        PWM_PWDUTY1 -= pwm1;
        del_timer(acceleration_timer);
        setup_timer(acceleration_timer, timer_handler, 0);
        mod_timer(acceleration_timer, jiffies + msecs_to_jiffies(accel_period));
	remaining_steps --;
	} else {
	PWM_PWDUTY0 = 0;
	PWM_PWDUTY1 = 0;
	}
}*/

static int mycar_release(struct inode *inode, struct file *filp)
{
	/* Success */
	return 0;
}
/*
void set_accel_timer(){
	setup_timer(acceleration_timer, timer_handler, 0);
        mod_timer(acceleration_timer, jiffies + msecs_to_jiffies(accel_period));
}*/
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
int m0 = 0;
int m1 = 0;
int dir = 1;
void back(int * ref_motor){
		int motor = *ref_motor;
                if (motor>STEP){
                motor -= STEP;
                dir = 1;
                }
                else if(motor==STEP){//zero
                motor -= STEP;
                dir = 0;
                }else if(motor>-LEVEL && motor<=STEP){
                motor -= STEP;
                dir = 0;
                }
		*ref_motor = motor;
}
void front(int * ref_motor){
		int motor = *ref_motor;
                if (motor<0){
                dir = 0;
                motor += STEP;
                } else if (motor < LEVEL && motor>0){
                motor += STEP;
                dir = 1;
                }else if (motor==0){
                dir = 1;
                motor += STEP;
                }
		*ref_motor = motor;
}
void turn(int *ref_turn_motr, int *ref_stop_motr){
		int turn_motr = *ref_turn_motr;
		int stop_motr = *ref_stop_motr;
                if(stop_motr<0){
                stop_motr += STEP;
                }else if (stop_motr>0){
                stop_motr -= STEP;
                }
                if (turn_motr < LEVEL){
                turn_motr += STEP;
                }else if(turn_motr ==0){
                turn_motr += STEP;
                dir = 1;
                }
		*ref_turn_motr = turn_motr;
		*ref_stop_motr = stop_motr;
}
void half_turn_front(int *ref_turn_motr, int *ref_half_motr){
                int turn_motr = *ref_turn_motr;
                int half_motr = *ref_half_motr;
                if (half_motr < HALF){
                half_motr += STEP;
                }else if(half_motr ==0){
                half_motr += STEP;
                dir = 1;
                }else if(half_motr >HALF){
		half_motr -= STEP;
		}
                if (turn_motr < LEVEL){
                turn_motr += STEP;
                }else if(turn_motr ==0){
                turn_motr += STEP;
                dir = 1;
                }
                *ref_turn_motr = turn_motr;
                *ref_half_motr = half_motr;
}
void half_turn_back(int *ref_turn_motr, int *ref_half_motr){
                int turn_motr = *ref_turn_motr;
                int half_motr = *ref_half_motr;
                if (half_motr > STEP){
		half_motr -= STEP;
		}else if(half_motr ==STEP){
                half_motr -= STEP;
                dir = 0;
                }else if (half_motr > HALF){
                half_motr -= STEP;
                }else if(half_motr < HALF){
                half_motr += STEP;
                }
                if (turn_motr > -LEVEL){
                turn_motr -= STEP;
                }else if(turn_motr ==STEP){
                turn_motr -= STEP;
                dir = 0;
                }
                *ref_turn_motr = turn_motr;
                *ref_half_motr = half_motr;
}

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
#if 0
	printk(KERN_INFO "write %s\n",buffer);

        if (buffer[0] == 'F') {
		printk(KERN_INFO "F-\n");
		front(&m0);
		front(&m1);
	} 
     else if (buffer[0] == 'Q') {
                printk(KERN_INFO "Q-\n");
		half_turn_front(&m1,&m0);
		//PWM_PWDUTY0 = HALF;	
		//PWM_PWDUTY1 = LEVEL;
        } 
        else if (buffer[0] == 'E') {
                printk(KERN_INFO "E-\n");
		//dir = 1;
                //PWM_PWDUTY0 = LEVEL;
		//PWM_PWDUTY1 = HALF; 
		half_turn_front(&m0,&m1);
       }
	else if (buffer[0] == 'B') {
		printk(KERN_INFO "B-\n"); 
		//dir = 0;
		/*if (m0>0){
		m0 -= STEP;
		dir = 1;
		}
		else if(m0==1){//zero
		m0 -= STEP;
		dir = 0;
		}else if(m0>-LEVEL && m0<=0){
                m0 -= STEP;
                dir = 0;
                }*/
		back(&m0);
		back(&m1);
        
	        /*if (m1>0){
                m1 -= STEP;
		dir = 1;
                }
                else if(m1>-LEVEL && m1<0){
                m1 -= STEP;
		dir = 0;
                }else if(m1 ==0){//zero
                m1 -= STEP;
                dir = 0;
                }*/
		//PWM_PWDUTY0 = ABS(m0);
		//PWM_PWDUTY1 = ABS(m1);
        }
        else if (buffer[0] == 'A') {
                printk(KERN_INFO "A-\n");
		//dir = 0;
                //PWM_PWDUTY0 = HALF;
		//PWM_PWDUTY1 = LEVEL;
		half_turn_back(&m1,&m0);
        }
        else if (buffer[0] == 'D') {
                printk(KERN_INFO "D-\n");
		//dir = 0;
                //PWM_PWDUTY0 = LEVEL;
		//PWM_PWDUTY1 = HALF;
		half_turn_back(&m0,&m1);
        }
	else if (buffer[0] == 'L'){
		printk(KERN_INFO "L-\n");
		//dir = 1;
               
		turn(&m1,&m0);
/* 
		if(m0<0){
		m0 += STEP;
		}else if (m0>0){//zero
                m0 -= STEP;
                }
                if (m1 < LEVEL){
                m1 += STEP;
                }else if(m1 ==0){
		m1 += STEP;
		dir = 1;
		}
*/  
      }
	
	else if (buffer[0] == 'R'){
        	printk(KERN_INFO "R-\n");  
		//dir = 1;
		turn(&m0,&m1);
/*                if (m0 < LEVEL){
                m0 += STEP;
                //l_PWDUTY0 = ABS(m0);
                }else if(m0 ==0){
                m0 += STEP;
                dir = 1;
                }
	
                if (m1>0){
                m1 -= STEP; 
	       }else if (m1<0){
                m1 += STEP;
                }
*/  
              //M_PWDUTY1 = m1;
	}

	else if (buffer[0] == 'S'){
		printk(KERN_INFO "S-\n"); 
		//dir = 1;
//PWM_PWDUTY0 = 0;
//PWM_PWDUTY1 = 1;
		if (m0>0){ 
		m0 -= STEP;
		}else if(m0<0){
		m0 += STEP;
		}
		//PWM_PWDUTY0 = ABS(m0);
		if (m1>0){
		m1 -= STEP;
		}else if(m1<0){
		m1 += STEP;
		}
		//PWM_PWDUTY1 = ABS(m1);

	/*	if (PWM_PWDUTY0 >= HALF || PWM_PWDUTY1 >= HALF){
		remaining_steps = accel_steps;
		pwm0 = PWM_PWDUTY0/accel_steps;
		pwm1 = PWM_PWDUTY1/accel_steps;
		set_accel_timer();
		}
	*/
	}
	
	else {
          	printk(KERN_INFO "Incorrect usage\n");
        }
/*
	if (buffer[0]!='S'){
		remaining_steps = 0;
		del_timer(acceleration_timer);
	}
*/	
	if (dir) {
          	pxa_gpio_set_value(leds[0],0);
          	pxa_gpio_set_value(leds[1],1);
 		pxa_gpio_set_value(leds[2],1);
                pxa_gpio_set_value(leds[3],0);
	} else {
                pxa_gpio_set_value(leds[0],1);
                pxa_gpio_set_value(leds[1],0);
		pxa_gpio_set_value(leds[2],0);
                pxa_gpio_set_value(leds[3],1);
	}
	printk(KERN_ALERT "m0: %d\n",m0);
	printk(KERN_ALERT "m1: %d\n",m1);
                PWM_PWDUTY1 = ABS(m1);
                PWM_PWDUTY0 = ABS(m0);
#endif
	int l,r;l=0,r=0;
/*if(1){
	while (l==0 || r==0)//while sensors see white
	{
        l = pxa_gpio_get_value(IR_L);
        r = pxa_gpio_get_value(IR_R);
	printk(KERN_ALERT "L: %d\n",l);
        printk(KERN_ALERT "R: %d\n",r);
	if(l==0)//if left white go left
		PWM_PWDUTY0 = LEVEL;
	if(r==0)
		PWM_PWDUTY1 = LEVEL;
	}
	}
*/	
	return count;
}
static ssize_t mycar_read(struct file *filp, char *buf, 
							size_t count, loff_t *f_pos)
{
	/*char tbuf[256], *tbptr;*/
	/*tbptr = tbuf;*/
	/*memset(tbptr,'\0',256);*/
	/*if (times_read == 0){*/
		/*tbptr += sprintf(tbptr,"%d %5c %8s %5s %6c\n", counter, speed, cnt ? "Count" : "Hold", dir ? "Up" : "Down", brightness[global] );*/
		/*times_read++;*/
	/*}		*/
	/*else{*/
		/*times_read = 0;	*/
	/*}*/
	/*if (copy_to_user(buf, tbuf, sizeof(tbuf)))*/
	/*{*/
		/*printk(KERN_INFO "fault transfering data to user 216\n");*/
		/*return -EFAULT;*/
	/*}*/
	/*count = strlen(tbuf);*/
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
	unregister_chrdev(mycar_major, "mycar");
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
/*        del_timer(acceleration_timer);
        if (acceleration_timer)
                kfree(acceleration_timer);
*/
}
int ll=0;lll=0;
void del_func(unsigned long);
void btn_func(unsigned long unused){
	del_timer(irq_timer);
r = pxa_gpio_get_value(IR_R);
l = pxa_gpio_get_value(IR_L);
int delay = 1;
	if(!l && !r){
	lll=ll;ll=0;
                pxa_gpio_set_value(leds[0],0);
                pxa_gpio_set_value(leds[1],1);
                pxa_gpio_set_value(leds[2],1);
                pxa_gpio_set_value(leds[3],0);
	PWM_PWDUTY1 = LEVEL;PWM_PWDUTY0 = LEVEL;
	delay = 50;
	} else if(!l){
                pxa_gpio_set_value(leds[0],0);
                pxa_gpio_set_value(leds[1],1);
                pxa_gpio_set_value(leds[2],0);
                pxa_gpio_set_value(leds[3],1);
		PWM_PWDUTY0 = LEVEL;PWM_PWDUTY1 = LEVEL;
		delay = 100+ ll*200 + lll*300;
		lll = ll; ll = 1;
	//	msleep(100);
	//	PWM_PWDUTY0 = LEVEL;
	}else if(!r){
                pxa_gpio_set_value(leds[0],1);
                pxa_gpio_set_value(leds[1],0);
                pxa_gpio_set_value(leds[2],1);
                pxa_gpio_set_value(leds[3],0);
		lll=ll;ll=0;
		PWM_PWDUTY1 = LEVEL;PWM_PWDUTY0 = LEVEL;
		delay = 50;
	//	msleep(100);
	//	PWM_PWDUTY0 = LEVEL;
	}else{	
		lll=ll;ll=0;
		PWM_PWDUTY1 = 0;PWM_PWDUTY0 = 0;
	}
	//timer_running = 0;
                setup_timer(delay_timer, del_func, 0);
                mod_timer(delay_timer, jiffies + msecs_to_jiffies(delay));
}

void del_func(unsigned long unused){
del_timer(delay_timer);
		PWM_PWDUTY1 = 0;PWM_PWDUTY0 = 0;
                setup_timer(irq_timer, btn_func, 0);
                mod_timer(irq_timer, jiffies + msecs_to_jiffies(200));
}

irqreturn_t btnr_irq(int irq, void *dev_id, struct pt_regs *regs)
{
 //       int ret,l,r;
        r = pxa_gpio_get_value(IR_R);
	printk(KERN_ALERT "L: %d\n",l);
        printk(KERN_ALERT "R: %d\n",r);
	if(!timer_running){
		PWM_PWDUTY1 = 0;PWM_PWDUTY0 = 0;
                setup_timer(irq_timer, btn_func, 0);
                mod_timer(irq_timer, jiffies + msecs_to_jiffies(100));
                timer_running = 1;
	}
/*	if(r==0){
		if(!isLeft){
		PWM_PWDUTY1 = LEVEL_LINE;
		isRight = 1;
		}else{
		cache_right = 1;
		}
	} else {
		isRight = 0;
		if(!isLeft){
		PWM_PWDUTY1 = 0;
		}else{
		cache_right = 0;
		}
		if(cache_left){
			PWM_PWDUTY0 = LEVEL_LINE;
			cache_left = 0;
		}
	}
*/
	return IRQ_HANDLED;
}
irqreturn_t btnl_irq(int irq, void *dev_id, struct pt_regs *regs)
{
	
 //       int ret,l,r;
        l = pxa_gpio_get_value(IR_L);
	printk(KERN_ALERT "L: %d\n",l);
        printk(KERN_ALERT "R: %d\n",r);
	if(!timer_running){ 
		PWM_PWDUTY1 = 0;PWM_PWDUTY0 = 0;
                setup_timer(irq_timer, btn_func, 0);
                mod_timer(irq_timer, jiffies + msecs_to_jiffies(100));
                timer_running = 1;
	}
/*	if(l==0){//if left white go left
		if (!isRight){
		PWM_PWDUTY0 = LEVEL_LINE;
		isLeft = 1;
		}else{
		cache_left = 1;
		}
	} else {
		isLeft = 0;
		if (!isRight){
		PWM_PWDUTY0 = 0;
		}else{
		cache_left =0;
		}
		if(cache_right){
		PWM_PWDUTY1 = LEVEL_LINE;
		cache_right = 0;
		}
	}
*/
	return IRQ_HANDLED;
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
/*	acceleration_timer = (struct timer_list *) kmalloc(sizeof(struct timer_list), GFP_KERNEL);
        if (!acceleration_timer)
        {
                printk(KERN_ALERT "Timer creation failed\n");
                result = -ENOMEM;
                goto fail;
        }
*/
	/* Registering device */
	result = register_chrdev(mycar_major, "mycar", &mycar_fops);
	if (result < 0)
	{
		printk(KERN_ALERT
			"my_car: cannot obtain major number %d\n", mycar_major);
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

/*      int irql = IRQ_GPIO(IR_L);
        int irqr = IRQ_GPIO(IR_R);
        
        if (request_irq(irql, &btnl_irq, SA_INTERRUPT | SA_TRIGGER_RISING,// | SA_TRIGGER_FALLING,
                                "mygpio", NULL) != 0 ) {
                printk ( "irq not acquired \n" );
                return -1;
        }
        if (request_irq(irqr, &btnr_irq, SA_INTERRUPT | SA_TRIGGER_RISING,// | SA_TRIGGER_FALLING,
                                "mygpio", NULL) != 0 ) {
                printk ( "irq not acquired \n" );
                return -1;
        }
*/

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
