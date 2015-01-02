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

