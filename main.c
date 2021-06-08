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



//DEFINES ---------------------------------------
//#define SAMPLING_RATE     4000.0f//1000.0f  
#define DATA_ARRAY_FLAG	0 //1: create data array of samples for metric test; 0: single sample
#define NUM_ADC_CHANS 		3
#define LINUX				0
#define RTC					1
#define TEST_TYPE			RTC


	

//ATTRIBUTES: -----------------------------------------
int EXIT_PROGRAM = 	0;
char *PROG_MODE  = 	"TEST";		//TEST: GRAPHICS


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
char *filename = "lkm_linux_timer_integrity.csv";
bool saveToFile = false;



//FUNCTION DECLARATIONS -------------------------
void ctrl_c_handler(int sig);
void sys_delay();
void doTest();
//void sample_BufferMode();
void doGraph();
void sample_BufferMode();
void init_graph();

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
	
	
	if(SAMPLE_MODE == 0) //Delay mode
	{
		while(EXIT_PROGRAM == 0)
		{
			//Get sample
			for(int i=0; i< DATA_LEN;i++)
			{
				//Delay for set time: 					
				sys_delay();
				
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
				X_INT,Y_INT,X_GRAD,Y_GRAD,
				SHOW_GRID, G_INT,
				X_SHOW,Y_SHOW,ORI_LINE,BITMAP,
				BCK_COLOR, AUDIOFILE, INI_AUDIOFILE,
				NUM_CHANS, SAMPLE_RATE, BITS_PER_SAMPLE);
		
	 
	//Setup Legend:
	if(legend_flg)
	{
		l_caps.vmax_caption = LEGEND_CAPTION1;
		l_caps.vmin_caption = LEGEND_CAPTION2;
		l_caps.vavg_caption = LEGEND_CAPTION3;
		l_caps.vpp_caption = LEGEND_CAPTION4;
		 int color = 15; //Font color for captions
		 legendCaptions(l_caps,color);
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
			l_data.vmax = vmaxCopy;
			l_data.vmin = vminCopy;
			l_data.vavg = vavgCopy;
			l_data.vpp = vppCopy;
			legend(l_data,true); //This should erase previous data write
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
		 l_data.vmax = vmax; //sigFreq;
		 l_data.vmin = vmin; //vpp;
		 l_data.vavg = vavg; //Mean voltage
		 l_data.vpp = vpp;  //Peak to peak voltage
		legend(l_data,false); //Redraw legend
		//legend_flg = false; //temp
	}
	
	
	plotTrace(TRACE_COLOR);
	graduations(15);  //Refresh grduations
			
	copyDataBuf();  //Copy current graph trace
	copyLegendData();
	
	
}//doGraph	



void doTest()   //Measure time with Real Time Clock (ds3231)
{
	
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
			  sys_delay();   
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
         
         EXIT_PROGRAM = 1; //This will stop program
         
   }
   
         
}//ctrl_c_handler(int sig)

