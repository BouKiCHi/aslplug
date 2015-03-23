#ifndef __LOG_H__
#define __LOG_H__

enum
{
    LOG_MODE_NLG = 0,
    LOG_MODE_S98,
};

enum
{
    LOG_TYPE_NONE = 0,
    LOG_TYPE_SSG = 1, // YM2149
    LOG_TYPE_OPN,    // YM2203
    LOG_TYPE_OPN2,   // YM2612
    LOG_TYPE_OPNA,   // YM2608
    LOG_TYPE_OPM,    // YM2151
    LOG_TYPE_OPLL,   // YM2413
    LOG_TYPE_OPL,    // YM3526
    LOG_TYPE_OPL2,   // YM3812
    LOG_TYPE_OPL3,   // YMF262
    LOG_TYPE_PSG,    // AY-3-8910
    LOG_TYPE_DCSG,   // DCSG
};

enum
{
    LOG_PRIO_PSG = 0,
    LOG_PRIO_OPN,
    LOG_PRIO_OPM,
    LOG_PRIO_OPLL,
    LOG_PRIO_OPL3,
};

enum
{
    LOG_CTC0 = 0,
    LOG_CTC3,
};

#define MAX_MAP 4

typedef struct {
    int mode;
    void *log_ctx;
    
    int fm_count;
    int device_count;
    char map[MAX_MAP];
    int freq[MAX_MAP];
} LOGCTX;

// ログの作成
LOGCTX *CreateLOG(const char *file, int mode);

// ログのクローズ
void CloseLOG(LOGCTX *ctx);


// デバイスの追加(返り値はデバイスID)
int AddMapLOG(LOGCTX *ctx, int type, int freq, int prio);

// デバイス追加の終了
void MapEndLOG(LOGCTX *ctx);

// データ書き込み
void WriteLOG_Data(LOGCTX *ctx, int device, int addr, int data);

// CTC出力
void WriteLOG_CTC(LOGCTX *ctx, int type, int value);

// 時間出力
void WriteLOG_Timing(LOGCTX *ctx, int us);

// シンク出力
void WriteLOG_SYNC(LOGCTX *ctx);

// ラフモード出力
void SetRoughModeLOG(LOGCTX *ctx, int denom);

#endif
