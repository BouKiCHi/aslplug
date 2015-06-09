//
//  glue2.h
//  nezplay_asl
//
//  Copyright (c) 2015 BouKiCHi. All rights reserved.
//

#ifndef __glue2__
#define __glue2__

#include <stdio.h>

void glue2_mem_free();
void glue2_set_exec_path(const char *path);
void glue2_set_driver_path(const char *path);

void glue2_change_ext(char *dest, const char *file, const char *ext);
int glue2_make_binary(const char *infile, const char *outfile);


#ifndef NOUSE_NEZ
#include "nezplug.h"
int glue2_load_file(NEZ_PLAY *ctx, const char *file, int freq, int ch, int vol, int songno);

#endif

#endif
