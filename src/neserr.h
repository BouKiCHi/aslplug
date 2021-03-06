#ifndef NESERR_H__
#define NESERR_H__

typedef enum {
	NESERR_NOERROR,
	NESERR_SHORTOFMEMORY,
	NESERR_FORMAT,
	NESERR_PARAMETER
} NESERR_CODE;

#endif /* NESERR_H__ */

#ifndef WIN32
#include <assert.h>
#define ASSERT(x) assert(x)
#else
#define ASSERT(x)
#endif
