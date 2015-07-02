#ifndef __OUTPUT_LOG_H__
#define __OUTPUT_LOG_H__

#ifdef __cplusplus
extern "C" {
#endif


#define LOG_X(msg) output_log("%s:%d %s",__func__, __LINE__, msg);

// log output
void output_log(const char *format, ...);

#ifdef __cplusplus
}
#endif

#endif

