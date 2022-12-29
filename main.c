/*
 ======================================================================================
 Name        : main.c
 Author      : Michael McGetrick
 Version     :
 Copyright   : Your copyright notice
 Description : Program application to sample at a given frequency an incoming signal
              from theMCP3008 ADC, and display the signal graphically.
               
              NB: This program must be run as root to access GPIO and SPI functionality 
                  (Use sudo ./bcm_spi) 
                   
             RPI connections(SPI0):
             Phys Pin	GPIO	Function
             19			10		MOSI
             21			9		MISO
             23			11		CLK
             24         8		CE   
                   
 
 Guidelines for Sampling:
 ------------------------
 i) Range 1-100Hz: Use bcm delay function
 ii) Range 100-1000Hz: Use fast looping, SPI CLK FREQ 0.68MHz 
 iii) Range 1000Hz+ :  Usze fats looping, SPI CLK FREQ 1.37MHz                   
                   
 	 	 	   
 =====================================================================================
*/
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
//#include <unistd.h>
#include <sys/time.h>
#include <signal.h>
#include <errno.h>

#include "graph.h"
#include "mcp3008.h"
#include "ds3231.h"

#include <sys/mman.h>  //temp
#include <sys/types.h>



//DEFINES ---------------------------------------
//#define SAMPLING_RATE     4000.0f//1000.0f  
#define DATA_ARRAY_FLAG	0 //1: create data array of samples for metric test; 0: single sample
#define NUM_ADC_CHANS 		3
#define LINUX				0
#define RTC					1
#define TEST_TYPE			RTC


	

//ATTRIBUTES: -----------------------------------------
int EXIT_PROGRAM = 	0;
char *PROG_MODE  = 	"GRAPHICS";		//TEST: GRAPHICS


//MCP3008 Parameters
int chanIndex = 0; // Index of ADC channel
int readMode = 1 ; //Single:1; Differential: 0
int chanVal[NUM_ADC_CHANS];
 
uint64_t sample_delay;
float data[2000];

struct timespec req,rem;

//File attributes
FILE *fp;
//char *filename = "rtc_metrics_clk_0_68.csv";
//char *filename = "rtc_metrics_clk_1_37.csv";
//char *filename = "linux_metrics_clk_1_37_fastloop.csv";
//char *filename = "lkm_linux_timer_integrity_corr.csv";
char *filename = "spi_data.txt";
bool saveToFile = false;



//FUNCTION DECLARATIONS -------------------------
void ctrl_c_handler(int sig);
void sys_delay();
void doTest();
//void sample_BufferMode();
void doGraph();
void sample_BufferMode();
void init_graph();

// ---BCM2835 functions and attributes for accessing
// --- System Timer registers: -----------------------------------------
int bcm2835_init(void);   
int bcm2835_close(void);
static void *mapmem(const char *msg, size_t size, int fd, off_t off);
static void unmapmem(void **pmem, size_t size);
uint64_t bcm2835_st_read();
void bcm2835_st_delay(uint64_t offset_micros, uint64_t micros);
uint32_t bcm2835_peri_read(volatile uint32_t* paddr);
void bcm2835_delayMicroseconds(uint64_t micros);


static uint8_t debug = 0;
volatile uint32_t *bcm2835_st	       = (uint32_t *)MAP_FAILED;
uint32_t *bcm2835_peripherals = (uint32_t *)MAP_FAILED;

/*! On all recent OSs, the base of the peripherals is read from a /proc file */
#define BMC2835_RPI2_DT_FILENAME "/proc/device-tree/soc/ranges"

#define BCM2835_PERI_BASE               0x20000000
/*! Size of the peripherals block on RPi 1 */
#define BCM2835_PERI_SIZE               0x01000000
/*! Alternate base address for RPI  2 / 3 */
#define BCM2835_RPI2_PERI_BASE          0x3F000000
/*! Alternate base address for RPI  4 */


//Base Address of the System Timer registers
#define BCM2835_ST_BASE					0x3000
#define BCM2835_ST_CLO 			0x0004 /*!< System Timer Counter Lower 32 bits */
#define BCM2835_ST_CHI 			0x0008 /*!< System Timer Counter Upper 32 bits */


/*! Virtual memory address of the mapped peripherals block */
extern uint32_t *bcm2835_peripherals;
size_t bcm2835_peripherals_size;
off_t bcm2835_peripherals_base;




//---End BM2835 functions-----------------------------------------------

//-----------------------------------------------
int main(void) {

   //Register handler for capturing CTL-C signal:
	signal(SIGINT,ctrl_c_handler);
            
	
	    
	//Initialise SPI for MCP3008:
	printf("Initialising SPI for MCP3008 peripheral.....\n");
	
	
	int res = MCP3008_Init(SAMPLE_RATE);
	if(res < 0)
	{
		printf("Problem initialising device - exiting program!\n");
		exit(0);	
	}	
	
	//set parameters (if not default)
	//setParams(readMode,chanIndex);	
	printf("Sample rate: %f\n",SAMPLE_RATE);
	
					 
	sample_delay = getSamplingRate();
	printf("Delay in micro-secs: %lld\n",sample_delay);
		
	
	if(sample_delay < 1000000)
	{
		req.tv_sec = 0;
	   req.tv_nsec = sample_delay*1000;
	}
	else
	{
		req.tv_sec = sample_delay/1000000;
	   req.tv_nsec = 0;
		
	}	
		
	 
	if(strcmp(PROG_MODE,"TEST") == 0)
	{
		
		doTest();
		
		
	}
	else if(strcmp(PROG_MODE,"GRAPHICS") == 0)
	{
		
		 sample_BufferMode();
 	
	}	 

	
	
	printf("Exiting program!\n");
	exit(0);      
	


}//main

void sys_delay()
{
	//Notes:
	// If the interval specified in struct timespec req is not an exact
	// multiple of the underlying clock, then the interval will be rounded
	// up to the next multiple.
	//TBC for more accuracy with fast sampling
	
	
	//Delay execution of program for desired time:
	int res = nanosleep(&req,&rem);
	if(res < 0)
	{
		printf("nanosleep() ERROR!\n");
		printf("Error number %d: %s\n",errno,strerror(errno));
		if(errno == EINVAL)
		{
			printf("Nanosecond parameter out of bounds (< 0 or > 999999999)\n");
		}
		if(errno == EINTR)
		{
			printf("Sleep interrupted by a signal handler\n");
			//USe this to schedule the remainin time
			//If we have defined &rem, this will contain the remaining time
			//needed to complete request by calling nanosleep(&rem,NULL);
		}	
		
	}	
	
	
}//sys_delay	


void sample_BufferMode()  //Send data for graphing when data buffer 
								  //filled - use when high sampling rates
								  //required. Data captured without intermediate
								  //plotting which reduces the effective sample
								  //rate 
{
	
	//Allocate memory for display buffer:
	if((PLOT_BUFFER = malloc(DATA_LEN*sizeof(float))) == NULL)
	{
		printf("Problem allocationg mermory for display data\n");
		printf("Exiting program!\n");
	   exit(1);
	}
	
	
	
	if((PLOT_COPY = malloc(DATA_LEN*sizeof(float))) == NULL)
	{
		printf("Problem allocationg mermory for display data\n");
		printf("Exiting program!\n");
	   exit(1);
	}	
	
	
	//Initialise data buffer:
	for(int i=0;i<DATA_LEN;i++)
	{
		PLOT_BUFFER[i] = 0.0;
		PLOT_COPY[i] = 0.0;
		
	}
	
	//Intialise ref count:
	//Get the number of buffers of length DATA_LEN
	//that give one second of time
	//Update trace each second
	int count = 0;
	//refCnt = (int) round( SAMPLE_RATE/DATA_LEN);
	
	
	//CONTROL PARAMETERS FOR UPDATING THE DISPLAY:
	float weight = 0.05;
	refCnt = (int) round( SAMPLE_RATE*weight);
	if(refCnt < DATA_LEN)
	{
		//Restrict refCnt to minium value DATA_LEN
		//This is the minimum samples to be processed from  data buffer 
		refCnt = DATA_LEN;	
	}	 
	//Delay of refCount == SAMPLE_RATE will provide updates at 1s
	//We can vary this by weight value:
	//e.g. weight = 0.5 will provide update at 0.5 sec	

   printf("DATA_LEN: %d\n",(int)DATA_LEN);
	printf("refCnt: %d\n",refCnt);
			
	
	//Initialise graph:
	init_graph();
	
	//Open file for data:
	 if(saveToFile)
	 {
		 if( (fp=fopen(filename,"wt")) == NULL)
		 {
			printf("Error opening file -aborting program!\n");
			exit(0);	 
		 }
    }
 
	
	
	if(SAMPLE_MODE == 0) //Delay mode
	{
		while(EXIT_PROGRAM == 0)
		{
			//Get sample
			for(int i=0; i< DATA_LEN;i++)
			{
				//Delay for set time: 					
				//sys_delay();
				bcm2835_delayMicroseconds(sample_delay);
				
				PLOT_BUFFER[i] = readADC();
				
			}	
			
			//Graph when buffer is full
			count += DATA_LEN;
		   if(count >= refCnt)
			{		
				
				doGraph();
				count = 0;
			}	
			
		}
	}////SAMPLE_mODE ==0	
			
	if(SAMPLE_MODE == 1) //Fast-loop mode
	{
		while(EXIT_PROGRAM == 0)
		{
			//Get sample
			for(int i=0; i< DATA_LEN;i++)
			{
				
				PLOT_BUFFER[i] = readADC();
			}	
			
			//Graph when buffer is full
			count += DATA_LEN;
		   if(count >= refCnt)
			{		
				doGraph();
				count = 0;
			}
			
		}
	}//SAMPLE_MODE ==1	
	 	
	
				
	
}//sample_BufferMode	



void init_graph()
{
	
	//Initialise Graphics			
	Graphic_Init(X_WIN_SIZE,Y_WIN_SIZE,PLOT_MODE,LOG_MODE, X_LABEL, Y_LABEL, GRAPH_TITLE,
					Y_MIN,Y_MAX,X_MIN, X_MAX,
					X_INT,Y_INT,X_GRAD,Y_GRAD,X_LABEL_PREC,Y_LABEL_PREC,
					SHOW_GRID, G_INT,
					X_SHOW,Y_SHOW,ORI_LINE,ORI_LINE_COLOR,BITMAP,
					BCK_COLOR, AUDIOFILE, INI_AUDIOFILE,
					NUM_CHANS, AUDIO_SAMPLE_RATE, BITS_PER_SAMPLE);
			
	//Show the X grauation lines (currently swithed off when switching off XAXis number scaling
	
					
					
	if (DRAW_VER_MARKER)
		{
			drawVerticalMarker (V_MARKER_POS,V_MARKER_COLOR,MARKER_WIDTH);	
		}				
		
	 
	//Setup Legend:
	if(legend_flg)
	{
		l_caps.data1_caption = LEGEND_CAPTION1;
		l_caps.data2_caption = LEGEND_CAPTION2;
		l_caps.data3_caption = LEGEND_CAPTION3;
		l_caps.data4_caption = LEGEND_CAPTION4;
		
		l_data.data1_unit = "V";
		l_data.data2_unit = "V";
		l_data.data3_unit = "s";
		l_data.data4_unit = "Hz";
		
		 int color = WHITE; //Font color for captions
		 legendCaptions(l_caps,color,LEGEND_NUM_BOXES,LEGEND_LEFT,LEGEND_TOP);
	}
	
	
	
}//init_graph	


void doGraph()
{
	
	//Refresh for next plot (put data points back tograph background color):
	if(plotStart == true)
	{
		//Do not Refreshcanvas before first trace has been done
		plotStart = false;
				
	}
	else
	{
		refreshCanvas();
		//Refresh legend area:
		if(legend_flg)
		{
			l_data.data1 = vavgCopy;
			l_data.data2 = vampCopy;
			l_data.data3 = time_divCopy;
			l_data.data4 = freqCopy;
			legend(l_data,true,LEGEND_NUM_BOXES,PLOT1_COLOR,LEGEND_LEFT,LEGEND_TOP); //This should erase previous data write
			//legend_flg = false; //temp
		}
		
		
	 }
	
	//Plot current trace:	
	for(int i=0;i<DATA_LEN;i++)
	{
		
		PLOT_BUFFER[i] += DC_OFFSET;
		
		if(PLOT_BUFFER[i] > Y_MAX)
		{
			PLOT_BUFFER[i] = Y_MAX;
			mSuppress = true;
			
		}	
		if(PLOT_BUFFER[i] < Y_MIN)
		{
			PLOT_BUFFER[i] = Y_MIN;
			mSuppress = true;
		}
		
			  
	}//i	
	
	if(legend_flg)
	{
		 l_data.data1 = vavg; //Mean sigFreq;
		 l_data.data2 = vamp; //Amplitude;
		 l_data.data3 = time_div; //Time for one display division
		 l_data.data4 = freq;  //Peak to peak voltage
		legend(l_data,false,LEGEND_NUM_BOXES,PLOT1_COLOR,LEGEND_LEFT,LEGEND_TOP); //Redraw legend
		//legend_flg = false; //temp
	}
	
	
	plotTrace(PLOT1_COLOR);
	graduations(ORI_LINE_COLOR);  //Refresh grduations
			
	copyDataBuf();  //Copy current graph trace
	copyLegendData();
	
	//legend_flg = false;  //temp
	
}//doGraph	



void doTest()   //Measure time with Real Time Clock (ds3231)
{
	
	
	//Initialise code to get access to System Timer Registers
	bcm2835_init(); 
	
	
	struct timeval tv_start,tv_end;
	uint32_t ts_start, ts_end, time_diff;
	uint64_t MAX_CNT;
	
	uint32_t TIME_INT = 60*2; //Time interval for sampling (secs) 
		
	
	int numSamples = 24;
	float sample_rates[] = {1.0f,10.0f,50.0f,100.0f,200.0f,250.0f,
		                     500.0f,750.0f,1000.0f,1250.0f,1500.0f,2000.0f, 
		                     4000.0f,5000.0f,6000.0f,7000.0f,8000.0f,9000.0f,
		                     10000.0f,11000.0f,12000.0f,13000.0f,14000.0f,15000.0f};
		                     
   bool save_raw_data = false;  //true: save raw data as opposed to metric test info
	
	//Set up required data variable  ---------------------
#if DATA_ARRAY_FLAG == 0
	float data;  //Just retain current sample 
#else
	float *data;   //Create data sample array 
#endif	 
	
	 //Sample and save data to file for subsequent analysis:
	if(TEST_TYPE == RTC)
	{
		printf("\n\nSAMPLING METRICS TEST WITH RTC\n");
	}
	else
	{
		printf("\n\nSAMPLING METRICS TEST WITH LINUX TIME METHODS\n");
	}	
	printf("----------------------------------\n");
	
	
	//Close current MCP3008 session (opened in main program)
	MCP3008_Close();
	
#if TEST_TYPE == RTC	
	//Initialise RTC driver:
   init_dev();
#endif   
      
		 
	 //Open file for data:
	 if(saveToFile)
	 {
		 if( (fp=fopen(filename,"wt")) == NULL)
		 {
			printf("Error opening file -aborting program!\n");
			exit(0);	 
		 }
    }
 
	 
	 if(SAMPLE_MODE == 1)     //Fast looping
	 {
		 numSamples = 1;
	 }	  	 
	 for(int j=0;j<numSamples;j++)
	 {
		 
		 if(SAMPLE_MODE == 0)
		 {
		 	printf("Metric test for sampling at %.2f Hz ... \n",sample_rates[j]);
		 }
		 else
		 {
			printf("Metric test for fast looping ...\n");
		 }	 	
		 MCP3008_Init(sample_rates[j]);
		 sample_delay = getSamplingRate();
		 
	 
		 //printf("Delay in micro-secs: %lld...\n",sample_delay);
		 if(SAMPLE_MODE == 0)
		 {	
	    	MAX_CNT = sample_rates[j]*TIME_INT;
	    }
	    else
	    {
			MAX_CNT = 1000000; //1000000; //Fast loop mode
			
		 }	 
#if DATA_ARRAY_FLAG == 1
		 
		 if((data = malloc(MAX_CNT*sizeof(float))) == NULL)
		 {
			printf("Problem allocating memory for metric test\n");
			printf("Exiting program!\n");
			exit(1);
		 }
#endif		 	

gettimeofday(&tv_start,NULL);
#if TEST_TYPE == RTC
		 ts_start = getHMSTimestamp();
#else
		 gettimeofday(&tv_start,NULL);
#endif		 
		 for(int i=0; i<MAX_CNT;i++)
		 {             
			  
#if DATA_ARRAY_FLAG == 1			  
			  data[i] = readADC();
#else			  
			  data = readADC();			  
#endif			  
#if SAMPLE_MODE == 0
			  //sys_delay();
			  bcm2835_delayMicroseconds(sample_delay);   
#endif						 
		 } 


#if TEST_TYPE == RTC
		 ts_end = getHMSTimestamp();
		 time_diff = ts_end - ts_start;
#else
		 gettimeofday(&tv_end,NULL);	
		 time_diff = (tv_end.tv_sec*(uint64_t)1000000 + (uint64_t)tv_end.tv_usec)
					  - (tv_start.tv_sec*(uint64_t)1000000 + (uint64_t)tv_start.tv_usec);
#endif		 			 
		 
		 //printf("Time start:  %lu\n",ts_start);
		 //printf("Time end:  %lu\n",ts_end);
#if TEST_TYPE == RTC		 
		 printf("Time difference for loop (secs):  %lu\n",time_diff);
		 printf("Max count: %llu\n",MAX_CNT);
		 printf("Time difference per sample (secs):  %f\n",(float)time_diff/(float)MAX_CNT);
		 float eff_rate = 1.0/( ((float)time_diff/(float)MAX_CNT));
		 printf("Effective sampling rate:  %.2f\n",eff_rate) ;          				  
#else
       printf("Time difference for loop (us):  %llu\n",time_diff);
		 printf("Max count: %llu\n",MAX_CNT);
		 printf("Time difference per sample (us):  %llu\n",time_diff/MAX_CNT);
		 //printf("Effective sampling rate:  %f\n",1.0/( ((float)time_diff/(float)MAX_CNT))*1000000 ) ;
		 float eff_rate = 1.0/( ((float)time_diff/(float)MAX_CNT))*1000000;
		 printf("Effective sampling rate:  %.2f\n",eff_rate) ;          				  
		 
#endif		 
		 //Save sampled data to file:
		 if(saveToFile)
		 {
			 
			 if(save_raw_data)
			 {
				 for(int i=0;i<MAX_CNT;i++)
				 {
 #if DATA_ARRAY_FLAG == 1
					 fprintf(fp,"%.2f\n",data[i]);
#else
					 fprintf(fp,"%.2f\n",data);
#endif					 
				 } 
		    }
		    else
		    {
			 	if(SAMPLE_MODE == 0)
			 	{
			 		fprintf(fp,"%.2f %.2f\n",sample_rates[j],eff_rate);
			 	}
			 	else
			 	{
					fprintf(fp,"fast-looping sample rate:  %.2f\n",eff_rate);	
				}		
			 }	 
			 
		 }
		 
		 printf("Closing down SPI interface...\n");
		 MCP3008_Close();
		 printf("SPI closed down\n");
		 //Free dynamically allocated memory
#if DATA_ARRAY_FLAG == 1
	 	free(data);
#endif	    
		 
	}//j
	
	if(saveToFile)
	{ 
	 	//Close file:
		 fclose(fp);
		 printf("Data file closed\n");
 
	}
	
#if TEST_MODE == RTC	
	 //Close RTC driver:
    close_dev();
#endif    
	 
	 //Close deice /dev/mem
	 bcm2835_close();
	 printf("Exiting program\n");              
	 exit(0);					
    
   

}//doTest_RTC



//NB: This handler is working for the CTL-C keyboard signal
//	  This will exit the while loop sple (us):  1157
//   to exit the program gracefully
//	  (We need to close the connection to the serial terminal)
void ctrl_c_handler(int sig)
{
   if(sig == SIGINT) //Check it is the right user ID //NB: Not working at the moment
   {
         printf("\nWe have received the CTRL-C signal - aborting sample looping!\n");
         free(PLOT_BUFFER);
         free(PLOT_COPY);
         if(GRAPH_MODE == 0)
         {
				free(dataX);
				free(dataY);
		   }	
         
         //EXIT_PROGRAM = 1; //This will stop program
         exit(0);
   }
   
         
}//ctrl_c_handler(int sig)


// BCM DELAY FUNCTIONS HERE ----------------------------------
/* microseconds */
void bcm2835_delayMicroseconds(uint64_t micros)
{
    struct timespec t1;
    uint64_t        start;
	
    
    /* Calling nanosleep() takes at least 100-200 us, so use it for
    // long waits and use a busy wait on the System Timer for the rest.
    */
    start =  bcm2835_st_read();
    
    /* Not allowed to access timer registers (result is not as precise)*/
    if (start==0)
    {
		
				
		if(micros < 1000000)
		{
			t1.tv_sec = 0;
			t1.tv_nsec = 1000 * (long)(micros);
		}
		else
		{
			t1.tv_sec = micros/1000000;
			t1.tv_nsec = 0;
			
		}
		
				
		nanosleep(&t1, NULL);
		return;
    }
    
    
    if (micros > 450)   //450us -> ~2222Hz
    {
		t1.tv_sec = 0;
		t1.tv_nsec = 1000 * (long)(micros - 200);
		nanosleep(&t1, NULL);
    }    
     
    bcm2835_st_delay(start, micros);
}

/* Read the System Timer Counter (64-bits) */
uint64_t bcm2835_st_read(void)
{
    volatile uint32_t* paddr;
    uint32_t hi, lo;
    uint64_t st;

    if (bcm2835_st==MAP_FAILED)
	return 0;

    paddr = bcm2835_st + BCM2835_ST_CHI/4;
    hi = bcm2835_peri_read(paddr);

    paddr = bcm2835_st + BCM2835_ST_CLO/4;
    lo = bcm2835_peri_read(paddr);
    
    paddr = bcm2835_st + BCM2835_ST_CHI/4;
    st = bcm2835_peri_read(paddr);
    
    /* Test for overflow */
    if (st == hi)
    {
        st <<= 32;
        st += lo;
    }
    else
    {
        st <<= 32;
        paddr = bcm2835_st + BCM2835_ST_CLO/4;
        st += bcm2835_peri_read(paddr);
    }
    return st;
}

/* Read with memory barriers from peripheral
 *
 */
uint32_t bcm2835_peri_read(volatile uint32_t* paddr)
{
    uint32_t ret;
    if (debug)
    {
		printf("bcm2835_peri_read  paddr %p\n", (void *) paddr);
		return 0;
    }
    else
    {
       __sync_synchronize();
       ret = *paddr;
       __sync_synchronize();
       return ret;
    }
}

/* Delays for the specified number of microseconds with offset */
void bcm2835_st_delay(uint64_t offset_micros, uint64_t micros)
{
    uint64_t compare = offset_micros + micros;

    while(bcm2835_st_read() < compare)
	;
}


static void *mapmem(const char *msg, size_t size, int fd, off_t off)
{
    void *map = mmap(NULL, size, (PROT_READ | PROT_WRITE), MAP_SHARED, fd, off);
    if (map == MAP_FAILED)
	fprintf(stderr, "bcm2835_init: %s mmap failed: %s\n", msg, strerror(errno));
    return map;
    
}//mapmen



static void unmapmem(void **pmem, size_t size)
{
    if (*pmem == MAP_FAILED) return;
    munmap(*pmem, size);
    *pmem = MAP_FAILED;
    
}//unmapmem



int bcm2835_init(void)   //Set up memory map interface to System Timer
{
	int  memfd;
   FILE *fp;
	
	
	if ((fp = fopen(BMC2835_RPI2_DT_FILENAME , "rb")))
   {
        unsigned char buf[16];
        uint32_t base_address;
        uint32_t peri_size;
        if (fread(buf, 1, sizeof(buf), fp) >= 8)
        {
            base_address = (buf[4] << 24) |
              (buf[5] << 16) |
              (buf[6] << 8) |
              (buf[7] << 0);
            
            peri_size = (buf[8] << 24) |
              (buf[9] << 16) |
              (buf[10] << 8) |
              (buf[11] << 0);
            
            if (!base_address)
            {
                /* looks like RPI 4 */
                base_address = (buf[8] << 24) |
                      (buf[9] << 16) |
                      (buf[10] << 8) |
                      (buf[11] << 0);
                      
                peri_size = (buf[12] << 24) |
                (buf[13] << 16) |
                (buf[14] << 8) |
                (buf[15] << 0);
            }
            /* check for valid known range formats */
            if ((buf[0] == 0x7e) &&
                    (buf[1] == 0x00) &&
                    (buf[2] == 0x00) &&
                    (buf[3] == 0x00) &&
                    (base_address == BCM2835_RPI2_PERI_BASE ))  //Base address for RPI2 and RPI3
            {
                bcm2835_peripherals_base = (off_t)base_address;
                bcm2835_peripherals_size = (size_t)peri_size;
                //printf("Base mapping is for RPI2/3\n");
					 //printf("The value is 0x%0x\n",base_address);
            }
            
        
        }
                
		fclose(fp);
		
		/* Open the master /dev/mem device */
      if ((memfd = open("/dev/mem", O_RDWR | O_SYNC) ) < 0) 
		{
		  fprintf(stderr, "bcm2835_init: Unable to open /dev/mem: %s\n",
			  strerror(errno)) ;
		  printf("Exiting program ...\n");
		  exit(0);
		}
		
		/* Base of the peripherals block is mapped to VM */
      bcm2835_peripherals = mapmem("gpio", bcm2835_peripherals_size, memfd, bcm2835_peripherals_base);
      if (bcm2835_peripherals == MAP_FAILED)
      {
			printf("Failed to map memory for peripherals - Exiting program ... \n");
			exit(0);	
		}	 
      
      //Get base address for system timer: 
      
      bcm2835_st   = bcm2835_peripherals + BCM2835_ST_BASE/4;
      
      return 1;
		
		
		
   }
   else
   {
		//Could not open device tree file:
		printf("Could not access device tree file - exiting program ...\n");
		exit(0);	
		
	}	
	
	
}//bcm2835_init	


int bcm2835_close(void)
{
	
	 unmapmem((void**) &bcm2835_peripherals, bcm2835_peripherals_size);
    bcm2835_peripherals = MAP_FAILED;
    bcm2835_st   = MAP_FAILED;
    
    
    return 1; /* Success */
	

}//bcm2835_close

