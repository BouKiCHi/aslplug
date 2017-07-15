#include <stdio.h>
#include <signal.h>
#include "SDL.h"

#include "memory_wr.h"
#include "audio.h"

// PCM構造体
struct pcm_struct pcm;

#define PRNDBG(xx) if (pcm.verbose) printf(xx)

// オーディオ初期化
int audio_init(void) {
  memset(&pcm, 0, sizeof(pcm));
}

// SDL初期化
int audio_sdl_init(void) {
  if (SDL_Init(SDL_INIT_AUDIO)) {
    printf("Failed to Initialize!!\n");
    return 1;
  }
  return 0;
}

static void audio_callback(void *param, Uint8 *data, int len);

// SDLオーディオを開く
int audio_sdl_open(int freq) {
  SDL_AudioSpec af;

  af.freq     = freq;
  af.format   = AUDIO_S16;
  af.channels = 2;
  af.samples  = 2048;
  af.callback = audio_callback;
  af.userdata = NULL;

  if (SDL_OpenAudio(&af, NULL) < 0) {
    printf("Audio Error!!\n");
    return 1;
  }

  SDL_PauseAudio(0);
  return 0;
}

int audio_play(void) {
  pcm.on = 1;
}

int audio_stop(void) {
  pcm.on = 0;
}

void audio_pause(int pause) {
  SDL_PauseAudio(pause);
}

unsigned long audio_get_ticks(void) {
  return SDL_GetTicks();
}

void audio_delay(int ms) {
  SDL_Delay(ms);
}

void audio_reset_frame(void) {
  pcm.seconds = 0;
  pcm.frames = 0;
  pcm.total_frames = 0;
  pcm.update_info = 1;
  pcm.fading = 0;
}

// オーディオ開放
void audio_free(void) {
    PRNDBG("audio_free\n");

    SDL_CloseAudio();
    SDL_Quit();
}

int audio_poll_event(void) {
  SDL_Event evt;

  while(SDL_PollEvent(&evt)) {
    switch(evt.type) {
      case SDL_QUIT:
        return -1;
      break;
    }
  }

  return 0;
}

// audio_callback 再生時に呼び出される
// data : データ
// len  : 必要なデータ長(バイト単位)
static void audio_callback(void *param, Uint8 *data, int len) {
  int i;
  int under = 0;

  short *audio_buffer = (short *)data;

  if (!pcm.on) {
    memset(data, 0, len);
    return;
  }

  int count = pcm.count;

  for(i = 0; i < len / 2; i++) {
    if (count > 0) {
        audio_buffer[i] = pcm.buffer[pcm.play++];
        count--;
    } else {
        under = 1;
        audio_buffer[i] = 0;
    }
    if (pcm.play >= PCM_BUFFER_LEN) pcm.play = 0;
  }

  pcm.count = count;

  if (under) pcm.under++;
}

// audio_info : 時間表示
void audio_info() {
  printf("Time: %02d:%02d / %02d:%02d ",
  pcm.seconds / 60, pcm.seconds % 60, pcm.length / 60, pcm.length % 60 );

  if (pcm.cpu_usage_cb) {
    double cpu_usage = pcm.cpu_usage_cb(); 
    if (cpu_usage >= 0) printf("CPU: %.3lf%% ", cpu_usage);
  }

  if (pcm.verbose) printf(
      "o:%5d u:%5d c:%5d w:%6d p:%6d ",
      pcm.over, pcm.under, pcm.count, pcm.write, pcm.play);

  printf("\r");
  fflush(stdout);
}

void audio_print_info(void) {
  if (!pcm.debug && pcm.update_info) {
    audio_info();
    pcm.update_info = 0;
  }
}

int audio_check_poll(void) {
  if (audio_poll_event() >= 0) return 0;
  SDL_PauseAudio(1);
  return -1;
}

int audio_check_fade(void) {
  return pcm.fading;
}


int audio_wait_buffer(void) {
  __sync_synchronize();
  // 書き込み可能になるのを待つ
  while(pcm.count > PCM_WAIT_SIZE) {
    // 閉じる
    if (audio_check_poll() < 0) return -1;

    pcm.over++;
    SDL_Delay(1);
  }
  return 0;
}


// Ctrl+Cで呼ばれる
void audio_sig_handle(int sig) {
  pcm.stop = 1;
}

void audio_set_handle(void) {
  signal(SIGINT, audio_sig_handle);
}

void audio_add_frame(int frame) {
  pcm.frames += frame;
  pcm.total_frames += frame;

  /* 今までのフレーム数が一秒以上なら秒数カウントを進める */
  if (pcm.frames >= pcm.freq) {
    pcm.update_info = 1;

    while(pcm.frames >= pcm.freq) {
      pcm.frames -= pcm.freq; pcm.seconds++;
    }

    /* 終了より3秒以内であればフェーダーを起動する */
    if (pcm.seconds >= (pcm.length - 3)) {
      pcm.fading = 1;
    }
  }
}

void audio_next_buffer(void) {
  pcm.write += PCM_BLOCK_SIZE;
  pcm.count += PCM_BLOCK_SIZE;

  if (pcm.write >= PCM_BUFFER_LEN) pcm.write = 0;

  audio_add_frame(PCM_BLOCK);
}


void audio_set_length(int length) {
  pcm.length = length;
}

void audio_set_frequency(int freq) {
  pcm.freq = freq;
}

void audio_set_debug(int value) {
  pcm.debug = value;
}

void audio_set_verbose(int value) {
  pcm.verbose = value;
}

int audio_get_frequency(void) {
  return pcm.freq;
}


int audio_is_continue(void) {
  return (pcm.seconds < pcm.length && !pcm.stop);
}

int audio_is_stop(void) {
  return (pcm.stop);
}

short *audio_get_current_buffer(void) {
  return (pcm.buffer + pcm.write);
}


#define WAV_CH 2
#define WAV_BPS 2

// ヘッダを出力する
void audio_write_wav_header(FILE *fp) {
  byte hdr[0x80];

  if (!fp) return;
  
  long pcm_bytesize = pcm.total_frames * PCM_CH * PCM_BYTE_PER_SAMPLE;

  memcpy(hdr,"RIFF", 4);
  WriteDWORD(hdr + 4, pcm_bytesize + 44);
  memcpy(hdr + 8,"WAVEfmt ", 8);
  WriteDWORD(hdr + 16, 16); // chunk length
  WriteWORD(hdr + 20, 01); // pcm id
  WriteWORD(hdr + 22, WAV_CH); // ch
  WriteDWORD(hdr + 24, pcm.freq); // freq
  WriteDWORD(hdr + 28, pcm.freq * WAV_CH * WAV_BPS); // bytes per sec
  WriteWORD(hdr + 32, WAV_CH * WAV_BPS); // bytes per frame
  WriteWORD(hdr + 34, WAV_BPS * 8 ); // bits

  memcpy(hdr + 36, "data",4);
  WriteDWORD(hdr + 40, pcm_bytesize); // pcm size

  fseek(fp, 0, SEEK_SET);
  fwrite(hdr, 44, 1, fp);

  fseek(fp, 0, SEEK_END);
}

void audio_set_callback_cpu_usage(AUDIO_CPU_USAGE_CALLBACK cb) {
  pcm.cpu_usage_cb = cb;
}