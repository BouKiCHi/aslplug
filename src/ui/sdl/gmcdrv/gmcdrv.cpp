/*
 GIMIC driver for nezplay asl
 2015-04-22 ver 0.11
 m88 GIMIC対応版のソースを多いに参考にさせていただきましたが、
 BSDライセンスということで一つよろしくお願いいたします。BouKiCHi

*/

#include <stdio.h>
#include <windows.h>
#include "c86ctl.h"
#include "gmcdrv.h"


static HMODULE			g_mod = 0;
static IRealChipBase	*g_chipbase = 0;

struct GMCDRV
{
	IGimic *gimic;
	IRealChip *chip;
	int	type;
	bool reserve;
};

// 最大モジュール数
#define MAX_GMCMOD 4

static GMCDRV gmcdrv[4];
static int gmcdrv_count = 0;


// DLLの呼び出し
static bool LoadDLL()
{
	// まだDLLがロードされてない
	if (!g_mod) 
	{
		g_mod = ::LoadLibrary( "c86ctl.dll" );

		if (!g_mod)
			return false;

		// インスタンス作成を呼び出す
		typedef HRESULT (WINAPI *TCreateInstance)( REFIID, LPVOID* );
		TCreateInstance pCI;
		pCI = (TCreateInstance)::GetProcAddress( g_mod, "CreateInstance" );

		if (pCI)
		{
			// チップを列挙するクラスの取得
			(*pCI)(IID_IRealChipBase, (void**)&g_chipbase);
			
			// 成功したら初期化
			if (g_chipbase) 
			{
				g_chipbase->initialize();
			} 
			else
			{
				::FreeLibrary( g_mod );
				g_mod = 0;
			}
		} 
		else 
		{
			// 失敗
			::FreeLibrary( g_mod );
			g_mod = 0;
		}
	}
	return g_mod ? true : false;
}

// DLLの開放
static void FreeDLL()
{
	if (g_chipbase) 
	{
		g_chipbase->deinitialize();
		g_chipbase = 0;
	}
	
	if (g_mod) 
	{
		::FreeLibrary( g_mod );
		g_mod = 0;
	}
}

// ---------------------------------------------------------------------------
//
//

int gimic_init()
{
	int i = 0;

    ::printf("gimic_init()\n");

	if (!LoadDLL())
		return -1;
		
	// 初期化
	gmcdrv_count = 0;
	memset(gmcdrv, 0, sizeof(gmcdrv));
	
	// チップの個数を得る
	int numOfChips = g_chipbase->getNumberOfChip();

    ::printf("numOfChips:%d\n", numOfChips);
    
    // チップの個数分、繰り返す
	for (i = 0; i < numOfChips; i++) 
	{
		IGimic *gimic;
		IRealChip *chip;
		
		// インターフェースの取得
		g_chipbase->getChipInterface( i, IID_IGimic, (void**)&gimic );
		g_chipbase->getChipInterface( i, IID_IRealChip, (void**)&chip );
		
		// モジュール上方の取得
		Devinfo info;
		gimic->getModuleInfo( &info );
		
		// モジュール名の表示
		char buf[256];
        int len = sizeof(info.Devname);
        strncpy(buf, info.Devname, len);
        buf[len] = 0x00;
        
        ::printf("Module:%s\n", buf);
        
        // GMC-OPL3
        if ( strncmp( info.Devname, "GMC-OPL3", sizeof(info.Devname) ) == 0 ) 
        {
        	// 表示
        	::printf("Added:OPL3\n");

        	
        	// インターフェースの設定
        	int pos = gmcdrv_count;

        	gmcdrv[pos].gimic = gimic;
        	gmcdrv[pos].chip = chip;
			gmcdrv[pos].type = GMCDRV_OPL3;
			chip->reset();
			
			// 最大個数に達した場合は終了
			if (gmcdrv_count < MAX_GMCMOD)
				gmcdrv_count++;
			else
				break;
		}
        
        // GMC-OPM
        if ( strncmp( info.Devname, "GMC-OPM", sizeof(info.Devname) ) == 0 )
        {
            // 表示
            ::printf("Added:OPM\n");
            
            
            // インターフェースの設定
            int pos = gmcdrv_count;
            
            gmcdrv[pos].gimic = gimic;
            gmcdrv[pos].chip = chip;
            gmcdrv[pos].type = GMCDRV_OPM;
            chip->reset();
            
            // 最大個数に達した場合は終了
            if (gmcdrv_count < MAX_GMCMOD)
                gmcdrv_count++;
            else
                break;
        }

	}
	
	// チップが存在しなかった
	if (gmcdrv_count == 0) 
		return -1;
    
	return 0;
}

void gimic_free()
{
    FreeDLL();
}

// count個目のチップを得る
int gimic_getmodule(int type, int count)
{
	int i;
	for(i = 0; i < gmcdrv_count; i++)
	{
		if (gmcdrv[i].type == type)
        {
            if (count > 0)
                count--;
            else
                return i;
        }
	}
	return -1;
}

void gimic_reset(int mno)
{
	if (mno < 0)
		return;

	gmcdrv[mno].chip->reset();
}

void gimic_write(int mno, int addr, int data)
{
	if (mno < 0)
		return;
	
	gmcdrv[mno].chip->out(addr, data);
}

int gimic_read(int mno, int addr)
{
	if (mno < 0)
		return 0x00;
	
	return gmcdrv[mno].chip->in(addr);
}


void gimic_setPLL(int mno, int clock)
{
	if (mno < 0)
	return;
	
	gmcdrv[mno].gimic->setPLLClock( clock );
}

void gimic_setSSGvol(int mno, int vol)
{
	if (mno < 0)
	return;
	
	gmcdrv[mno].gimic->setSSGVolume( vol );
}

