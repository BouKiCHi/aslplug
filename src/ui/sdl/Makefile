#
# Makefile
#

NAME = nezplay_asl

ifeq ($(OS),Windows_NT)
WIN32=1
endif

ifeq ($(WIN32),1)
include Makefile.w32_cross
else
include Makefile.unix
endif

#
# common
#

USE_OPL3 = 1
USE_OPM = 1
USE_FMGEN = 1

#
#
#

LIBNEZ = libnez.a

LIBS = $(SDL_LIBS)
SLIBS = $(SDL_SLIBS)
CFLAGS += $(SDL_CFLAGS)

SRCDIR = ../..

FMTDIR    = format
DEVDIR    = device
NESDIR    = device/nes
OPLDIR    = device/opl

CPUDIR = cpu

KM6502DIR = cpu/km6502
KMZ80DIR  = cpu/kmz80

MAINDIR   = .
LOGDIR = ui/sdl/log

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

MAINSRCS = \
sdlplay.c glue2.c

LOGSRCS = \
nlg.c s98x.c log.c


DIRS = $(FMTDIR) $(KMZ80DIR) $(KM6502DIR)
DIRS += $(DEVDIR) $(CPUDIR) $(NESDIR) $(OPLDIR) $(LOGDIR)


LIBSRCS = \
$(addprefix $(FMTDIR)/,$(FMTSRCS)) \
$(addprefix $(KMZ80DIR)/,$(KMZ80SRCS)) \
$(addprefix $(DEVDIR)/,$(DEVSRCS)) \
$(addprefix $(NESDIR)/,$(NESSRCS)) \
$(addprefix $(OPLDIR)/,$(OPLSRCS)) \
$(addprefix $(LOGDIR)/,$(LOGSRCS)) \


####  OPL3
ifeq ($(USE_OPL3),1)

DEVSRCS += s_opl4.c
CFLAGS += -DUSE_OPL3
OPL3DIR = device/mame
OPL3SRCS = ymf262.c
DIRS += $(OPL3DIR)
LIBSRCS += $(addprefix $(OPL3DIR)/,$(OPL3SRCS))

endif

#### OPM
ifeq ($(USE_OPM),1)

DEVSRCS += s_opm.c
CFLAGS += -DUSE_OPM
OPMDIR = device/ym2151
OPMSRCS = ym2151.c
DIRS += $(OPMDIR)
LIBSRCS += $(addprefix $(OPMDIR)/,$(OPMSRCS))

endif

#### FMGEN
ifeq ($(USE_FMGEN),1)

DEVSRCS += s_opm_gen.cpp
CFLAGS += -DUSE_FMGEN
GENDIR = device/fmgen
GENSRCS = fmgen.cpp fmtimer.cpp opm.cpp file.cpp
DIRS += $(GENDIR)
LIBSRCS += $(addprefix $(GENDIR)/,$(GENSRCS))

endif


MAINSRCFILES = $(MAINSRCS)

SRCS =  \
$(addprefix $(MAINDIR)/,$(MAINSRCS)) \

LIBOBJS = $(addprefix $(OBJDIR)/,$(addsuffix .o,$(basename $(LIBSRCS))))
OBJS = $(addprefix $(OBJDIR)/,$(MAINSRCFILES:.c=.o ))

LIBNEZ := $(OBJDIR)/$(LIBNEZ)

CFLAGS += $(addprefix -I$(SRCDIR)/,$(DIRS))
CFLAGS += -I$(SRCDIR)
CFLAGS += -I.

#
#
#

dynamic : $(TARGET)

static : $(TARGET_S)

lib : $(LIBNEZ)

release : $(TARGET) $(TARGET_S)
	$(STRIP) $(TARGET)
	$(STRIP) $(TARGET_S)

#
# Rules
#

$(OBJDIR)/%.o : %.c
	$(CC) -c $(CFLAGS) -o $@ $<

$(OBJDIR)/%.o : $(SRCDIR)/%.c
	$(CC) -c $(CFLAGS) -o $@ $<

$(OBJDIR)/%.o : $(SRCDIR)/%.cpp
	$(CXX) -c $(CFLAGS) -o $@ $<


$(LIBNEZ) : $(OBJDIR) $(LIBOBJS)
	$(AR) rcs $@ $(LIBOBJS)

$(OBJDIR) :
	mkdir -p $(OBJDIR)
	mkdir -p $(addprefix $(OBJDIR)/,$(DIRS))


$(TARGET) : $(OBJDIR) $(LIBNEZ) $(OBJS)
	$(CXX) $(LFLAGS) $(OBJS) $(LIBNEZ) -o $@ $(LIBS)

$(TARGET_S) : $(OBJDIR) $(LIBNEZ) $(OBJS)
	$(CXX) $(LFLAGS) $(OBJS) $(LIBNEZ) -o $@ $(SLIBS)

clean :
	rm -rf $(OBJS)
	rm -rf $(OBJDIR)
	rm -f $(TARGET)
	rm -f $(TARGET_S)
