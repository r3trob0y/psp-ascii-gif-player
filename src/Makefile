TARGET = AsciiGif
OBJS = audio/pspaalib.o audio/pspaalibeffects.o audio/pspaalibwav.o main.o 

INCDIR = 
CFLAGS = -O2 -G0 -Wall
CXXFLAGS = $(CFLAGS) -fno-exceptions -fno-rtti
ASFLAGS = $(CFLAGS)

LIBDIR =
LDFLAGS =

LIBS = -lpspaudio -lpspaudiocodec -lpspaudiolib

EXTRA_TARGETS = EBOOT.PBP
PSP_EBOOT_TITLE = ASCII GIF Player
PSP_EBOOT_ICON = ascii.png

PSPSDK=$(shell psp-config --pspsdk-path)
include $(PSPSDK)/lib/build.mak