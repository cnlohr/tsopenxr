all : demo minimal

# Linux Makefile

CFLAGS:=-I. -g -Og
LDFLAGS:=-lX11 -lm -lGL ./libopenxr_loader.so

demo : demo.c
	$(CC) -o $@ $^ $(CFLAGS) $(LDFLAGS)

minimal : minimal.c
	$(CC) -o $@ $^ $(CFLAGS) $(LDFLAGS)

clean :
	rm -rf *.o *~ demo minimal

