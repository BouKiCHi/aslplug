#ifndef __GMCDRV_H__
#define __GMCDRV_H__

enum {
	GMCDRV_NONE = 0,
	GMCDRV_OPL3 = 1,
	GMCDRV_OPM = 2,
	GMCDRV_OPN3L = 3,
    GMCDRV_OPNA = 4,
};

#ifdef __cplusplus
extern "C" {
#endif

// 初期化と開放
int gimic_init();
void gimic_free();

// 認識デバイス数の取得
int gimic_getcount(void);


// チップのマップ番号を返す。存在しない場合 = -1
int gimic_getchip(int type, int count);

// リセット
void gimic_reset(int mno);

// 書き込み・読み出し
void gimic_write(int mno, int addr, int data);
int gimic_read(int mno, int addr);

// 設定
void gimic_setPLL(int mno, int clock);
void gimic_setSSGvol(int mno, int vol);
    
#ifdef __cplusplus
}
#endif

#endif