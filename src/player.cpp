#include <functional>

#include "player.h"
#include "audio.h"
#include "demux.h"
#include "log.h"
#include "playerctx.h"
#include "sdlapp.h"
#include "videodecode.h"


static double get_audio_clock(FFmpegPlayerCtx *player_ctx)
{
    double pts;
    // hw_buf_size : 已经解码出来但是还没有取走的数据大小
    int hw_buf_size, bytes_per_sec, n;

    pts = player_ctx->audio_clock;    // pts 在解码后就已经更新
    hw_buf_size = player_ctx->audio_buf_size - player_ctx->audio_buf_index;
    bytes_per_sec = 0;
    n = player_ctx->aCodecCtx->ch_layout.nb_channels * 2;

    if(player_ctx->audio_st) {
        bytes_per_sec = player_ctx->aCodecCtx->sample_rate * n;
    }

    // 减去已经解码但是没有被取走的数据所占用的时间
    if (bytes_per_sec) {
        pts -= (double)hw_buf_size / bytes_per_sec;
    }
    return pts;
}

static Uint32 sdl_refresh_timer_cb(Uint32 /*interval*/, void *opaque)
{
    SDL_Event event;
    event.type = FF_REFRESH_EVENT;
    event.user.data1 = opaque;
    SDL_PushEvent(&event);

    // If the callback returns 0, the periodic alarm player_ctx cancelled
    return 0;
}

static void schedule_refresh(FFmpegPlayerCtx *player_ctx, int delay)
{
    SDL_AddTimer(delay, sdl_refresh_timer_cb, player_ctx);
}

static void video_display(FFmpegPlayerCtx *player_ctx)
{
    VideoPicture *vp =  &player_ctx->pictq[player_ctx->pictq_rindex];
    if (vp->bmp && player_ctx->imgCb) {  // decode image : 调用 FN_DecodeImage_Cb ，更新纹理
        player_ctx->imgCb(vp->bmp->data[0], player_ctx->vCodecCtx->width, player_ctx->vCodecCtx->height, player_ctx->cbData);
    }
}

static void FN_Audio_Cb(void *userdata, Uint8 *stream, int len)
{
    AudioDecodeThread *dt = (AudioDecodeThread*)userdata;
    dt->getAudioData(stream, len);
}

void stream_seek(FFmpegPlayerCtx *player_ctx, int64_t pos, int rel)
{
    if (!player_ctx->seek_req) {
        player_ctx->seek_pos = pos;
        player_ctx->seek_flags = rel < 0 ? AVSEEK_FLAG_BACKWARD : 0;
        player_ctx->seek_req = 1;
    }
}

// FFmpegPlayer::FFmpegPlayer()
// {

// }

void FFmpegPlayer::setFilePath(const char *filePath)
{
    m_filePath = filePath;
}

// ImageCallback 函数会使用 userData 进行图像纹理渲染
void FFmpegPlayer::setImageCb(ImageCallback cb, void *userData)
{
    playerCtx.imgCb  = cb;
    playerCtx.cbData = userData;
}

int FFmpegPlayer::initPlayer()
{
    // init ctx
    playerCtx.init();
    strncpy(playerCtx.filename, m_filePath.c_str(), m_filePath.size());

    // create demux thread
    m_demuxThread = new DemuxThread;
    m_demuxThread->setPlayerCtx(&playerCtx);
    if (m_demuxThread->initDemuxThread() != 0) {
        ff_log_line("DemuxThread init Failed.");
        return -1;
    }

    // create audio decode thread
    m_audioDecodeThread = new AudioDecodeThread;
    m_audioDecodeThread->setPlayerCtx(&playerCtx);

    // create video decode thread
    m_videoDecodeThread = new VideoDecodeThread;
    m_videoDecodeThread->setPlayerCtx(&playerCtx);

    // render audio params
    audio_wanted_spec.freq = 48000;
    audio_wanted_spec.format = AUDIO_S16SYS;
    audio_wanted_spec.channels = 2;
    audio_wanted_spec.silence = 0;
    audio_wanted_spec.samples = SDL_AUDIO_BUFFER_SIZE;
    audio_wanted_spec.callback = FN_Audio_Cb;
    audio_wanted_spec.userdata = m_audioDecodeThread;

    // create and open audio play device
    m_audioPlay = new AudioPlay;
    if (m_audioPlay->openDevice(&audio_wanted_spec) <= 0) {
        ff_log_line("open audio device Failed.");
        return -1;
    }

    // install player event
    auto refreshEvent = [this](SDL_Event *e) {
        onRefreshEvent(e);
    };

    auto keyEvent = [this](SDL_Event *e) {
        onKeyEvent(e);
    };

    sdlApp->registerEvent(FF_REFRESH_EVENT, refreshEvent);
    sdlApp->registerEvent(SDL_KEYDOWN, keyEvent);

    return 0;
}

// Player 在主线程运行，不会阻塞任务
void FFmpegPlayer::start()
{
    m_demuxThread->start();
    m_videoDecodeThread->start();
    m_audioDecodeThread->start();
    m_audioPlay->start();

    schedule_refresh(&playerCtx, 40);

    m_stop = false;
}

#define FREE(x) \
    delete x; \
    x = nullptr

void FFmpegPlayer::stop()
{
    m_stop = true;

    // stop audio decode
    ff_log_line("audio decode thread clean...");
    if (m_audioDecodeThread) {
        m_audioDecodeThread->stop();
        FREE(m_audioDecodeThread);
    }
    ff_log_line("audio decode thread finished.");

    // stop audio thread
    ff_log_line("audio play thread clean...");
    if (m_audioPlay) {
        m_audioPlay->stop();
        FREE(m_audioPlay);
    }
    ff_log_line("audio device finished.");

    // stop video decode thread
    ff_log_line("video decode thread clean...");
    if (m_videoDecodeThread) {
        m_videoDecodeThread->stop();
        FREE(m_videoDecodeThread);
    }
    ff_log_line("video decode thread finished.");

    // stop demux thread
    ff_log_line("demux thread clean...");
    if (m_demuxThread) {
        m_demuxThread->stop();
        m_demuxThread->finiDemuxThread();
        FREE(m_demuxThread);
    }
    ff_log_line("demux thread finished.");

    ff_log_line("player ctx clean...");
    playerCtx.finish();
    ff_log_line("player ctx finished.");
}

void FFmpegPlayer::pause(PauseState state)
{
    playerCtx.pause = state;

    // reset frame_timer when restore pause state
    playerCtx.frame_timer = av_gettime() / 1000000.0;
}

void FFmpegPlayer::onRefreshEvent(SDL_Event *e)
{
    if (m_stop) {
        return;
    }

    FFmpegPlayerCtx *player_ctx = (FFmpegPlayerCtx *)e->user.data1;
    VideoPicture *vp;
    double actual_delay, delay, sync_threshold, ref_clock, diff;

    if(player_ctx->video_st) {
        if(player_ctx->pictq_size == 0) {
            schedule_refresh(player_ctx, 1 /*ms*/);
        } else {
            vp = &player_ctx->pictq[player_ctx->pictq_rindex];

            // vp->pts : from video file or decode video clock
            delay = vp->pts - player_ctx->frame_last_pts;   // 当前帧相对于上一帧，应该增加的时间

            if(delay <= 0 || delay >= 1.0) {
                delay = player_ctx->frame_last_delay;
            }

            // 刷新新一帧的时间间隔，最小为 AV_SYNC_THRESHOLD
            sync_threshold = (delay > AV_SYNC_THRESHOLD) ? delay : AV_SYNC_THRESHOLD;

            // save for next time
            player_ctx->frame_last_delay = delay; 
            player_ctx->frame_last_pts = vp->pts;

            ref_clock = get_audio_clock(player_ctx);
            diff = vp->pts - ref_clock;   // 当前帧向对于 音频基准 的时间差

            // >= AV_NOSYNC_THRESHOLD 直接不调整，相当于忽略了问题帧
            if (fabs(diff) < AV_NOSYNC_THRESHOLD) {
                if (diff <= -sync_threshold) {   // 视频慢了超过一帧
                    delay = 0;
                } else if (diff >= sync_threshold) {   // 视频快了超过一帧
                    delay = 2 * delay;
                }
            }

            player_ctx->frame_timer += delay;  // 视频累计时间
            // 视频累计时间和系统时间的差值
            actual_delay = player_ctx->frame_timer - (av_gettime() / 1000000.0);
            // 限制实际延时大于等于 0.01 ，即刷新延时一定大于等于 0.01s
            if (actual_delay < 0.010) {
                actual_delay = 0.010;
            }

            schedule_refresh(player_ctx, (int)(actual_delay * 1000 + 0.5));   // 设置下一次定时刷新事件时间

            video_display(player_ctx);  // 显示当前读取的帧

            // pictq_rindex 一直从 0 读取，重置 read index
            if (++player_ctx->pictq_rindex == VIDEO_PICTURE_QUEUE_SIZE) {
                player_ctx->pictq_rindex = 0;
            }
            SDL_LockMutex(player_ctx->pictq_mutex);
            player_ctx->pictq_size--;
            SDL_CondSignal(player_ctx->pictq_cond);
            SDL_UnlockMutex(player_ctx->pictq_mutex);
        }
    } else {
        schedule_refresh(player_ctx, 100 /*ms*/); 
    }
}

void FFmpegPlayer::onKeyEvent(SDL_Event *e)
{
    double incr, pos;
    switch(e->key.keysym.sym) {
    case SDLK_LEFT:
        incr = -10.0;
        goto do_seek;
    case SDLK_RIGHT:
        incr = 10.0;
        goto do_seek;
    case SDLK_UP:
        incr = 60.0;
        goto do_seek;
    case SDLK_DOWN:
        incr = -60.0;
        goto do_seek;
do_seek:
        if (true) {
            pos = get_audio_clock(&playerCtx);
            pos += incr;
            if (pos < 0) {
                pos = 0;
            }
            ff_log_line("seek to %lf v:%lf a:%lf", pos, get_audio_clock(&playerCtx), get_audio_clock(&playerCtx));
            
            // 设置 seek 参数，这个参数在 demux thread 的 decode_loop 中被检测，执行跳转功能
            // pos * AV_TIME_BASE : 相当于时间单位转化，比如 s 转到 us
            stream_seek(&playerCtx, (int64_t)(pos * AV_TIME_BASE), (int)incr);
        }
        break;
    case SDLK_q:
        // do quit
        ff_log_line("request quit, player will quit");

        // stop player
        stop();

        // quit sdl event loop
        sdlApp->quit();
    case SDLK_SPACE:
        ff_log_line("request pause, cur state=%d", (int)playerCtx.pause);
        if (playerCtx.pause == UNPAUSE) {
            pause(PAUSE);
        } else {
            pause(UNPAUSE);
        }

    default:
        break;
    }
}
