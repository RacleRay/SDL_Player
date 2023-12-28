
#include <cstdio>
#include <functional>


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

#include "audio.h"
#include "log.h"
#include "player.h"
#include "playerctx.h"
#include "renderview.h"
#include "sdlapp.h"
#include "timer.h"

struct RenderPairData {
    RenderItem *item = nullptr;
    RenderView *view = nullptr;
};

static void
FN_DecodeImage_Cb(unsigned char *data, int w, int h, void *userdata) {
    // 创建纹理
    auto *cbData = (RenderPairData *)userdata;
    if (!cbData->item) {
        cbData->item = cbData->view->createRGB24Texture(w, h);
    }

    // 更新纹理
    cbData->view->updateTexture(cbData->item, data, h);
}

int main(int argc, char **argv) {
    if (argc < 2) {
        ff_log_line("Usage: %s <input file>", argv[0]);
        return -1;
    }
    ff_log_line("%s", "test");

    SDLApp app;

    RenderView view;
    view.initSDL();

    auto *callback_data = new RenderPairData;
    callback_data->view = &view;

    Timer timer;
    auto cb = []() {
        printf("%s", "timer callback");
    };
    timer.start(&cb, 30);

    FFmpegPlayer player;
    player.setFilePath("/home/racle/Dev/sdl/build/Makefile/output.mp4");
    player.setImageCb(FN_DecodeImage_Cb, callback_data);

    ff_log_line("Player init success. start play...");

    player.start();

    app.exec();
    return 0;
}