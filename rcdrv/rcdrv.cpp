/*
 RC Driver
 2016-12-11 ver 0.2
*/

#include <stdio.h>
#include <windows.h>
// c86ctl
#include "c86ctl.h"
// scci
#include "scci.h"
#include "SCCIDefines.h"
//
#include "rcdrv.h"


static HMODULE g_mod = 0;
static HMODULE g_scci = 0;
static IRealChipBase *g_chipbase = 0;

SCCIFUNC funcGetSoundInterfaceManager = NULL;

struct C86XDRV {
	SoundChip *sndchip;
	IGimic *gimic;
	IRealChip *realchip;
	int	type;
	bool reserve;
};

// 最大モジュール数
#define MAX_GMCMOD 16

static C86XDRV c86xdrv[MAX_GMCMOD];
static int rc_count = 0;

static bool LoadC86X() {
	if (g_mod) return true;

	g_mod = ::LoadLibrary("c86ctl.dll");

	if (!g_mod) return false;

	// インスタンス作成を呼び出す
	typedef HRESULT (WINAPI *TCreateInstance)( REFIID, LPVOID* );
	TCreateInstance pCI;
	pCI = (TCreateInstance)::GetProcAddress(g_mod, "CreateInstance");

	if (pCI) {
		// チップを列挙するクラスの取得
		(*pCI)(IID_IRealChipBase, (void**)&g_chipbase);
		
		// 成功したら初期化
		if (g_chipbase) g_chipbase->initialize();
		else {
			::FreeLibrary( g_mod );
			g_mod = 0;
		}
	} else {
		// 失敗
		::FreeLibrary( g_mod );
		g_mod = 0;
	}
	
	return g_mod ? true : false;
}

static void FreeC86X() {
	if (g_chipbase) {
		g_chipbase->deinitialize();
		g_chipbase = 0;
	}
	
	if (g_mod) {
		::FreeLibrary( g_mod );
		g_mod = 0;
	}
}


static bool LoadSCCI() {
	g_scci = ::LoadLibrary("scci");
	if (g_scci == NULL) return false;

	funcGetSoundInterfaceManager = (SCCIFUNC)(::GetProcAddress(g_scci,"getSoundInterfaceManager"));

	if(funcGetSoundInterfaceManager == NULL) {
		printf("Failed to get the address of getSoundInterfaceManager\n");
		::FreeLibrary(g_scci);
		g_scci = NULL;
		return false;
	}

	return g_scci ? true : false;
}

static void FreeSCCI() {
	if (!g_scci) return;

	funcGetSoundInterfaceManager = NULL;

	::FreeLibrary(g_scci);
	g_scci = 0;
}

// ---------------------------------------------------------------------------
//
//

// デバイス名の確認と追加
static int rc_check_devname(IGimic *gimic, IRealChip *realchip,
            const char *devname, const char *gmc_id, int chiptype) {

    if (strcmp(devname, gmc_id) != 0) return 0;

	// 表示
	::printf("Added:%s\n", devname);
	
	// インターフェースの設定
	int pos = rc_count;
	
	c86xdrv[pos].gimic = gimic;
	c86xdrv[pos].realchip = realchip;
	c86xdrv[pos].type = chiptype;
	realchip->reset();
	
	rc_count++;

    // 最大個数に達した場合は終了
    if (rc_count >= MAX_GMCMOD) return -1;
    
    return 0;
}

static void rc_add_scci() {
	if (!funcGetSoundInterfaceManager) return;
	SoundInterfaceManager *pManager = funcGetSoundInterfaceManager();
	if (pManager == NULL) return;

	pManager->initializeInstance();
	pManager->reset();
	pManager->setDelay(20);

	int InterfaceCount = pManager->getInterfaceCount();

    // 個数分、繰り返す
	for (int i = 0; i < InterfaceCount; i++) {
		SoundInterface *si = pManager->getInterface(i);
		int ChipCount = si->getSoundChipCount();
		for(int j = 0; j < ChipCount; j++) {
			// 最大個数に達した場合は終了
			if (rc_count >= MAX_GMCMOD) break;

			SoundChip *chip = si->getSoundChip(j);
			SCCI_SOUND_CHIP_INFO *chipinfo = chip->getSoundChipInfo();

			int type = -1;
			switch(chipinfo->iSoundChip) {
				case SC_TYPE_AY8910:
				case SC_TYPE_YMZ294:
					type = RCDRV_PSG;
				break;
				case SC_TYPE_YM2151:
					type = RCDRV_OPM;
				break;
				case SC_TYPE_YM2413:
					type = RCDRV_OPLL;
				break;
				case SC_TYPE_YMF274:
				case SC_TYPE_YMF262:
					type = RCDRV_OPL3;
				break;
				case SC_TYPE_YM2608:
				case SC_TYPE_YMF288:
					type = RCDRV_OPNA;
				break;
			}

			if (type < 0) continue;

			c86xdrv[rc_count].type = type;
			c86xdrv[rc_count].sndchip = chip;
			rc_count++;

		}
	}
}

static void rc_add_c86x() {
	// チップの個数を得る
	if (!g_chipbase) return;
	int numOfChips = g_chipbase->getNumberOfChip();

    // ::printf("numOfChips:%d\n", numOfChips);
    
    // チップの個数分、繰り返す
	for (int i = 0; i < numOfChips; i++) {
		IGimic *gimic;
		IRealChip *chip;
		
		// インターフェースの取得
		g_chipbase->getChipInterface( i, IID_IGimic, (void**)&gimic );
		g_chipbase->getChipInterface( i, IID_IRealChip, (void**)&chip );
		
		// モジュール上方の取得
		Devinfo info;
		gimic->getModuleInfo( &info );
		
		// モジュール名の文字列化
		char devname[256];
        int len = sizeof(info.Devname);
        strncpy(devname, info.Devname, len);
        devname[len] = 0x00;
        
        // ::printf("Module:%s\n", devname);
        
        // GMC-OPL3
        if (rc_check_devname(gimic, chip, devname, "GMC-OPL3", RCDRV_OPL3) < 0)
            break;
        
        // GMC-OPM
        if (rc_check_devname(gimic, chip, devname, "GMC-OPM", RCDRV_OPM) < 0)
            break;

        // GMC-OPN3L
        if (rc_check_devname(gimic, chip, devname, "GMC-OPN3L", RCDRV_OPN3L) < 0)
            break;

        // GMC-OPNA
        if (rc_check_devname(gimic, chip, devname, "GMC-OPNA", RCDRV_OPNA) < 0)
            break;
    }
}

// 初期化する
int rc_init(int flag) {
	int result = -1;

	if (flag & RCDRV_FLAG_C86X) if (LoadC86X()) result = 0;
	if (flag & RCDRV_FLAG_SCCI) if (LoadSCCI()) result = 0;

	if (result < 0) return -1;
		
	// 初期化
	rc_count = 0;
	memset(c86xdrv, 0, sizeof(c86xdrv));

	// デバイス追加
	rc_add_scci();	
	rc_add_c86x();
	
	// 追加できるチップが存在しなかった
	if (rc_count == 0) return -1;
    
	return 0;
}

// 開放する
void rc_free() {
    int i;
    for(i = 0; i < rc_count; i++) {
			// if (c86xdrv[i].sndchip) c86xdrv[i].sndchip->init();
			if (c86xdrv[i].realchip) c86xdrv[i].realchip->reset();
    }

    FreeC86X();
    FreeSCCI();
    rc_count = 0;
}

// デバイス数を得る
int rc_getcount(void) {
    return rc_count;
}

// typeのチップのcount個目のidを得る
int rc_getchip(int type, int count) {
	int i;
	for(i = 0; i < rc_count; i++) {
		if (c86xdrv[i].type == type) {
				if (count > 0) count--; 
				else return i;
    }
	}
	return -1;
}

// リセットする
void rc_reset(int mno) {
	if (mno < 0) return;
	// if (c86xdrv[mno].sndchip) c86xdrv[mno].sndchip->init();
	if (c86xdrv[mno].realchip) c86xdrv[mno].realchip->reset();
}

// 書き込む
void rc_write(int mno, int addr, int data) {
	if (mno < 0) return;
	if (c86xdrv[mno].sndchip) c86xdrv[mno].sndchip->setRegister(addr, data);
	if (c86xdrv[mno].realchip) c86xdrv[mno].realchip->out(addr, data);
}

// 読み出す
int rc_read(int mno, int addr) {
	if (mno < 0) return 0x00;

	if (c86xdrv[mno].sndchip) return c86xdrv[mno].sndchip->getRegister(addr);
	if (c86xdrv[mno].realchip) return c86xdrv[mno].realchip->in(addr);
	return 0x00;
}

// PLLの周波数設定
void rc_setPLL(int mno, int clock) {
	if (mno < 0) return;
	if (c86xdrv[mno].gimic) c86xdrv[mno].gimic->setPLLClock( clock );
}

// SSGボリュームの設定
void rc_setSSGvol(int mno, int vol) {
	if (mno < 0) return;
	if (c86xdrv[mno].gimic) c86xdrv[mno].gimic->setSSGVolume( vol );
}

