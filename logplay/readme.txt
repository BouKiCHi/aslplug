NLGSIM Ver 1.5

■これは？
NLGファイルとS98ファイルをPC上で再生するソフトウェアです。

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

■履歴
2015-10-18 Ver 1.5
S98/NLG出力機能を追加した。

2015-05-02 Ver 1.4
RTモードを追加した。

2015-03-06 Ver 1.31
SDLのオーディオ再生部分を改善した。

2015-02-14 Ver 1.3
発音部分がnezplug_asl相当になった。
S98V3のOPL3に対応した。
接続される音源の名前を表示できるようにした。

2015-01-01 Ver 1.2
S98V3でOPLLのログを再生できるようにした。
NLGではSSG音源が選択されるようにした。
タイミングの修正。

2014-12-30 Ver 1.1a
Win32バイナリ生成のバグを修正。
ドキュメントの加筆。

2014-12-29 Ver 1.1
fmgen008の追加。
S98ファイルへの対応。
NLG再生時の音量調節(NRTDRV)。

2014-12-09 Ver 1.0
初版。



■作者
BouKiCHi
