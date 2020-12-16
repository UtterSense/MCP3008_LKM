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

struct DateTime {
	char Year[LEN];
	char Month[LEN];
	char Day[LEN];
	char Date[LEN];
	char Hour[LEN];
	char Minute[LEN];
	char Second[LEN];
};	

struct DateTime dt;

struct Temperature {
	char TempUpper[LEN];   //Integer part
	char TempLower[LEN];   //Decimal part
};	

struct Temperature temp;


	

void mcp3008_test(void);
char* jni_test(void);
int init_dev(void);
void close_dev(void);
int32_t read_byte(char addr);
int32_t read_data(char* elem);
int32_t write_byte(char elem, char data);


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
