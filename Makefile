
# for warning: implicit declaration of function ‘grantpt’
LDFLAGS += -D_GNU_SOURCE

# LDFLAGS += -Wall

all: remserial

REMOBJ=remserial.o stty.o
remserial: $(REMOBJ)
	$(CC) $(LDFLAGS) -o remserial $(REMOBJ)

stty.o: stty.c stty.h
	$(CC) $(LDFLAGS) -c stty.c

remserial.o: remserial.c remserial.h stty.h
	$(CC) $(LDFLAGS) -c remserial.c

clean:
	rm -f remserial *.o

