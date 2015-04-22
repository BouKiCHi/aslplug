#ifndef S_OPL3_H__
#define S_OPL3_H__

#include "kmsnddev.h"

#ifdef __cplusplus
extern "C" {
#endif
    
#define BASECLOCK_OPL3 14318180

KMIF_SOUND_DEVICE *OPL3SoundAlloc(int use_gmc);

#ifdef __cplusplus
}
#endif

#endif /* S_OPM_H__ */
