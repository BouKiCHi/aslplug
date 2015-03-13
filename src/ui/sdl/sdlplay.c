//
// sdlplay.c
// for nezplay
//

#include <stdio.h>
#include <string.h>
#include <getopt.h>
#include "SDL.h"

#include <signal.h>
#define USE_SDL

// for stat
#include <sys/stat.h> 

#include "glue2.h"
#include "nezplug.h"

#include "log.h"

NEZ_PLAY *nezctx = NULL;


#define NSF_FNMAX 1024
int nsf_verbose = 0;
int debug = 0;

#define NEZ_VER "2015-03-09"
#define PRGNAME "NEZPLAY_ASL"

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
    
    short buffer[PCM_BUFFER_LEN];
} pcm;

#define PRNDBG(xx) if (nsf_verbose) printf(xx)

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

#define INLINE


INLINE void write_dword(byte *p,dword v)
{
    p[0] = v & 0xff;
    p[1] = (v>>8) & 0xff;
    p[2] = (v>>16) & 0xff;
    p[3] = (v>>24) & 0xff;
}

INLINE void write_word(byte *p,word v)
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
    if (! debug )
    {
        if (nsf_verbose)
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
        float s = ((float)data[i]) * volume;
        if (s < -0x8000)
            s = -0x8000;
        if (s > 0x7fff)
            s = 0x7fff;
        data[i] = (short)s;
    }
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

    fade_init();
    
    sec = 
    frames =
    total_frames = 0;
    
    if (audio_poll_event() < 0)
    {
        SDL_PauseAudio(1);
        return;
    }

    if (!debug)
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
    
        if (nezctx)
        {
	        NEZRender(nezctx, pcm.buffer + pcm.write, PCM_BLOCK);
            audio_volume(pcm.buffer + pcm.write, PCM_BLOCK, pcm.volume);            
        }
        // RenderNSF(pcm.buffer + pcm.write, PCM_BLOCK);
                
        if (fade_is_running())
            fade_stereo(pcm.buffer + pcm.write, PCM_BLOCK);

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
            
            if (!debug)
                audio_info(sec, len);
            
            /* フェーダーを起動する */
            if ( sec >= ( len - 3 ) )
            {
                if (!fade_is_running())
                    fade_start(freq, 1);
            }
        }


    }while(sec < len && !pcm.stop );
    
    if (!debug)
        printf("\n");
    
    PRNDBG("Stopping...\n");

    SDL_PauseAudio(1);

    PRNDBG("OK\n");

}


// audio_loop_file : 音声をデータ化する
// freq : 再生周波数
// len : 長さ(秒)
static void audio_loop_file(const char *file, int freq , int len )
{
    FILE *fp = NULL;

    int sec;
    
    int frames;
    int total_frames;

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
    
    if (!debug)
        audio_info(sec, len);
    
    do
    {
        if (nezctx)
        {
	        NEZRender(nezctx, pcm.buffer, PCM_BLOCK);
            audio_volume(pcm.buffer, PCM_BLOCK, pcm.volume);
        }
        
        if (fade_is_running())
            fade_stereo (pcm.buffer, PCM_BLOCK);
        
        if (fp)
            fwrite(pcm.buffer, PCM_BLOCK_BYTES, 1, fp);

        frames += PCM_BLOCK;
        total_frames += PCM_BLOCK;
        
        /* 今までのフレーム数が一秒以上なら秒数カウントを進める */
        while(frames >= freq)
        {
            frames -= freq;
            sec++;
            
            if (!debug)
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
    
    if (!debug)
        printf("\n");
}

//
// usage
//
void usage(void)
{
    printf(
    "Usage %s [ options ...] <file>\n", PRGNAME);
    
    printf(
    "\n"
    " Options ...\n"
    " -s rate   : Set playback rate\n"
    " -n no     : Set song number\n"
    " -v vol    : Set volume(def. 1.0)\n"
    " -l n      : Set song length (n secs)\n"
    " -q dir    : Set driver's path\n"
    "\n"
    " -o file   : Generate an Wave file(PCM)\n"
    " -p        : NULL PCM mode.\n"
    "\n"
    " -z        : Set N163 mode\n"
    "\n"
    " -9        : Set sound log mode to S98V3\n"
    " -r file   : Record a sound log\n"
    " -b        : Record a sound log without sound\n"
    "\n"
#ifdef USE_FMGEN
    " -g        : Use FMGEN for OPM\n"
#endif
    " -a        : Force turbo CPU mode\n"
    " -x        : Set strict mode\n"
    " -w        : Set verbose mode\n"
    "\n"
    " -h        : Help (this)\n"
    "\n"
    );
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

int audio_main(int argc, char *argv[])
{
    char log_path[NSF_FNMAX];
    
    char *drvpath = NULL;
    char *logfile = NULL;
    char *pcmfile = NULL;
    int opt;
    
    int rate   = 44100;
    int vol    = -1;
    int len    = 360;
    int songno = -1;
    
    int log_mode = 0;
    int nosound = 0;
    
    int s98mode = 0;
    int n163mode = 0;
    int strictmode = 0;

    int turbo_mode = 0;
    int use_fmgen = 0;

#ifdef _WIN32   
    freopen("CON", "wt", stdout);
    freopen("CON", "wt", stderr);
#endif

    audio_sdl_init();
    
    
    signal(SIGINT, audio_sig_handle);
    
    printf(
        "NEZPLAY on SDL Version %s\n"
        "Built at %s\n"
        "Ctrl-C to stop\n"
           , NEZ_VER, __DATE__);

	// SetNSFExecPath(argv[0]);
	
    if (argc < 2)
    {
        usage();
        return 0;
    }
    
    debug = 0;
    pcm.volume = 1.0f;
    
    while ((opt = getopt(argc, argv, "9q:s:n:v:l:d:o:r:btxhpzwag")) != -1)
    {
        switch (opt) 
        {
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
            case 'd':
                break;
            case 'o':
                pcmfile = optarg;
                break;
            case 'l':
                len = get_length(optarg);
                break;
            case 'x':
                strictmode = 1;
                break;
            case 'w':
                nsf_verbose = 1;
                break;
            case 't':
                // NESAudioSetDebug(1);
                debug = 1;
                break;
            case 'h':
            default:
                usage();
                return 1;
        }
    }
    
    if (rate < 8000)
        rate = 8000;
    
    glue2_set_exec_path(argv[0]);
    
    if (audio_init(rate))
    {
        printf("Failed to initialize audio hardware\n");
        return 1;
    }
    
    LOGCTX *log_ctx = NULL;
    
    nezctx = NEZNew();
    nezctx->turbo = turbo_mode;
    nezctx->use_fmgen = use_fmgen;
    
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
        }
        
        nezctx->log_ctx = log_ctx;
        
        // ファイルの読み出し
        if (glue2_load_file(nezctx, playfile, rate, 2, vol, songno) < 0)
        {
            printf("File open error : %s\n", playfile);
            
            glue2_mem_free();
            CloseLOG(log_ctx);
            
            return 0;
        }

        if (!debug)
            printf("Freq = %d, SongNo = %d\n", rate, songno);
        
        // ループ実行
        if (nosound || pcmfile)
            audio_loop_file(pcmfile, rate, len); // 音なし
        else
            audio_loop(rate, len); // 音出力
        
        glue2_mem_free();
        
        CloseLOG(log_ctx);
        log_ctx = NULL;

        if (pcm.stop)
            break;
    }
    
    audio_free();
    
    PRNDBG("NEZDelete\n");

    if (nezctx)
    {
	    NEZDelete(nezctx);
	    nezctx = NULL;
    }

    PRNDBG("done\n");

    return 0;
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


