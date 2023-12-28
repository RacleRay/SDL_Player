#include "audio.h"
#include "log.h"
#include "player.h"


//=================================================================================
// AudioPlay class

// 打开音频设备
// SDL_AudioSpec wanted_spec 参数设置如下：
// wanted_spec.freq = 48000;               // 48kHz
// wanted_spec.format = AUDIO_S16LSB;      // 有符号16位小端格式数据
// wanted_spec.channels = 2;               // 频道数
// wanted_spec.silence = 0;
// wanted_spec.samples = 1024;             // 每次回调的采样数
// wanted_spec.callback = audio_callback;  // 音频数据回调函数（audio_callback）
// wanted_spec.userdata = nullptr;            // 用户私有数据
SDL_AudioDeviceID AudioPlay::openDevice(const SDL_AudioSpec *spec) {
    // desired 参数为期望参数，一般设置该参数
    // obtained 参数，一般设为 nullptr，让SDL严格按照 desired 执行
    m_devId = SDL_OpenAudioDevice(nullptr, 0, spec, nullptr, 0);
    return m_devId;
}

void AudioPlay::start() {
    // 立即开始播放，并回调音频数据
    SDL_PauseAudioDevice(m_devId, 0);
}

void AudioPlay::stop() {
    SDL_PauseAudioDevice(m_devId, 1);
}

//=================================================================================
// AudioDecodeThread

void AudioDecodeThread::setPlayerCtx(FFmpegPlayerCtx *ctx) {
    player_ctx = ctx;
}

// get `len` sized audio data to stream
void AudioDecodeThread::getAudioData(unsigned char *stream, int len) {
    // decoder player_ctx not ready or in pause state, output silence
    if (!player_ctx->audioCodecCtx || player_ctx->pause == PAUSE) {
        memset(stream, 0, len);
        return;
    }

    int remain_size, audio_size;
    double pts;

    while (len > 0) {
        // no more data to move to stream
        if (player_ctx->audio_buf_index >= player_ctx->audio_buf_size) {
            audio_size = audio_decode_frame(player_ctx, &pts);
            if (audio_size < 0) {
                player_ctx->audio_buf_size = 1024;
                memset(player_ctx->audio_buf, 0, player_ctx->audio_buf_size);
            } else {
                player_ctx->audio_buf_size = audio_size;
            }
            player_ctx->audio_buf_index = 0;
        }

        remain_size = player_ctx->audio_buf_size - player_ctx->audio_buf_index;
        if (remain_size > len) {
            remain_size = len;
        }

        memcpy(stream, (uint8_t *)player_ctx->audio_buf + player_ctx->audio_buf_index, remain_size);
        len -= remain_size;
        stream += remain_size;
        player_ctx->audio_buf_index += remain_size;
    }
}

void AudioDecodeThread::run() {
    // do nothing
}

int AudioDecodeThread::audio_decode_frame(FFmpegPlayerCtx *player_ctx, double *pts_ptr) {
    int tmp_len, n, data_size = 0;
    AVPacket *pkt = player_ctx->audio_pkt;
    double pts;
    int ret = 0;

    for (;;) {
        // 从解码上下文取数据帧
        while (player_ctx->audio_pkt_size > 0) {
            ret = avcodec_send_packet(player_ctx->audioCodecCtx, pkt);
            if (ret != 0) {
                // error: just skip frame
                player_ctx->audio_pkt_size = 0;
                break;
            }

            // TODO process multiframe output by one packet
            av_frame_unref(player_ctx->audio_frame);
            ret = avcodec_receive_frame(player_ctx->audioCodecCtx, player_ctx->audio_frame);
            if (ret != 0) {
                // error: just skip frame
                player_ctx->audio_pkt_size = 0;
                break;
            }

            if (ret == 0) {
                int upper_bound_samples = swr_get_out_samples(player_ctx->swr_ctx, player_ctx->audio_frame->nb_samples);
                if (upper_bound_samples < 0) {
                    // error: just skip frame
                    player_ctx->audio_pkt_size = 0;
                    continue;
                }

                uint8_t *out[4] = {nullptr};
                out[0] = (uint8_t *)av_malloc((size_t)upper_bound_samples * 2 * 2);

                // number of samples output per channel
                int samples = swr_convert(
                    player_ctx->swr_ctx,
                    out,
                    upper_bound_samples,
                    (const uint8_t **)player_ctx->audio_frame->data,
                    player_ctx->audio_frame->nb_samples);
                if (samples > 0) {
                    memcpy(player_ctx->audio_buf, out[0], (size_t)samples * 2 * 2);
                }

                av_free(out[0]);

                data_size = samples * 2 * 2;
            }

            tmp_len = pkt->size;
            player_ctx->audio_pkt_data += tmp_len;
            player_ctx->audio_pkt_size -= tmp_len;

            if (data_size <= 0) {
                // No data yet, need more frames
                continue;
            }

            // 根据 audio_clock 更新 pts
            pts = player_ctx->audio_clock;
            *pts_ptr = pts;

            // 乘以 2 的操作表明音频数据的采样格式是每个采样点使用 16 位表示的
            // 在这种情况下，每个采样点使用 16 位（2 个字节）来表示
            n = 2 * player_ctx->audioCodecCtx->ch_layout.nb_channels;
            // 更新 audio_clock
            player_ctx->audio_clock += (double)data_size / (double)(n * (player_ctx->audioCodecCtx->sample_rate));

            return data_size;
        }

        if (m_stop) {
            ff_log_line("request quit while decode audio");
            return -1;
        }

        // demux 线程中，设置清空 buffer 时，条件成立
        if (player_ctx->flush_actx) {
            player_ctx->flush_actx = false;
            ff_log_line("avcodec_flush_buffers(aCodecCtx) for seeking");
            avcodec_flush_buffers(player_ctx->audioCodecCtx);
            continue;
        }

        av_packet_unref(pkt);

        // 从 audioq 中取音频帧
        if (player_ctx->audioq.packetGet(pkt, m_stop) < 0) { return -1; }

        player_ctx->audio_pkt_data = pkt->data;
        player_ctx->audio_pkt_size = pkt->size;

        // 若包中有 pts ，则更新 audio_clock，并转换为以秒为单位
        if (pkt->pts != AV_NOPTS_VALUE) {
            player_ctx->audio_clock = av_q2d(player_ctx->audio_stream->time_base) * (double)pkt->pts;
        }
    }
}
