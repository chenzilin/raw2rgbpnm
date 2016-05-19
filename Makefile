CROSS_COMPILE ?=

CC	:= $(CROSS_COMPILE)gcc
CFLAGS	?= -O2 -W -Wall
LDFLAGS	?=

%.o : %.c
	$(CC) $(CFLAGS) -c -o $@ $<

raw2rgbpnm: raw2rgbpnm.o raw_to_rgb.o utils.o

clean:
	rm -f *.o
	rm -f raw2rgbpnm
