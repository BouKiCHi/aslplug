/*********
 s98x.c by BouKiCHi 2014
 2014-12-29

 This file is no warranty, but free to use. 
 ********/

#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include "s98x.h"

typedef unsigned char  byte;
typedef unsigned short word;
typedef unsigned long  dword;


static void WriteHeaderS98(S98CTX *ctx);

/*
 // 変数書き出し(WORD)
static void WriteWORD(byte *p, word val)
{
    p[0] = (val & 0xff);
    p[1] = ((val>>8) & 0xff);
}

// 変数読み出し(WORD)
static word ReadWORD(byte *p)
{
	return
 ((word)p[0]) |
 ((word)p[1])<<8;
}

*/

// 変数読み出し(DWORD)
static dword ReadDWORD(byte *p)
{
	return
		((dword)p[0]) |
		((dword)p[1])<<8 |
		((dword)p[2])<<16 |
		((dword)p[3])<<24;
}

// ファイルを開く
S98CTX *OpenS98(const char *file)
{
	int i;
	byte hdr[0x80];
    
    S98CTX *ctx = (S98CTX *)malloc(sizeof(S98CTX));
    
    if (!ctx)
    {
        printf("Failed to malloc!");
        return NULL;
    }
    memset(ctx, 0, sizeof(S98CTX));

	ctx->file = fopen(file, "rb");
	
    if (!ctx->file)
	{
		printf("File open error:%s\n", file);
        free(ctx);
		return NULL;
	}
    
    ctx->mode = S98_READMODE;

	fread(hdr, 0x20, 1, ctx->file);
	
	// フォーマット確認
	if (memcmp(hdr, "S98", 3) != 0)
	{
		printf("This is not S98 format!\n");
		fclose(ctx->file);
        
        free(ctx);
		return NULL;
	}
	
	// バージョン確認
	ctx->version = 1;
	if (hdr[3] == '3')
		ctx->version = 3;
	
	// タイミング 
	ctx->time_nume = (int)ReadDWORD(hdr + 0x04);
	ctx->time_denom = (int)ReadDWORD(hdr + 0x08);
	
	// デフォルト値の設定
	if (ctx->time_nume == 0)
		ctx->time_nume = 10;
	
	if (ctx->time_denom == 0)
		ctx->time_denom = 1000;
	
	// ウェイトをus単位に変換する
	ctx->time_us = (int)(((double)ctx->time_nume * S98_US) / ctx->time_denom);
	
	// データ位置の取得
	ctx->dump_ptr = (int)ReadDWORD(hdr + 0x14);
	ctx->dump_loop = (int)ReadDWORD(hdr + 0x18);

	// デバイスカウント	
	ctx->dev_count = (int)ReadDWORD(hdr + 0x1c);
	
	// デバイス情報の取得
	for(i = 0; i < ctx->dev_count && i < MAX_S98DEV; i++)
	{
		fread(hdr, 0x10, 1, ctx->file);
		ctx->dev_type[i] = (int)ReadDWORD(hdr + 0x00);
		ctx->dev_freq[i] = (int)ReadDWORD(hdr + 0x04);
	} 
	// データ先頭までシーク
	fseek(ctx->file, ctx->dump_ptr, SEEK_SET); 
	
	return ctx;
}

// ファイルを閉じる
void CloseS98(S98CTX *ctx)
{
    if (!ctx)
        return;
    
    if (ctx->file)
    {
#ifndef S98_READONLY
        if (ctx->mode != S98_READMODE)
        {
            // 終了マーカーを出力
            fputc(S98_CMD_END, ctx->file);

            WriteHeaderS98(ctx);
        }
#endif
        fclose(ctx->file);
        ctx->file = NULL;
    }
    
    free(ctx);
}




#ifndef S98_READONLY

// 変数書き出し(DWORD)
static void WriteDWORD(byte *p, dword val)
{
    p[0] = (val & 0xff);
    p[1] = ((val>>8) & 0xff);
    p[2] = ((val>>16) & 0xff);
    p[3] = ((val>>24) & 0xff);
}

static void WriteHeaderS98(S98CTX *ctx)
{
	int i;
    byte hdr[0x80];
    
    memset(hdr, 0, 0x80);
    memcpy(hdr, "S983", 4);
    
    WriteDWORD(hdr + 0x04, ctx->time_nume); 
    WriteDWORD(hdr + 0x08, ctx->time_denom); 
    
    // ヘッダ計算
    ctx->dump_ptr = (0x20 + (0x10 * MAX_S98DEV));
    
	WriteDWORD(hdr + 0x14, ctx->dump_ptr); 
	WriteDWORD(hdr + 0x18, ctx->dump_loop); 

	WriteDWORD(hdr + 0x1c, ctx->dev_count); 

    // ヘッダ書き込み
	fseek(ctx->file, 0, SEEK_SET);
	fwrite(hdr, 0x20, 1, ctx->file);
	
    // デバイスリスト書き込み
	for(i = 0; i < MAX_S98DEV; i++)
	{
		memset(hdr, 0, 0x20);
		WriteDWORD(hdr + 0x00, ctx->dev_type[i]);
		WriteDWORD(hdr + 0x04, ctx->dev_freq[i]);
		
		fwrite(hdr, 0x10, 1, ctx->file);
	}
}

// ファイルを開く
S98CTX *CreateS98(const char *file)
{
    
    S98CTX *ctx = (S98CTX *)malloc(sizeof(S98CTX));
    
    if (!ctx)
    {
        printf("Failed to malloc!");
        return NULL;
    }
    memset(ctx, 0, sizeof(S98CTX));

    
    ctx->file = fopen(file, "wb");
    
    if (!ctx->file)
    {
        printf("File open error:%s\n", file);
        free(ctx);
        return NULL;
    }

    ctx->mode = S98_WRITEMODE;
    ctx->dev_count = 0;
    
    WriteHeaderS98(ctx);
    
    return ctx;
}

// デバイスマッピング
int addMapS98(S98CTX *ctx, int type, int freq)
{
    if (!ctx)
        return -1;

    int idx = ctx->dev_count;

    ctx->dev_type[idx] = type;
    ctx->dev_freq[idx] = freq;
    
    ctx->dev_count++;
    
    return idx;
    
}

// データ書き込み
void WriteDataS98(S98CTX *ctx, int id, int addr, int data)
{
    if (id < 0 || !ctx)
        return;

    int cmd = id * 2;

    if (addr > 0xff)
        cmd += 1;
    
    fputc(cmd, ctx->file);
    fputc(addr & 0xff, ctx->file);
    fputc(data & 0xff, ctx->file);
}

// シンク出力
void WriteSyncS98(S98CTX *ctx)
{
    if (!ctx)
        return;
    
    fputc(S98_CMD_SYNC, ctx->file);
}

// タイミング設定
void SetTimingS98(S98CTX *ctx, int us)
{
    if (!ctx)
        return;
    
    double sec = ((double)us / 1000000);
    
    int denom = 10000;
    
    ctx->time_nume = (int)(sec * denom);
    ctx->time_denom = denom;
}


#endif

// データの読み出し
int ReadS98(S98CTX *ctx)
{
	return fgetc(ctx->file);
}

// ファイルポインタの位置を取得
long TellS98(S98CTX *ctx)
{
	return ftell(ctx->file);
}

// ティックの読み出し
int GetTickS98(S98CTX *ctx)
{
    return ctx->time_us;
}


// デバイス数の読み出し
int GetDeviceCountS98(S98CTX *ctx)
{
    return ctx->dev_count;
}


// デバイスタイプの読み出し
int GetDeviceTypeS98(S98CTX *ctx, int idx)
{
    return ctx->dev_type[idx];
}

// デバイス周波数の読み出し
int GetDeviceFreqS98(S98CTX *ctx, int idx)
{
    return ctx->dev_freq[idx];
}


// 可変長データの読み出し
int ReadS98VV(S98CTX *ctx)
{
	int out = 0;
	int v = 0;
	int s = 0;

	do
	{
		v = fgetc(ctx->file);
		out |= (v & 0x7f) << s;
		s += 7;
	}while(v & 0x80);
	
	return out + 2;
}

