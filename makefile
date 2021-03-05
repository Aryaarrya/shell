CC=gcc

all: 
	$(CC) source.c -o exec

clean:
	find . ! -name source.c ! -name makefile -type f -delete
	#for deleting all files that the execution of 014 might have generated