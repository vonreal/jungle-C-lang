CC = gcc
CFLAGS = -g -Wall
LDFLAGS = -lpthread

all: echoserveri

echoserveri: echoserveri.o echo.o csapp.o
	$(CC) $(CFLAGS) -o echoserveri echoserveri.o echo.o csapp.o $(LDFLAGS)

echoserveri.o: echoserveri.c ../csapp.h
	$(CC) $(CFLAGS) -c echoserveri.c

echo.o: echo.c ../csapp.h
	$(CC) $(CFLAGS) -c echo.c

csapp.o: ../csapp.c ../csapp.h
	$(CC) $(CFLAGS) -c ../csapp.c

clean:
	rm -f *.o echoserveri
