#ifndef __GMCDRV_H__
#define __GMCDRV_H__

enum {
	GMCDRV_NONE = 0,
	GMCDRV_OPL3 = 1,
	GMCDRV_OPM = 2,
};

#ifdef __cplusplus
extern "C" {
#endif


int gimic_init();
void gimic_free();

// マップ番号を返す。存在しない場合 = -1
int gimic_getmodule(int type, int count);

void gimic_reset(int mno);

void gimic_write(int mno, int addr, int data);
int gimic_read(int mno, int addr);

void gimic_setPLL(int mno, int clock);
void gimic_setSSGvol(int mno, int vol);
    
#ifdef __cplusplus
}
#endif

#endif