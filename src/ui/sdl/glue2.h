//
//  glue2.h
//  nezplay_asl
//
//  Copyright (c) 2015 BouKiCHi. All rights reserved.
//

#ifndef __glue2__
#define __glue2__

#include <stdio.h>

struct glue2_setting
{
    int track_id;

    int songno;
    
    int freq;
    int ch;
    float vol;
    
    int turbo;
    int use_fmgen;
    
    int use_gmc;
    void *log_ctx;
};

void glue2_init(void);
void glue2_free(void);

void glue2_mem_free(void);
void glue2_set_exec_path(const char *path);
void glue2_set_driver_path(const char *path);

void glue2_change_ext(char *dest, const char *file, const char *ext);
int glue2_make_binary(const char *infile, const char *outfile);

// フェードアウト開始
void glue2_start_fade(void);

// サンプル生成
void glue2_make_samples(short *buf, int len);

#ifndef NOUSE_NEZ

#include "nezplug.h"

int glue2_load_file(const char *file, int track, struct glue2_setting *gs);
void glue2_close(int track);

#endif

#endif
