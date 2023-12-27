#ifndef __LOG__H__
#define __LOG__H__

extern "C" {

void ff_log(char *format, ...);

void ff_log_line(const char *format, ...);

}
#endif  //!__LOG__H__