/***
 NLGSIM Copyright (c) 2014, BouKiCHi
 All rights reserved.

 Redistribution and use in source and binary forms, with or without
 modification, are permitted provided that the following conditions are met:

 1. Redistributions of source code must retain the above copyright notice,
 this list of conditions and the following disclaimer.

 THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND
 ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE LIABLE FOR
 ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 ***/


#include <stdio.h>
#include <getopt.h>
#include "SDL.h"

#include "nlg.h"
#include "s98x.h"
#include "log.h"

#include "render.h"
#include "rcdrv.h"
#include "audio.h"


#ifndef PATH_MAX
#define PATH_MAX 1024
#endif

#include "version.h"

Uint8 chmask[0x200];

struct {
  int debug;
  int song_length;
} player;

#define MAX_MAP 8

typedef struct {
  double clock_per_sample; // 1サンプルの生成で進むクロック
  double log_ticks; // クロックカウント

  int opll_count;
  int opm_count;
  int psg_count;

  int map[MAX_MAP];
  int volume[MAX_MAP];
  int logmap[MAX_MAP];

  char *outfile;

  NLGCTX *ctx;
  S98CTX *ctx_s98;
  LOGCTX *ctx_log;

  int log_time_us;
  int log_count_us;
  int log_mode;

  int rate;
  int rt_mode;
  int nullout;
  int devsel;
} NLG;

enum {
  DEV_NONE = 0,
  DEV_OPM1,
  DEV_OPM2,
  DEV_OPLL1,
  DEV_OPLL2,
  DEV_PSG1,
};

enum {
  NLG_PSG = 0,
  NLG_FM1,
  NLG_FM2,
  NLG_FM3,
};


// 時間表記を秒数に変換する
int get_length(const char *str) {
  if (strchr(str, ':') == NULL)
    return atoi(optarg);
  else {
    int min = 0, sec = 0;
    sscanf(str,"%d:%d", &min, &sec);
    return (min * 60) + sec;
  }
}


// 同期を出力
void audio_step_log(NLG *np) {
  if (!np->ctx_log) return;

  while(np->log_count_us >= np->log_time_us) {
    WriteLOG_SYNC(np->ctx_log);
    np->log_count_us -= np->log_time_us;
  }
}

// NLGログ入力＆レジスタ出力
int audio_decode_log(NLG *np, int samples) {
  int id, addr, data, cmd;
  double calc_ticks = samples * np->clock_per_sample;

  if (!np) return -1;

  while(np->log_ticks < calc_ticks) {
    cmd = ReadNLG(np->ctx);
    if (cmd == EOF) return -1;

    addr = 0;
    data = 0;
    id = -1;
    if (cmd < 0x80) {
      switch(cmd & 0x3f) {
        case CMD_PSG:
        id = NLG_PSG;
      break;
      case CMD_OPM:
        id = NLG_FM1;
      break;
      case CMD_OPM2:
        id = NLG_FM2;
        break;
      }
    } else {
      switch (cmd) {
        case CMD_IRQ:
          np->log_ticks += GetTickNLG(np->ctx);
          np->log_count_us += GetTickUsNLG(np->ctx);
        break;
        case CMD_CTC0: {
          int ctc = ReadNLG(np->ctx);
          SetCTC0_NLG(np->ctx, ctc);
        }
        break;
        case CMD_CTC3: {
          int ctc = ReadNLG(np->ctx);
          SetCTC3_NLG(np->ctx, ctc);
        }
        break;
      }
    }

    if (id >= 0) {
      // 初期タイミング値をセットする
      if (np->ctx_log && np->log_time_us == 0) {
        np->log_time_us = WriteLOG_Timing(np->ctx_log, GetTickUsNLG(np->ctx));
      }
      // 進める
      audio_step_log(np);

      addr = ReadNLG(np->ctx);
      data = ReadNLG(np->ctx);

      if (cmd >= 0x40) addr += 0x100;
      WriteDevice(np->map[id], addr, data);
      if (np->ctx_log) WriteLOG_Data(np->ctx_log, np->logmap[id] , addr, data);
    }
  }
  return 0;
}

// ログ入力＆レジスタ出力(S98)
int audio_decode_log_s98(NLG *np, int samples) {
  int log_id;
  int id = 0;
  int addr, data, cmd;
  int tmp = 0;

  double calc_ticks = samples * np->clock_per_sample;

  if (!np) return -1;

  while(np->log_ticks < calc_ticks) {
    cmd = ReadS98(np->ctx_s98);
    if (cmd == EOF) return -1;

    addr = data = 0;

    switch (cmd) {
      case S98_CMD_SYNC:
          tmp = GetTickUsS98(np->ctx_s98);
          np->log_count_us += tmp;
          np->log_ticks += tmp;
          break;
      case S98_CMD_NSYNC:
          tmp = GetTickUsS98(np->ctx_s98);
          tmp *= ReadS98VV(np->ctx_s98);
          np->log_count_us += tmp;
          np->log_ticks += tmp;
          break;
      case S98_CMD_END:
        // printf("end:%ld\n",TellS98(np->ctx_s98));
        return -1;
      break;
      default:
        // 初期タイミング値をセットする
        if (np->ctx_log && np->log_time_us == 0) {
          np->log_time_us = WriteLOG_Timing(np->ctx_log, GetTickUsS98(np->ctx_s98));
        }

        // タイミングだけ進める
        audio_step_log(np);

        addr = ReadS98(np->ctx_s98); // addr
        data = ReadS98(np->ctx_s98); // data

        id = -1;
        log_id = -1;
        // マップの範囲内であればidを得る
        if (cmd < (MAX_MAP * 2)) {
            id = np->map[cmd / 2];
            log_id = np->logmap[cmd / 2];
        }

        // デバイス存在
        if (id >= 0) {
          if (cmd & 1) addr += 0x100;
          WriteDevice(id, addr, data);
          if (np->ctx_log && log_id >= 0) WriteLOG_Data(np->ctx_log, log_id, addr, data);
        }
        break;
    }
  }
  return 0;
}

// ログをPCM化する
int audio_render_log(NLG *np, short *dest, int samples) {
  int len = 0;
  int pos = 0;

  while(samples > 0) {
    // サンプル生成のためのクロック数が足りない場合、ログを進める
    while(np->log_ticks < np->clock_per_sample) {
        if (np->ctx) {
            if (audio_decode_log(np, 1) < 0) return pos;
        } else {
            if (audio_decode_log_s98(np, 1) < 0) return pos;
        }
    }

    /* 次のログデータまでの長さを求める */
    len = (int)(np->log_ticks / np->clock_per_sample);

    if (len > samples) len = samples;

    DoRender(dest + (pos * PCM_CH), len);
    pos += len;
    samples -= len;
    np->log_ticks -= (np->clock_per_sample * len);
  }

  return pos;
}

// ループ
static void audio_loop(NLG *np, int freq) {
  printf("Playing...(ctrl-C to stop)\n");

  audio_reset_frame();

    if (audio_check_poll() < 0) return;

    do {
      audio_print_info();
      audio_wait_buffer();

      int len = audio_render_log(np, audio_get_current_buffer(), PCM_BLOCK);
      audio_next_buffer();
      if (len == 0) break;
    }while(audio_is_continue());

    if (!player.debug) printf("\n");
}

// audio_loop_output_pcmfile : 音声をデータ化する
// freq : 再生周波数
static void audio_loop_output_pcmfile(NLG *np, const char *file, int freq) {
  FILE *fp = NULL;

  audio_reset_frame();

  if (file) {
    fp = fopen(file, "wb");
    if (fp == NULL) {
      printf("Can't write a PCM file!\n");
      return;
    }
  }

  audio_write_wav_header(fp);
  short *buffer = audio_get_current_buffer();

  do {
    audio_print_info();
    int len = audio_render_log(np, buffer, PCM_BLOCK);
    if (fp) fwrite(buffer, PCM_BLOCK_BYTES, 1, fp);
    audio_add_frame(PCM_BLOCK);
    if (len == 0) break;

  }while(audio_is_continue());

  audio_write_wav_header(fp);
  if (fp) fclose(fp);
  if (!player.debug) printf("\n");
}

// audio_rt_out : データをリアルタイム出力する
// freq : 再生周波数
static void audio_rt_out(NLG *np) {
  // 経過秒数に対するサンプル数
  double left_len = 0;
  Uint32 old_ticks = audio_get_ticks();
  double freq = (double)audio_get_frequency();

  audio_reset_frame();

  do {
    audio_print_info();

    // 生成可能サンプル数が1未満なら待つ
    if (left_len < 1) {
        // 10ms経過待ち
        while((audio_get_ticks() - old_ticks) < 10) {
            audio_delay(1);
        }

        int new_ticks = audio_get_ticks();
        int ms = (new_ticks - old_ticks);
        old_ticks = new_ticks;
        left_len += (freq / 1000) * ms;
    }

    // レンダリングするバイト数を決定する
    int render_len = (int)left_len;
    if (render_len > PCM_BLOCK) render_len = PCM_BLOCK;
    render_len = audio_render_log(np, audio_get_current_buffer(), render_len);

    // 進めたサンプル分を引く
    left_len -= render_len;
    audio_add_frame(render_len);
  }while(audio_is_continue());

  if (!player.debug) printf("\n");
}

#ifdef USE_FMGEN
#define HELP_MODE "0 = OPM / 1 = OPLL / 2 = OPM(FMGEN) / 3 = OPL3 / 4 = OPNA"
#else
#define HELP_MODE "0 = OPM / 1 = OPLL / 3 = OPL3 / 4 = OPNA"
#endif


/* 使用方法 */
void usage(void) {
  printf("Usage %s [options ...] <file>\n", PROG);

  printf(
"\n"
" Options ...\n"
" -s / --rate <rate>   : Set playback rate\n"
" -m <device> : Select device for NLG %s\n"
" --rt  : RealTime output for real device\n"
" -l <seconds | mm:ss>  : Set song length\n"
"\n"
" -b : Output Log mode\n"
" --s98log : Output to S98\n"
" --nlglog : Output to NLG(Default)\n"
" --null    : No sound mode\n"
" -o file   : Output to WAV file\n"
"\n"
" -h / --help : Help (this)\n"
"\n", HELP_MODE
  );
}

enum {
  OUTLOG_NONE = 0,
  OUTLOG_S98,
  OUTLOG_NLG
};

// ファイルを再生する
int audio_play_file(NLG *np, const char *playfile) {
  int baseclk = 4000000;
  char log_path[PATH_MAX];
  np->log_time_us = np->log_count_us = 0;

  printf("File:%s\n", playfile);

  // ログ作成
  if (np->log_mode == OUTLOG_S98) {
    if (!MakeOutputFileLOG(log_path, playfile, LOG_EXT_S98)) {
        np->ctx_log = CreateLOG(log_path, LOG_MODE_S98);
    }
  }

  if (np->log_mode == OUTLOG_NLG) {
    if (!MakeOutputFileLOG(log_path, playfile, LOG_EXT_NLG)) {
      np->ctx_log = CreateLOG(log_path, LOG_MODE_NLG);
    }
  }

  if (np->ctx_log) printf("CreateLOG:%s\n", log_path);

  /* 初期化 */
  char *ext = strrchr(playfile, '.');
  if (!ext) {
    printf("Missing extension!\n");
    return -1;
  }

  if (strcasecmp(ext, ".s98") == 0) {
    np->ctx_s98 = OpenS98(playfile);
    if (!np->ctx_s98) {
      printf("Failed to open S98 file!\n");
      return -1;
    }

    // クロックは1usとする。
    np->clock_per_sample = (double)S98_US / np->rate;
  } else {
    np->ctx = OpenNLG(playfile);

    if (!np->ctx) {
      printf("Failed to open NLG file!\n");
      return -1;
    }

    baseclk = GetBaseClkNLG(np->ctx);
    np->clock_per_sample = (double)baseclk / np->rate;
  }

  np->log_ticks = 0;

  // c86ctl初期化
  rc_init(RCDRV_FLAG_ALL);

  // レンダラ初期化
  InitRender();
  SetRenderFreq(np->rate);

  RenderSetting rs;
  memset(&rs, 0, sizeof(RenderSetting));

  // 初期値
  rs.vol = 1;
  rs.freq = np->rate;
  rs.use_gmc = np->rt_mode;

  // S98は動的にデバイスを設定する
  if (np->ctx_s98) {
    int i;
    int id = -1;

    // デバイス数を得る
    if (GetDeviceCountS98(np->ctx_s98) > 0) {
      // デバイス数だけ情報を得る
      for (i = 0; i < GetDeviceCountS98(np->ctx_s98); i++) {
        int type = GetDeviceTypeS98(np->ctx_s98, i);
        int bc = GetDeviceFreqS98(np->ctx_s98, i);

        switch (type) {
        case S98_OPL3:
          rs.type = RENDER_TYPE_OPL3;
          break;
        case S98_OPLL:
          rs.type = RENDER_TYPE_OPLL;
          break;
        case S98_OPM:
          if (np->devsel == MODE_OPM_FMGEN) rs.type = RENDER_TYPE_OPM_FMGEN;
          else rs.type = RENDER_TYPE_OPM;
          break;
        case S98_SSG:
          rs.type = RENDER_TYPE_SSG;
          break;
        case S98_PSG:
          rs.type = RENDER_TYPE_PSG;
          break;
        case S98_OPNA:
        case S98_OPN:
          rs.type = RENDER_TYPE_OPNA;
          break;
        }

        // ログ出力のマップに追加
        if (np->ctx_log) np->logmap[i] = AddMapLOG(np->ctx_log, type, bc, LOG_PRIO_NORM);
        rs.baseclock = bc;
        id = AddRender(&rs);
        if (np->volume[i] >= 0) VolumeRender(id, np->volume[i]);

        np->map[i] = id;
      }
    } else {
      // 設定が無い場合はOPNAを1つ追加する
      rs.type = RENDER_TYPE_OPNA;
      rs.baseclock = RENDER_BC_7M98;

      // ログ出力のマップに追加
      if (np->ctx_log) np->logmap[0] = AddMapLOG(np->ctx_log, rs.type, rs.baseclock, LOG_PRIO_NORM);

      id = AddRender(&rs);
      np->map[0] = id;
    }
  } else {
      // NLGのセットアップ
      int i;
      int bc = baseclk;
      int log_type;

      // OPLLモードであれば、BC=3.57MHzにする
      if (np->devsel == MODE_OPLL) bc = RENDER_BC_3M57;
      rs.baseclock = bc;
      rs.type = RENDER_TYPE_SSG;
      log_type = LOG_TYPE_SSG;

      // ログ出力のマップに追加
      if (np->ctx_log) np->logmap[NLG_PSG] = AddMapLOG(np->ctx_log, log_type, bc, LOG_PRIO_PSG);
      np->map[NLG_PSG] = AddRender(&rs);

      // FMGENを使うかどうか
      log_type = LOG_TYPE_OPM;
      rs.type = RENDER_TYPE_OPM;

      switch(np->devsel) {
        case MODE_OPM_FMGEN:
          rs.type = RENDER_TYPE_OPM_FMGEN;
        break;
        case MODE_OPLL:
          rs.type = RENDER_TYPE_OPLL;
          log_type = LOG_TYPE_OPLL;
        break;
        case MODE_OPL3:
          rs.type = RENDER_TYPE_OPL3;
          log_type = LOG_TYPE_OPL3;
          rs.baseclock = bc = RENDER_BC_14M3;
        break;
        case MODE_OPNA:
          rs.type = RENDER_TYPE_OPNA;
          log_type = LOG_TYPE_OPNA;
          rs.baseclock = bc = RENDER_BC_7M98;
        }

      // ログ出力のマップに追加
      if (np->ctx_log) {
        np->logmap[NLG_FM1] = AddMapLOG(np->ctx_log, log_type, bc, LOG_PRIO_FM);
        np->logmap[NLG_FM2] = AddMapLOG(np->ctx_log, log_type, bc, LOG_PRIO_FM);
      }

      np->map[NLG_FM1] = AddRender(&rs);
      np->map[NLG_FM2] = AddRender(&rs);

      // 音量設定
      for(i = 0; i < 3; i++) {
        if (np->volume[i] >= 0) VolumeRender(np->map[i], np->volume[i]);
      }
  }

  // ログ出力のマップを終了
  if (np->ctx_log) MapEndLOG(np->ctx_log);
  if (!player.debug) printf("Freq = %d\n", np->rate);

  // メインループ
  if (np->rt_mode) audio_rt_out(np);
  else if (np->nullout || np->outfile) audio_loop_output_pcmfile(np, np->outfile, np->rate);
  else audio_loop(np, np->rate);

  /* 終了 */
  CloseNLG(np->ctx);

  /* ログ終了 */
  if (np->ctx_log) CloseLOG(np->ctx_log);

  ReleaseRender();
  rc_free();
  return 0;
}

/* エントリ */
int audio_main(int argc, char *argv[]) {
  int i;
  int opt;

  NLG n;

#ifdef _WIN32
  freopen("CON", "wt", stdout);
  freopen("CON", "wt", stderr);
#endif

  memset(chmask, 1, sizeof(chmask));

  // オーディオ初期化
  audio_init();
  audio_set_handle();

  // プログラム情報表示
  printf( "%s Version %s" "\nBuilt at %s\n", PROG, VERSION, __DATE__);

  // 長いオプション
  struct option long_opts[] = {
    {"rt", 0, NULL, 1},
    {"s98log", 0, NULL, 2},
    {"nlglog", 0, NULL, 3},
    {"null", 0, NULL, 4},
    {"rate", 1, NULL, 's'},
    {"help", 0, NULL, 'h'},
    {0, 0, 0, 0}};

  /* 構造体の初期化 */
  memset(&n, 0, sizeof(NLG));

  player.song_length = 0;
  player.debug = 0;  
  n.rate = 44100;

  int idx = 0;
  int vol = -1;

  // ボリューム調整
  for (idx = 0; idx < MAX_MAP; idx++) n.volume[idx] = -1;

  // オプション処理
  int opt_idx = 0;
  while (1) {
    opt = getopt_long(argc, argv, "s:m:o:v:l:hb", long_opts, &opt_idx);
    if (opt == -1) break;

    switch (opt) {
      case 1: // rt
        n.rt_mode = 1;
        break;

      case 2: // s98log
        n.log_mode = OUTLOG_S98;
        break;
      case 3: // nlglog
        n.log_mode = OUTLOG_NLG;
        break;
      case 4:
        n.nullout = 1;
        break;
      case 'b':
        n.nullout = 1;
        if (n.log_mode == OUTLOG_NONE) n.log_mode = OUTLOG_NLG;
        break;

      case 'l':
        player.song_length = get_length(optarg);
        break;

      case 's':
        n.rate = atoi(optarg);
        break;
      case 'm':
        n.devsel = atoi(optarg);
        break;
      case 'v':
        sscanf(optarg, "%d=%d", &idx, &vol);
        if (idx < MAX_MAP) {
          printf("volume idx:%d=%d\n", idx, vol);
          n.volume[idx] = vol;
        }
        break;
      case 'o':
        n.outfile = optarg;
        break;

      case 'h':
      default:
        usage();
        return 1;
    }
  }

  /* ファイルの指定があれば開く */
  if (argc <= 1 || argc <= optind) {
    usage();
    return 1;
  }

  audio_set_frequency(n.rate);
  audio_set_length(player.song_length);

  // RTモード
  if (n.rt_mode) printf("RealTime output mode\n");
  else {
    audio_sdl_init();
    if (audio_sdl_open(n.rate)) {
        printf("Failed to initialize audio hardware\n");
        return 1;
    }
    audio_play();
  }

  // ファイルの再生
  for (i = optind; i < argc; i++) {
    audio_play_file(&n, argv[i]);
    if (audio_is_stop()) break;
  }

  // オーディオ終了
  audio_free();
  return 0;
}

// disable SDLmain for win32 console app
#ifdef _WIN32
#undef main
#endif

int main(int argc, char *argv[]) {
    int ret = audio_main(argc, argv);
    return ret;
}
