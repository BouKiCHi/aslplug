#
# Makefile for LOGPLAY
#

# DEBUG = 1

ifeq ($(OS),Windows_NT)
WIN32=1
endif

NAME = logplay

USE_FMGEN = 1
USE_OPL3 = 1
USE_RCDRV = 1

LIB = libnb.a
SDL_DLL = 

# win32
ifdef WIN32

SFX = .exe
PFX = 
LIBS =
SDLCONF = sdl-config
OBJDIR = objs_w32
OSSFX = _w32

SDL_CFLAGS := $(shell $(SDLCONF) --cflags | sed -e "s/-Dmain=SDL_main//g")

SDL_CONFIG = sdl-config
SDL_DIR = /mingw32/lib

SDL_LIBS := -mconsole 
SDL_LIBS += -L$(SDL_DIR) -lSDLmain 
SDL_LIBS += ../resource/dll/SDL.dll
SDL_DLL = ../resource/dll/SDL.dll
SDL_LIBS += -static


# unix
else

SFX =
PFX =
LIBS =
SDLCONF = sdl-config
OBJDIR = objs
OSSFX =

SDL_CFLAGS = $(shell $(SDLCONF) --cflags)
SDL_LIBS = $(shell $(SDLCONF) --libs)

endif

TARGET = $(NAME)$(SFX)
ZIPSRC = $(NAME)_$(shell date +"%y%m%d")_src.zip
ZIPBIN = $(NAME)_$(shell date +"%y%m%d")$(OSSFX).zip

#
# Commands
#

AR = $(PFX)ar
CC = $(PFX)gcc
CXX = $(PFX)g++
STRIP = $(PFX)strip

#
# Flags
#

CFLAGS = $(ARCHFLAGS) -g
LFLAGS = $(ARCHFLAGS) -g

CFLAGS += $(SDL_CFLAGS)
LIBS += $(SDL_LIBS)

ifdef DEBUG
CFLAGS += -O0
else
CFLAGS += -O3
endif

#
# OPM CFLAGS
#
CFLAGS += -DUSE_OPM

#
# Sources
#

EXTTOP = ..
SRCTOP = ../src

FMTDIR = format
DEVDIR = device
YMDIR  = device/ym2151
OPLDIR = device/opl
LIBDIRS = $(FMTDIR) $(DEVDIR) $(YMDIR) $(OPLDIR)

LOGDIR = log
EXTDIRS += $(LOGDIR)

FMTSRCS = \
 audiosys.c \
 handler.c \
 songinfo.c

DEVSRCS = \
 s_logtbl.c

DEVSRCS += \
 s_opm.c \
 s_psg.c

OPLSRCS = \
 s_deltat.c \
 s_opl.c \
 s_opltbl.c

YMSRCS = \
 ym2151.c

LOGSRCS = \
 nlg.c \
 s98x.c \
 log.c

NBSRCS = \
 sdlplay.c 

COMDIR = common
EXTDIRS += $(COMDIR)

COMSRCS = \
 memory_wr.c io_dummy.c render.c

#
# Library 
#

LIBSRCS = $(addprefix $(FMTDIR)/,$(FMTSRCS))
LIBSRCS += $(addprefix $(DEVDIR)/,$(DEVSRCS))
LIBSRCS += $(addprefix $(YMDIR)/,$(YMSRCS))
LIBSRCS += $(addprefix $(OPLDIR)/,$(OPLSRCS))
LIBSRCS += $(addprefix $(LOGDIR)/,$(LOGSRCS))
LIBSRCS += $(addprefix $(COMDIR)/,$(COMSRCS))

#
# fmgen
#

ifeq ($(USE_FMGEN), 1)
CFLAGS += -DUSE_FMGEN
FMGENDIR = device/fmgen

FMGENSRCS = file.cpp \
 fmgen.cpp \
 fmtimer.cpp \
 opm.cpp \
 opna.cpp \
 psg.cpp

DEVSRCS += s_opm_gen.cpp s_opna_gen.cpp

LIBSRCS += $(addprefix $(FMGENDIR)/,$(FMGENSRCS))
LIBDIRS += $(FMGENDIR)

endif

#
# OPL3
#

ifeq ($(USE_OPL3), 1)
CFLAGS += -DUSE_OPL3
MAMEDIR = device/mame

MAMESRCS = ymf262.c
DEVSRCS += s_opl4.c

LIBSRCS += $(addprefix $(MAMEDIR)/,$(MAMESRCS))
LIBDIRS += $(MAMEDIR)

endif

#
# RCDRV
#
ifeq ($(USE_RCDRV),1)

CFLAGS += -DUSE_RCDRV
RCDIR = rcdrv
RCSRCS = rcdrv.cpp
EXTDIRS += $(RCDIR)
LIBSRCS += $(addprefix $(RCDIR)/,$(RCSRCS))

endif


#
# nlgsim source
#
SRCS =  $(NBSRCS)

#
# Libary
#

LIB := $(OBJDIR)/$(LIB)
LIBOBJDIR = $(OBJDIR)
LIBOBJS = $(addprefix $(LIBOBJDIR)/,$(addsuffix .o,$(basename $(LIBSRCS))))

CFLAGS += -I$(SRCTOP) $(addprefix -I$(SRCTOP)/,$(LIBDIRS))
CFLAGS += $(addprefix -I$(EXTTOP)/,$(EXTDIRS))
CFLAGS += -I.

#
# main objects
#
OBJS = $(addprefix $(OBJDIR)/,$(addsuffix .o,$(basename $(SRCS))))

#
# Release Files
#
RELEASE_FILES = readme.txt history.txt

#
# Rules
#
all : $(TARGET)

lib : $(LIB)

strip : $(TARGET)
	$(STRIP) $(TARGET)


$(OBJDIR)/%.o : %.c
	$(CC) -c $(CFLAGS) -o $@ $<

$(OBJDIR)/%.o : %.cpp
	$(CXX) -c $(CFLAGS) -o $@ $<


$(OBJDIR)/%.o : $(SRCTOP)/%.c
	$(CC) -c $(CFLAGS) -o $@ $<

$(OBJDIR)/%.o : $(SRCTOP)/%.cpp
	$(CXX) -c $(CFLAGS) -o $@ $<


$(OBJDIR)/%.o : $(EXTTOP)/%.c
	$(CC) -c $(CFLAGS) -o $@ $<

$(OBJDIR)/%.o : $(EXTTOP)/%.cpp
	$(CXX) -c $(CFLAGS) -o $@ $<

#
# Library
#

$(LIB) : $(OBJDIR) $(LIBOBJS)
	$(AR) rcs $@ $(LIBOBJS)

$(OBJDIR) :
	mkdir -p $(OBJDIR)
	mkdir -p $(addprefix $(LIBOBJDIR)/,$(LIBDIRS))
	mkdir -p $(addprefix $(LIBOBJDIR)/,$(EXTDIRS))

#
# TARGET
#

$(TARGET) : $(OBJDIR) $(LIB) $(OBJS)
	$(CXX) $(LFLAGS) $(OBJS) $(LIB) $(LIBS) -o $@

#
# Options
#

clean :
	rm -rf $(OBJDIR)
	rm -f $(TARGET)

zipclean :
	rm -f $(ZIPBIN) $(ZIPSRC)

zipsrc:
	zip $(ZIPSRC) Makefile
	zip -r $(ZIPSRC) $(SRCDIR)
	zip -r $(ZIPSRC) $(LOGDIR)
	zip $(ZIPSRC) render.* sdlplay.c
	zip $(ZIPSRC) $(RELEASE_FILES)

zip : strip
	rm -f $(ZIPBIN)
	zip $(ZIPBIN) $(TARGET) $(RELEASE_FILES)
	zip -j $(ZIPBIN) $(SDL_DLL)

