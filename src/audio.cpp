#include "audio.h"
#include "log.h"
#include "player.h"


// 打开音频设备
// SDL_AudioSpec wanted_spec;  // 示例
// wanted_spec.freq = 48000;               // 48kHz
// wanted_spec.format = AUDIO_S16LSB;      // 有符号16位小端格式数据
// wanted_spec.channels = 2;               // 频道数
// wanted_spec.silence = 0;
// wanted_spec.samples = 1024;             // 每次回调的采样数
// wanted_spec.callback = audio_callback;  // 音频数据回调函数（audio_callback）
// wanted_spec.userdata = NULL;            // 用户私有数据
SDL_AudioDeviceID AudioPlay::openDevice(const SDL_AudioSpec *spec)
{
    // const SDL_AudioSpec *desired, 期望参数
    // SDL_AudioSpec* obtained, 实际参数，一般设为 NULL，让SDL严格按照 desired 执行
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

void AudioDecodeThread::setPlayerCtx(FFmpegPlayerCtx *ctx)
{
    is = ctx;
}

void AudioDecodeThread::getAudioData(unsigned char *stream, int len)
{
    // decoder is not ready or in pause state, output silence
    if (!is->aCodecCtx || is->pause == PauseState::PAUSE) {
        memset(stream, 0, len);
        return;
    }

    int len1, audio_size;
    double pts;

    while(len > 0) {
        if (is->audio_buf_index >= is->audio_buf_size) {
            audio_size = audio_decode_frame(is, &pts);
            if (audio_size < 0) {
                is->audio_buf_size = 1024;
                memset(is->audio_buf, 0, is->audio_buf_size);
            } else {
                is->audio_buf_size = audio_size;
            }
            is->audio_buf_index = 0;
        }

        len1 = is->audio_buf_size - is->audio_buf_index;
        if (len1 > len)
            len1 = len;

        memcpy(stream, (uint8_t *)is->audio_buf + is->audio_buf_index, len1);
        len -= len1;
        stream += len1;
        is->audio_buf_index += len1;
    }
}

void AudioDecodeThread::run()
{
    // do nothing
}

int AudioDecodeThread::audio_decode_frame(FFmpegPlayerCtx *is, double *pts_ptr)
{
    int len1, data_size = 0, n;
    AVPacket *pkt = is->audio_pkt;
    double pts;
    int ret = 0;

    for(;;) {
        // 从解码上下文取数据帧
        while (is->audio_pkt_size > 0) {
            ret = avcodec_send_packet(is->aCodecCtx, pkt);
            if(ret != 0) {
                // error: just skip frame
                is->audio_pkt_size = 0;
                break;
            }

            // TODO process multiframe output by one packet
            av_frame_unref(is->audio_frame);
            ret = avcodec_receive_frame(is->aCodecCtx, is->audio_frame);
            if (ret != 0) {
                // error: just skip frame
                is->audio_pkt_size = 0;
                break;
            }

            if (ret == 0) {
                int upper_bound_samples = swr_get_out_samples(is->swr_ctx, is->audio_frame->nb_samples);

                uint8_t *out[4] = {0};
                out[0] = (uint8_t*)av_malloc(upper_bound_samples * 2 * 2);

                // number of samples output per channel
                int samples = swr_convert(is->swr_ctx,
                                          out,
                                          upper_bound_samples,
                                          (const uint8_t**)is->audio_frame->data,
                                          is->audio_frame->nb_samples
                                          );
                if (samples > 0) {
                    memcpy(is->audio_buf, out[0], samples * 2 * 2);
                }

                av_free(out[0]);

                data_size = samples * 2 * 2;
            }

            len1 = pkt->size;
            is->audio_pkt_data += len1;
            is->audio_pkt_size -= len1;

            if (data_size <= 0) {
                // No data yet, need more frames
                continue;
            }

            // 获取到可读的帧

            pts = is->audio_clock;
            *pts_ptr = pts;
            
            n = 2 * is->aCodecCtx->ch_layout.nb_channels;
            // 更新已解码数据对应的所处的时间
            is->audio_clock += (double)data_size / (double)(n * (is->aCodecCtx->sample_rate));

            return data_size;
        }

        if (m_stop) {
            ff_log_line("request quit while decode audio");
            return -1;
        }

        // demux 线程中，设置清空 buffer 时，条件成立
        if (is->flush_actx) {
            is->flush_actx = false;
            ff_log_line("avcodec_flush_buffers(aCodecCtx) for seeking");
            avcodec_flush_buffers(is->aCodecCtx);
            continue;
        }

        av_packet_unref(pkt);

        // 从 audioq 中取音频帧
        if (is->audioq.packetGet(pkt, m_stop) < 0) {
            return -1;
        }

        is->audio_pkt_data = pkt->data;
        is->audio_pkt_size = pkt->size;

        // 将音频时钟设置包的 pts ，并转换为以秒为单位
        if (pkt->pts != AV_NOPTS_VALUE) {
            is->audio_clock = av_q2d(is->audio_st->time_base) * pkt->pts;
        }
    }
}

