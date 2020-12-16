obj-m+=uttersense.o

INCLUDES = -I/home/pi/CProjects/Graphics/Simple-Graphics

all:
	make -C /lib/modules/$(shell uname -r)/build/ M=$(PWD) modules
	$(CC) $(INCLUDES) main.c  graph.c mcp3008.c  ../../Graphics/Simple-Graphics/graphic_lx.c -lgraph -lm -o test
clean:
	make -C /lib/modules/$(shell uname -r)/build/ M=$(PWD) clean
	rm test

