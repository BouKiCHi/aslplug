

typedef unsigned char  byte;
typedef unsigned short word;
typedef unsigned long  dword;

void WriteWORD(byte *p, word val);
void WriteDWORD(byte *p, dword v);
word ReadWORD(byte *p);
dword ReadDWORD(byte *p);
