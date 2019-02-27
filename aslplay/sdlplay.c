
//
// sdlplay.c 
//

#include <SDL.h>
#include <stdio.h>
#include <string.h>
#include <getopt.h>

#ifdef USE_RCDRV
#include "rcdrv.h"
#endif

#include "audio.h"

#ifndef USE_SDL
#define USE_SDL
#endif

// for stat
#include <sys/stat.h>

#include "glue2.h"
#include "log.h"

#ifndef WIN32
void _sleep(int val) { }
#else
void sleep(int val) { }
#endif

struct {
    NEZ_PLAY *ctx;
    NEZ_PLAY *ctx2;
    int verbose;
    int debug;
} player;

#include "version.h"

#define NSF_FNMAX 1024

#define USE_SYNC

#define PRNDBG(xx) if (player.verbose) printf(xx)


static void loop_audio() {

  audio_reset_frame();

  if (audio_check_poll() < 0) return;

  do {
    audio_print_info();
    audio_wait_buffer();

    // サンプル生成
    glue2_make_samples(audio_get_current_buffer(), PCM_BLOCK);
    audio_next_buffer();
    if (audio_check_fade()) glue2_start_fade();

  }while(audio_is_continue());

  if (!player.debug) printf("\n");

  PRNDBG("Stopping...\n");

  audio_pause(1);
  PRNDBG("OK\n");
}

// データをリアルタイム出力する
static void rt_out()
{
  // 経過秒数に対するサンプル数
  double left_len = 0;
  Uint32 old_ticks = audio_get_ticks();
  double freq = (double)audio_get_frequency();

  audio_reset_frame();

  do {
    audio_print_info();

    // 生成可能サンプル数が1バイト未満なら待つ
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
    glue2_make_samples(audio_get_current_buffer(), render_len);

    // 進めたサンプル分を引く
    left_len -= render_len;

    audio_add_frame(render_len);

  }while(audio_is_continue());

  if (!player.debug) printf("\n");
}


// 音声をデータ化する
static void loop_file(const char *file) {
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
    glue2_make_samples(buffer, PCM_BLOCK);
    if (fp) fwrite(buffer, PCM_BLOCK_BYTES, 1, fp);
    audio_add_frame(PCM_BLOCK);
    if (audio_check_fade()) glue2_start_fade();

  }while(audio_is_continue());

  audio_write_wav_header(fp);

  if (fp) fclose(fp);

  if (!player.debug) printf("\n");
}

#define NORMAL 1
#define SAMEPATH 2

#define WAV_EXT ".WAV"
#define NLG_EXT ".NLG"

int audio_check_nlgmode(const char *file) {
  char *p = strrchr(file, '.');
  if (!p) return 0;

  if (strcasecmp(p, NLG_EXT) == 0) return 1;

  return 0;
}

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


//
// usage
//
void usage(void)
{
  printf(
  "Usage %s [options ...] <file>\n", PRG_NAME);

  printf(
"\n"
"Options ...\n"
"-s / --rate <rate>   : Set playback rate\n"
"-v / --vol <vol>     : Set volume(def. 1.0)\n"
"-l / --len <n|mm:ss> : Set song length (n secs)\n"
"\n"
"--rt         : RealTime output for real device\n"
"\n"
"-n <num>     : Set song number\n"
"-q <dir>     : Set driver's path\n"
"\n"
"--wav        : Generate an WAV file(PCM)\n"
"-o <file>    : Generate an WAV file(PCM) with filename\n"
"-p           : NULL PCM mode.\n"
"--nosound    : NULL PCM mode.\n"
"\n"
"--dump       : Dump mode.\n"
"\n"
"-z           : Set N163 mode\n"
"\n"
"--log        : Record a sound log\n"
"-r <file>    : Record a sound log with filename\n"
"-b           : Record a sound log without sound\n"
"--nlg        : Set sound log mode to NLG\n"
"-9 / --s98   : Set sound log mode to S98V3 (default)\n"
"-u / --rough : S98 rough tempo mode (default)\n"
"--fine       : S98 fine tempo mode\n"
"\n"
"--title <str>   : Title in S98 Tag\n"
"--artist <str>  : Artist in S98 Tag\n"
"--game <str>    : Game in S98 Tag\n"
"\n"
"--sub <file>   : Sub slot song file\n"
"--subvol <vol> : Set volume for Sub slot(def. 1.0)\n"
"--subnum <num> : Set song number for Sub slot\n"
"\n"

#ifdef USE_FMGEN
"-g          : Use FMGEN for OPM\n"
#endif
"-a          : Force turbo CPU mode\n"
"-x          : Set strict mode\n"
"-w          : Set verbose mode\n"
"\n"
"-h / --help : Help (this)\n"
"\n"
  );
}

// ファイル名の複製
void copy_filename(char *dest, const char *name) {
  char *p = strrchr(name, PATH_DEFSEP);

  if (p) strcpy(dest, p + 1);
  else strcpy(dest, name);
}

// メイン処理
int audio_main(int argc, char *argv[]) {
  char log_path[NSF_FNMAX];
  char wav_path[NSF_FNMAX];
  char dump_path[NSF_FNMAX];

  char *drvpath = NULL;
  char *logfile = NULL;
  char *wavfile = NULL;

  char *subfile = NULL;

  char *tag_title = NULL;
  char *tag_artist = NULL;
  char *tag_game = NULL;

  int opt;

  int rate   = 44100;
  int len    = 360;
  int songno = -1;
  int songno_sub = -1;

  // ダンプ作成
  int dump_mode = 0;

  int wav_mode = 0;

  int log_mode = 0;
  int nosound = 0;

  int s98mode = 1;
  int n163mode = 0;
  int strictmode = 0;

  int rt_mode = 0;
  int turbo_mode = 0;
  int use_fmgen = 0;
  int rough_mode = 1;

  int result = 0;

  float volume_main = 1;
  float volume_sub = 1;

  // 初期化
  memset(&player, 0, sizeof(player));
  audio_init();

#ifdef _WIN32
  freopen("CON", "wt", stdout);
  freopen("CON", "wt", stderr);
#endif

  audio_set_handle();

  printf(
    "%s on SDL Version %s\n"
    "Built at %s\n"
    "Ctrl-C to stop\n", PRG_NAME, PRG_VER, __DATE__);

  // SetNSFExecPath(argv[0]);

  if (argc < 2) {
      usage();
      return 0;
  }

  double adj_vol[8] = {1,1,1,1,1,1,1,1};

  player.debug = 0;

  // 長いオプション
  struct option long_opts[] = {
    {"fine", 0, NULL, 0},
    {"rt", 0, NULL, 1},
    {"sub", 1, NULL, 2},
    {"subvol", 1, NULL, 3},
    {"subnum", 1, NULL, 4},
    {"log", 0, NULL, 5},
    {"wav", 0, NULL, 6},
    {"nosound", 0, NULL, 7},

    {"artist", 1, NULL, 8},
    {"title", 1, NULL, 9},
    {"game", 1, NULL, 10},

    {"nlg", 0, NULL, 11},
    {"adjvol", 1, NULL, 13},

    // ダンプモード
    {"dump", 0, NULL, 12},

    {"rough", 0, NULL, 'u'},
    {"rate", 1, NULL, 's'},
    {"len", 1, NULL,  'l'},
    {"vol", 1, NULL, 'v'},
    {"s98", 0, NULL, '9'},
    {"help", 0, NULL, 'h'},
    {0, 0, 0, 0}
  };

  int opt_idx = 0;


  while ((opt = getopt_long(argc, argv, "9q:s:n:v:l:d:o:r:btxhpzwagu", long_opts, &opt_idx)) != -1) {
    switch (opt) {
      case 0: // fineモード
        rough_mode = 0;
      break;
      case 1: // RealTime
        rt_mode = 1;
      break;
      case 2: // サブスロット ファイル
        subfile = optarg;
      break;
      case 3: // サブスロット　合成音量
        sscanf(optarg, "%f", &volume_sub);
        break;
      case 4: // subnum
        songno_sub = atoi(optarg);
      break;
      case 5: // --log
        log_mode = SAMEPATH;
        logfile = NULL;
      break;
      case 6: // --wav
        wav_mode = SAMEPATH;
        wavfile = NULL;
        nosound = 1;
      case 7: // --nosound
        nosound = 1;
      break;
      case 8: // --artist
        tag_artist = optarg;
      break;
      case 9: // --title
        tag_title = optarg;
      break;
      case 10: // --game
        tag_game = optarg;
      break;
      case 11: // --nlg
        s98mode = 0;
      break;
      case 12: // --dump / ダンプモード
        dump_mode = 1;
      break;
      case 13: { // adjvol
          int adjvol_idx;
          double adjvol_val;
          sscanf(optarg,"%d=%lf",&adjvol_idx,&adjvol_val);
          if (adjvol_idx < 0 || adjvol_idx > 7) { printf("Wrong Device Index!\n"); break; }
          adj_vol[adjvol_idx] = adjvol_val;
      }
      break;
      case 'a':
          turbo_mode = 1;
          break;
      case 'g':
          use_fmgen = 1;
          break;
      case '9':
          s98mode = 1;
          break;
      case 'q': // ドライバパス
          drvpath = optarg;
          break;
      case 'b':
          log_mode = SAMEPATH;
          logfile = NULL;
          nosound = 1;
      break;
      case 'r':
          log_mode = NORMAL;
          logfile = optarg;
          break;
      case 'p':
          nosound = 1;
          break;
      case 'z': // N163モード
          n163mode = 1;
      break;
      case 's': // サンプリング周波数
          rate = atoi(optarg);
          break;
      case 'n': // 曲選択
          songno = atoi(optarg);
          break;
      case 'v': // メイン音量
          sscanf(optarg, "%f", &volume_main);
          break;
      case 'u': // ラフモード
          rough_mode = 1;
          break;
      case 'd': // デバッグ
          break;
      case 'o': // WAVファイル出力
          wavfile = optarg;
          nosound = 1;
          break;
      case 'l': // 曲の長さ
          len = get_length(optarg);
          break;
      case 'x':
          strictmode = 1;
          break;
      case 'w':
          player.verbose = 1;
          break;
      case 't':
          // NESAudioSetDebug(1);
          player.debug = 1;
          break;
      case 'h':
      default:
          usage();
          return 1;
      }
  }

    audio_set_callback_cpu_usage(glue2_cpu_usage);
    audio_set_verbose(player.verbose);
    audio_set_debug(player.debug);

  // サンプリング周波数の下限
  if (rate < 8000) rate = 8000;

  // ドライバパスの設定
  if (drvpath) {
      printf("DriverPath:%s\n", drvpath);
      glue2_set_driver_path(drvpath);
  }

  // 実行パスの設定
  glue2_set_exec_path(argv[0]);

  if (!rt_mode) {
    audio_sdl_init();

    if (audio_sdl_open(rate)) {
        printf("Failed to initialize audio hardware\n");
        return 1;
    }
  }

#ifdef USE_RCDRV
  // C86初期化
  rc_init(RCDRV_FLAG_ALL);
#endif

  // glue2初期化
  glue2_init();

  struct glue2_setting gs;
  memset(&gs, 0, sizeof(gs));

  LOGCTX *log_ctx = NULL;

  audio_play();

  // ファイルの数だけ処理
  for(;optind < argc; optind++) {
    char *playfile = argv[optind];

    // ログモードの設定
    if (log_mode) {
      // 拡張子
      char *log_ext = LOG_EXT_NLG;

      if (s98mode)
          log_ext = LOG_EXT_S98;

      // 同じディレクトリに出力
      if (log_mode == SAMEPATH) {
          MakeFilenameLOG(log_path, playfile, log_ext);

          logfile = log_path;
      }

      printf("CreateLOG:%s\n",logfile);

      int log_mode = LOG_MODE_NLG;
      if (s98mode)
          log_mode = LOG_MODE_S98;

      log_ctx = CreateLOG(logfile, log_mode);

      if (!rough_mode) {
          printf("Fine Log Mode\n");
          SetDenomLOG(log_ctx, 10000);
      }
    }

    if (rt_mode)
        printf("RealTime output mode\n");

    // WAV出力パスの設定
    if (wav_mode) {
      const char *wav_ext = WAV_EXT;

      // 同じディレクトリに出力
      if (wav_mode == SAMEPATH) {
          MakeFilenameLOG(wav_path, playfile, wav_ext);

          wavfile = wav_path;
      }
    }

    // ダンプモード
    if (dump_mode) {
      const char *ext = ".KSS";
      MakeFilenameLOG(dump_path, playfile, ext);
      glue2_set_dump_path(dump_path);
      printf("Dump : %s\n", dump_path);
    }

    if (wavfile) printf("CreateWAV:%s\n", wavfile);

    // 最低限必要な設定
    gs.freq = rate;
    gs.ch = 2;
    gs.songno = songno;
    gs.vol = volume_main;

    // オプショナル
    gs.turbo = turbo_mode;
    gs.use_fmgen = use_fmgen;

    // その他
    gs.use_gmc = rt_mode;
    gs.log_ctx = log_ctx;

    printf("File : %s\n", playfile);

    // タグ書き出し
    if (log_ctx) {
      char title[NSF_FNMAX];
      copy_filename(title, playfile);

      if (!tag_title) tag_title = title;

      if (tag_title) {
          printf("Title : %s\n", tag_title);
          WriteLOG_SetTitle(log_ctx, LOG_STR_TITLE, tag_title);
      }

      if (tag_artist) {
          printf("Artist : %s\n", tag_artist);
          WriteLOG_SetTitle(log_ctx, LOG_STR_ARTIST, tag_artist);
      }

      if (tag_game) {
          printf("Game : %s\n", tag_game);
          WriteLOG_SetTitle(log_ctx, LOG_STR_GAME, tag_game);
      }
    }

    // ファイルの読み出し
    if (glue2_load_file(playfile, 0, &gs) < 0) {
        printf("File open error : %s\n", playfile);
        result = 1;
        goto err_end;
    }

    // サブファイルがある場合
    if (subfile) {
      printf("Subfile : %s\n", subfile);

      gs.songno = songno_sub;
      gs.vol = volume_sub;

      // ファイルの読み出し
      if (glue2_load_file(subfile, 1, &gs) < 0) {
          printf("File open error : %s\n", subfile);
          result = 1;
          goto err_end;
      }
    }

    if (!player.debug) printf("Freq = %d, SongNo = %d\n", rate, songno);

    int i = 0;
    for(i = 0; i < 8; i++) glue2_set_adjust_volume(i,adj_vol[i]);

    audio_set_frequency(rate);
    audio_set_length(len);


    // ループ実行
    if (rt_mode) {
        // リアルタイム実行(音なし)
        rt_out();
    } else {
      if (nosound) loop_file(wavfile); // 音なし
      else loop_audio(); // 音出力
    }

    glue2_mem_free();

    CloseLOG(log_ctx);
    log_ctx = NULL;

    if (audio_is_stop()) break;
  }

err_end:
  if (log_ctx) {
      CloseLOG(log_ctx);
      log_ctx = NULL;
  }

#ifdef USE_RCDRV
  rc_free();
#endif
  if (!rt_mode) audio_free();

  glue2_free();

  return result;
}

// disable SDLmain for win32 console app
#ifdef _WIN32
#undef main
#endif

int main(int argc, char *argv[]) {
    int ret = audio_main(argc, argv);

    return ret;
}
