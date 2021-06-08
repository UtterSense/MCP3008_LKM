obj-m+=mcp3008_dt.o

#INCLUDES = -I/home/pi/CProjects/Graphics/Simple-Graphics
CC = gcc
GPH_INCLUDE = /home/pi/CProjects/Graphics/Simple-Graphics/
GPH_SRC_DIR = ../../Graphics/Simple-Graphics/
DS_SRC_DIR = /home/pi/GitRepos/UtterSense/ds3231/
DS_INCLUDE = $(DS_SRC_DIR)

#Define source file for shared library:
SHR_LIB_SRC = ds3231
TARGET = test

all:
	make -C /lib/modules/$(shell uname -r)/build/ M=$(PWD) modules
#	$(CC) $(INCLUDES) main.c  graph.c mcp3008.c  ../../Graphics/Simple-Graphics/graphic_lx.c -lgraph -lm -o test
	$(CC) -I$(GPH_INCLUDE) -I$(DS_INCLUDE) main.c  graph.c mcp3008.c  ../../Graphics/Simple-Graphics/graphic_lx.c $(DS_SRC_DIR)ds3231.c -lbcm2835 -lgraph -lm -o $(TARGET)

clean:
	make -C /lib/modules/$(shell uname -r)/build/ M=$(PWD) clean
	rm test

