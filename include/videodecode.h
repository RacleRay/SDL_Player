#ifndef __VIDEODECODE__H__
#define __VIDEODECODE__H__

#include "threadbase.h"

struct FFmpegPlayerCtx;
struct AVFrame;

// 从视频队列读取数据，解码数据，把解码后的数据 AVFrame 放到图像队列 pictq 中
class VideoDecodeThread : public ThreadBase {
  public:
    VideoDecodeThread() = default;

    ~VideoDecodeThread() override = default;

    void setPlayerCtx(FFmpegPlayerCtx *ctx);

    void run() override;

  private:
    int video_entry();

    int queue_picture(FFmpegPlayerCtx *player_ctx, AVFrame *pFrame, double pts);

  private:
    FFmpegPlayerCtx *m_player_context = nullptr;
};

#endif //!__VIDEODECODE__H__