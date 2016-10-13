
//
// sdlplay.c
// for nezplay
//

#include <stdio.h>
#include <string.h>
#include <getopt.h>
#include "SDL.h"

#ifdef USE_C86XDRV
#include "gmcdrv.h"
#endif

#include <signal.h>
#define USE_SDL

// for stat
#include <sys/stat.h>

#include "glue2.h"

#include "log.h"

struct {
    NEZ_PLAY *ctx;
    NEZ_PLAY *ctx2;
    int verbose;
    int debug;
} player;

#define NSF_FNMAX 1024

#define PRG_VER "2016-01-31"
#define PRG_NAME "ASLPLAY"

#define PCM_BLOCK 512
#define PCM_NUM_BLOCKS 16
#define PCM_BYTE_PER_SAMPLE 2
#define PCM_CH  2

#define PCM_BLOCK_SIZE (PCM_BLOCK * PCM_CH) // 1つのブロック
#define PCM_BLOCK_BYTES (PCM_BLOCK_SIZE * PCM_BYTE_PER_SAMPLE) // 1ブロックのバイト換算
#define PCM_BUFFER_LEN (PCM_BLOCK_SIZE * PCM_NUM_BLOCKS) // すべてのブロック
#define PCM_WAIT_SIZE (PCM_BUFFER_LEN - (PCM_BLOCK_SIZE)) // このサイズで空きを待つ

#define USE_SYNC

// PCM構造体
static struct pcm_struct
{
    int on;
    int stop;

	// 再生位置とバッファされているサンプル数
    int write;
    int play;
    int count;

    int over;
    int under;
    int file_mode;

    float volume;
    float mixvol;

    short buffer[PCM_BUFFER_LEN];
    short mixbuf[PCM_BLOCK_SIZE];
} pcm;

#define PRNDBG(xx) if (player.verbose) printf(xx)


#ifndef WIN32
void _sleep(int val)
{
}
#else
void sleep(int val)
{
}
#endif

// audio_callback 再生時に呼び出される
// data : データ
// len  : 必要なデータ長(バイト単位)
static void audio_callback( void *param , Uint8 *data , int len )
{
    int i;
    int under = 0;

    short *audio_buffer = (short *)data;

    if (!pcm.on)
    {
        memset(data, 0, len);
        return;
    }

    int count = pcm.count;

    for(i = 0; i < len / 2; i++)
    {
        if (count > 0)
        {
            audio_buffer[i] = pcm.buffer[pcm.play++];
            count--;
        }
        else
        {
            under = 1;
            audio_buffer[i] = 0;
        }
        if (pcm.play >= PCM_BUFFER_LEN)
            pcm.play = 0;
    }

    pcm.count = count;


    if (under)
        pcm.under++;
}

// SDL初期化
static int audio_sdl_init()
{

//  if ( SDL_Init( SDL_INIT_VIDEO | SDL_INIT_AUDIO ) )

    if (SDL_Init(SDL_INIT_AUDIO))
    {
        printf("Failed to Initialize!!\n");
        return 1;
    }

    memset(&pcm, 0, sizeof(pcm));

    return 0;
}

// オーディオ初期化
static int audio_init(int freq)
{
    SDL_AudioSpec af;
    PRNDBG("audio_init\n");


    // SDL_SetVideoMode ( 320 , 240 , 32 , SDL_SWSURFACE );

    af.freq     = freq;
    af.format   = AUDIO_S16;
    af.channels = 2;
    af.samples  = 2048;
    af.callback = audio_callback;
    af.userdata = NULL;

    if (SDL_OpenAudio( &af , NULL ) < 0)
    {
        printf("Audio Error!!\n");
        return 1;
    }

    SDL_PauseAudio(0);

    return 0;
}

// オーディオ開放
static void audio_free(void)
{
    PRNDBG("audio_free\n");

    SDL_CloseAudio();
    SDL_Quit();
}

static int audio_poll_event(void)
{
    SDL_Event evt;

    while(SDL_PollEvent(&evt))
    {
        switch(evt.type)
        {
            case SDL_QUIT:
                return -1;
            break;
        }
    }

    return 0;
}

// Ctrl+Cで呼ばれる
static void audio_sig_handle(int sig)
{
    pcm.stop = 1;
}

typedef unsigned char byte;
typedef unsigned short word;
typedef unsigned long dword;

void write_dword(byte *p,dword v)
{
    p[0] = v & 0xff;
    p[1] = (v>>8) & 0xff;
    p[2] = (v>>16) & 0xff;
    p[3] = (v>>24) & 0xff;
}

void write_word(byte *p,word v)
{
    p[0] = v & 0xff;
    p[1] = (v>>8) & 0xff;
}

#define WAV_CH 2
#define WAV_BPS 2

// audio_write_wav_header : ヘッダを出力する
// freq : 再生周波数
// pcm_bytesize : データの長さ
static void audio_write_wav_header(FILE *fp, long freq, long pcm_bytesize)
{
    byte hdr[0x80];

    if (!fp)
        return;

    memcpy(hdr,"RIFF", 4);
    write_dword(hdr + 4, pcm_bytesize + 44);
    memcpy(hdr + 8,"WAVEfmt ", 8);
    write_dword(hdr + 16, 16); // chunk length
    write_word(hdr + 20, 01); // pcm id
    write_word(hdr + 22, WAV_CH); // ch
    write_dword(hdr + 24, freq); // freq
    write_dword(hdr + 28, freq * WAV_CH * WAV_BPS); // bytes per sec
    write_word(hdr + 32, WAV_CH * WAV_BPS); // bytes per frame
    write_word(hdr + 34, WAV_BPS * 8 ); // bits

    memcpy(hdr + 36, "data",4);
    write_dword(hdr + 40, pcm_bytesize); // pcm size

    fseek(fp, 0, SEEK_SET);
    fwrite(hdr, 44, 1, fp);

    fseek(fp, 0, SEEK_END);

}

// audio_info : 時間表示

static void audio_info(int sec, int len)
{
    if (!player.debug)
    {
			printf("Time: %02d:%02d / %02d:%02d ",
					sec / 60 , sec % 60 , len / 60 , len % 60 );
			
			double cpu_usage = glue2_cpu_usage();
			
			if (cpu_usage >= 0)
				printf("CPU: %.3lf%% ", cpu_usage);
			
				if (player.verbose)
						printf("o:%5d u:%5d c:%5d w:%6d p:%6d ",
									 pcm.over, pcm.under, pcm.count,
									 pcm.write, pcm.play
						);
		
			printf("\r");
    }
    fflush(stdout);
}


// audio_loop : SDLオーディオへの出力
// freq : 再生周波数
// len : 長さ(秒)
static void audio_loop(int freq, int len)
{
    int sec;

    int frames;
    int total_frames;

    int update_info = 1;

    // len = 5;

    sec =
    frames =
    total_frames = 0;

    if (audio_poll_event() < 0)
    {
        SDL_PauseAudio(1);
        return;
    }

    do
    {
        if (!player.debug && update_info)
        {
            audio_info(sec, len);
            update_info = 0;
        }

        __sync_synchronize();
        // 書き込み可能になるのを待つ
        while(pcm.count > PCM_WAIT_SIZE)
        {
            // 閉じる
            if (audio_poll_event() < 0)
            {
                SDL_PauseAudio(1);
                return;
            }
            pcm.over++;
            SDL_Delay(1);
        }

        // サンプル生成
        glue2_make_samples(pcm.buffer + pcm.write, PCM_BLOCK);

        pcm.write += PCM_BLOCK_SIZE;
        pcm.count += PCM_BLOCK_SIZE;

        if (pcm.write >= PCM_BUFFER_LEN)
                pcm.write = 0;

        frames += PCM_BLOCK;
        total_frames += PCM_BLOCK;

        /* 今までのフレーム数が一秒以上なら秒数カウントを進める */
        if (frames >= freq)
        {
            update_info = 1;

            while(frames >= freq)
            {
                frames -= freq;
                sec++;
            }

            /* 終了より3秒以内であればフェーダーを起動する */
            if (sec >= (len - 3))
            {
                glue2_start_fade();
            }
        }


    }while(sec < len && !pcm.stop);

    if (!player.debug)
        printf("\n");

    PRNDBG("Stopping...\n");

    SDL_PauseAudio(1);

    PRNDBG("OK\n");

}

// audio_rt_out : データをリアルタイム出力する
// freq : 再生周波数
// len : 長さ(秒)
static void audio_rt_out(int freq, int len)
{
    int sec;
    int frames, total_frames;

    int update_info = 1;

    // 経過秒数に対するサンプル数
    double left_len = 0;

    Uint32 old_ticks = SDL_GetTicks();

    sec = frames = total_frames = 0;

    if (!player.ctx)
        return;

    do
    {
        // 情報出力
        if (update_info)
        {
            if (!player.debug)
                audio_info(sec, len);

            update_info = 0;
        }

        // 生成可能サンプル数が1バイト未満なら待つ
        if (left_len < 1)
        {
            // 10ms経過待ち
            while((SDL_GetTicks() - old_ticks) < 10)
            {
                SDL_Delay(1);
            }

            int new_ticks = SDL_GetTicks();

            int ms = (new_ticks - old_ticks);
            old_ticks = new_ticks;

            left_len += ((double)freq / 1000) * ms;
        }

        // レンダリングするバイト数を決定する
        int render_len = (int)left_len;

        if (render_len > PCM_BLOCK)
            render_len = PCM_BLOCK;

        glue2_make_samples(pcm.buffer, render_len);

        // 進めたサンプル分を引く
        left_len -= render_len;

        frames += render_len;
        total_frames += render_len;

        /* 今までのフレーム数が一秒以上なら秒数カウントを進める */
        while(frames >= freq)
        {
            frames -= freq;
            sec++;

            update_info = 1;
        }

    }while(sec < len && !pcm.stop);


    if (!player.debug)
        printf("\n");

}


// audio_loop_file : 音声をデータ化する
// freq : 再生周波数
// len : 長さ(秒)
static void audio_loop_file(const char *file, int freq , int len)
{
    FILE *fp = NULL;

    int sec;
    int frames, total_frames;

    int update_info = 1;

    // len = 5;

    sec = frames = total_frames = 0;


    if (file)
        fp = fopen(file, "wb");

    if (file && fp == NULL)
    {
        printf("Can't write a PCM file!\n");
        return;
    }

    audio_write_wav_header(fp, freq, 0);

    do
    {
        if (update_info)
        {
            if (!player.debug)
                audio_info(sec, len);

            update_info = 0;
        }


        glue2_make_samples(pcm.buffer, PCM_BLOCK);

        if (fp)
            fwrite(pcm.buffer, PCM_BLOCK_BYTES, 1, fp);

        frames += PCM_BLOCK;
        total_frames += PCM_BLOCK;

        /* 今までのフレーム数が一秒以上なら秒数カウントを進める */
        if (frames >= freq)
        {
            while(frames >= freq)
            {
                frames -= freq;
                sec++;
            }

            update_info = 1;


            /* フェーダーを起動する */
            if (sec >= (len - 3))
                glue2_start_fade();
        }

    }while(sec < len && !pcm.stop);

    audio_write_wav_header(fp, freq,
        total_frames * PCM_CH * PCM_BYTE_PER_SAMPLE);

    if (fp)
        fclose(fp);

    if (!player.debug)
        printf("\n");
}

#define NORMAL 1
#define SAMEPATH 2

#define WAV_EXT ".WAV"
#define NLG_EXT ".NLG"

int audio_check_nlgmode(const char *file)
{
    char *p = strrchr(file, '.');
    if (!p)
        return 0;

    if (strcasecmp(p, NLG_EXT) == 0)
        return 1;

    return 0;
}


// 時間表記を秒数に変換する
int get_length(const char *str)
{
    if (strchr(str, ':') == NULL)
        return atoi(optarg);
    else
    {
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
void copy_filename(char *dest, const char *name)
{
    char *p = strrchr(name, PATH_DEFSEP);

    if (p)
        strcpy(dest, p + 1);
    else
        strcpy(dest, name);
}

// メイン処理
int audio_main(int argc, char *argv[])
{
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

#ifdef _WIN32
    freopen("CON", "wt", stdout);
    freopen("CON", "wt", stderr);
#endif

    audio_sdl_init();


    signal(SIGINT, audio_sig_handle);

    printf(
        "%s on SDL Version %s\n"
        "Built at %s\n"
        "Ctrl-C to stop\n"
           , PRG_NAME, PRG_VER, __DATE__);

	// SetNSFExecPath(argv[0]);

    if (argc < 2)
    {
        usage();
        return 0;
    }
    
    double adj_vol[8] = {1,1,1,1,1,1,1,1};

    player.debug = 0;

    // 長いオプション
    struct option long_opts[] =
    {
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
    

    while ((opt = getopt_long(argc, argv, "9q:s:n:v:l:d:o:r:btxhpzwagu", long_opts, &opt_idx)) != -1)
    {
        switch (opt)
        {
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
            case 13: // adjvol
            {
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

    // サンプリング周波数の下限
    if (rate < 8000)
        rate = 8000;

    // ドライバパスの設定
    if (drvpath)
    {
        printf("DriverPath:%s\n", drvpath);
        glue2_set_driver_path(drvpath);
    }

    // 実行パスの設定
    glue2_set_exec_path(argv[0]);

    if (audio_init(rate))
    {
        printf("Failed to initialize audio hardware\n");
        return 1;
    }

#ifdef USE_C86XDRV
    // C86初期化
    c86x_init();
#endif

    // glue2初期化
    glue2_init();

    struct glue2_setting gs;
    memset(&gs, 0, sizeof(gs));

    LOGCTX *log_ctx = NULL;

    pcm.on = 1;
    


    // ファイルの数だけ処理
    for(;optind < argc; optind++)
    {
        char *playfile = argv[optind];

        // ログモードの設定
        if (log_mode)
        {
            // 拡張子
            char *log_ext = LOG_EXT_NLG;

            if (s98mode)
                log_ext = LOG_EXT_S98;

            // 同じディレクトリに出力
            if (log_mode == SAMEPATH)
            {
                MakeFilenameLOG(log_path, playfile, log_ext);

                logfile = log_path;
            }

            printf("CreateLOG:%s\n",logfile);

            int log_mode = LOG_MODE_NLG;
            if (s98mode)
                log_mode = LOG_MODE_S98;

            log_ctx = CreateLOG(logfile, log_mode);

            if (!rough_mode)
            {
                printf("Fine Log Mode\n");
                SetDenomLOG(log_ctx, 10000);
            }
        }

        if (rt_mode)
            printf("RealTime output mode\n");

        // WAV出力パスの設定
        if (wav_mode)
        {
            const char *wav_ext = WAV_EXT;

            // 同じディレクトリに出力
            if (wav_mode == SAMEPATH)
            {
                MakeFilenameLOG(wav_path, playfile, wav_ext);

                wavfile = wav_path;
            }
        }

        // ダンプモード
        if (dump_mode)
        {
          const char *ext = ".KSS";
          MakeFilenameLOG(dump_path, playfile, ext);
          glue2_set_dump_path(dump_path);
          printf("Dump : %s\n", dump_path);
        }

        if (wavfile)
            printf("CreateWAV:%s\n", wavfile);

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
        if (log_ctx)
        {
            char title[NSF_FNMAX];
            copy_filename(title, playfile);

            if (!tag_title)
                tag_title = title;

            if (tag_title)
            {
                printf("Title : %s\n", tag_title);
                WriteLOG_SetTitle(log_ctx, LOG_STR_TITLE, tag_title);
            }

            if (tag_artist)
            {
                printf("Artist : %s\n", tag_artist);
                WriteLOG_SetTitle(log_ctx, LOG_STR_ARTIST, tag_artist);
            }

            if (tag_game)
            {
                printf("Game : %s\n", tag_game);
                WriteLOG_SetTitle(log_ctx, LOG_STR_GAME, tag_game);
            }

        }


        // ファイルの読み出し
        if (glue2_load_file(playfile, 0, &gs) < 0)
        {
            printf("File open error : %s\n", playfile);
            result = 1;

            goto err_end;
        }

        // サブファイルがある場合
        if (subfile)
        {
            printf("Subfile : %s\n", subfile);


            gs.songno = songno_sub;
            gs.vol = volume_sub;

            // ファイルの読み出し
            if (glue2_load_file(subfile, 1, &gs) < 0)
            {
                printf("File open error : %s\n", subfile);
                result = 1;

                goto err_end;
            }
        }

        if (!player.debug)
            printf("Freq = %d, SongNo = %d\n", rate, songno);
        
        for(int i = 0; i < 8; i++) glue2_set_adjust_volume(i,adj_vol[i]);

        // ループ実行
        if (rt_mode)
        {
            // リアルタイム実行(音なし)
            audio_rt_out(rate, len);
        }
        else
        {
            if (nosound)
                audio_loop_file(wavfile, rate, len); // 音なし
            else
                audio_loop(rate, len); // 音出力
        }

        glue2_mem_free();

        CloseLOG(log_ctx);
        log_ctx = NULL;

        if (pcm.stop)
            break;
    }

err_end:
    if (log_ctx)
    {
        CloseLOG(log_ctx);
        log_ctx = NULL;
    }

#ifdef USE_C86XDRV
    c86x_free();
#endif

    audio_free();

    glue2_free();

    return result;
}

// disable SDLmain for win32 console app
#ifdef _WIN32
#undef main
#endif

int main(int argc, char *argv[])
{
	int ret = audio_main(argc, argv);

	return ret;
}
