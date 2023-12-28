#ifndef __PLAYERCTX__H__
#define __PLAYERCTX__H__

extern "C" {
#include <SDL.h>
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

#include <atomic>
#include <functional>
#include <string>

#include "packqueue.h"

#define MAX_AUDIO_FRAME_SIZE (192000)

#define VIDEO_PICTURE_QUEUE_SIZE (1)

#define FF_BASE_EVENT    (SDL_USEREVENT + 100)
#define FF_REFRESH_EVENT (FF_BASE_EVENT + 20)
#define FF_QUIT_EVENT    (FF_BASE_EVENT + 30)

#define MAX_AUDIOQ_SIZE (5 * 16 * 1024)
#define MAX_VIDEOQ_SIZE (5 * 256 * 1024)

#define AV_SYNC_THRESHOLD   (0.01)
#define AV_NOSYNC_THRESHOLD (10.0)

#define SDL_AUDIO_BUFFER_SIZE (1024)


using ImageCallback = void (*)(unsigned char* data, int w, int h, void *userdata);


struct VideoPicture {
    AVFrame  *bmp = nullptr;
    double pts = 0.0;
};


enum PauseState {
    UNPAUSE = 0,
    PAUSE = 1
};

// ==================================================================================

// 整个播放器的上下文
struct FFmpegPlayerCtx {
    AVFormatContext *formatCtx = nullptr;

    AVCodecContext *audioCodecCtx = nullptr;
    AVCodecContext *videoCodecCtx = nullptr;

    int             videoStreamIdx = -1;
    int             audioStreamIdx = -1;

    AVStream        *audio_stream = nullptr;
    AVStream        *video_stream = nullptr;

    PacketQueue     audioq;
    PacketQueue     videoq;

    uint8_t         audio_buf[(MAX_AUDIO_FRAME_SIZE * 3) / 2];
    unsigned int    audio_buf_size = 0;
    unsigned int    audio_buf_index = 0;
    AVFrame         *audio_frame = nullptr;
    AVPacket        *audio_pkt = nullptr;
    uint8_t         *audio_pkt_data = nullptr;
    int             audio_pkt_size = 0;

    // seek 操作的上下文信息
    std::atomic<int> seek_req {0};
    int              seek_flags;      // direction flag
    int64_t          seek_pos;        // seek position

    // 用以 seek 操作的状态机
    std::atomic<bool> flush_actx {false};
    std::atomic<bool> flush_vctx {false};

    // 用于音视频同步
    double          audio_clock = 0.0;
    double          frame_timer = 0.0;
    double          frame_last_pts = 0.0;
    double          frame_last_delay = 0.0;
    double          video_clock = 0.0;

    // 图像队列
    VideoPicture     pictq[VIDEO_PICTURE_QUEUE_SIZE];
    std::atomic<int> pictq_size {0};
    std::atomic<int> pictq_rindex {0};
    std::atomic<int> pictq_windex {0};
    SDL_mutex       *pictq_mutex = nullptr;
    SDL_cond        *pictq_cond = nullptr;

    char            filename[1024];

    SwsContext      *sws_ctx = nullptr;
    SwrContext      *swr_ctx = nullptr;

    std::atomic<int> pause {PauseState::UNPAUSE};

    // image callback
    ImageCallback   dec_img_callback = nullptr;
    void            *dec_img_cb_data = nullptr;

    void init()
    {
        audio_frame = av_frame_alloc();
        audio_pkt = av_packet_alloc();

        pictq_mutex = SDL_CreateMutex();
        pictq_cond  = SDL_CreateCond();
    }

    void finish()
    {
        if (audio_frame) {
            av_frame_free(&audio_frame);
        }

        if (audio_pkt) {
            av_packet_free(&audio_pkt);
        }

        if (pictq_mutex) {
            SDL_DestroyMutex(pictq_mutex);
        }

        if (pictq_cond) {
            SDL_DestroyCond(pictq_cond);
        }
    }
};

#endif //!__PLAYERCTX__H__