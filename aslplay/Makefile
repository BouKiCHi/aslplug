#
# Makefile for ASLPLAY
#

NAME = aslplay

ifeq ($(OS),Windows_NT)
WIN32=1
endif

ifeq ($(WIN32),1)
include Makefile.w32_cross
USE_RCDRV = 1
else
include Makefile.unix
USE_RCDRV = 0
endif

include Makefile.inc

ZIPBIN = $(NAME)_$(shell date +"%y%m%d")$(OSSFX).zip


#
# common
#

USE_OPL3 = 1
USE_OPM = 1
USE_FMGEN = 1

#
# resource
#
RELEASE_FILES = readme.txt changelog


#
# Sources
#
BASEDIR = ..
SRCDIR = $(BASEDIR)/src

LIBNEZ = libnez.a

SLIBS = $(SDL_SLIBS)
CFLAGS += $(SDL_CFLAGS)

ifeq ($(STATIC),1)
LIBS = $(SDL_SLIBS)
else
LIBS = $(SDL_LIBS)
endif

#
# directory for library
#

FMTDIR    = format
DEVDIR    = device
NESDIR    = device/nes
OPLDIR    = device/opl

CPUDIR = cpu

KM6502DIR = cpu/km6502
KMZ80DIR  = cpu/kmz80


FMTSRCS = \
audiosys.c handler.c m_gbr.c \
m_hes.c m_kss.c m_nsd.c m_nsf.c m_sgc.c \
m_zxay.c nezplug.c nsf6502.c songinfo.c

NESSRCS = \
logtable.c s_apu.c s_fds.c s_fds1.c s_fds2.c s_fds3.c \
s_fme7.c s_mmc5.c s_n106.c s_vrc6.c s_vrc7.c

DEVSRCS = \
s_dmg.c s_hesad.c s_psg.c s_sng.c \
s_hes.c s_logtbl.c s_scc.c

OPLSRCS = \
s_deltat.c s_opl.c s_opltbl.c

KMZ80SRCS = \
kmdmg.c kmevent.c kmr800.c kmz80.c kmz80c.c kmz80t.c

#
# directory for main
#

MAINDIR   = aslplay

COMMONDIR = common
LOGDIR = log
GLUEDIR = glue

MAINSRCS = sdlplay.c
GLUESRCS = glue2.c
LOGSRCS = nlg.c s98x.c log.c

COMSRCS = audio.c memory_wr.c


DIRS = $(MAINDIR) $(LOGDIR) $(GLUEDIR) $(COMMONDIR) 

LIBDIRS = $(FMTDIR) $(KMZ80DIR) $(KM6502DIR)
LIBDIRS += $(DEVDIR) $(CPUDIR) $(NESDIR) $(OPLDIR)

SRC =
LIBSRCS = 
LIBSRCS += $(addprefix $(FMTDIR)/,$(FMTSRCS))
LIBSRCS += $(addprefix $(KMZ80DIR)/,$(KMZ80SRCS))
LIBSRCS += $(addprefix $(DEVDIR)/,$(DEVSRCS))
LIBSRCS += $(addprefix $(NESDIR)/,$(NESSRCS))
LIBSRCS += $(addprefix $(OPLDIR)/,$(OPLSRCS))
LIBSRCS += $(addprefix $(LOGDIR)/,$(LOGSRCS))
LIBSRCS += $(addprefix $(COMMONDIR)/,$(COMSRCS))


####  OPL3
ifeq ($(USE_OPL3),1)

DEVSRCS += s_opl4.c
CFLAGS += -DUSE_OPL3
OPL3DIR = device/mame
OPL3SRCS = ymf262.c
LIBDIRS += $(OPL3DIR)
LIBSRCS += $(addprefix $(OPL3DIR)/,$(OPL3SRCS))

endif

#### OPM
ifeq ($(USE_OPM),1)

DEVSRCS += s_opm.c
CFLAGS += -DUSE_OPM
OPMDIR = device/ym2151
OPMSRCS = ym2151.c
LIBDIRS += $(OPMDIR)
LIBSRCS += $(addprefix $(OPMDIR)/,$(OPMSRCS))

endif

#### FMGEN
ifeq ($(USE_FMGEN),1)

DEVSRCS += s_opm_gen.cpp
CFLAGS += -DUSE_FMGEN
GENDIR = device/fmgen
GENSRCS = fmgen.cpp fmtimer.cpp opm.cpp file.cpp
LIBDIRS += $(GENDIR)
LIBSRCS += $(addprefix $(GENDIR)/,$(GENSRCS))

endif

#### RCDRV
ifeq ($(USE_RCDRV),1)

CFLAGS += -DUSE_RCDRV
RCDIR = rcdrv
RCSRCS = rcdrv.cpp
DIRS += $(RCDIR)
SRCS += $(addprefix $(RCDIR)/,$(RCSRCS))

endif

SRCS += $(addprefix $(MAINDIR)/,$(MAINSRCS))
SRCS += $(addprefix $(GLUEDIR)/,$(GLUESRCS))
SRCS += $(addprefix $(LOGDIR)/,$(LOGSRCS))


LIBOBJS = $(addprefix $(OBJDIR)/,$(addsuffix .o,$(basename $(LIBSRCS))))
OBJS = $(addprefix $(OBJDIR)/,$(addsuffix .o,$(basename $(SRCS))))

LIBNEZ := $(OBJDIR)/$(LIBNEZ)

#
# include
#

# CFLAGS += $(addprefix -I$(SRCDIR)/,$(LIBDIRS))
CFLAGS += $(addprefix -I$(BASEDIR)/,$(DIRS))
CFLAGS += -I$(MAINDIR)
CFLAGS += -I$(SRCDIR)

#
#
#

all : $(TARGET)

lib : $(LIBNEZ)

#
# Rules
#

$(OBJDIR)/%.o : %.c
	$(CC) -c $(CFLAGS) -o $@ $<

# files in DIRS
$(OBJDIR)/%.o : $(BASEDIR)/%.c
	$(CC) -c $(CFLAGS) -o $@ $<

$(OBJDIR)/%.o : $(BASEDIR)/%.cpp
	$(CXX) -c $(CFLAGS) -o $@ $<

# files in LIBDIR
$(OBJDIR)/%.o : $(SRCDIR)/%.c
	$(CC) -c $(CFLAGS) -o $@ $<

$(OBJDIR)/%.o : $(SRCDIR)/%.cpp
	$(CXX) -c $(CFLAGS) -o $@ $<


$(LIBNEZ) : $(OBJDIR) $(LIBOBJS)
	$(AR) rcs $@ $(LIBOBJS)

#
# Make object directory
#

$(OBJDIR) :
	mkdir -p $(OBJDIR)
	mkdir -p $(addprefix $(OBJDIR)/,$(LIBDIRS))
	mkdir -p $(addprefix $(OBJDIR)/,$(DIRS))


$(TARGET) : $(OBJDIR) $(LIBNEZ) $(OBJS)
	$(CXX) -o $@ $(LFLAGS) $(OBJS) $(LIBNEZ) $(LIBS)

$(TARGET_S) : $(OBJDIR) $(LIBNEZ) $(OBJS)
	$(CXX) -o $@ $(LFLAGS) $(OBJS) $(LIBNEZ)  $(SLIBS)

clean :
	rm -f $(ZIPBIN)
	rm -rf $(OBJDIR)
	rm -f $(TARGET)
	
zipclean :
	rm -f $(ZIPBIN)

zip : strip
	rm -f $(ZIPBIN)
	zip $(ZIPBIN) $(TARGET) $(RELEASE_FILES) 
	zip -j $(ZIPBIN) $(SDL_DLL)

strip : $(TARGET)
	strip $(TARGET)

release :  zip
