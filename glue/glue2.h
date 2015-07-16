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

// 初期化
void glue2_init(void);
// 終了
void glue2_free(void);


// メモリ開放
void glue2_mem_free(void);

// 実行パスの設定
void glue2_set_exec_path(const char *path);

// ドライバパスの設定
void glue2_set_driver_path(const char *path);


// 拡張子をextに変更する
// ext = ".abc"
void glue2_change_ext(char *dest, const char *file, const char *ext);

// バイナリ出力生成
int glue2_make_binary(const char *infile, const char *outfile);

// フェードアウト開始
void glue2_start_fade(void);

// フェード確認
int glue2_is_fade_running(void);

// フェード終了確認
int glue2_is_fade_end(void);


#ifndef NOUSE_NEZ

#include "nezplug.h"

// サンプル生成 / len = フレーム数
void glue2_make_samples(short *buf, int len);

// ファイル読み出し
int glue2_load_file(const char *file, int track, struct glue2_setting *gs);


// 曲番号取得
int glue2_get_songno(int track);

// 曲番号設定
void glue2_set_songno(int track, int songno);


// ポーズ
void glue2_pause(int track, int flag);

// リセット
void glue2_reset(int track);

// ファイルクローズ
void glue2_close(int track);

#endif

#endif
