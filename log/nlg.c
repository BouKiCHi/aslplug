/*********
 NLG.C by BouKiCHi 2013-2014
 2013-xx-xx 開始
 2014-02-11 書き込み機能を追加(NLG format 1.10)
 2014-05-18 コンテキスト化
 2014-11-07 ベースクロック設定の追加
 ********/

#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include "nlg.h"

#include "memory_wr.h"

#define NLG_VER (110)
#define NLG_BASECLK (4000000)

// NLGファイルを開く
NLGCTX *OpenNLG(const char *file) {
    byte hdr[0x80];

    NLGCTX *ctx = (NLGCTX *)malloc(sizeof(NLGCTX));

    if (!ctx) {
        printf("Failed to malloc!");
        return NULL;
    }
    memset(ctx, 0 ,sizeof(NLGCTX));

    ctx->file = fopen(file,"rb");

    if (!ctx->file) {
        printf("File open error:%s\n", file);
        free(ctx);
        return NULL;
    }

    fread(hdr, 0x60, 1, ctx->file);

    if (memcmp(hdr, "NLG1", 4) != 0) {
        printf("Unknown format!\n");
        fclose(ctx->file);

        free(ctx);
        return NULL;
    }

    ctx->mode = NLG_READ;
    ctx->version = ReadWORD(hdr + 4);

    memcpy(ctx->title, hdr + 8, 64);

    ctx->title[64]  = 0;

    ctx->baseclk    = (int)ReadDWORD(hdr + 72);
    ctx->tick       = (int)ReadDWORD(hdr + 76);

    ctx->length     = (int)ReadDWORD(hdr + 88);
    ctx->loop_ptr   = (int)ReadDWORD(hdr + 92);

    fseek(ctx->file, 0x60, SEEK_SET);

    ctx->irq_count = 0;
    ctx->ctc0 = ctx->ctc3 = 0;

    return ctx;
}


// ファイルを閉じる
void CloseNLG(NLGCTX *ctx) {
    if (!ctx) return;

    if (ctx->file) {
#ifndef NLG_READONLY
        if (ctx->mode) {
            WriteHeaderNLG(ctx);
        }
#endif
        fclose(ctx->file);
        ctx->file = NULL;
    }

    free(ctx);
}

/////////////////////////////////////////////////
#ifndef NLG_READONLY

void WriteHeaderNLG(NLGCTX *ctx) {
    byte hdr[0x80];

    memset(hdr, 0, 0x80);
    memcpy(hdr, "NLG1", 4);

    int len = (int)strlen(ctx->title);
    if (len > 64) len = 64;

    memcpy(hdr + 8, ctx->title, len);

    WriteWORD(hdr + 0x04, ctx->version); // Version
    WriteDWORD(hdr + 0x48, ctx->baseclk); // BaseCLK
    WriteDWORD(hdr + 0x4c, ctx->tick); // Tick
    WriteDWORD(hdr + 0x58, ctx->length); // Length
    WriteDWORD(hdr + 0x5c, ctx->loop_ptr); // Loop Pointer

    SeekNLG(ctx, 0);
    fwrite(hdr, 0x60, 1, ctx->file);
}

// 書き込み用NLGファイルを開く
NLGCTX *CreateNLG(const char *file) {
    char *name;

    NLGCTX *ctx = malloc(sizeof(NLGCTX));

    if (!ctx) {
        printf("Failed to malloc!");
        return NULL;
    }

    memset(ctx, 0, sizeof(NLGCTX));

    ctx->mode = NLG_WRITE;
    ctx->file = fopen(file, "wb");

    if (!ctx->file) {
        printf("File open error:%s\n", file);
        free(ctx);
        return NULL;
    }

    name = strrchr(file, PATH_SEP);

    int len = 0;
    if (name) {
        name++; // skip seperator
        len = (int)strlen(name);
    } else len = (int)strlen(file);

    if (len > 64) len = 64;

    if (name) strncpy(ctx->title, name, len);
    else strncpy(ctx->title, file, len);

    ctx->title[len] = 0;

    ctx->version = NLG_VER;
    ctx->baseclk = NLG_BASECLK;
    ctx->tick = 0;
    ctx->length = 0;
    ctx->loop_ptr = 0;

    ctx->irq_count = 0;
    ctx->ctc0 = ctx->ctc3 = 0;

    WriteHeaderNLG(ctx);
    return ctx;
}

// ベース周波数の設定
void WriteNLG_SetBaseClock(NLGCTX *ctx, int clock) {
    ctx->baseclk = clock;
}

// コマンドの書き込み
void WriteNLG_CMD(NLGCTX *ctx, int cmd) {
    if (!ctx || !ctx->file) return;
    fputc(cmd, ctx->file);
}

// IRQの書き込み
void WriteNLG_IRQ(NLGCTX *ctx) {
    if (!ctx || !ctx->file) return;
    ctx->irq_count++;
    fputc(CMD_IRQ, ctx->file);
}


// CTC定数の書き込み
// 256clock in 4MHz is 64us
// (CMD_CTC0 * 64us) * (CMD_CTC3)
void WriteNLG_CTC(NLGCTX *ctx, int cmd, int ctc) {
    if (!ctx || !ctx->file) return;
    fputc(cmd, ctx->file);
    fputc(ctc, ctx->file);
}

// データの書き込み
void WriteNLG_Data(NLGCTX *ctx, int cmd, int addr, int data) {
    if (!ctx || !ctx->file) return;
    if (addr >= 0x100) {
        cmd |= 0x40;
        addr &= 0xff;
    }
    fputc(cmd, ctx->file);
    fputc(addr, ctx->file);
    fputc(data, ctx->file);
}

#endif
/////////////////////////////////////////////////


// データの読み出し
int ReadNLG(NLGCTX *ctx) {
    return fgetc(ctx->file);
}

// ファイルポインタの位置を取得
long TellNLG(NLGCTX *ctx) {
    return ftell(ctx->file);
}

// ファイルポインタの位置を設定
void SeekNLG(NLGCTX *ctx, long pos) {
    fseek(ctx->file, pos, SEEK_SET);
}

// タイトルの取得
char *GetTitleNLG(NLGCTX *ctx) {
    return ctx->title;
}

// ティックの取得
int GetTickNLG(NLGCTX *ctx) {
    return ctx->tick;
}

// CTC0値の設定
void SetCTC0_NLG(NLGCTX *ctx, int value) {
    ctx->ctc0 = value;
    ctx->tick = ((ctx->ctc0 * 256) * ctx->ctc3);
}

// CTC3値の設定
void SetCTC3_NLG(NLGCTX *ctx, int value) {
    ctx->ctc3 = value;
    ctx->tick = ((ctx->ctc0 * 256) * ctx->ctc3);
}

// NLGの現在のtickをマイクロ秒で返す
int GetTickUsNLG(NLGCTX *ctx) {
  return (ctx->tick / 4);
}

// 曲の長さを得る
int GetLengthNLG(NLGCTX *ctx) {
    return ctx->length;
}

// ループポインタを得る
int GetLoopPtrNLG(NLGCTX *ctx) {
    return ctx->loop_ptr;
}

// ベースクロックを得る
int GetBaseClkNLG(NLGCTX *ctx) {
    return ctx->baseclk;
}


#ifdef NLG_TEST
// テスト用

int main(int argc,char *argv[]) {
    if (argc < 2) {
        printf("nlgtest <file>\n");
        return -1;
    }

    NLGCTX *ctx = OpenNLG(argv[1]);

    if (!ctx) {
        printf("Failed to open:%s\n", argv[1]);
        return -1;
    }

    printf("Title:%s %d %d %dsec %d\n",
        GetTitleNLG(ctx), GetBaseClkNLG(ctx), GetTickNLG(ctx),
        GetLengthNLG(ctx), GetLoopPtrNLG(ctx));

    int tick = 0;
    int cmd = 0;
    int addr = 0;
    int data = 0;

    while (1) {
        cmd = ReadNLG(ctx);
        if (cmd == EOF) break;

        switch (cmd) {
            case CMD_PSG:
                addr = ReadNLG(ctx); // addr
                data = ReadNLG(ctx); // reg
                printf("PSG:%02X:%02X\n", addr, data);
            break;
            case CMD_OPM:
                addr = ReadNLG(ctx); // addr
                data = ReadNLG(ctx); // reg
                printf("OPM1:%02X:%02X\n", addr, data);
            break;
            case CMD_OPM2:
                addr = ReadNLG(ctx); // addr
                data = ReadNLG(ctx); // reg
                printf("OPM2:%02X:%02X\n", addr, data);
            break;
            case CMD_IRQ:
                tick += GetTickNLG(ctx);
                printf("IRQ:%d\n", GetTickNLG(ctx));
            break;
            case CMD_CTC0:
                data = ReadNLG(ctx);
                SetCTC0_NLG(ctx, data);
                printf("CTC0:%02X\n", data);
            break;
            case CMD_CTC3:
                data = ReadNLG(ctx);
                SetCTC3_NLG(ctx, data);
                printf("CTC3:%02X\n", data);
            break;
        }
    }

    printf("tick = %d\n", tick);

    CloseNLG(ctx);
    ctx = NULL;

    return 0;
}

#endif
