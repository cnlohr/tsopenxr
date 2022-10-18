
ifeq ($(OS),Windows_NT)
CFLAGS:=-I. -g -Og
LDFLAGS:=-luser32 -lm -lopengl32 -lgdi32 ./openxr_loader.dll
EX_:=.exe
else
CFLAGS:=-I. -g -Og
LDFLAGS:=-lX11 -lm -lGL ./libopenxr_loader.so
endif

PROJECTS:=demo$(EX_) minimal$(EX_)

all : $(PROJECTS)

demo$(EX_) : demo.c
	$(CC) -o $@ $^ $(CFLAGS) $(LDFLAGS)

minimal$(EX_) : minimal.c
	$(CC) -o $@ $^ $(CFLAGS) $(LDFLAGS)

clean :
	rm -rf *.o *~ $(PROJECTS)
