/*uttersense.c  -------------------------------------------------------
 * Kernel module driver for the board DS3231
 * The DS3231 board has
 * RTC chip
 * EEPROM (24C32) chip
 * Both are accessed by I2C communitation
 * 
 * NOTES ON DRIVER IMPLEMENTATION
 * 
 * i) Set up a driver structure (that will drive the target device)
 *    We need to define:
 *     - driver name (must match the name given in device board info
 *       Also, use same name of the LKM module we are compiling
 *     - probe callback function
 *     - remove callback function
 *     - id table for client devices  
 *
 * NB: Designating driver name:
 *     In order to avoid possible conflicts of operation, choose a name that does not
 *     resemble possible devices already registed in i2c sub-system 
 *     If it call to a pre-registered div##evice, the probe callback is not activated,
 *     and the near-match device is also loaded as a driver!
 *     For the moment, we designate the driver name by uttersense.
 *  
 * ii) Set up ID Table defined in i)
 *     Define the chip type to support with an id
 *     For DS3231 - RTC and EEPROM chips
 * 
 * iii) Define structure i2c_board_info
 *       This should give the address and name of the chipboard we need to support
 *       
 * 
 *  iv) Set up a probe callback function
 *      This will handle a callback from i2c sub system if driver nad device names match 
 *       a) We need to write own functionality to ensure we can communicate with the devicee
 *          e.g. send a test message
 * 
 * v) Initialise the driver in LKM  _init
 *    i) Get the i2c adapter (we know which bus our clients reside on)
 *    ii) Get the i2c_client reference (neede for read/wrtie to clients
 *   iii) Rergister the driver 
 * 
 * 
  --------------------------------------------------------------------*/ 
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/module.h>
#include <asm/io.h>
#include <linux/device.h>
#include <linux/err.h>
#include <linux/mm.h>
#include <linux/ioctl.h>               //Communication facility between kernel and user space
#include <linux/uaccess.h>          // Required for the copy to user function
#include <linux/delay.h>
#include <linux/i2c.h>
#include <linux/signal.h>
//#include <linux/sched/signal.h>      //Use instead of <linux/signal.h> - header file location chenaged in more recent kernels
#include <linux/fs.h>



#define  DEVICE_NAME "ds3231"    ///< The device will appear at /dev/rpichar using this value
#define  CLASS_NAME  "cl_ds3231"        ///< The device class -- this is a character device driver

//RTC register addresses:
#define RTC_YEAR 0x06
#define RTC_MONTH 0x05
#define RTC_DATE 0x04
#define RTC_DAY 0x03
#define RTC_HOUR 0x02
#define RTC_MINUTE 0x01
#define RTC_SECOND 0x00

//Temperature Registers
#define RTC_TEMP_UPPER 0x11    //Upper byte
#define RTC_TEMP_LOWER 0x12    //Lower byte (fractional value)


char filename[20];
int fd;


//I2C defines:
#define I2C_BUS_NUM 1  //This is the number of the i2c bus that the devices reside on
//RTC Client address:
#define I2C_RTC_ADDR 0x68

static struct i2c_client *utter_client;


//Set up I2C Structures and prototypes:
//Function prototypes required forI2C Driver
static int uttersense_probe(struct i2c_client *client, const struct i2c_device_id *id); 


//Sep up board infomration i2c_board_info
//NB: The i2c subsystem will create an i2c_client structure out of this to represent a particular
//    i2c client
static struct i2c_board_info  uttersense_board_info = 
{  
     I2C_BOARD_INFO("uttersense", I2C_RTC_ADDR)  //RTC chip
   
};


//Id Table structure:
static struct i2c_device_id uttersense_id[] = 
{
  {"uttersense",
    I2C_RTC_ADDR },
  {} 
   
};   
MODULE_DEVICE_TABLE(i2c,uttersense_id);


//I2C Driver: (the driver related to the required client devices)
static struct i2c_driver  uttersense_driver = {  //This is for the DS3231 board with clients RTC and 24C32 eeprom
         .driver = {
          .name = "uttersense",    //Must match the name of the kernel module
          .owner = THIS_MODULE,  
          },
          .probe = uttersense_probe,
          //.remove = uttersense_remove,
          .id_table = uttersense_id,  
       
};


static int    majorNumber;                  ///< Stores the device number -- determined automatically
static struct class*  i2cClass  = NULL; ///< The device-driver class struct pointer
static struct device* i2cDevice = NULL; ///< The device-driver device struct pointer

static char   message[256] = {0};           ///< Memory for the string that is passed from userspace
static short  size_of_message;              ///< Used to remember the size of the string stored

static char rec_buf[256] = {0};

//Decode function for RTC register values:
void decode(char* elem, char byte);
//Cleanup functions for buffers:
void cleanUp(void);

// The prototype functions for the character driver -- must come before the struct definition
static int     dev_open(struct inode *, struct file *);
static int     dev_release(struct inode *, struct file *);
static ssize_t dev_read(struct file *, char *, size_t, loff_t *);
static ssize_t dev_write(struct file *, const char *, size_t, loff_t *);



static struct file_operations fops =
{
   .open = dev_open,
   .read = dev_read,
   .write = dev_write,
   .release = dev_release,
};



//I2C Callbacks
static int uttersense_probe(struct i2c_client *client, const struct i2c_device_id *id)
{
      //If we have a client match, the probe function returns with a valid i2c_client structure
      //The we can do our required configuration
      // and do cleint registration?
      
      printk(KERN_INFO "Have called back to probe function");
      
      //Check the device address is OK:
      if( client->addr != I2C_RTC_ADDR)
      {
         printk(KERN_INFO "Problem with client address");
         return -1;
      }   
      
      utter_client = client;
      
      //Register char device to facilitate user program interaction with i2c:
      // Try to dynamically allocate a major number for the device -- more difficult but worth it
      majorNumber = register_chrdev(0, DEVICE_NAME, &fops);
      if (majorNumber<0){
         printk(KERN_ALERT "%s  failed to register a major number\n",DEVICE_NAME);
         return majorNumber;
      }
      printk(KERN_INFO "%s registered correctly with major number %d\n", DEVICE_NAME,majorNumber);

      
      // Register the device class
      i2cClass = class_create(THIS_MODULE, CLASS_NAME);
      if (IS_ERR(i2cClass)){                // Check for error and clean up if there is
         unregister_chrdev(majorNumber, DEVICE_NAME);
         printk(KERN_ALERT "Failed to register device class %s\n", CLASS_NAME);
         return PTR_ERR(i2cClass);          // Correct way to return an error on a pointer
      }
      printk(KERN_INFO "%s device class registered correctly\n",CLASS_NAME);

      
      // Register the device driver
      i2cDevice = device_create(i2cClass, NULL, MKDEV(majorNumber, 0), NULL, DEVICE_NAME);
      if (IS_ERR(i2cDevice)){               // Clean up if there is an error
         class_destroy(i2cClass);           // Repeated code but the alternative is goto statements
         unregister_chrdev(majorNumber, DEVICE_NAME);
         printk(KERN_ALERT "Failed to create the device %s\n",DEVICE_NAME);
         return PTR_ERR(i2cDevice);
      }
      printk(KERN_INFO "%s device created correctly\n",DEVICE_NAME); // Made it! device was initialized
      
          
      
      
      return 0;
}    





static int dev_open(struct inode *inodep, struct file *filep){
   
   printk(KERN_INFO "Device %s has been opened \n",DEVICE_NAME);
      
    
   return 0;
}



static int dev_release(struct inode *inodep, struct file *filep){
   
   printk(KERN_INFO "Device %s has been closed \n",DEVICE_NAME);
   return 0; 
   
}
 



static ssize_t dev_write(struct file *filep, const char *buffer, size_t len, loff_t *offset){
   
   
   int32_t res = 0;
   int error_count = 0;

   //------------------------------------
   /*
   sprintf(message, "%s(%zu letters)", buffer, len);   // appending received string with its length
   size_of_message = strlen(message);                 // store the length of the stored message
   printk(KERN_INFO "EBBChar: Received %zu characters from the user\n", len);
   */
   //NB: Copy user space data to kernel space
   error_count = copy_from_user(rec_buf,buffer,sizeof(buffer));
   
   if(rec_buf[0] == 'Y')
   {
      res = i2c_smbus_write_byte_data(utter_client,RTC_YEAR,buffer[1]);
   }   
   else if(rec_buf[0] == 'M')
   {
      res = i2c_smbus_write_byte_data(utter_client,RTC_MONTH,buffer[1]);
   }
   else if(rec_buf[0] == 'D')
   {
      res = i2c_smbus_write_byte_data(utter_client,RTC_DATE,buffer[1]);
   }
   else if(rec_buf[0] == 'd')
   {
      res = i2c_smbus_write_byte_data(utter_client,RTC_DAY,buffer[1]);
   }
   else if(rec_buf[0] == 'h')
   {
      res = i2c_smbus_write_byte_data(utter_client,RTC_HOUR,buffer[1]);
   }
   else if(rec_buf[0] == 'm')
   {
      res = i2c_smbus_write_byte_data(utter_client,RTC_MINUTE,buffer[1]);
   }
   else if(rec_buf[0] == 's')
   {
      res = i2c_smbus_write_byte_data(utter_client,RTC_SECOND,buffer[1]);
   }
   
   if(res < 0)
   {
      printk(KERN_INFO "Failed to write to i2c client...\n");
            
   }	
   //printk(KERN_INFO "Written to i2c client OK!\n");   
   
   return len;
      
    
}




static ssize_t dev_read(struct file *filep, char *buffer, size_t len, loff_t *offset){
   
   //Test read:
   int32_t res = 0;
   int error_count = 0;
   
      
   //printk(KERN_INFO "JUST BEFORE READING BUFFER buffer...\n");
   //NB: Need this function to copy from user space to kernel space!!
   //    If not, we are likely to crash (as in Android implementation)
   error_count = copy_from_user(rec_buf,buffer,sizeof(buffer));
   printk(KERN_INFO "Received:  %s\n",rec_buf);
	

   if( strcmp(rec_buf,"yr") == 0 )
   {
      res = i2c_smbus_read_byte_data(utter_client,RTC_YEAR);
      decode("yr", res);
      
   }
   else if( strcmp(rec_buf,"mo") == 0 )
   {
      res = i2c_smbus_read_byte_data(utter_client,RTC_MONTH);
      decode("mo", res);
   }
   else if( strcmp(rec_buf,"Da") == 0 )
   {
      res = i2c_smbus_read_byte_data(utter_client,RTC_DATE);
      decode("Da", res);
      
   }
   else if( strcmp(rec_buf,"da") == 0 )
   {
      res = i2c_smbus_read_byte_data(utter_client,RTC_DAY);
      decode("da", res);
      
   }
   else if( strcmp(rec_buf,"ho") == 0 )
   {
      res = i2c_smbus_read_byte_data(utter_client,RTC_HOUR);
      decode("ho", res);
      
   }
   else if( strcmp(rec_buf,"mi") == 0 )
   {
      res = i2c_smbus_read_byte_data(utter_client,RTC_MINUTE);
      decode("mi", res);
      
   }
   else if( strcmp(rec_buf,"se") == 0 )
   {
      res = i2c_smbus_read_byte_data(utter_client,RTC_SECOND);
      decode("se", res);
      
   }
   else if( strcmp(rec_buf,"tu") == 0 )
   {
      res = i2c_smbus_read_byte_data(utter_client,RTC_TEMP_UPPER);
      decode("tu", res);
      
   }
   else if( strcmp(rec_buf,"tl") == 0 )
   {
      res = i2c_smbus_read_byte_data(utter_client,RTC_TEMP_LOWER);
      decode("tl", res);
      
   }     
   
      
   printk(KERN_INFO "Message: %s\n", message);   
   size_of_message = strlen(message);
   
   //NB: Need this function to copy from kernel space to user space!!
   error_count = copy_to_user(buffer, message, size_of_message);
     
   if (error_count==0){            // if true then have success
      printk(KERN_INFO "rpi_i2c: Sent %d characters to the user\n", size_of_message);
      return (size_of_message=0);  // clear the position to the start and return 0
   }
   else {
      printk(KERN_INFO "rpi_i2c: Failed to send %d characters to the user\n", error_count);
      return -EFAULT;              // Failed -- return a bad address message (i.e. -14)
   } 
   
   
}

void cleanUp(void)
{
   //Clean up the receive/message arrays:
   strcpy(rec_buf,"");
   strcpy(message,"");
   
   
}//cleanUp   


void decode(char* elem, char byte)
{
   
   char digit1, digit2;
	char mask = 0xf;
	   
   if( strcmp(elem,"yr") == 0 )
   {
      //Get first digit:
	   digit1 = byte >> 4;
	   //Get second digit:
	   digit2 = byte & mask;
	
	   sprintf(message,"%d%d",digit1,digit2);
      return;
	
   }
   else if( strcmp(elem,"mo") == 0 )
   {
      digit1 = byte >> 4;
      //Get second digit:
      digit2 = byte & mask;
      switch (byte)
      {
         case 0x1 :
            sprintf(message,"%s","Jan");	
         break;
         case 0x2 :
            sprintf(message,"%s","Feb");	
         break;
         case 0x3 :
            sprintf(message,"%s","Mar");	
         break;
         case 0x4 :
            sprintf(message,"%s","Apr");	
         break;
         case 0x5 :
            sprintf(message,"%s","May");	
         break;
         case 0x6 :
            sprintf(message,"%s","Jun");	
         break;
         case 0x7 :
            sprintf(message,"%s","Jul");	
         break;
         case 0x8 :
            sprintf(message,"%s","Aug");	
         break;
         case 0x9 :
            sprintf(message,"%s","Sep");	
         break;
         case 0x0a :
            sprintf(message,"%s","Oct");	
         break;
         case 0x0b :
            sprintf(message,"%s","Nov");	
         break;
         case 0xc :
            sprintf(message,"%s","Dec");	
         break;
      }
      return;
	
   }
   else if( strcmp(elem,"Da") == 0 )
   {
       //Get first digit:
	   digit1 = byte >> 4;
	   //Get second digit:
	   digit2 = byte & mask;
	   sprintf(message,"%d%d",digit1, digit2);
      return;
   }
   else if( strcmp(elem,"da") == 0 )
   {
      switch (byte)
      {
         case 1 :
            sprintf(message,"%s","Sun");	
         break;
         case 2 :
            sprintf(message,"%s","Mon");	
         break;
         case 3 :
            sprintf(message,"%s","Tue");	
         break;
         case 4 :
            sprintf(message,"%s","Wed");	
         break;
         case 5 :
            sprintf(message,"%s","Thu");	
         break;
         case 6 :
            sprintf(message,"%s","Fri");	
         break;
         case 7 :
            sprintf(message,"%s","Sat");	
         break;
      }
      return;
   }
   else if( strcmp(elem,"ho") == 0 )
   {
       //Get first digit:
	   digit1 = byte >> 4;
	   //Get second digit:
	   digit2 = byte & mask;
	  //printf(" -- %d%d: ",digit1,digit2);
	   sprintf(message,"%d%d",digit1,digit2);
	   return;
   } 
   else if( strcmp(elem,"mi") == 0 )
   {
       //Get first digit:
	   digit1 = byte >> 4;
	   //Get second digit:
	   digit2 = byte & mask;
	   sprintf(message,"%d%d",(int8_t) digit1, (int8_t) digit2);
      
	   return;
   } 
   else if( strcmp(elem,"se") == 0 )
   {
         //Get first digit:
        digit1 = byte >> 4;
        //Get second digit:
        digit2 = byte & mask;
        sprintf(message,"%d%d",digit1,digit2);     
        return;
   }
   else if( strcmp(elem,"tu") == 0 )
   {
	 //Temperature - upper byte (integer)
        sprintf(message,"%d",byte);     
        return;
   }
   else if( strcmp(elem,"tl") == 0 )
   {
	//Temperature - lower nibble byte (decimal part)
        byte = byte >> 6;
	if(byte == 0x00)
	{
	    digit1 = 0x00;
	    digit2 = 0x00;
	}
	else if(byte == 0x01)
	{
	    digit1 = 0x02;
	    digit2 = 0x05;
	}
	else if(byte == 0x02)
	{
	    digit1 = 0x05;
	    digit2 = 0x00;
	}
	else if(byte == 0x03)
	{
	    digit1 = 0x07;
	    digit2 = 0x05;
	}   
	
	sprintf(message,"%d%d",digit1,digit2);    
	return;
   } 
        
}   


static int __init I2CModule_init(void)
{
   int res;
   
     
    struct i2c_adapter *adapter = i2c_get_adapter(I2C_BUS_NUM);
    if(!adapter)
    {
      printk(KERN_INFO "Failed to get i2c adapter\n"); 
      res = -ENODEV;
      return res;
       
    }   
     
    utter_client = i2c_new_device(adapter, &uttersense_board_info); 
    if(!utter_client)
    {
       printk(KERN_INFO "Failed to register i2c device\n"); 
      res = -ENODEV;
      i2c_put_adapter(adapter);
      return res;
    }   
    
    res = i2c_add_driver(&uttersense_driver);
    if(res < 0)
    {
         printk(KERN_INFO "Failed to add driver\n");
         i2c_unregister_device(utter_client);
         return res;
         
    }    
    printk(KERN_INFO "Initialised LKM OK!\n");   
   return res;
}


static void __exit I2CModule_exit(void)
{
        
   
   //Cleanup I2C components:
   i2c_del_driver(&uttersense_driver);
   i2c_unregister_device(utter_client);      
   printk(KERN_INFO "Unregistered I2c driver and device\n");      
   printk(KERN_INFO "%s: Goodbye from the LKM!\n",DEVICE_NAME);
    
         
   //Unregister the char device driver
   device_destroy(i2cClass, MKDEV(majorNumber, 0));     // remove the device
   class_unregister(i2cClass);                          // unregister the device class
   class_destroy(i2cClass);                             // remove the device class
   unregister_chrdev(majorNumber, DEVICE_NAME);             // unregister the major number
   
       
       
}

module_init(I2CModule_init);
module_exit(I2CModule_exit);


MODULE_LICENSE("GPL");
MODULE_AUTHOR("Michael McGetrick");
MODULE_DESCRIPTION("I2C test driver for the Raqspberry Pi");
MODULE_VERSION("0.1");

