#ifndef __AUDIO__H__
#define __AUDIO__H__

#include <SDL.h>

#include "threadbase.h"

class AudioPlay {
  public:
    AudioPlay() = default;

    SDL_AudioDeviceID openDevice(const SDL_AudioSpec *spec);

    void start();

    void stop();

  private:
    SDL_AudioDeviceID m_devId = -1;
};

struct FFmpegPlayerCtx;

class AudioDecodeThread : public ThreadBase {
  public:
    AudioDecodeThread() = default;

    ~AudioDecodeThread() override = default;

    void setPlayerCtx(FFmpegPlayerCtx *ctx);

    void getAudioData(unsigned char *stream, int len);

    void run() override;

  private:
    int audio_decode_frame(FFmpegPlayerCtx *player_ctx, double *pts_ptr);

  private:
    FFmpegPlayerCtx *player_ctx = nullptr;
};

#endif //!__AUDIO__H__