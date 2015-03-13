//
//  glue2.h
//  nezplay_asl
//
//  Copyright (c) 2015 BouKiCHi. All rights reserved.
//

#ifndef __glue2__
#define __glue2__

#include <stdio.h>
#include "nezplug.h"

void glue2_mem_free();
int glue2_load_file(NEZ_PLAY *ctx, const char *file, int freq, int ch, int vol, int songno);
void glue2_set_exec_path(const char *path);

#endif
