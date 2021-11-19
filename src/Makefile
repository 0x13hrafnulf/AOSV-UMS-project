CC = gcc
CFLAGS = -Wall -g
INCLUDES = -I./library
LFLAGS = -pthread
LIB_DIR = ./library
SRCS = ums_example_3.c $(LIB_DIR)/ums_lib.c
OBJS = $(SRCS:.c=.o)

MAIN = example3

.PHONY: clean

all: $(MAIN)

$(MAIN): $(OBJS) 
	$(CC) $(CFLAGS) $(INCLUDES) -o $(MAIN) $(OBJS) $(LFLAGS)

.c.o:
	$(CC) $(CFLAGS) $(INCLUDES) -c $<  -o $@

clean:
	$(RM) *.o *~ $(MAIN) $(LIB_DIR)/*.o
