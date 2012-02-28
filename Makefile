CFLAGS = -W -Wall

raw2rgbpnm: raw2rgbpnm.o raw_to_rgb.o utils.o

clean:
	rm -f *.o
	rm -f raw2rgbpnm
