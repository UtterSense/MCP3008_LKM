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

#include "graph.h"
#include "mcp3008.h"



//DEFINES ---------------------------------------
//#define SAMPLING_RATE     4000.0f//1000.0f  
#define NUM_ADC_CHANS 		3


	

//ATTRIBUTES: -----------------------------------------
int EXIT_PROGRAM = 	0;
char *PROG_MODE  = 	"TEST";		//TEST: GRAPHICS

//MCP3008 Parameters
int chanIndex = 0; // Index of ADC channel
int readMode = 1 ; //Single:1; Differential: 0
int chanVal[NUM_ADC_CHANS];
 
uint64_t sample_delay;
float data[1000];

//File attributes
FILE *fp;
char *filename = "data.csv";
bool saveToFile = false;



//FUNCTION DECLARATIONS -------------------------
void ctrl_c_handler(int sig);
//void doTest();
//void sample_BufferMode();
void doGraph();
void init_graph();

//-----------------------------------------------
int main(void) {

   //Register handler for capturing CTL-C signal:
	signal(SIGINT,ctrl_c_handler);
            
	    
	//Initialise SPI for MCP3008:
	printf("Initialising SPI for MCP3008 peripheral.....\n");
	
	MCP3008_Init();
	//set parameters (if not default)
	//setParams(readMode,chanIndex);	
	//int val = readADC();
	//printf("ADC reading: %d\n",val);	
		
		
	
	printf("ADC Readings\n");	
	fprintf(stdout, "COUNT    CHAN1    CHAN2    CHAN3\n");	
	
	int time_int = 1;  //seconds
	int numCnts = 100;
	for(int j=0;j < numCnts; j++)
	{
		for(int i=0; i < NUM_ADC_CHANS;i++)
		{
			//Delay
			sleep(time_int);
			
			//Change transmit control bytes
			setParams(readMode,i);
			//Read the ADC channel
			chanVal[i] = readADC();
		  
			
		}//i	
		
		//Print the current values:
		printf("%d         %d        %d     %d\n", j,chanVal[0],chanVal[1],chanVal[2]);
}//j

	
	//Close device:
	close_dev();
				
				
				
					 
			 
	printf("Exiting program!\n");
	exit(0);      
	
	
	 
	if(strcmp(PROG_MODE,"TEST") == 0)
	{
	   
		//doTest();
		printf("Peforming test!\n");
		exit(1);
		
	}
	else if(strcmp(PROG_MODE,"GRAPHICS") == 0)
	{
		
		 //sample_BufferMode();
 	
	}	 
		 

}//main


/*
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
	
	
	if(SAMPLE_MODE == 0) //bcm delay mode
	{
		while(EXIT_PROGRAM == 0)
		{
			//Get sample
			for(int i=0; i< DATA_LEN;i++)
			{
				//Delay for set time: 					
				bcm2835_delayMicroseconds(sample_delay);
				
				PLOT_BUFFER[i] = readSample();
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
				
				PLOT_BUFFER[i] = readSample();
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
*/


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
		legendCaptions(l_caps);
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




/*
void doTest()
{
		  
	 
	 //Sample and save data to file for subsequent analysis:
	 
	 struct timeval tv_start,tv_end;
	 uint64_t time_diff;
	 
	 
	 //Open file for data:
	 if(saveToFile)
	 {
		 if( (fp=fopen(filename,"wt")) == NULL)
		 {
			printf("Error opening file -aborting program!\n");
			exit(0);	 
		 }
    }
 
	 uint cnt = 0;
	 uint64_t max_cnt = 1000;
	 //while(EXIT_PROGRAM == 0)
	 gettimeofday(&tv_start,NULL);
	 for(int i=0; i<max_cnt;i++)
	 {             
		  data[i] = readSample();
		  
		  //bcm2835_delayMicroseconds(sample_delay);   
		       
					 
	 } 
	 gettimeofday(&tv_end,NULL);	
	  
	 
	 time_diff = (tv_end.tv_sec*(uint64_t)1000000 + (uint64_t)tv_end.tv_usec)
	           - (tv_start.tv_sec*(uint64_t)1000000 + (uint64_t)tv_start.tv_usec);
	 printf("Time difference for loop (us):  %llu\n",time_diff);
	 printf("Max count: %llu\n",max_cnt);
	 printf("Time difference per sample (us):  %llu\n",time_diff/max_cnt);
	 printf("Effective sampling rate:  %f\n",1.0/( ((float)time_diff/(float)max_cnt))*1000000 ) ;
	           				  
		
	 
	 //Save sampled data to file:
	 if(saveToFile)
	 {
		 for(int i=0;i<1000;i++)
		 {
			 fprintf(fp,"%.2f\n",data[i]);
		 } 
		 //Close file:
		 fclose(fp);
		 printf("Data file closed\n");
    }
	 
	 printf("Closing down SPI interface...\n");
	 MCP3008_Close();
	 printf("SPI closed down\n");
	 
	 
	 printf("Exiting program\n");              
	 exit(0);					
    
		
}//doTest	
*/


//NB: This handler is working for the CTL-C keyboard signal
//	  This will exit the while loop so as to exit the program gracefully
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

