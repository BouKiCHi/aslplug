/*
 RC Driver(dummy)
 2016-12-11 ver 0.2
*/

#include <stdio.h>
#include "rcdrv.h"


// ---------------------------------------------------------------------------
//
//

// 初期化と開放
int rc_init() {
	return 0;
}

void rc_free() { }

// 認識デバイス数を得る
int rc_getcount(void) {
    return 0;
}


int rc_getchip(int type, int count) {
	return -1;
}

void rc_reset(int mno) { }
void rc_write(int mno, int addr, int data) { }

int rc_read(int mno, int addr) {
    return 0x00;
}


void rc_setPLL(int mno, int clock) { }
void rc_setSSGvol(int mno, int vol) { }

