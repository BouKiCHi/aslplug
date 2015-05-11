/*
 GIMIC driver(dummy)
 2015-04-22 ver 0.11
*/

#include <stdio.h>
#include "gmcdrv.h"


// ---------------------------------------------------------------------------
//
//

// 初期化と開放
int gimic_init()
{
	return 0;
}

void gimic_free()
{
}

// 認識デバイス数を得る
int gimic_getcount(void)
{
    return 0;
}


int gimic_getchip(int type, int count)
{
	return -1;
}

void gimic_reset(int mno)
{
}

void gimic_write(int mno, int addr, int data)
{
}

int gimic_read(int mno, int addr)
{
    return 0x00;
}


void gimic_setPLL(int mno, int clock)
{
}

void gimic_setSSGvol(int mno, int vol)
{
}

