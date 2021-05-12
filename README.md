## SPI Communication with MCP3008 ADC using Custom LKM Device Driver

* [Description](#description)
* [MCP3008 Device Driver](#driver)
* [Graphics](#graphics)

### Description
This repository provides the code for a Raspberry Pi application
to process an external signal at a required sampling rate. The
project implements the SPI protocol to communicate with a MCP3008
8-channel Analog to Digital Converter. The MCP3008 takes an analog
signal from a signal source (e.g. signal generator) and converts it
to a 10-bit digital representation.

The code will process the external signal by using a custom built 
Linux kernel Modulke devce driver in order to interface with the MCP3008
using the SPI protocol.

### MCP3008 Device Driver

A detailed discussion of the efficiency of the device driver can be found at the project Wiki:\
https://github.com/MichaelMcGetrick/MCP3008_LKM/wiki

       	    	    
		
### Graphics		   
The signal can then be displayed by using appropriate graphics
code. The project will use the graphic_lx library to be found at:\
https://github.com/UtterSense/Simple-Graphics
     
       
       
       
       
             		
      
      
              
