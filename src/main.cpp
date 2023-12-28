
#include <cstdio>
#include <functional>

extern "C" {
// #define __STDC_CONSTANT_MACROS
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
}

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

// image callback
// w : image width,  h : image height
static void ON_DecodeImage_Cb(unsigned char *data, int w, int h, void *dec_img_param) {
    auto *cbData = (RenderPairData *)dec_img_param;
    if (!cbData->item) {
        cbData->item = cbData->view->createRGB24Texture(w, h);
    }
    cbData->view->updateTexture(cbData->item, data, h);
}

// 主线程执行事件循环
int main(int argc, char **argv) {
    if (argc < 2) {
        ff_log_line("usage: %s media_file_absolute_path", "./SDL_Player");
        return -1;
    }

    SDLApp app;

    RenderView view;
    view.initSDL();

    auto *dec_img_params = new RenderPairData;
    dec_img_params->view = &view;

    Timer timer;
    std::function<void()> cb = std::bind(&RenderView::onRefresh, &view);
    timer.start(&cb, 30);

    FFmpegPlayer player;
    player.setFilePath(argv[1]);
    // player.setFilePath("/home/racle/Multimedia/dev/SDL_Player/build/output.mp4");
    player.setImageCb(ON_DecodeImage_Cb, dec_img_params);
    if (player.initPlayer() != 0) {
        return -1;
    }

    ff_log_line("FFmpegPlayer init success");

    player.start();

    return app.exec();
}
