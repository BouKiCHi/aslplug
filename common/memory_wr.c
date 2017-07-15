#include "memory_wr.h"

// 変数書き出し(WORD)
void WriteWORD(byte *p, word v) {
    p[0] = (v & 0xff);
    p[1] = ((v>>8) & 0xff);
}

// 変数書き出し(DWORD)
void WriteDWORD(byte *p, dword v) {
    p[0] = (v & 0xff);
    p[1] = ((v>>8) & 0xff);
    p[2] = ((v>>16) & 0xff);
    p[3] = ((v>>24) & 0xff);
}

// 変数読み出し(WORD)
word ReadWORD(byte *p) {
	return ((word)p[0]) | ((word)p[1])<<8;
}

// 変数読み出し(DWORD)
dword ReadDWORD(byte *p) {
	return
		((dword)p[0]) |
		((dword)p[1])<<8 |
		((dword)p[2])<<16 |
		((dword)p[3])<<24;
}