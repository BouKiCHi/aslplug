#
# Makefile
#
NAME = scci

ifeq ($(OSX),1)
WIN32=0 
else
WIN32=1
endif

ifeq ($(WIN32),1)
include Makefile.w32_cross
else
include Makefile.unix
endif

ZIPBIN = sccisim_$(shell date +"%y%m%d")$(OSSFX).zip


#
# common
#

USE_OPL3 = 1
USE_OPM = 1
USE_FMGEN = 1

#
#
#

LIBNAME = librend.a

#
# directories
#

EXTTOP = ..
SRCTOP = ../src

FMTDIR = format
DEVDIR = device
NESDIR = device/nes
OPLDIR = device/opl
LIBDIRS = $(FMTDIR) $(DEVDIR) $(YMDIR) $(OPLDIR) $(MAINDIR)

GLUEDIR = glue
LOGDIR = log
EXTDIRS += $(LOGDIR) $(GLUEDIR)

OTHERDIR = other
DRVW32DIR = $(OTHERDIR)/snddrv/win32
DRVDIR = $(OTHERDIR)/snddrv
EXTDIRS += $(DRVW32DIR) $(DRVDIR) $(OTHERDIR)

FMTSRCS = \
 audiosys.c handler.c songinfo.c 

DEVSRCS = \
 s_logtbl.c s_psg.c
  
OPLSRCS = \
 s_deltat.c s_opl.c s_opltbl.c

MAINSRCS = \
 sccisim.cpp render.c io_dummy.c
 
LOGSRCS = \
 nlg.c s98x.c log.c

DRVSRCS = \
 snddrv.c

DRVW32SRCS = \
 snddrvwo.c snddrvds.c

LIBSRCS = \
       $(addprefix $(FMTDIR)/,$(FMTSRCS)) \
       $(addprefix $(DEVDIR)/,$(DEVSRCS)) \
       $(addprefix $(NESDIR)/,$(NESSRCS)) \
       $(addprefix $(OPLDIR)/,$(OPLSRCS)) \


SRCS = $(addprefix $(LOGDIR)/,$(LOGSRCS)) \
       $(addprefix $(DRVDIR)/,$(DRVSRCS)) \
       $(addprefix $(DRVW32DIR)/,$(DRVW32SRCS))

SRCS += $(MAINSRCS)

#### RCDRV

CFLAGS += -DUSE_RCDRV -DDLLMAKE

RCDIR = rcdrv

ifeq ($(WIN32),1)
RCSRCS = \
  rcdrv.cpp
else
RCSRCS = \
  rcdrv_dummy.cpp
endif

EXTDIRS += $(RCDIR)
SRCS += $(addprefix $(RCDIR)/,$(RCSRCS)) 

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

DEVSRCS += s_opm_gen.cpp s_opna_gen.cpp
CFLAGS += -DUSE_FMGEN
GENDIR = device/fmgen
GENSRCS = fmgen.cpp fmtimer.cpp opm.cpp opna.cpp psg.cpp file.cpp
LIBDIRS += $(GENDIR)
LIBSRCS += $(addprefix $(GENDIR)/,$(GENSRCS))

endif

#
#
#

# $(info source $(SRCS))
# $(info objs $(OBJS))


LIBOBJS = $(addprefix $(OBJDIR)/,$(addsuffix .o,$(basename $(LIBSRCS))))
OBJS = $(addprefix $(OBJDIR)/,$(addsuffix .o,$(basename $(SRCS))))

LIBARC := $(OBJDIR)/$(LIBNAME)

CFLAGS += -I$(SRCTOP) -I$(EXTTOP)
CFLAGS += $(addprefix -I$(SRCTOP)/,$(LIBDIRS))
CFLAGS += $(addprefix -I$(EXTTOP)/,$(EXTDIRS))
CFLAGS += -I. 

ifdef DEBUG
CFLAGS += -O0 -g
else
CFLAGS += -O3
endif

# DTESTOBJS = $(OBJDIR)/dtest.o

#
#
#

dynamic : $(TARGET)

static : $(TARGET_S)

test : $(TEST)

lib : $(LIBARC)

#
# Rules
#

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

$(LIBARC) : $(OBJDIR) $(LIBOBJS)
	$(AR) rcs $@ $(LIBOBJS)

$(OBJDIR) :
	mkdir -p $(OBJDIR)
	mkdir -p $(addprefix $(OBJDIR)/,$(LIBDIRS))
	mkdir -p $(addprefix $(OBJDIR)/,$(EXTDIRS))

#
# target
#

$(TARGET) : $(OBJDIR) $(LIBARC) $(OBJS) 
	$(CXX) -static -shared $(OBJS) $(LIBARC) -o $@ -lwinmm -ldsound -ldxguid

# $(TEST) : $(TARGET) $(DTESTOBJS)
#	$(CXX) $(CFLAGS) -mconsole -o $(TEST) $(DTESTOBJS) $(OBJS) $(LIBARC)  -lwinmm -ldsound -ldxguid

clean :
	rm -rf $(OBJDIR)
	rm -f $(ZIPBIN)
	rm -f $(TARGET)
#	rm -f $(TEST)

strip : $(TARGET)
	strip $(TARGET)

zip : strip
	zip $(ZIPBIN) $(TARGET) README.TXT	

release : zip
