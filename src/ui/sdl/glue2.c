//
//  glue2.c
//
//  Copyright (c) 2015 BouKiCHi. All rights reserved.
//

#include "glue2.h"

// for stat
#include <sys/stat.h>
#include "nezplug.h"

typedef unsigned char byte;
typedef unsigned short word;


#ifndef _WIN32
#define PATH_SEP '/'
#else
#define PATH_SEP '\\'
#endif


byte *glue_mem_ptr = NULL;
char glue_exec_path[1024];

// ファイルサイズの取得
long glue2_get_filesize(const char *file)
{
    struct stat st;
    
    if (stat(file, &st) < 0)
        return -1;
    
    return (long)st.st_size;
}

// イメージ用メモリの開放
void glue2_mem_free()
{
    if (glue_mem_ptr)
    {
        free(glue_mem_ptr);
        glue_mem_ptr = NULL;
    }
}

// 通常イメージの読み出し
int glue2_read_normal_image(const char *file)
{
    long size = glue2_get_filesize(file);
    
    if (size < 0)
        return (int)size;
    
    glue_mem_ptr = malloc(size);
    
    FILE *fp = fopen(file, "rb");
    
    fread(glue_mem_ptr, size, 1, fp);
    
    fclose(fp);
    
    return (int)size;
}


// メモリ書き込み(16bit LE)
void glue2_write_word(byte *p, word v)
{
    p[0] = v & 0xff;
    p[1] = (v>>8) & 0xff;
}

// メモリ書き込み(8bit LE)
void glue2_write_byte(byte *p, byte v)
{
    p[0] = v & 0xff;
}


// KSSヘッダを作る
void glue2_make_kss_header(byte *mem,
                    int load_addr, int data_size, int init_addr, int play_addr,
                    int start_bank, int bank_size, int ext_chip, int ext2)
{
    memset(mem, 0, 0x10);
    memcpy(mem, "KSCC", 0x04);
    glue2_write_word(mem + 0x04, load_addr);
    glue2_write_word(mem + 0x06, data_size);
    glue2_write_word(mem + 0x08, init_addr);
    glue2_write_word(mem + 0x0a, play_addr);
    glue2_write_byte(mem + 0x0c, start_bank);
    glue2_write_byte(mem + 0x0d, bank_size);
    glue2_write_byte(mem + 0x0e, ext2);
    glue2_write_byte(mem + 0x0f, ext_chip);
}

// ファイルの読み出し
int glue2_read_file(byte *mem, long size, const char *file)
{
    FILE *fp = fopen(file, "rb");
    
    if (!fp)
        return -1;
    
    fread(mem, size, 1, fp);

    fclose(fp);
    
    return 0;
}

// パスを作成する
void glue2_make_path(char *dest, const char *dir,const char *name)
{
    strcpy(dest, dir);
    char *sep = strrchr(dest, PATH_SEP);
    
    if (sep)
        *sep = 0;
    else
        sep = dest + strlen(dest);
    
    sprintf(sep,"%c%s", PATH_SEP, name);
}


// 曲ファイルの読み出し
int glue2_read_mdr_song(const char *file)
{
    char *glue_moon_bin = "moon.bin";
    char drv_path[1024];
    
    // 曲ファイルと同じディレクトリのドライバを開く
    glue2_make_path(drv_path, file, glue_moon_bin);
    long drv_size = glue2_get_filesize(drv_path);
    
    // 無ければワーキングディレクトリのドライバを開く
    if (drv_size < 0)
    {
        strcpy(drv_path, glue_moon_bin);
        drv_size = glue2_get_filesize(drv_path);
    }

    long song_size = glue2_get_filesize(file);
    
    if (song_size < 0 || drv_size < 0)
        return -1;
    
    // header_size = 0x10
    long size = 0x10 + drv_size + song_size;
    long song_banks = (song_size / 0x4000);
    
    // サイズ切り上げ
    if (song_size & 0x3fff)
        song_banks++;
    
    
    
    glue_mem_ptr = malloc(size);
    memset(glue_mem_ptr, 0, size);
    
    // ヘッダ作成
    glue2_make_kss_header(glue_mem_ptr,
                          0x4000,
                          (int)drv_size,
                          0x4000,
                          0x4003,
                          0x04,
                          (int)song_banks,
                          0x20,
                          0x81
                          );

    // ファイル読み込み
    glue2_read_file(glue_mem_ptr + 0x10, drv_size, drv_path);
    glue2_read_file(glue_mem_ptr + 0x10 + drv_size, song_size, file);
    
    return (int)size;
}

// ファイルの読み込み
int glue2_load_file(NEZ_PLAY *ctx, const char *file, int freq, int ch, int vol, int songno)
{
    // TODO:ファイル拡張子の確認
    // イメージの作成
    
    glue2_mem_free();
    
    long size = 0;
    
    // 拡張子の検出
    char *ext = strrchr(file,'.');
    
    if (ext && strcasecmp(ext,".mdr") == 0)
    {
        // MDRファイル
        size = glue2_read_mdr_song(file);
    }
    else
    {
        // 通常イメージ
        size = glue2_read_normal_image(file);
        
    }
    
    if (size < 0)
        return -1;
    
    NEZLoad(ctx, glue_mem_ptr, (Uint)size);
    
    NEZSetFrequency(ctx, freq);
    NEZSetChannel(ctx, ch);
    
    if (songno >= 0)
        NEZSetSongNo(ctx, songno);
    
    NEZReset(ctx);
    
    return 0;
}