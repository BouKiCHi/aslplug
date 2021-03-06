//
//  glue2.c
//
//  Copyright (c) 2015 BouKiCHi. All rights reserved.
//
//  supported :
//  NRD/MDR/MGS
//  NSF/KSS/GBR

#include "glue2.h"

// for stat
#include <sys/stat.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef unsigned char byte;
typedef unsigned short word;

#ifndef PATH_MAX
#define PATH_MAX 2048
#endif

#include "fade.h"

#define MAX_GLUE2_TRACK 2

// KSSNRT
unsigned char _kssnrt_bin[]= {
  0x4b,0x53,0x43,0x43,0x00,0x01,0x41,0x00,
  0x14,0x01,0x3e,0x01,0x00,0x00,0x00,0x20,
  0x4e,0x52,0x54,0x4c,0x44,0x52,0x00,0x00,
  0xc3,0x00,0x18,0xc3,0x03,0x18,0xc3,0x0f,
  0x18,0xc3,0x12,0x18,0xf3,0xed,0x56,0x32,
  0x3f,0x01,0x3e,0x76,0x32,0x38,0x00,0xaf,
  0x32,0x40,0x01,0xcd,0x08,0x01,0xed,0x5e,
  0xaf,0xed,0x47,0x3a,0x3f,0x01,0xfe,0x7f,
  0x28,0x06,0xaf,0xcd,0x11,0x01,0x18,0x04,
  0xaf,0xcd,0x0e,0x01,0xfb,0xc9,0xc9,0x00,
  0x00
};

int _kssnrt_len = 81;


#define PCM_BLOCK_SIZE 8192

#ifdef NOUSE_NEZ
#define NEZ_PLAY void
#endif


struct struct_glue2_main {
  NEZ_PLAY *ctx[MAX_GLUE2_TRACK];

  int pause[MAX_GLUE2_TRACK];
  struct glue2_setting setting[MAX_GLUE2_TRACK];

  short mixbuf[PCM_BLOCK_SIZE];

  int driver_version;
} g2;


// メモリポインタ
byte *glue_mem_ptr = NULL;
//　実行ファイルパス
char glue_exec_path[PATH_MAX] = "";
// ドライバパス
char glue_driver_path[PATH_MAX] = "";
// ダンプパス
char glue_dump_path[PATH_MAX] = "";


// Androidでログを出力する
#if !defined(ANDROID)

#include <stdarg.h>

void output_log(const char *format, ...) {
  char buf[2048];
  va_list arg;

  va_start(arg, format);
  vsprintf(buf, format, arg);
  va_end(arg);

  printf("%s:%s\n", "nezjni", buf);
}

#endif


// 初期化
void glue2_init(void) {
  memset(&g2, sizeof(g2), 0);
}

// 終了
void glue2_free(void) {
  int i;

  for(i = 0; i < MAX_GLUE2_TRACK; i++) {
#ifndef NOUSE_NEZ
    glue2_close(i);
#endif
  }
}

// ファイルサイズの取得
long glue2_get_filesize(const char *file) {
    struct stat st;

    if (stat(file, &st) < 0)
        return -1;

    return (long)st.st_size;
}

// イメージ用メモリの開放
void glue2_mem_free(void) {
    if (glue_mem_ptr) {
        free(glue_mem_ptr);
        glue_mem_ptr = NULL;
    }
}

// 通常イメージの読み出し
int glue2_read_normal_image(const char *file)
{
    long size = glue2_get_filesize(file);

    if (size < 0) return (int)size;

    glue_mem_ptr = malloc(size);

    FILE *fp = fopen(file, "rb");

    fread(glue_mem_ptr, size, 1, fp);

    fclose(fp);

    return (int)size;
}


// メモリ書き込み(16bit LE)
void glue2_write_word(byte *p, word v) {
    p[0] = v & 0xff;
    p[1] = (v>>8) & 0xff;
}

// メモリ書き込み(8bit LE)
void glue2_write_byte(byte *p, byte v) {
    p[0] = v & 0xff;
}

byte glue2_read_byte(byte *p) {
    return p[0];
}

// KSSヘッダを作る
void glue2_make_kss_header(
        byte *mem,
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


// ファイルの読み出し(posまでスキップ)
int glue2_read_file_pos(byte *mem, long size, const char *file, int pos) {
    FILE *fp = fopen(file, "rb");

    if (!fp) return -1;

    if (pos > 0) {
        fseek(fp, pos, SEEK_SET);
        size -= pos;
    }

    fread(mem, size, 1, fp);
    fclose(fp);
    return 0;
}


// ファイルの読み出し
int glue2_read_file(byte *mem, long size, const char *file) {
    return glue2_read_file_pos(mem, size, file, 0);
}


// ファイルの書き出し
int glue2_write_file(byte *mem, long size, const char *file) {
    FILE *fp = fopen(file, "wb");

    if (!fp) return -1;

    fwrite(mem, size, 1, fp);

    fclose(fp);

    return 0;
}


// セパレート文字を検出する
char *glue2_get_seppos(char *dest) {
    int sep_chr = PATH_SEP_W32;
    char *sep = strrchr(dest, sep_chr);

    if (!sep) {
        sep_chr = PATH_SEP;
        sep = strrchr(dest, sep_chr);
    }

    return sep;
}


// ファイル名を拡張子を変更してコピーする
void glue2_change_ext(char *dest, const char *file, const char *ext) {
    strcpy(dest, file);
    char *sep = strrchr(dest, '.');

    // セパレート文字がない場合は拡張子を追加する
    if (!sep) strcat(dest, ext); else strcpy(sep, ext); 
}


// ファイル名からパスを作成する
void glue2_make_path(char *dest, const char *dir, const char *name) {
    strcpy(dest, dir);

    char *sep = glue2_get_seppos(dest);

    // セパレート文字がない場合は文字列にセパレートと名前を追加する
  if (!sep) {
    sep = dest + strlen(dest);
      sprintf(sep,"%c%s", PATH_DEFSEP, name);
  } else {
    sprintf(sep + 1,"%s", name);
  }
}

// ディレクトリ名からパスを作成する
void glue2_make_dirpath(char *dest, const char *dir,const char *name) {
    strcpy(dest, dir);
    char *sep = glue2_get_seppos(dest);

    int sep_chr = PATH_DEFSEP;

    if (sep)
        sep_chr = sep[0];

    sep = dest + strlen(dest);
    sprintf(sep,"%c%s", sep_chr, name);
}


// 実行ファイルのパスをコピーする
void glue2_set_exec_path(const char *path) {
  strcpy(glue_exec_path, path);
}

// ドライバパスをコピーする
void glue2_set_driver_path(const char *path) {
  strcpy(glue_driver_path, path);
}

// ダンプパスをコピーする
void glue2_set_dump_path(const char *path) {
  strcpy(glue_dump_path, path);
}

// ファイルサイズの取得とパスの作成
long glue2_getpath_min(char *path, const char *dir, const char *name, int min_size) {
    long size = -1;

    // 曲ファイルと同じディレクトリのドライバを開く
    glue2_make_path(path, dir, name);
    size = glue2_get_filesize(path);

    // サイズがmin_size以上ならドライバとみなす
    if (size >= min_size)
        return size;

    // 実行ファイルと同じディレクトリのドライバを開く
    if (glue_exec_path[0]) {
        glue2_make_path(path, glue_exec_path, name);
        size = glue2_get_filesize(path);

        // サイズがmin_size以上ならドライバとみなす
        if (size >= min_size)
            return size;
    }

    // ドライバパスのドライバを開く
    if (glue_driver_path[0]) {
        glue2_make_dirpath(path, glue_driver_path, name);
        size = glue2_get_filesize(path);

        // サイズがmin_size以上ならドライバとみなす
        if (size >= min_size)
            return size;
    }

    // カレントディレクトリから読み込む
    strcpy(path, name);
    size = glue2_get_filesize(path);

    return size;
}


//
long glue2_getpath(char *path, const char *dir, const char *name) {
  return glue2_getpath_min(path, dir, name, 0);
}


// 曲ファイルの読み出し
int glue2_read_mdr_song(const char *file) {
    char *moon_bin = "moon.bin";
    char drv_path[1024];

    // ドライバファイルパスとサイズの取得
    long drv_size = glue2_getpath_min(drv_path, file, moon_bin, 512);

    // 曲ファイルのサイズを取得
    long song_size = glue2_get_filesize(file);

    // printf("drv_size:%ld\n", drv_size);

    if (song_size < 0 || drv_size < 0)
        return -1;

    // header_size = 0x10
    long size = 0x10 + drv_size + song_size;
    long song_banks = (song_size / 0x4000);

    // サイズ切り上げ
    if (song_size & 0x3fff)
        song_banks++;

    // メモリの確保
    glue_mem_ptr = malloc(size);
    memset(glue_mem_ptr, 0, size);

    // ヘッダ作成
    glue2_make_kss_header(
        glue_mem_ptr,
        0x4000, // ロードアドレス
        (int)drv_size, // データサイズ
        0x4000, // 初期化アドレス
        0x4003, // 再生アドレス
        0x04, // 開始バンク
        (int)song_banks, // 終了バンク
        0x20, // EXT 拡張モード
        0x81 // EXT2
    );

    // ファイル読み込み
    glue2_read_file(glue_mem_ptr + 0x10, drv_size, drv_path);
    glue2_read_file(glue_mem_ptr + 0x10 + drv_size, song_size, file);

    return (int)size;
}

int glue2_read_nrtdrv_song(const char *file, int drv_type) {
    char drv_path[PATH_MAX];

    const char *driver_file = "NRTDRV.BIN";

    if (drv_type == 1) {
        driver_file = "MJRDRV.BIN";
    }

    // ドライバファイルパスとサイズの取得
    long drv_size = glue2_getpath(drv_path, file, driver_file);
  
    // ドライバサイズがおかしい
    if (drv_size < 0) {
        printf("Driver not found!:%s\n", driver_file);
        return -1;
    }

    // ヘッダサイズの取得
    long hdr_size = _kssnrt_len;

    // 曲ファイルのサイズを取得
    long song_size = glue2_get_filesize(file);

    // サイズがおかしい
    if (song_size < 0 || hdr_size < 0)
        return -1;

    // 0x4000 = ドライバメモリ, 0xF0 = ヘッダ開始アドレス
    long size = 0x4000 - 0xF0;
    size += song_size;

    // メモリ確保
    glue_mem_ptr = malloc(size);
    memset(glue_mem_ptr, 0, size);

    // ファイル読み込み
    memcpy(glue_mem_ptr, _kssnrt_bin, _kssnrt_len);
    glue2_read_file(glue_mem_ptr + (0x1800 - 0xF0), drv_size, drv_path);
    glue2_read_file(glue_mem_ptr + (0x4000 - 0xF0), song_size, file);

    // バージョン取得
    g2.driver_version = 0;
    if (drv_type == 0) {
        g2.driver_version = glue2_read_byte(glue_mem_ptr + (0x1831 - 0xF0));
        printf("%s Driver Version:%d\n", driver_file, g2.driver_version);
    }

    // ファイルサイズの修正
    glue2_write_word(glue_mem_ptr + 0x06, size);

    // 独自拡張モードの設定
    glue2_write_byte(glue_mem_ptr + 0x0e, 0x42); // EXT2
    glue2_write_byte(glue_mem_ptr + 0x0f, 0x20); // EXT

    return (int)size;
}

// NRDファイルの読み出し
int glue2_read_nrd_song(const char *file) {
    return glue2_read_nrtdrv_song(file, 0);
}

// BINファイルの読み出し
int glue2_read_bin_song(const char *file) {
    return glue2_read_nrtdrv_song(file, 1);
}


/*****************
 MGS
 ******************/

// from mgs2kss
static const unsigned char mgsdrv_init[0x100] =
{
    /* FORCE FMPAC RYTHM MUTE */
    0x3E,0x0E,      /* LD A,0EH */
    0xD3,0x7C,      /* OUT(C),A */
    0x3E,0x20,      /* LD A,020H */
    0xD3,0x7D,      /* OUT(C),A */

    /* INIT */
    0xCD,0x10,0x60, /* CALL 06010H */
    0x3E,0x01,      /* LD A,1 */
    0xDD,0x77,0x00, /* LD (IX+0), A */
    0xDD,0x77,0x01, /* LD (IX+1), A */

    /* SAVE POINTER TO WORK AREA */
    0xDD,0x22,0xF0,0x7F, /* LD (07FF0H),IX */

    /* FORCE ACTIVATE SCC(MEGAROM MODE) */
    0xAF,           /* XOR A */
    0x32, 0xFE,0xBF,/* LD (0BFFEH), A */
    0x3E, 0x3F,     /* LD A, 03FH */
    0x32, 0x00,0x90,/* LD (09000H), A */

    /* PLAY */
    0x11,0x00,0x02, /* LD DE,0200H */
    0x06,0xFF,      /* LD B,0FFH */
    0x60,           /* LD H,B */
    0x68,           /* LD L,B */
    0xCD,0x16,0x60, /* CALL 06016H */

    /* INIT FLAG */
    0x3E,0x7F,      /* LD  A,07FH */
    0xD3,0x40,      /* OUT (040H),A ; SEL EXT I/O */
    0xAF,           /* XOR A */
    0xD3,0x41,      /* OUT (041H),A ; STOP FLAG  */
    0xD3,0x42,      /* OUT (042H),A ; LOOP COUNT */
    0xD3,0x43,
    0xD3,0x44,

    0xC9            /* RET */
} ;

// from mgs2kss
static const unsigned char mgsdrv_play[0x100] =
{
    0xCD, 0x1F, 0x60,       /* CALL 0601FH */
    0xDD, 0x2A, 0xF0, 0x7F, /* LD IX,(07FF0H) */
    0xDD, 0x7E, 0x08,       /* LD A,(IX+8) ; */
    0xFE, 0x00,             /* CP A,0 */
    0x20, 0x04,             /* JR NZ,+04 */
    0x3E, 0x01,
    0xD3, 0x41,             /* OUT (041H),A */
    0xDD, 0x7E, 0x05,       /* LD A,(IX+5) ; LOOP COUNTER */
    0xD3, 0x42,             /* OUT (042H),A */
    0xDD, 0x7E, 10,         /* LD A,(IX+10) ; @m ADDRESS(L) */
    0xD3, 0x43,             /* OUT (043H),A */
    0xDD, 0x7E, 11,         /* LD A,(IX+11) ; @m ADDRESS(H) */
    0xD3, 0x44,             /* OUT (044H),A */
    0xAF,                   /* XOR A */
    0xDD,0x77,10,           /* LD (IX+10), A */
    0xDD,0x77,11,           /* LD (IX+11), A */
    0xC9                    /* RET */
} ;


// MGSを結合する
int glue2_read_mgs_song(const char *file)
{

    char *drv_name = "MGSDRV.COM";
    char drv_path[PATH_MAX];

    // ドライバファイルパスとサイズの取得
    long drv_size = glue2_getpath(drv_path, file, drv_name);
  
    // ドライバサイズがおかしい
    if (drv_size < 0) {
        printf("Driver not found!:%s\n", drv_name);
        return -1;
    }

    // 曲ファイルのサイズを取得
    long song_size = glue2_get_filesize(file);

    // サイズがおかしい
    if (song_size < 0 || drv_size < 0)
        return -1;

    // ドライバの先頭 0x6000
    // 初期化ルーチンの先頭 0x8000
    // 初期化ルーチンのサイズ 0x200
    // 0xF0 = ヘッダ開始アドレス
    long size = 0x8000 + 0x200 - 0xF0;

    // メモリ確保
    glue_mem_ptr = malloc(size);
    memset(glue_mem_ptr, 0, size);

    // ヘッダ作成
    glue2_make_kss_header(
      glue_mem_ptr,
      0x100, // ロードアドレス
      (int)size, // データサイズ
      0x8000, // 初期化アドレス
      0x8100, // 再生アドレス
      0x00, // 開始バンク
      0x00, // 終了バンク
      0x01, // EXT 拡張モード OPLL
      0x00 // EXT2
    );

    // ファイル読み込み
    // MGSDRVは0x0dだけスキップする
    glue2_read_file_pos(glue_mem_ptr + (0x6000 - 0xF0), drv_size, drv_path, 0x0D);
    glue2_read_file(glue_mem_ptr + (0x200 - 0xF0), song_size, file);

    // ルーチンのコピー
    memcpy(glue_mem_ptr + (0x8000 - 0xF0), mgsdrv_init, sizeof(mgsdrv_init));
    memcpy(glue_mem_ptr + (0x8100 - 0xF0), mgsdrv_play, sizeof(mgsdrv_play));

    return (int)size;
}

/*****************
 BGM 
 ******************/

// from libkss
static unsigned char kinrou_init[0x100] = {
    0xCD, 0x20, 0x60,       /* CALL	6020H */
    0x3E, 0x3F,             /* LD	A,3FH */
    0x32, 0x10, 0x60,       /* LD	(6010H),A	; OPLL SLOT */
    0x32, 0x11, 0x60,       /* LD	(6011H),A	; SCC SLOT */
    0x21, 0x07, 0x80,       /* LD HL,A007H ; DATA ADDRESS */
    0xED, 0x5B, 0x01, 0x80, /* LD	DE,(A001H) ; COMPILE ADDRESS */
    /* INIT FLAG WORK */
    0x3E, 0x00,             /* LD	A,0 ; LOOP NUMBER */
    0xCD, 0x26, 0x60,       /* CALL	6026H */
    0xAF, 0xCD, 0x38, 0x60, /* CALL	6038H	*/

    0x3E, 0x7F, /* LD  A,0FFH */
    0xD3, 0x40, /* OUT (040H),A */
    0xAF,       /* XOR A */
    0xD3, 0x41, /* OUT (041H),A ; PLAY FLAG  */
    0xD3, 0x42, /* OUT (042H),A ; LOOP COUNT */

    0xC9 /* RET */
};

static unsigned char kinrou_play[0x100] = {
    0xCD, 0x29, 0x60, /* CALL PLAY */
    0xCD, 0x32, 0x60, /* CALL PLAYCHK */
    0xE6, 0x01,       /* AND 01H */
    0xAF,             /* XOR A */
    0xD3, 0x41,       /* OUT (041H),A ; STOP FLAG */
    0x2B,             /* DEC HL */
    0x7D,             /* LD A,L */
    0xD3, 0x42,       /* OUT (042H),A ; LOOP COUNT */
    0xC9              /* RET */
};


// BGMを結合する
int glue2_read_bgm_song(const char *file)
{
    char *drv_name = "KINROU5.DRV";
    char drv_path[PATH_MAX];

    // ドライバファイルパスとサイズの取得
    long drv_size = glue2_getpath(drv_path, file, drv_name);
  
    // ドライバサイズがおかしい
    if (drv_size < 0) {
        printf("Driver not found!:%s\n", drv_name);
        return -1;
    }

    // 曲ファイルのサイズを取得
    long song_size = glue2_get_filesize(file);

    // サイズがおかしい
    if (song_size < 0 || drv_size < 0) return -1;

    // ロードアドレス 0x6000
    // ドライバの先頭 0x6000
    // 曲データの先頭 0x8000
    long header_size = 0x10;
    long load_address = 0x6000;
    int songdata_address = 0x8000;

    long size = header_size + (songdata_address - load_address) + song_size;
    int init_address = 0x7E00;
    int play_address = 0x7F00;

    // メモリ確保
    glue_mem_ptr = malloc(size);
    memset(glue_mem_ptr, 0, size);

    // ヘッダ作成
    glue2_make_kss_header(
      glue_mem_ptr,
      load_address, // ロードアドレス
      (int)size, // データサイズ
      init_address, // 初期化アドレス
      play_address, // 再生アドレス
      0x00, // 開始バンク
      0x00, // 終了バンク
      0x01, // EXT 拡張モード OPLL
      0x00 // EXT2
    );

    // ファイル読み込み
    // KINROU5は0x07だけスキップする
    glue2_read_file_pos(glue_mem_ptr + header_size, drv_size, drv_path, 0x07);
    glue2_read_file(glue_mem_ptr + header_size + (songdata_address - load_address), song_size, file);

    // ルーチンのコピー
    memcpy(glue_mem_ptr + header_size + (init_address - load_address), kinrou_init, sizeof(kinrou_init));
    memcpy(glue_mem_ptr + header_size + (play_address - load_address), kinrou_play, sizeof(kinrou_play));

    return (int)size;
}



//////////////////
// ファイルの読み込み
long glue2_load_file_body(const char *file) {
    // イメージの作成
    glue2_mem_free();

    long size = 0;

    // 拡張子の検出
    char *ext = strrchr(file, '.');

    // 拡張子の確認
    if (ext) {
        // MDRファイルを読み出す
        if (strcasecmp(ext, ".mdr") == 0) {
            size = glue2_read_mdr_song(file);
        } else
        // NRDファイルを読み出す
        if (strcasecmp(ext, ".nrd") == 0) {
            size = glue2_read_nrd_song(file);
        } else
        // BINファイルを読み出す
        if (strcasecmp(ext, ".bin") == 0) {
            size = glue2_read_bin_song(file);
        } else
        // BGMファイルを読み出す
        if (strcasecmp(ext, ".bgm") == 0) {
            size = glue2_read_bgm_song(file);
        } else
        // MGSファイルを読み出す
        if (strcasecmp(ext, ".mgs") == 0) {
            size = glue2_read_mgs_song(file);
        } else {
            // 通常イメージ
            size = glue2_read_normal_image(file);           
        }
    } else {
        // 通常イメージ
        size = glue2_read_normal_image(file);
    }

    // サイズが異常(エラー)
    if (size < 0) return -1;

    return size;
}

// バッファよりファイルの作成
int glue2_make_binary(const char *infile, const char *outfile) {
    long size = 0;

    size = glue2_load_file_body(infile);

    if (size < 0) return -1;

    glue2_write_file(glue_mem_ptr, size, outfile);
    return 0;
}


#if 0
// 音量増幅
static void glue2_audio_volume(short *data, int len, float volume)
{
    // stereo
    int i = 0;
    for (i = 0; i < len * 2; i++)
    {
        double s = 0;

        s = ((double)data[i]) * volume;

        if (s < -0x7fff)
            s = -0x7fff;
        if (s > 0x7fff)
            s = 0x7fff;

        data[i] = (short)s;
    }
}
#endif

// サンプル加算合成
static void glue2_audio_mix(short *dest, short *in, int len, float volume) {
    // stereo
    int i = 0;
    for (i = 0; i < len * 2; i++) {
        double s = 0;

        s = ((double)in[i] * volume);

        if (s < -0x7fff)
            s = -0x7fff;
        if (s > 0x7fff)
            s = 0x7fff;

        dest[i] += s;
    }
}


// フェード開始
void glue2_start_fade(void) {
    if (fade_is_running()) return;

    if (g2.ctx[0]) fade_start(g2.setting[0].freq, 3);
}

// フェード確認
int glue2_is_fade_running(void) {
    return fade_is_running();
}

// フェード終了確認
int glue2_is_fade_end(void) {
    return fade_is_end();
}


#ifndef NOUSE_NEZ

#include "nezplug.h"

// サンプル生成
void glue2_make_samples(short *buf, int len) {
    int i;

    // 初期化
    memset(buf, 0, len * 4);

    for(i = 0; i < MAX_GLUE2_TRACK; i++) {
        if (g2.ctx[i] && !g2.pause[i]) {
            NEZRender(g2.ctx[i], buf, len);
            glue2_audio_mix(buf, g2.mixbuf, len, g2.setting[i].vol);
        }
    }
    // フェード機能
    fade_stereo(buf, len);
}


// CPU使用率
double glue2_cpu_usage(void) {
  if (g2.ctx[0]) return NEZGetCPUUsage(g2.ctx[0]);

  return -1;
}


// ファイルの読み込み
int glue2_load_file(const char *file, int track, struct glue2_setting *gs)
{
    long size = 0;

    if (track >= MAX_GLUE2_TRACK) return -1;

    // NEZ本体を初期化する
    if (!g2.ctx[track]) {
        NEZ_PLAY *tmp_ctx = NEZNew();

        if (!tmp_ctx) return -1;

        g2.ctx[track] = tmp_ctx;
    }

    // 設定を保存する
    g2.setting[track].songno = gs->songno;
    g2.setting[track].freq = gs->freq;
    g2.setting[track].ch = gs->ch;
    g2.setting[track].vol = gs->vol;

    g2.setting[track].turbo = gs->turbo;
    g2.setting[track].use_fmgen = gs->use_fmgen;

    // コンテキストに一部の情報を受け渡す
    g2.ctx[track]->use_gmc = gs->use_gmc;
    g2.ctx[track]->log_ctx = gs->log_ctx;
    g2.ctx[track]->turbo = gs->turbo;
    g2.ctx[track]->use_fmgen = gs->use_fmgen;

    size = glue2_load_file_body(file);

    if (size < 0)
        return -1;

    // メモリをダンプする
    if (glue_dump_path[0]) {
      glue2_write_file(glue_mem_ptr, size, glue_dump_path);
    }

    gs->track_id = track;

    // NEZ本体にメモリなどを渡す
    NEZLoad(g2.ctx[track], glue_mem_ptr, (Uint)size);

    NEZSetFrequency(g2.ctx[track], gs->freq);
    NEZSetChannel(g2.ctx[track], gs->ch);

    if (gs->songno >= 0) NEZSetSongNo(g2.ctx[track], gs->songno);

    NEZReset(g2.ctx[track]);

    fade_init();

    // glue側のメモリを開放する
    glue2_mem_free();

    return 0;
}

// 曲番号取得
int glue2_get_songno(int track) {
    if (!g2.ctx[track]) return -1;

    return NEZGetSongNo(g2.ctx[track]);
}


// 曲番号設定
void glue2_set_songno(int track, int no) {
    if (!g2.ctx[track]) return;

    NEZSetSongNo(g2.ctx[track], no);
}

// 音量調整
void glue2_set_adjust_volume(int device, double volume) {
    if (!g2.ctx[0]) return;
    
    NEZAdjVolume(g2.ctx[0],device,volume);
}

// リセット
void glue2_reset(int track) {
    if (!g2.ctx[track]) return;

    NEZReset(g2.ctx[track]);
}

// ポーズ
void glue2_pause(int track, int flag) {
    g2.pause[track] = flag;
}

// 閉じる
void glue2_close(int track) {
    if (!g2.ctx[track]) return;

    NEZDelete(g2.ctx[track]);
    g2.ctx[track] = NULL;
}


#endif
