#include "audio.h"
#include "log.h"
#include "player.h"


// AudioPlay::AudioPlay()
// {

// }

// 打开音频设备
// SDL_AudioSpec wanted_spec;  // 示例
// wanted_spec.freq = 48000;               // 48kHz
// wanted_spec.format = AUDIO_S16LSB;      // 有符号16位小端格式数据
// wanted_spec.channels = 2;               // 频道数
// wanted_spec.silence = 0;                
// wanted_spec.samples = 1024;             // 每次回调的采样数
// wanted_spec.callback = audio_callback;  // 音频数据回调函数（audio_callback）
// wanted_spec.userdata = nullptr;            // 用户私有数据
SDL_AudioDeviceID AudioPlay::openDevice(const SDL_AudioSpec *spec)
{
    // const SDL_AudioSpec *desired, 期望参数
    // SDL_AudioSpec* obtained, 实际参数，一般设为 nullptr，让SDL严格按照 desired 执行
    m_devId = SDL_OpenAudioDevice(nullptr, 0, spec, nullptr, 0);
    return m_devId;
}

void AudioPlay::start()
{
    // 立即开始播放，并回调音频数据
    SDL_PauseAudioDevice(m_devId, 0);
}

void AudioPlay::stop()
{
    SDL_PauseAudioDevice(m_devId, 1);
}



//=================================================================================
// AudioDecodeThread

// AudioDecodeThread::AudioDecodeThread() no
// {

// }

void AudioDecodeThread::setPlayerCtx(FFmpegPlayerCtx *ctx)
{
    player_ctx = ctx;
}

void AudioDecodeThread::getAudioData(unsigned char *stream, int len)
{
    // decoder player_ctx not ready or in pause state, output silence
    if (!player_ctx->aCodecCtx || player_ctx->pause == PAUSE) {
        memset(stream, 0, len);
        return;
    }

    int len1, audio_size;
    double pts;

    while(len > 0) {
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

        len1 = player_ctx->audio_buf_size - player_ctx->audio_buf_index;
        if (len1 > len)
            len1 = len;

        memcpy(stream, (uint8_t *)player_ctx->audio_buf + player_ctx->audio_buf_index, len1);
        len -= len1;
        stream += len1;
        player_ctx->audio_buf_index += len1;
    }
}

void AudioDecodeThread::run()
{
    // do nothing
}

int AudioDecodeThread::audio_decode_frame(FFmpegPlayerCtx *player_ctx, double *pts_ptr)
{
    int len1, data_size = 0, n;
    AVPacket *pkt = player_ctx->audio_pkt;
    double pts;
    int ret = 0;

    for(;;) {
        // 从解码上下文取数据帧
        while (player_ctx->audio_pkt_size > 0) {
            ret = avcodec_send_packet(player_ctx->aCodecCtx, pkt);
            if(ret != 0) {
                // error: just skip frame
                player_ctx->audio_pkt_size = 0;
                break;
            }

            // TODO process multiframe output by one packet
            av_frame_unref(player_ctx->audio_frame);
            ret = avcodec_receive_frame(player_ctx->aCodecCtx, player_ctx->audio_frame);
            if (ret != 0) {
                // error: just skip frame
                player_ctx->audio_pkt_size = 0;
                break;
            }

            if (ret == 0) {
                int upper_bound_samples = swr_get_out_samples(player_ctx->swr_ctx, player_ctx->audio_frame->nb_samples);

                uint8_t *out[4] = {0};
                out[0] = (uint8_t*)av_malloc(upper_bound_samples * 2 * 2);

                // number of samples output per channel
                int samples = swr_convert(player_ctx->swr_ctx,
                                          out,
                                          upper_bound_samples,
                                          (const uint8_t**)player_ctx->audio_frame->data,
                                          player_ctx->audio_frame->nb_samples
                                          );
                if (samples > 0) {
                    memcpy(player_ctx->audio_buf, out[0], samples * 2 * 2);
                }

                av_free(out[0]);

                data_size = samples * 2 * 2;
            }

            len1 = pkt->size;
            player_ctx->audio_pkt_data += len1;
            player_ctx->audio_pkt_size -= len1;

            if (data_size <= 0) {
                // No data yet, need more frames
                continue;
            }

            // 获取到可读的帧

            pts = player_ctx->audio_clock;
            *pts_ptr = pts;
 
            n = 2 * player_ctx->aCodecCtx->ch_layout.nb_channels;
            // 更新已解码数据对应的所处的时间
            player_ctx->audio_clock += (double)data_size / (double)(n * (player_ctx->aCodecCtx->sample_rate));

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
            avcodec_flush_buffers(player_ctx->aCodecCtx);
            continue;
        }

        av_packet_unref(pkt);

        // 从 audioq 中取音频帧
        if (player_ctx->audioq.packetGet(pkt, m_stop) < 0) {
            return -1;
        }

        player_ctx->audio_pkt_data = pkt->data;
        player_ctx->audio_pkt_size = pkt->size;

        // 将音频时钟设置包的 pts ，并转换为以秒为单位
        if (pkt->pts != AV_NOPTS_VALUE) {
            player_ctx->audio_clock = av_q2d(player_ctx->audio_st->time_base) * pkt->pts;
        }
    }
}
