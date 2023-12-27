#include <functional>

#include <cstdio>

#ifdef __cplusplus
#define __STDC_CONSTANT_MACROS
extern "C" {
#endif
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libavutil/avassert.h>
#include <libavutil/channel_layout.h>
#include <libavutil/imgutils.h>
#include <libavutil/mathematics.h>
#include <libavutil/opt.h>
#include <libavutil/time.h>
#include <libavutil/timestamp.h>
#include <libswresample/swresample.h>
#include <libswscale/swscale.h>
#ifdef __cplusplus
}
#endif

#include "log.h"
#include "sdlapp.h"
#include "timer.h"


int main(int argc, char **argv) {
    // if (argc < 2) {
    // ff_log_line("Usage: %s <input file>", argv[0]);
    // }
    ff_log_line("%s", "test");

    SDLApp app;

    Timer timer;
    auto cb = []() { printf("%s", "timer callback"); };
    timer.start(&cb, 30);

    app.exec();
    return 0;
}