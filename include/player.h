#ifndef __PLAYER__H__
#define __PLAYER__H__

#include "playerctx.h"

class DemuxThread;
class VideoDecodeThread;
class AudioDecodeThread;
class AudioPlay;

class FFmpegPlayer {
  public:
    FFmpegPlayer();

    void setFilePath(const char *filePath);

    void setImageCb(ImageCallback cb, void *userData);

    int initPlayer();

    void start();

    void stop();

    void pause(PauseState state);

  public:
    void onRefreshEvent(SDL_Event *e);
    void onKeyEvent(SDL_Event *e);

  private:
    FFmpegPlayerCtx playerCtx;
    std::string m_filePath;
    SDL_AudioSpec audio_wanted_spec;
    std::atomic<bool> m_stop{false};

  private:
    DemuxThread *m_demuxThread = nullptr;
    VideoDecodeThread *m_videoDecodeThread = nullptr;
    AudioDecodeThread *m_audioDecodeThread = nullptr;
    AudioPlay *m_audioPlay = nullptr;
};

#endif //!__PLAYER__H__