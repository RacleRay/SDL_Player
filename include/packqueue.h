#ifndef __PACKQUEUE__H__
#define __PACKQUEUE__H__

#include <atomic>
#include <list>

extern "C" {
#include <SDL.h>
#include <libavcodec/avcodec.h>
}

class PacketQueue {
  public:
    PacketQueue();

    int packetPut(AVPacket *pkt);

    int packetGet(AVPacket *pkt, std::atomic<bool> &quit);

    void packetFlush();

    int packetSize();

  private:
    std::atomic<int> size{0};

    std::list<AVPacket> pkts;
    SDL_mutex *mutex = nullptr;
    SDL_cond *cond = nullptr;
};

#endif //!__PACKQUEUE__H__