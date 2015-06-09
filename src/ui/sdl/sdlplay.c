//
// sdlplay.c
// for nezplay
//

#include <stdio.h>
#include <string.h>
#include <getopt.h>
#include "SDL.h"

#include "gmcdrv.h"

#include <signal.h>
#define USE_SDL

// for stat
#include <sys/stat.h> 

#include "glue2.h"
#include "nezplug.h"

#include "log.h"

struct {
    NEZ_PLAY *ctx;
    NEZ_PLAY *ctx2;
    int verbose;
    int debug;
} player;


#define NSF_FNMAX 1024

#define PRG_VER "2015-06-09"
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

#include "fade.h"

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

static void audio_info(int sec, int len)
{
    if (!player.debug)
    {
        if (player.verbose)
            printf("\rTime : %02d:%02d / %02d:%02d o:%5d u:%5d c:%5d w:%6d p:%6d",
                   sec / 60, sec % 60,
                   len / 60, len % 60,
                   pcm.over, pcm.under, pcm.count,
                   pcm.write, pcm.play
                   );
        
        else
        printf("\rTime : %02d:%02d / %02d:%02d",
               sec / 60 , sec % 60 , len / 60 , len % 60 );
    }
    fflush(stdout);
}

// audio_volume : 音量増幅
static void audio_volume(short *data, int len, float volume)
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

// audio_mix : サンプル加算
static void audio_mix(short *dest, short *in, int len, float volume)
{
    // stereo
    int i = 0;
    for (i = 0; i < len * 2; i++)
    {
        double s = 0;
        
        s = ((double)in[i] * volume);

        if (s < -0x7fff)
            s = -0x7fff;
        if (s > 0x7fff)
            s = 0x7fff;
        
        dest[i] += s;
    }
}


// サンプル生成をする
static void audio_make_samples(short *buf, int len)
{
    NEZRender(player.ctx, buf, len);
    audio_volume(buf, len, pcm.volume);
    
    // サブチャンネル
    if (player.ctx2)
    {
        NEZRender(player.ctx2, pcm.mixbuf, len);
        audio_mix(buf, pcm.mixbuf, len, pcm.mixvol);
    }
    
    // フェード機能
    if (fade_is_running())
        fade_stereo(buf, len);

}


// audio_loop : 再生時にループする
// freq : 再生周波数
// len : 長さ(秒)
static void audio_loop(int freq, int len)
{
    int sec;
    
    int frames;
    int total_frames;

    // len = 5;
    
    if (!player.ctx)
        return;

    fade_init();
    
    sec = 
    frames =
    total_frames = 0;
    
    if (audio_poll_event() < 0)
    {
        SDL_PauseAudio(1);
        return;
    }

    if (!player.debug)
        audio_info(sec, len);
    
    do
    {
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
        audio_make_samples(pcm.buffer + pcm.write, PCM_BLOCK);


        pcm.write += PCM_BLOCK_SIZE;
        pcm.count += PCM_BLOCK_SIZE;
        
        if (pcm.write >= PCM_BUFFER_LEN)
                pcm.write = 0;
        
        frames += PCM_BLOCK;
        total_frames += PCM_BLOCK;
        
        /* 今までのフレーム数が一秒以上なら秒数カウントを進める */
        while(frames >= freq)
        {
            frames -= freq;
            sec++;
            
            if (!player.debug)
                audio_info(sec, len);
            
            /* フェーダーを起動する */
            if ( sec >= ( len - 3 ) )
            {
                if (!fade_is_running())
                    fade_start(freq, 1);
            }
        }


    }while(sec < len && !pcm.stop );
    
    if (!player.debug)
        printf("\n");
    
    PRNDBG("Stopping...\n");

    SDL_PauseAudio(1);

    PRNDBG("OK\n");

}

// audio_rt_out : データをリアルタイム出力する
// freq : 再生周波数
// len : 長さ(秒)
static void audio_rt_out(int freq , int len)
{
    int sec;
    int frames, total_frames;

    // 経過秒数に対するサンプル数
    double left_len = 0;
    
    Uint32 old_ticks = SDL_GetTicks();
   
    sec = frames = total_frames = 0;
    
    if (!player.ctx)
        return;
    
    if (!player.debug)
        audio_info(sec, len);
    
    do
    {
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
        
        NEZRender(player.ctx, pcm.buffer, render_len);
        
        // 進めたサンプル分を引く
        left_len -= render_len;
        
        frames += render_len;
        total_frames += render_len;
        
        /* 今までのフレーム数が一秒以上なら秒数カウントを進める */
        while(frames >= freq)
        {
            frames -= freq;
            sec++;
            
            if (!player.debug)
                audio_info(sec, len);
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

    if (!player.ctx)
        return;
    
    // len = 5;

    fade_init();
    
    sec = frames = total_frames = 0;
    
    
    if (file)
        fp = fopen(file, "wb");
    
    if (file && fp == NULL)
    {
        printf("Can't write a PCM file!\n");
        return;
    }
    
    audio_write_wav_header(fp, freq, 0);
    
    if (!player.debug)
        audio_info(sec, len);
    
    do
    {
        
        audio_make_samples(pcm.buffer, PCM_BLOCK);
        
        
        if (fp)
            fwrite(pcm.buffer, PCM_BLOCK_BYTES, 1, fp);

        frames += PCM_BLOCK;
        total_frames += PCM_BLOCK;
        
        /* 今までのフレーム数が一秒以上なら秒数カウントを進める */
        while(frames >= freq)
        {
            frames -= freq;
            sec++;
            
            if (!player.debug)
                audio_info(sec, len);
            
            /* フェーダーを起動する */
            if (sec >= (len - 3))
            {
                if (!fade_is_running())
                    fade_start(freq, 1);
            }
        }

    }while( sec < len && !pcm.stop );
    
    audio_write_wav_header(fp, freq,
        total_frames * PCM_CH * PCM_BYTE_PER_SAMPLE);

    if (fp)
        fclose(fp);
    
    if (!player.debug)
        printf("\n");
}

#define NLG_NORMAL 1
#define NLG_SAMEPATH 2

#define NLG_EXT ".NLG"
#define S98_EXT ".S98"

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
    "Usage %s [ options ...] <file>\n", PRG_NAME);
    
    printf(
    "\n"
    " Options ...\n"
    " -s / --rate <rate>   : Set playback rate\n"
    " -v / --vol <vol>     : Set volume(def. 1.0)\n"
    " -l / --len <n|mm:ss> : Set song length (n secs)\n"
    "\n"
    " --rt         : RealTime output for real device\n"
    "\n"
    " -n <num>      : Set song number\n"
    " -q <dir>     : Set driver's path\n"
    "\n"
    " -o <file>    : Generate an WAV file(PCM)\n"
    " -p           : NULL PCM mode.\n"
    "\n"
    " -z           : Set N163 mode\n"
    "\n"
    " -9 / --s98   : Set sound log mode to S98V3\n"
    " -u / --rough : S98 rough tempo mode (default)\n"
    " --fine       : S98 fine tempo mode\n"
    " -r <file>    : Record a sound log\n"
    " -b           : Record a sound log without sound\n"
    "\n"
    " --sub <file> : Sub slot song file\n"
    " --subvol <vol> : Set volume for Sub slot(def. 1.0)\n"
    " --subnum <num> : Set song number for Sub slot\n"
    "\n"

#ifdef USE_FMGEN
    " -g          : Use FMGEN for OPM\n"
#endif
    " -a          : Force turbo CPU mode\n"
    " -x          : Set strict mode\n"
    " -w          : Set verbose mode\n"
    "\n"
    " -h / --help : Help (this)\n"
    "\n"
    );
}

// メイン処理
int audio_main(int argc, char *argv[])
{
    char log_path[NSF_FNMAX];
    
    char *drvpath = NULL;
    char *logfile = NULL;
    char *pcmfile = NULL;

    char *subfile = NULL;
    
    int opt;
    
    int rate   = 44100;
    int vol    = -1;
    int len    = 360;
    int songno = -1;
    int songno_sub = -1;
    
    int log_mode = 0;
    int nosound = 0;
    
    int s98mode = 0;
    int n163mode = 0;
    int strictmode = 0;
    
    int rt_mode = 0;
    int turbo_mode = 0;
    int use_fmgen = 0;
    int rough_mode = 1;
    
    int result = 0;

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
    
    player.debug = 0;
    pcm.volume = 1.0f;
    pcm.mixvol = 1.0f;
    
    // 長いオプション
    struct option long_opts[] = {
     {"fine", 0, NULL, 0},
     {"rt", 0, NULL, 1},
     {"sub", 1, NULL, 2},
     {"subvol", 1, NULL, 3},
     {"subnum", 1, NULL, 4},

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
            case 2: // sub slot
                subfile = optarg;
            break;
            case 3: // mix volume
                sscanf(optarg, "%f", &pcm.mixvol);
                break;
            case 4: // subnum
                songno_sub = atoi(optarg);
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
            case 'q':
                drvpath = optarg;
                break;
            case 'b':
                log_mode = NLG_SAMEPATH;
                logfile = NULL;
                nosound = 1;
                break;
            case 'r':
                log_mode = NLG_NORMAL;
                logfile = optarg;
                break;
            case 'p':
                nosound = 1;
                break;
            case 'z':
                n163mode = 1;
            break;
            case 's':
                rate = atoi(optarg);
                break;
            case 'n':
                songno = atoi(optarg);
                break;
            case 'v':
                sscanf(optarg, "%f", &pcm.volume);
                break;
            case 'u':
                rough_mode = 1;
                break;
            case 'd':
                break;
            case 'o':
                pcmfile = optarg;
                nosound = 1;
                break;
            case 'l':
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
    
    if (rate < 8000)
        rate = 8000;
    
    if (drvpath)
        glue2_set_driver_path(drvpath);

    glue2_set_exec_path(argv[0]);
    
    if (audio_init(rate))
    {
        printf("Failed to initialize audio hardware\n");
        return 1;
    }
    
    gimic_init();
    
    LOGCTX *log_ctx = NULL;
    
    player.ctx = NEZNew();
    player.ctx->turbo = turbo_mode;
    player.ctx->use_fmgen = use_fmgen;
    
    pcm.on = 1;
    

    // ファイルの数だけ処理
    for(;optind < argc; optind++)
    {
        char *playfile = argv[optind];
        
        // ログモードの設定
        if (log_mode)
        {
            char *log_ext = NLG_EXT;
            if (s98mode)
                log_ext = S98_EXT;
            
            if (log_mode == NLG_SAMEPATH)
            {
                strcpy(log_path, playfile);
                char *p = strrchr(log_path, '.');
                
                if (p)
                    strcpy(p, log_ext);
                else
                    strcat(log_path, log_ext);
                
                logfile = log_path;
            }

            printf("CreateLOG:%s\n",logfile);
            
            int log_mode = LOG_MODE_NLG;
            if (s98mode)
                log_mode = LOG_MODE_S98;
            
            log_ctx = CreateLOG(logfile, log_mode);
            
            if (rough_mode)
            {
                SetRoughModeLOG(log_ctx, 1000);
            } 
            else
            {
            	printf("Fine Log Mode\n");
            }
        }
        
        if (rt_mode)
            printf("RealTime output mode\n");
        
        player.ctx->use_gmc = rt_mode;
        player.ctx->log_ctx = log_ctx;
        
        
        printf("File:%s\n", playfile);
        
        // ファイルの読み出し
        if (glue2_load_file(player.ctx, playfile, rate, 2, vol, songno) < 0)
        {
            printf("File open error : %s\n", playfile);
            result = 1;
            
            goto err_end;
        }
        
        // サブファイルがある場合
        if (subfile)
        {
            printf("Subfile:%s\n", subfile);

            player.ctx2 = NEZNew();
            player.ctx2->turbo = turbo_mode;
            player.ctx2->use_fmgen = use_fmgen;
            
            // ファイルの読み出し
            if (glue2_load_file(player.ctx2, subfile, rate, 2, vol, songno_sub) < 0)
            {
                printf("File open error : %s\n", subfile);
                result = 1;

                goto err_end;
            }
        }

        if (!player.debug)
            printf("Freq = %d, SongNo = %d\n", rate, songno);
        
        // ループ実行
        if (rt_mode)
        {
            // リアルタイム実行(音なし)
            audio_rt_out(rate, len);
        }
        else
        {
            if (nosound)
                audio_loop_file(pcmfile, rate, len); // 音なし
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
    
    gimic_free();
    audio_free();
    
    PRNDBG("delete\n");

    if (player.ctx2)
    {
        NEZDelete(player.ctx2);
        player.ctx2 = NULL;
    }

    
    if (player.ctx)
    {
	    NEZDelete(player.ctx);
	    player.ctx = NULL;
    }

    PRNDBG("done\n");

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


