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
USE_GMCDRV = 1

LIB = libnb.a


ifdef WIN32

# win32

SFX = .exe
PFX = 
LIBS =
SDLCONF = sdl-config
OBJDIR = objs_w32
OSSFX = _w32

SDL_CFLAGS := $(shell $(SDLCONF) --cflags | sed -e "s/-Dmain=SDL_main//g")

SDL_LIBS := -mconsole
SDL_LIBS += $(shell $(SDLCONF) --libs | sed -e "s|-mwindows||g" | \
 sed -e  "s|-lSDLmain||g" | sed -e  "s|-lmingw32||g")
SDL_LIBS += -static-libgcc

else

# unix

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

COMDIR = common
EXTDIRS += $(COMDIR)


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
 io_dummy.c \
 sdlplay.c \
 render.c

COMSRCS = \
 memory_wr.c

#
# Sources
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
# gmcdrv
#

ifeq ($(USE_GMCDRV), 1)
CFLAGS += -DUSE_GMCDRV
GMCDIR = gmcdrv

ifeq ($(WIN32),1)
GMCSRCS = gmcdrv.cpp
else
GMCSRCS = gmcdrv_dummy.cpp
endif


LIBSRCS += $(addprefix $(GMCDIR)/,$(GMCSRCS))
EXTDIRS += $(GMCDIR)

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
# Rules
#
all : $(TARGET)

lib : $(LIB)

release : $(TARGET)
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
	zip $(ZIPSRC) readme.txt

zip :
	zip $(ZIPBIN) $(TARGET) readme.txt SDL.dll
