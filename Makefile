CC = gcc
CFLAGS = -Wall -Werror

myshell: myshell.c
	$(CC) $(CFLAGS) -o myshell myshell.c

run: myshell
	./myshell

all: myshell

clean:
	rm -f myshell
