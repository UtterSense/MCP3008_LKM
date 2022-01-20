/*   MODULE HEADER FILE: mcp3008.h --------------------------------------- 
 * 
 *   Header file for building a shared library to be used by JNI for
 *   communicationg with the LKM (device driver) for the MCP3008 8-chanel
 *   Analog to digital converter.
 ----------------------------------------------------------------------*/ 
#ifndef MCP3008_H
#define MCP3008_H


//#define ANDROID_APP      //Add this define if we are running C Code as part of Android App 
                         //to facilitate adb logging          
#ifdef ANDROID_APP
 #include <android/log.h>
 #include <jni.h>
#endif

#define LEN 10
#define DRIVER_NAME	"mcp3008"
#define VREF              3.3    //Reference voltage for analog measurements    
#define MAX_ANALOG_VAL    1024   //Max. integer value of 10 bit analog signal 


unsigned int delay_ms;      //Delay in milliseconds
uint64_t delay_us;          //Delay in microseconds    



struct adcSettings {
	int readMode;   
	int chanIndex;   
};	

struct adcSettings settings;


void mcp3008_test(void);
uint64_t getSamplingRate(void);
static char* jni_test(void);
int MCP3008_Init(float sample_rate);
void setParams(int rm, int ch);  //Set ADC params
float readADC();
uint16_t readADC1();
uint16_t getVal(uint8_t msb,uint8_t lsb);


//File operations
void MCP3008_Close(void);
int32_t readByte(char addr);
int32_t readData(char* elem);
int32_t writeByte(char elem, char data);


#ifdef ANDROID_APP
	void init_callbacks(char *f_name, char *f_sig);
	void sendToJava(char *c);
#endif


#ifdef ANDROID_APP
	JNIEnv *glo_env;
	JavaVM *JVM;
	jclass glo_clazz;
	jobject glo_obj;
	char *callback_name; 
	char *callback_sig;
#endif

#endif
