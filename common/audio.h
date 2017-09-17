#ifndef __AUDIO_H__
#define __AUDIO_H__

#define PCM_BLOCK 512
#define PCM_NUM_BLOCKS 16
#define PCM_BYTE_PER_SAMPLE 2
#define PCM_CH  2

#define PCM_BLOCK_SIZE (PCM_BLOCK * PCM_CH) // 1つのブロック
#define PCM_BLOCK_BYTES (PCM_BLOCK_SIZE * PCM_BYTE_PER_SAMPLE) // 1ブロックのバイト換算
#define PCM_BUFFER_LEN (PCM_BLOCK_SIZE * PCM_NUM_BLOCKS) // すべてのブロック
#define PCM_WAIT_SIZE (PCM_BUFFER_LEN - (PCM_BLOCK_SIZE)) // このサイズで空きを待つ

typedef double (*AUDIO_CPU_USAGE_CALLBACK)(void); 

// PCM構造体
struct pcm_struct {
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

  int freq;

  int frames;
  int total_frames;
  int seconds;
  int length;

  int update_info;

  int fading;

  int verbose;
  int debug;

  int sdl_open;

  AUDIO_CPU_USAGE_CALLBACK cpu_usage_cb;
};

int audio_init(void);
int audio_sdl_init(void);
int audio_sdl_open(int freq);
int audio_play(void);
int audio_stop(void);
void audio_pause(int pause);

unsigned long audio_get_ticks(void);
void audio_delay(int ms);

void audio_reset_frame(void);
void audio_free(void);

void audio_print_info(void);

int audio_check_poll(void);
int audio_check_fade();

int audio_wait_buffer(void);

void audio_set_handle(void);
void audio_add_frame(int frame);
void audio_next_buffer(void);

void audio_set_length(int length);
void audio_set_frequency(int freq);
void audio_set_debug(int value);
void audio_set_verbose(int value);

int audio_get_frequency(void);

int audio_is_continue(void);
int audio_is_stop(void);

short *audio_get_current_buffer(void);

void audio_next_buffer(void);

void audio_write_wav_header(FILE *fp);

void audio_set_callback_cpu_usage(AUDIO_CPU_USAGE_CALLBACK cb);

#endif