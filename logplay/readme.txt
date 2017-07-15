LOGPLAY(ex. NLGSIM)

■これは？
NLGファイルとS98ファイルをPC上で再生するソフトウェアです。

■NLG楽曲の再生

OPM楽曲の再生
>nlgsim -m 0 OPMSONG.NLG

OPLL楽曲の再生
>nlgsim -m 1 OPLLSONG.NLG

OPM楽曲の再生(fmgen使用)
>nlgsim -m 2 OPMSONG.NLG

■S98再生
OPM/OPLL/OPNAのS98V3に対応しています。
最大8個まで、複数のハードウェアをマッピング可能です。

>nlgsim -m 2 S98SONG.S98

■ライセンス

本ソフトウェアは以下のものを除き修正BSDライセンスで提供されます。

・NEZplug(オリジナルはPDSライセンス)
・YM2151.c/h(ym2151.c独自)
・ymf262.c/h(MAME)
・fmgen(fmgen独自)

■作者
BouKiCHi
