#include <functional>

#include "demux.h"
#include "log.h"
#include "player.h"

void DemuxThread::setPlayerCtx(FFmpegPlayerCtx *ctx) {
    player_ctx = ctx;
}

int DemuxThread::initDemuxThread() {
    AVFormatContext *formatCtx = nullptr;
    // 自动分配上下文 formatCtx
    if (avformat_open_input(&formatCtx, player_ctx->filename, nullptr, nullptr)
        != 0) {
        ff_log_line("avformat_open_input Failed.");
        return -1;
    }
    player_ctx->formatCtx = formatCtx;

    // 查找流信息
    if (avformat_find_stream_info(formatCtx, nullptr) < 0) {
        ff_log_line("avformat_find_stream_info Failed.");
        return -1;
    }

    // 将输入文件信息打印出来
    av_dump_format(formatCtx, 0, player_ctx->filename, 0);

    // 打开音频解码器
    if (stream_open(player_ctx, AVMEDIA_TYPE_AUDIO) < 0) {
        ff_log_line("open audio stream Failed.");
        return -1;
    }

    // 打开视频解码器
    if (stream_open(player_ctx, AVMEDIA_TYPE_VIDEO) < 0) {
        ff_log_line("open video stream Failed.");
        return -1;
    }

    return 0;
}

void DemuxThread::finiDemuxThread() {
    if (player_ctx->formatCtx) {
        avformat_close_input(&player_ctx->formatCtx);
        player_ctx->formatCtx = nullptr;
    }

    if (player_ctx->audioCodecCtx) {
        avcodec_free_context(&player_ctx->audioCodecCtx);
        player_ctx->audioCodecCtx = nullptr;
    }

    if (player_ctx->videoCodecCtx) {
        avcodec_free_context(&player_ctx->videoCodecCtx);
        player_ctx->videoCodecCtx = nullptr;
    }

    if (player_ctx->swr_ctx) {
        swr_free(&player_ctx->swr_ctx);
        player_ctx->swr_ctx = nullptr;
    }

    if (player_ctx->sws_ctx) {
        sws_freeContext(player_ctx->sws_ctx);
        player_ctx->sws_ctx = nullptr;
    }
}

void DemuxThread::run() {
    decode_loop();
}

int DemuxThread::decode_loop() {
    AVPacket *packet = av_packet_alloc();

    for (;;) {
        if (m_stop) {
            ff_log_line("request quit while decode_loop");
            break;
        }

        // begin seek
        //      主线程：读取键盘事件，并设置 seek 参数
        //      demux线程：检查 seek 参数，执行 av_seek_frame
        //      跳转，并清空音视频队列
        if (player_ctx->seek_req) {
            int stream_index = -1;
            int64_t seek_target = player_ctx->seek_pos;

            if (player_ctx->videoStreamIdx >= 0) {
                stream_index = player_ctx->videoStreamIdx;
            } else if (player_ctx->audioStreamIdx >= 0) {
                stream_index = player_ctx->audioStreamIdx;
            }

            if (stream_index >= 0) {
                // 转换时间基，转化到 time_base 的时间基
                seek_target = av_rescale_q(
                    seek_target, AVRational{1, AV_TIME_BASE},
                    player_ctx->formatCtx->streams[stream_index]->time_base);
            }

            // 执行跳转
            if (av_seek_frame(player_ctx->formatCtx, stream_index, seek_target, player_ctx->seek_flags) < 0) {
                ff_log_line("%s: error while seeking\n", player_ctx->filename);
            } else {
                if (player_ctx->audioStreamIdx >= 0) {
                    player_ctx->audioq.packetFlush();
                    player_ctx->flush_actx = true; // 清除音频buffer的 flag
                }
                if (player_ctx->videoStreamIdx >= 0) {
                    player_ctx->videoq.packetFlush();
                    player_ctx->flush_vctx = true;
                }
            }

            // reset to zero when seeking done
            player_ctx->seek_req = 0;
        }

        // 若音视频队列中，数据大于阈值，则延时 10ms , 再读取 AVPacket
        if (player_ctx->audioq.packetSize() > MAX_AUDIOQ_SIZE || player_ctx->videoq.packetSize() > MAX_VIDEOQ_SIZE) {
            SDL_Delay(10);
            continue;
        }

        // 将未解码的数据存储在 AVPacket 中
        if (av_read_frame(player_ctx->formatCtx, packet) < 0) {
            ff_log_line("av_read_frame error");
            break;
        }

        // 视频帧一般为一帧数据，音频帧一般为多个帧
        if (packet->stream_index == player_ctx->videoStreamIdx) {
            player_ctx->videoq.packetPut(packet);
        } else if (packet->stream_index == player_ctx->audioStreamIdx) {
            player_ctx->audioq.packetPut(packet);
        } else { // 字幕等其他 packet 未处理
            av_packet_unref(packet);
        }
    }

    while (!m_stop) { SDL_Delay(100); }

    av_packet_free(&packet);

    SDL_Event event;
    event.type = FF_QUIT_EVENT;
    event.user.data1 = player_ctx;
    SDL_PushEvent(&event);

    return 0;
}

int DemuxThread::stream_open(FFmpegPlayerCtx *player_ctx, int media_type) {
    AVFormatContext *formatCtx = player_ctx->formatCtx;
    AVCodecContext *codecCtx = nullptr;
    AVCodec *codec = nullptr;

    // 根据历史经验返回的最好的一个流，并返回对应流的解码器，此时解码器是未打开状态
    int stream_index = av_find_best_stream(formatCtx, (AVMediaType)media_type, -1, -1, (const AVCodec **)&codec, 0);
    if (stream_index < 0 || stream_index >= (int)formatCtx->nb_streams) {
        ff_log_line("Cannot find a audio stream in the input file\n");
        return -1;
    }

    // 创建解码器上下文
    codecCtx = avcodec_alloc_context3(codec);
    // 将 formatCtx 中的信息，填充到解码器上下文中
    avcodec_parameters_to_context(codecCtx, formatCtx->streams[stream_index]->codecpar);

    // 打开解码器
    if (avcodec_open2(codecCtx, codec, nullptr) < 0) {
        ff_log_line("Failed to open codec for stream #%d\n", stream_index);
        return -1;
    }

    switch (codecCtx->codec_type) {
        case AVMEDIA_TYPE_AUDIO:
            player_ctx->audioStreamIdx = stream_index;
            player_ctx->audioCodecCtx = codecCtx;
            player_ctx->audio_stream = formatCtx->streams[stream_index];
            // 创建 SwrContext 用于处理音频重采样
            player_ctx->swr_ctx = swr_alloc();

            av_opt_set_chlayout(player_ctx->swr_ctx, "in_chlayout", &codecCtx->ch_layout, 0);
            av_opt_set_int(player_ctx->swr_ctx, "in_sample_rate", codecCtx->sample_rate, 0);
            av_opt_set_sample_fmt(player_ctx->swr_ctx, "in_sample_fmt", codecCtx->sample_fmt, 0);

            AVChannelLayout outLayout;
            // use stereo
            av_channel_layout_default(&outLayout, 2);

            av_opt_set_chlayout(player_ctx->swr_ctx, "out_chlayout", &outLayout, 0);
            av_opt_set_int(player_ctx->swr_ctx, "out_sample_rate", 48000, 0);
            av_opt_set_sample_fmt(player_ctx->swr_ctx, "out_sample_fmt", AV_SAMPLE_FMT_S16, 0);

            swr_init(player_ctx->swr_ctx);

            break;
        case AVMEDIA_TYPE_VIDEO:
            player_ctx->videoStreamIdx = stream_index;
            player_ctx->videoCodecCtx = codecCtx;
            player_ctx->video_stream = formatCtx->streams[stream_index];
            player_ctx->frame_timer = (double)av_gettime() / 1000000.0;
            player_ctx->frame_last_delay = 40e-3;
            // 创建 SwsContext 用于视频图像缩放、颜色空间转换
            player_ctx->sws_ctx = sws_getContext(
                codecCtx->width, codecCtx->height, codecCtx->pix_fmt,
                codecCtx->width, codecCtx->height, AV_PIX_FMT_RGB24,
                SWS_BILINEAR, nullptr, nullptr, nullptr);
            break;
        default:
            break;
    }

    return 0;
}
