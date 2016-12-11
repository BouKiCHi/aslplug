#ifndef __RCDRV_H__
#define __RCDRV_H__

enum {
	RCDRV_NONE = 0,
	RCDRV_OPL3 = 1,
	RCDRV_OPM = 2,
	RCDRV_OPN3L = 3,
  RCDRV_OPNA = 4,
	RCDRV_PSG = 5,
	RCDRV_OPLL = 6,
};

#ifdef __cplusplus
extern "C" {
#endif

#define RCDRV_FLAG_C86X 1
#define RCDRV_FLAG_SCCI 2
#define RCDRV_FLAG_ALL 3

// 初期化と開放
int rc_init(int flag);
void rc_free();

// 認識デバイス数の取得
int rc_getcount(void);


// チップのマップ番号を返す。存在しない場合 = -1
int rc_getchip(int type, int count);

// リセット
void rc_reset(int mno);

// 書き込み・読み出し
void rc_write(int mno, int addr, int data);
int rc_read(int mno, int addr);

// 設定
void rc_setPLL(int mno, int clock);
void rc_setSSGvol(int mno, int vol);
    
#ifdef __cplusplus
}
#endif

#endif