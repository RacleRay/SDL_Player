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

  private:
    int decode_loop();

    int audio_decode_frame(FFmpegPlayerCtx *is, double *pts_ptr);

    int stream_open(FFmpegPlayerCtx *is, int media_type);

  private:
    FFmpegPlayerCtx *is = nullptr;
};

#endif //!__DEMUX__H__