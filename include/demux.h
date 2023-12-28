#ifndef __DEMUX__H__
#define __DEMUX__H__

#include "playerctx.h"
#include "threadbase.h"

// 多媒体文件解封装线程，负责创建上下文、打开输入、读取帧
class DemuxThread : public ThreadBase {
  public:
    DemuxThread() = default;

    void setPlayerCtx(FFmpegPlayerCtx *ctx);

    int initDemuxThread();

    void finiDemuxThread();

    void run() override;

    ~DemuxThread() override = default;

  private:
    int decode_loop();

    int audio_decode_frame(FFmpegPlayerCtx *player_ctx, double *pts_ptr);

    int stream_open(FFmpegPlayerCtx *player_ctx, int media_type);

  private:
    FFmpegPlayerCtx *player_ctx = nullptr;
};

#endif //!__DEMUX__H__