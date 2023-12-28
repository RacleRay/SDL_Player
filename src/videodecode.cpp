#include "videodecode.h"
#include "log.h"
#include "player.h"

static double
synchronize_video(FFmpegPlayerCtx *player_ctx, AVFrame *src_frame, double pts) {
    double frame_delay;

    if (pts != 0) {
        // if we have pts, set video clock to it
        player_ctx->video_clock = pts;
    } else {
        // if we aren't given a pts, set it to the clock
        pts = player_ctx->video_clock;
    }
    // update the video clock
    frame_delay = av_q2d(player_ctx->videoCodecCtx->time_base);
    // if we are repeating a frame, adjust clock accordingly
    frame_delay += src_frame->repeat_pict * (frame_delay * 0.5);
    player_ctx->video_clock += frame_delay;

    return pts;
}

//=================================================================================
// VideoDecodeThread class

void VideoDecodeThread::setPlayerCtx(FFmpegPlayerCtx *ctx) {
    m_player_context = ctx;
}

void VideoDecodeThread::run() {
    int ret = video_entry();
    ff_log_line("VideoDecodeThread finished, ret=%d", ret);
}

int VideoDecodeThread::video_entry() {
    FFmpegPlayerCtx *player_ctx = m_player_context;
    AVPacket *packet = av_packet_alloc();
    AVCodecContext *pCodecCtx = player_ctx->videoCodecCtx;
    int ret = -1;
    double pts = 0;

    AVFrame *pFrame = av_frame_alloc();
    AVFrame *pFrameRGB = av_frame_alloc();

    av_image_alloc(
        pFrameRGB->data, pFrameRGB->linesize, pCodecCtx->width,
        pCodecCtx->height, AV_PIX_FMT_RGB24, 32);

    for (;;) {
        if (m_stop) { break; }

        if (player_ctx->pause == PAUSE) {
            SDL_Delay(5);
            continue;
        }

        if (player_ctx->flush_vctx) {
            ff_log_line("avcodec_flush_buffers(vCodecCtx) for seeking");
            avcodec_flush_buffers(player_ctx->videoCodecCtx);
            player_ctx->flush_vctx = false;
            continue;
        }

        av_packet_unref(packet);

        if (player_ctx->videoq.packetGet(packet, m_stop) < 0) { break; }

        // Decode video frame
        ret = avcodec_send_packet(pCodecCtx, packet);
        if (ret == 0) { ret = avcodec_receive_frame(pCodecCtx, pFrame); }

        if (packet->dts == AV_NOPTS_VALUE && pFrame->opaque && *(uint64_t *)pFrame->opaque != AV_NOPTS_VALUE) {
            pts = (double)*(uint64_t *)pFrame->opaque;
        } else if (packet->dts != AV_NOPTS_VALUE) {
            pts = (double)packet->dts;
        } else {
            pts = 0;
        }
        pts *= av_q2d(player_ctx->video_stream->time_base);

        // frame ready
        if (ret == 0) {
            ret = sws_scale(
                player_ctx->sws_ctx, 
                (uint8_t const *const *)pFrame->data,
                pFrame->linesize, 
                0, 
                pCodecCtx->height, 
                pFrameRGB->data,
                pFrameRGB->linesize);

            pts = synchronize_video(player_ctx, pFrame, pts);

            if (ret == pCodecCtx->height) {
                if (queue_picture(player_ctx, pFrameRGB, pts) < 0) { break; }
            }
        }
    }

    av_frame_free(&pFrame);
    av_frame_free(&pFrameRGB);
    av_packet_free(&packet);

    return 0;
}

int VideoDecodeThread::queue_picture(
    FFmpegPlayerCtx *player_ctx, AVFrame *pFrame, double pts) {
    VideoPicture *vp;

    // wait until we have space for a new pic
    SDL_LockMutex(player_ctx->pictq_mutex);
    while (player_ctx->pictq_size >= VIDEO_PICTURE_QUEUE_SIZE) {
        // 带 timeout，为了方便程序随时退出
        // 保持 queue 读取一帧，queue 渲染一帧
        SDL_CondWaitTimeout(
            player_ctx->pictq_cond, player_ctx->pictq_mutex, 500);
        if (m_stop) { break; }
    }
    SDL_UnlockMutex(player_ctx->pictq_mutex);

    if (m_stop) { return 0; }

    // windex player_ctx set to 0 initially
    vp = &player_ctx->pictq[player_ctx->pictq_windex];

    if (!vp->bmp) {
        SDL_LockMutex(player_ctx->pictq_mutex);
        vp->bmp = av_frame_alloc();
        // 给 AVFrame 分配图像数据
        av_image_alloc(
            vp->bmp->data, 
            vp->bmp->linesize, 
            player_ctx->videoCodecCtx->width,
            player_ctx->videoCodecCtx->height, 
            AV_PIX_FMT_RGB24, 
            32);
        // 或者使用 av_frame_get_buffer()
        // 既可以给视频分配空间，也可以给音频分配空间
        SDL_UnlockMutex(player_ctx->pictq_mutex);
    }

    // Copy the pic data and set pts
    memcpy(vp->bmp->data[0], pFrame->data[0], (size_t)player_ctx->videoCodecCtx->height * pFrame->linesize[0]);
    vp->pts = pts;

    // now we inform our display thread that we have a pic ready
    if (++player_ctx->pictq_windex == VIDEO_PICTURE_QUEUE_SIZE) {
        player_ctx->pictq_windex = 0;
    }

    SDL_LockMutex(player_ctx->pictq_mutex);
    player_ctx->pictq_size++;
    SDL_UnlockMutex(player_ctx->pictq_mutex);

    return 0;
}
