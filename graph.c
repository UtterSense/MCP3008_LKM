/*
 =======================================================================
 Name        : graph.c
 Author      : Michael McGetrick
 Version     :
 Copyright   : 
 Description : Graph module to set up graphing for animated plotting 
 	 	 	      
 	 	 	         

 	 	 	    
 =======================================================================
 */
#include <stdio.h>
#include <stdlib.h>
#include <strings.h>
#include <string.h>
#include <stdint.h>

#include "graph.h"


int refCnt = 1;

double vmax = 0.0;
double vmin = 0.0;
double vavg = 0.0;
double vpp = 0.0;
double vmaxCopy = 0.0;
double vminCopy = 0.0;
double vavgCopy = 0.0;
double vppCopy = 0.0;


//PLOTTING ATTRIBUTES
//Simulator incrementor
int j=0;
//Plotting control flag
bool plotStart = false;
bool legend_flg = true;
bool mSuppress = false;



//GRAPH FUNCTIONS -------------------------------------------------------------
void plotTrace(int color)
{
	  	  
	  vmax = Y_MIN;
	  vmin = Y_MAX;
	  
	  if(GRAPH_MODE == 0)   //Simulation
     {
		  
		  //Allocate memory for display data:
		  if((dataX = malloc(DATA_LEN*sizeof(float))) == NULL)
		  {
				printf("Problem allocationg mermory for display data\n");
				printf("Exiting program!\n");
				exit(1);
		  }	
	
		  if((dataY = malloc(DATA_LEN*sizeof(float))) == NULL)
		  {
				printf("Problem allocationg mermory for display data\n");
				printf("Exiting program!\n");
				exit(1);
		  }
		  
		  
		  for(int i=0;i<DATA_LEN;i++)
		  {
				dataX[i] = i;
		  }
	   
	   
		  for(int i=0;i<DATA_LEN;i++)
		  {
				float T = 10;
				float val = (2*3.412*(i+j))/T;
				dataY[i] = sin(val);
		  }
		  
			 
		 for(int i= 0;i <DATA_LEN;i++)
		 { 
			plotSig(dataY[i],dataX[i],color);      
		 }
		  
		 setLinePlotFlag(0); //Ensure we do not use last position of current plot to draw (lineplot)
		 
     }//if(GRAPH_MODE == 0)   //Simulation
     if(GRAPH_MODE == 1)   //Real data
     {
		 
		 if(REFRESH_MODE == 0)
		 {
			 for(int i= 0;i < DATA_LEN;i++)
			 { 
				
				if(color == TRACE_COLOR)
				{
					plotSig(PLOT_BUFFER[i],i,color);
					
					//Get Vpp:
					if(legend_flg)   //Only do metrics if legend is requested
					{
						if(PLOT_BUFFER[i] > vmax)
						{
							vmax = PLOT_BUFFER[i];	
						}
						if(PLOT_BUFFER[i] < vmin)
						{
							vmin = PLOT_BUFFER[i];	
						}
						
				   }//if(legend_flg)	
				}	
				if(color == BCK_COLOR)
				{
					plotSig(PLOT_COPY[i],(float) i,color);  
				}   
				
				     
			 }
			 	  
			 vpp = vmax - vmin;
			 vavg = (vmax+vmin)/2.0;
			 
			 //sigFreq = t2-t1;//10.0; //TBD - perform RT value for frequency
			  
			  
			 setLinePlotFlag(0); //Ensure we do not use last position of current plot to draw (lineplot)
		 
	     }//if(REFRESH_MODE == 0)
	     if(REFRESH_MODE == 1)
		 {
			 for(int i= 0;i < DATA_LEN;i++)
			 { 
				
				if(color == TRACE_COLOR)
				{
					plotSig(PLOT_BUFFER[i],i,color);	
				}	
				if(color == BCK_COLOR)
				{
					plotSig(PLOT_COPY[i],(float) i,color);  
				}       
			 }
			  
			 setLinePlotFlag(0); //Ensure we do not use last position of current plot to draw (lineplot)
		 
	     }//if(REFRESH_MODE == 1)
		 
	 }//if(GRAPH_MODE == 1)   //Real data	 
     
}//plotTrace



void refreshCanvas()
{ 
   plotTrace(BCK_COLOR);
   if(SHOW_GRID)
  {
      //graduations();
  }
  
  if(show_ori_line)
  {
      setcolor(15);
      drawOriginLine();
  }    
    
  
   
   
}//refreshCanvas   


void copyDataBuf()
{
	int k;
	for(k=0;k < DATA_LEN;k++)
	{
			PLOT_COPY[k] = PLOT_BUFFER[k];
	}	
	
}//copyDataBuf	

void copyLegendData()
{
	//Hold copies of previous version for graphic refresh purposes
	vmaxCopy = vmax;
	vminCopy = vmin;	
	vavgCopy = vavg;
	vppCopy = vpp;
}	


void removeGrid()
{
   graduations(BCK_COLOR);
}   

//-----------------------------------------------------------------------
