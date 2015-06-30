/*********
 s98x.c by BouKiCHi 2014
 2015-02-25 Added priority settings.
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
		printf("Invalid S98 header!!\n");
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

// ヘッダの作成
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
	
    // ポインタ
    struct prio_list *ptr = ctx->prio_top;

    // デバイスがない
    if (!ptr)
        return;
    
    // デバイスマップの作成
    for(i = 0; i < MAX_S98DEV; i++)
    {
        // ptr->index = device_id
        ctx->dev_map[i] = ptr->index;
        
        if (ptr->right)
            ptr = ptr->right;
        else
            break;
    }
    
    // デバイスリスト書き込み
	for(i = 0; i < MAX_S98DEV; i++)
	{
		memset(hdr, 0, 0x20);
        
		WriteDWORD(hdr + 0x00, ctx->dev_type[ctx->dev_map[i]]);
		WriteDWORD(hdr + 0x04, ctx->dev_freq[ctx->dev_map[i]]);
		
		fwrite(hdr, 0x10, 1, ctx->file);
	}
}

// ファイルを開く
S98CTX *CreateS98(const char *file)
{
    
    S98CTX *ctx = (S98CTX *)malloc(sizeof(S98CTX));
    memset(ctx, 0 , sizeof(S98CTX));
    
    
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
    ctx->def_denom = 10000;
    
    return ctx;
}

// マップ終了
void MapEndS98(S98CTX *ctx)
{
    WriteHeaderS98(ctx);
}

// ソートマップを作る
void AddSortMapS98(S98CTX *ctx, int id, int prio)
{
    struct prio_list *obj = &ctx->prio_map[ctx->dev_count];

    // まだマップなし
    if (!ctx->prio_top)
    {
        ctx->prio_top = obj;
        ctx->prio_top->index = id;
        ctx->prio_top->prio = prio;
        return;
    }
    
    struct prio_list *ptr = ctx->prio_top;

    // トップより大きい
    if (prio > ptr->prio)
    {
        ptr->left = obj;
        obj->right = ptr;
        ctx->prio_top = obj;
        
        obj->index = id;
        obj->prio = prio;
        return;
    }
    
    // 下をたどる
    while(ptr->prio > prio)
    {
        // 一番下だった
        if (!ptr->right)
        {
            ptr->right = obj;
            obj->left = ptr;
            
            obj->prio = prio;
            obj->index = id;
            return;
        }
        ptr = ptr->right;
    }
    
    // 途中に挿入する
    // ptr > obj > ptr->right
    
    obj->right = ptr->right;
    obj->left = ptr;
    ptr->right = obj;
    
    obj->prio = prio;
    obj->index = id;
    
}


// デバイスマッピング
int AddMapS98(S98CTX *ctx, int type, int freq, int prio)
{
    if (!ctx)
        return -1;

    int idx = ctx->dev_count;

    ctx->dev_type[idx] = type;
    ctx->dev_freq[idx] = freq;
    
    AddSortMapS98(ctx, idx, prio);
    
    ctx->dev_count++;
    
    return idx;
    
}

// データ書き込み
void WriteDataS98(S98CTX *ctx, int id, int addr, int data)
{
    if (id < 0 || !ctx)
        return;
    
    id = ctx->dev_map[id];

    // レジスタは表と裏が想定されている
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

    ctx->sys_time += ctx->sys_step;
    while((ctx->sys_time - ctx->s98_step) >= 0)
    {
        ctx->sys_time -= ctx->s98_step;
        fputc(S98_CMD_SYNC, ctx->file);
    }
}

// 分母設定
void SetDenomS98(S98CTX *ctx,int denom)
{
    ctx->def_denom = denom;
}

// タイミング設定
void SetTimingS98(S98CTX *ctx, int us)
{
    if (!ctx)
        return;
    
    double step_sec = ((double)us / 1000000);
    
    int denom = ctx->def_denom;
    int nume = (int)(step_sec * denom);
    
    // S98設定値
    ctx->time_nume = nume;
    ctx->time_denom = denom;

    // 積算用数値
    ctx->sys_step = step_sec;
    ctx->s98_step = ((double)nume/denom);
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

