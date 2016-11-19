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

#include <signal.h>

#include "nlg.h"
#include "s98x.h"
#include "log.h"

#include "render.h"

#include "gmcdrv.h"

#ifndef PATH_MAX 
#define PATH_MAX 1024
#endif

// マルチスレッド対策でシンクロさせる
#define USE_SYNC

#define PROG "NLGSIM"
#define VERSION "1.5(2015-10-18)"

#if 0
typedef unsigned char byte;
typedef unsigned short word;
typedef unsigned long dword;
#endif

int debug = 0;

#define PCM_BLOCK 2048
#define PCM_NUM_BLOCKS 8

#define PCM_CH  2
#define PCM_BYTE_PER_SAMPLE 2

#define PCM_BLOCK_FRAME (PCM_BLOCK)
#define PCM_BLOCK_SHORT (PCM_BLOCK * PCM_CH)
#define PCM_BLOCK_BYTES (PCM_BLOCK_SHORT * PCM_BYTE_PER_SAMPLE)

#define PCM_BUFFER_FRAME (PCM_BLOCK_FRAME * PCM_NUM_BLOCKS)
#define PCM_BUFFER_SHORT (PCM_BLOCK_SHORT * PCM_NUM_BLOCKS)

Uint8 chmask[0x200];

struct
{
    int baseclk;
    int freq;

	int on;
	int write;
	int play;
	int stop;

    int output_count;
    int sec;

    int pos;
    int count;

	short buffer[PCM_BUFFER_SHORT];
} pcm;

#define MAX_MAP 8

typedef struct
{
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


// audio_callback
// 変数の読み書きがあるもの(count)はマルチスレッドの際に問題が生じるので注意
static void audio_callback(void *param, Uint8 *dest, int len_in_bytes)
{
	int i;

	short *buf = (short *)dest;

	if (!pcm.on)
	{
		memset(dest, 0, len_in_bytes);
		return;
	}

    int count = pcm.count;
    int out_count = pcm.output_count;

    for(i = 0; i < len_in_bytes / 2;)
    {
        if (count > 0)
        {
            buf[i++] = pcm.buffer[pcm.play++];
            buf[i++] = pcm.buffer[pcm.play++];
            count -= 2;
        }
        else
        {
            buf[i++] = 0;
            buf[i++] = 0;
        }
        out_count++;
        if (pcm.play >= PCM_BUFFER_SHORT)
            pcm.play = 0;
    }
#ifdef USE_SYNC
    __sync_synchronize();
#endif
    pcm.output_count = out_count;
    pcm.count = count;

}


// SDL初期化
static int audio_init(int freq)
{
	SDL_AudioSpec af;

    memset(&pcm, 0 ,sizeof(pcm));


	if (SDL_Init(SDL_INIT_AUDIO))
	{
		printf("Failed to Initialize!!\n");
		return 1;
	}

    af.freq     = freq;
	af.format   = AUDIO_S16;
	af.channels = PCM_CH;
	af.samples  = PCM_BLOCK; // PCM_BLOCK
	af.callback = audio_callback;
	af.userdata = NULL;

	if (SDL_OpenAudio(&af, NULL) < 0)
	{
		printf("Audio Error!!\n");
		return 1;
	}

	return 0;
}

// SDL開放
static void audio_free( void )
{
	SDL_CloseAudio();
    SDL_Quit();
}


////////////////////
// wav関連
typedef unsigned char byte;
typedef unsigned short word;
typedef unsigned long dword;


void write_dword(byte *p, dword v)
{
    p[0] = v & 0xff;
    p[1] = (v>>8) & 0xff;
    p[2] = (v>>16) & 0xff;
    p[3] = (v>>24) & 0xff;
}

void write_word(byte *p, word v)
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
    unsigned char hdr[0x80];

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

// イベント監視
static int audio_poll_event(void) {
	SDL_Event evt;

	while(SDL_PollEvent(&evt)) {
		switch ( evt.type ) {
			case SDL_QUIT:
				return -1;
			break;
		}
	}
	return 0;
}

// ハンドラ
static void audio_sig_handle(int sig) {
	pcm.stop = 1;
}

// 同期を出力
void audio_step_log(NLG *np) {
  if (!np->ctx_log) return;

  while(np->log_count_us >= np->log_time_us) {
    WriteLOG_SYNC(np->ctx_log);
    np->log_count_us -= np->log_time_us;
  }
}

// ログ入力＆レジスタ出力
int audio_decode_log(NLG *np, int samples)
{
    int id;
    int addr, data;
    int cmd;

    double calc_ticks = samples * np->clock_per_sample;


    if (!np)
        return -1;

    while(np->log_ticks < calc_ticks)
    {
        cmd = ReadNLG(np->ctx);
        if (cmd == EOF)
            return -1;

        addr = 0;
        data = 0;
        id = -1;

        switch (cmd)
        {
            case CMD_PSG:
              id = NLG_PSG;
            break;
            case CMD_OPM:
              id = NLG_FM1;
            break;
            case CMD_OPM2:
              id = NLG_FM2;
              break;
            case CMD_IRQ:
                np->log_ticks += GetTickNLG(np->ctx);
                np->log_count_us += GetTickUsNLG(np->ctx);
                break;
            case CMD_CTC0:
                {
                    int ctc = ReadNLG(np->ctx);
                    SetCTC0_NLG(np->ctx, ctc);
                }
                break;
            case CMD_CTC3:
                {
                    int ctc = ReadNLG(np->ctx);
                    SetCTC3_NLG(np->ctx, ctc);
                }
                break;
        }

        if (id >= 0)
        {
          // 初期タイミング値をセットする
          if (np->ctx_log && np->log_time_us == 0)
          {
            np->log_time_us = GetTickUsNLG(np->ctx);
            WriteLOG_Timing(np->ctx_log, np->log_time_us);
          }
          // 進める
          audio_step_log(np);

          addr = ReadNLG(np->ctx); // addr
          data = ReadNLG(np->ctx); // data

          WriteDevice(np->map[id], addr, data);

          if (np->ctx_log)
            WriteLOG_Data(np->ctx_log, np->logmap[id] , addr, data);
        }
    }
    return 0;
}

// ログ入力＆レジスタ出力(S98)
int audio_decode_log_s98(NLG *np, int samples)
{
    int log_id;
    int id = 0;
    int addr;
    int data;
    int cmd;
    int tmp = 0;

    double calc_ticks = samples * np->clock_per_sample;


    if (!np)
        return -1;

    while(np->log_ticks < calc_ticks)
    {
        cmd = ReadS98(np->ctx_s98);

        if (cmd == EOF)
            return -1;

        addr = 0;
        data = 0;

        switch (cmd)
        {
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
                if (np->ctx_log && np->log_time_us == 0)
                {
                  np->log_time_us = GetTickUsS98(np->ctx_s98);
                  WriteLOG_Timing(np->ctx_log, np->log_time_us);
                }

                // タイミングだけ進める
                audio_step_log(np);

                addr = ReadS98(np->ctx_s98); // addr
                data = ReadS98(np->ctx_s98); // data

                id = -1;
                log_id = -1;
                // マップの範囲内であればidを得る
                if (cmd < (MAX_MAP * 2))
                {
                    id = np->map[cmd / 2];
                    log_id = np->logmap[cmd / 2];
                }

                // デバイス存在
                if (id >= 0)
                {
                    if (cmd & 1)
                        addr += 0x100;

                    WriteDevice(id, addr, data);

                    if (np->ctx_log && log_id >= 0)
                      WriteLOG_Data(np->ctx_log, log_id, addr, data);
                }

                break;
        }
    }
    return 0;
}


// ログモード
int audio_render_log(NLG *np, short *dest, int samples)
{
    int len = 0;
    int pos = 0;

    while(samples > 0)
    {
        // サンプル生成のためのクロック数が足りない場合、ログを進める
        while(np->log_ticks < np->clock_per_sample)
        {
            if (np->ctx)
            {
                if (audio_decode_log(np, 1) < 0)
                    return pos;
            }
            else
            {
                if (audio_decode_log_s98(np, 1) < 0)
                    return pos;
            }
        }

        /* 次のログデータまでの長さを求める */
        len = (int)(np->log_ticks / np->clock_per_sample);

        if (len > samples)
            len = samples;

        DoRender(dest + (pos * PCM_CH), len);
        pos += len;
        samples -= len;
        //
        np->log_ticks -= (np->clock_per_sample * len);

    }

    return pos;
}


// 再生時間表示
static void audio_info(void)
{
	// 一秒ごとに更新する
    if (pcm.output_count >= pcm.freq)
    {
        while(pcm.output_count >= pcm.freq)
        {
            pcm.sec++;
            pcm.output_count -= pcm.freq;
        }
        printf("\rTime : %02d:%02d",
               pcm.sec / 60 , pcm.sec % 60
               );

        fflush(stdout);
    }
}


// ループ
static void audio_loop(NLG *np, int freq)
{
    int len;

    printf("Playing...(ctrl-C to stop)\n");

    // バッファを埋める
    len = audio_render_log(np, pcm.buffer, PCM_BUFFER_FRAME);

    pcm.play = 0;
    pcm.write = 0;
    pcm.count = len * PCM_CH;
    pcm.output_count = 0;
    pcm.sec = 0;

    // ログの末端
    if (!len)
        return;

    // 開始
    SDL_PauseAudio(0);

    while(!pcm.stop)
    {
    	//　時間情報の表示
        audio_info();

        // バッファにBLOCK_SIZE以上の空きがある
        if (PCM_BUFFER_SHORT - pcm.count > PCM_BLOCK_SHORT)
        {
            len = audio_render_log(np, pcm.buffer + pcm.write, PCM_BLOCK_SHORT);

#ifdef USE_SYNC
            __sync_synchronize();
#endif

            pcm.write += (len * PCM_CH);
            pcm.count += (len * PCM_CH);

            if (pcm.write >= PCM_BUFFER_SHORT)
                pcm.write = 0;

            // logが終了した場合に0を返す
            if (len == 0) break;
        }
        else
        {
            // バッファに空きがない
            SDL_Delay(1);
        }

        // ポーリング
        if (audio_poll_event() < 0)
            break;

    }

	printf("\n");

	SDL_PauseAudio(1);
}

// audio_loop_file : 音声をデータ化する
// freq : 再生周波数
static void audio_loop_file(NLG *np, const char *file, int freq)
{
    FILE *fp = NULL;

    int len = 0;
    int total_frames = 0;

    if (file)
        fp = fopen(file, "wb");

    if (!np->nullout && fp == NULL)
    {
        printf("Can't write a PCM file!\n");
        return;
    }

    audio_write_wav_header(fp, freq, 0);

    do
    {
        audio_info();

        len = audio_render_log(np, pcm.buffer, PCM_BLOCK_FRAME);

        if (fp)
            fwrite(pcm.buffer, PCM_BLOCK_BYTES, 1, fp);

        pcm.output_count += PCM_BLOCK_FRAME;
        total_frames += PCM_BLOCK_FRAME;

    }while( len > 0 && !pcm.stop );

    audio_write_wav_header(fp, freq,
                           total_frames * PCM_CH * PCM_BYTE_PER_SAMPLE);

    if (fp)
        fclose(fp);

    printf("\n");
}

// audio_rt_out : データをリアルタイム出力する
// freq : 再生周波数
static void audio_rt_out(NLG *np, int freq)
{
    int sec;
    int frames, total_frames;
	int render_len = 0;

    // 経過秒数に対するサンプル数
    double left_len = 0;

    Uint32 old_ticks = SDL_GetTicks();

    sec = frames = total_frames = 0;

    do
    {
        audio_info();

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
        render_len = (int)left_len;

        if (render_len > PCM_BLOCK)
            render_len = PCM_BLOCK;

        // 音の生成とログを進める
        render_len = audio_render_log(np, pcm.buffer, render_len);

        // 進めたサンプル分を引く
        left_len -= render_len;

        // 生成カウントを進める
        pcm.output_count += render_len;
        total_frames += render_len;

    }while(render_len > 0 && !pcm.stop);

    printf("\n");

}


#ifdef USE_FMGEN
#define HELP_MODE "0 = OPM / 1 = OPLL / 2 = OPM(FMGEN)"
#else
#define HELP_MODE "0 = OPM / 1 = OPLL"
#endif


/* 使用方法 */
void usage(void)
{
	printf(
	"Usage %s [options ...] <file>\n", PROG);

	printf(
	"\n"
	" Options ...\n"
    " -s / --rate <rate>   : Set playback rate\n"
	" -m <device> : Select device for NLG %s\n"
	" --rt  : RealTime output for real device\n"
  " --s98out : Output to S98\n"
  " --nlgout : Output to NLG\n"

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
int audio_play_file(NLG *np, const char *playfile)
{
    int baseclk = 4000000;

    char log_path[PATH_MAX];

    np->log_time_us = np->log_count_us = 0;

    printf("File:%s\n", playfile);

    // ログ作成
    if (np->log_mode == OUTLOG_S98)
    {
        if (!MakeOutputFileLOG(log_path, playfile, LOG_EXT_S98))
        {
            np->ctx_log = CreateLOG(log_path, LOG_MODE_S98);
        }
    }

    if (np->log_mode == OUTLOG_NLG)
    {
        if (!MakeOutputFileLOG(log_path, playfile, LOG_EXT_NLG))
        {
            np->ctx_log = CreateLOG(log_path, LOG_MODE_NLG);
        }
    }

    if (np->ctx_log)
    {
        printf("CreateLOG:%s\n", log_path);
    }

    /* 初期化 */
    char *ext = strrchr(playfile, '.');
    if (ext)
    {
        if (strcasecmp(ext,".s98") == 0)
        {
            // s98のデータを開く

            np->ctx_s98 = OpenS98(playfile);
            if (!np->ctx_s98)
            {
                printf("Filed to open S98 file!\n");
                return -1;
            }

            // クロックは1usとする。
            np->clock_per_sample = (double)S98_US / np->rate;
            np->log_ticks = 0;
        }
        else
        {
            // NLGを開く
            np->ctx = OpenNLG(playfile);

            if (!np->ctx)
            {
                printf("Filed to open NLG file!\n");
                return -1;
            }

            baseclk = GetBaseClkNLG(np->ctx);
            np->clock_per_sample = (double)baseclk / np->rate;
            np->log_ticks = 0;

        }
    }

    // c86ctl初期化
    c86x_init();

    // レンダラ初期化
    InitRender();
    SetRenderFreq(np->rate);

    RenderSetting rs;
    memset(&rs, 0, sizeof(RenderSetting));

    rs.freq = np->rate;
    rs.use_gmc = np->rt_mode;

    // S98は動的にデバイスを設定する
    if (np->ctx_s98)
    {
        int i;
        int id = -1;

        // デバイス数を得る
        if (GetDeviceCountS98(np->ctx_s98) > 0)
        {
            // デバイス数だけ情報を得る
            for (i = 0; i < GetDeviceCountS98(np->ctx_s98); i++)
            {
                int type = GetDeviceTypeS98(np->ctx_s98, i);
                int bc = GetDeviceFreqS98(np->ctx_s98, i);

                switch(type)
                {
                    case S98_OPL3:
                        rs.type = RENDER_TYPE_OPL3;
                        break;
                    case S98_OPLL:
                        rs.type = RENDER_TYPE_OPLL;
                        break;
                    case S98_OPM:
                        if (np->devsel == MODE_OPM_FMGEN)
                            rs.type = RENDER_TYPE_OPM_FMGEN;
                        else
                            rs.type = RENDER_TYPE_OPM;
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
                if (np->ctx_log)
                    np->logmap[i] = AddMapLOG(np->ctx_log, type, bc, LOG_PRIO_NORM);

                rs.baseclock = bc;
                id = AddRender(&rs);

                if (np->volume[i] >= 0)
                    VolumeRender(id, np->volume[i]);

                np->map[i] = id;
            }

        }
        else
        {
            // 設定が無い場合はOPNAを1つ追加する
            rs.type = RENDER_TYPE_OPNA;
            rs.baseclock = RENDER_BC_7M98;

            // ログ出力のマップに追加
            if (np->ctx_log)
                np->logmap[0] = AddMapLOG(np->ctx_log, rs.type, rs.baseclock, LOG_PRIO_NORM);

            id = AddRender(&rs);
            np->map[0] = id;
        }
    }
    else
    {
        // NLGのセットアップ
        int i;
        int bc = baseclk;
        int log_type;

        // OPLLモードであれば、BC=3.57MHzにする
        if (np->devsel == MODE_OPLL)
            bc = RENDER_BC_3M57;

        rs.baseclock = bc;

        rs.type = RENDER_TYPE_SSG;
        log_type = LOG_TYPE_SSG;

        // ログ出力のマップに追加
        if (np->ctx_log)
            np->logmap[NLG_PSG] = AddMapLOG(np->ctx_log, log_type, bc, LOG_PRIO_PSG);

        np->map[NLG_PSG] = AddRender(&rs);


        // FMGENを使うかどうか
        log_type = LOG_TYPE_OPM;
        if (np->devsel == MODE_OPM_FMGEN)
            rs.type = RENDER_TYPE_OPM_FMGEN;
        else
            rs.type = RENDER_TYPE_OPM;

        // OPLLモード
        if (np->devsel == MODE_OPLL)
        {
            rs.type = RENDER_TYPE_OPLL;
            log_type = LOG_TYPE_OPLL;
        }

        // ログ出力のマップに追加
        if (np->ctx_log)
        {
            np->logmap[NLG_FM1] = AddMapLOG(np->ctx_log, log_type, bc, LOG_PRIO_FM);
            np->logmap[NLG_FM2] = AddMapLOG(np->ctx_log, log_type, bc, LOG_PRIO_FM);
        }

        np->map[NLG_FM1] = AddRender(&rs);
        np->map[NLG_FM2] = AddRender(&rs);

        // 音量設定
        for(i = 0; i < 3; i++)
        {
            if (np->volume[i] >= 0)
                VolumeRender(np->map[i], np->volume[i]);
        }

    }

    // ログ出力のマップを終了
    if (np->ctx_log)
        MapEndLOG(np->ctx_log);

    pcm.baseclk = baseclk;
    pcm.freq = np->rate;
    pcm.on = 1;

    if (!debug)
        printf("Freq = %d\n", np->rate);

    // メインループ
    if (np->rt_mode) // リアルタイムモード
        audio_rt_out(np, np->rate);
    else
        if (np->nullout || np->outfile) // ファイル出力モード
            audio_loop_file(np, np->outfile, np->rate);
        else // ログ再生モード
            audio_loop(np, np->rate);

    /* 終了 */
    CloseNLG(np->ctx);

    /* ログ終了 */
    if (np->ctx_log)
        CloseLOG(np->ctx_log);

    ReleaseRender();

    c86x_free();


    return 0;
}



/* エントリ */
int audio_main(int argc, char *argv[])
{
  int i;
	int opt;

    NLG n;

#ifdef _WIN32
	freopen("CON", "wt", stdout);
	freopen("CON", "wt", stderr);
#endif

	memset(chmask, 1, sizeof(chmask));

	// プログラム情報表示
	printf(
		"%s Version %s"
		"\nBuilt at %s\n"
		, PROG, VERSION, __DATE__);

	debug = 0;

    // 長いオプション
    struct option long_opts[] = {
     {"rt", 0, NULL, 1},
     {"s98log", 0, NULL, 2},
     {"nlglog", 0, NULL, 3},
     {"null", 0, NULL, 4},
     {"rate", 1, NULL, 's'},
     {"help", 0, NULL, 'h'},
     {0, 0, 0, 0}
    };

    /* 構造体の初期化 */
    memset(&n, 0, sizeof(NLG));

    n.rate = 44100;

    int idx = 0;
    int vol = -1;

    // ボリューム調整
    for(idx = 0; idx < MAX_MAP; idx++)
    {
        n.volume[idx] = -1;
    }

	// オプション処理

	int opt_idx = 0;
	while ((opt = getopt_long(
		argc, argv, "s:m:o:v:h", long_opts, &opt_idx)) != -1)
	{
        switch (opt)
		{
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
		case 's':
            n.rate = atoi(optarg);
            break;
        case 'm':
            n.devsel = atoi(optarg);
            break;
        case 'v':
            sscanf(optarg,"%d=%d", &idx, &vol);
            if (idx < MAX_MAP)
            {
                printf("volume idx:%d=%d\n",idx, vol);
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
    if (argc <= 1 || argc <= optind)
    {
			usage();
			return 1;
  	}

    // 割り込みハンドラの設定
	signal(SIGINT, audio_sig_handle);

	// RTモード
	if (n.rt_mode)
		printf("RealTime output mode\n");

    // オーディオ初期化
    if (audio_init(n.rate))
    {
        printf("Failed to initialize audio hardware\n");
        return 1;
    }

    for(i = optind; i < argc; i++)
    {
      // ファイルの再生
      audio_play_file(&n, argv[i]);
    }

    // オーディオ終了
	audio_free();


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
