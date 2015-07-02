
#define FADE_NONE 0
#define FADE_RUNNING 1
#define FADE_END 2

struct 
{
    int mode;
	int count;
	float step;
	float volume;
} 
fade = 
{
	0, 0, 0, 1
};


static void fade_init()
{
    fade.mode = FADE_NONE;
	fade.count  = 0;
	fade.step   = 0;
	fade.volume = 1;
}

static void fade_start(int rate, int sec)
{
    fade.mode = FADE_RUNNING;
	fade.count = rate * sec;
	fade.step = ((float)1)/fade.count;
	fade.volume = 1;
	
}

// フェードアウト本体
static void fade_stereo(short *data, int len)
{
	// stereo
	int i = 0;

    // フェード起動前
    if (fade.mode == FADE_NONE)
        return;
    
    // フェード終了後
    if (fade.mode == FADE_END)
    {
        for ( i = 0; i < len * 2; i += 2 )
            data[i] = data[i + 1] = 0;
            return;
    }
    
    // フェード中
	for ( i = 0; i < len * 2; i += 2 )
	{
		data[i]   = ((float)data[i])   * fade.volume;
		data[i+1] = ((float)data[i+1]) * fade.volume;
        
		if (fade.count > 0)
		{
			fade.count--;
            
			if (fade.volume > 0)
				fade.volume -= fade.step;
			else
				fade.volume = 0;
			
			// printf("fv=%f\n",fade_volume);
			
		}
        else
        {
            fade.mode = FADE_END;
			fade.volume = 0;
        }
	}
}

// フェードアウト実行中であれば1
static int fade_is_running(void)
{
	if (fade.mode == FADE_RUNNING)
		return 1;
	
	return 0;
}

// フェード終了であれば1
static int fade_is_end(void)
{
	if (fade.mode == FADE_END)
		return 1;

	return 0;
}

