#ifndef __TIMER__H__
#define __TIMER__H__


#include "SDL.h"

using TimeoutCallback = void (*)();

class Timer {
public:
    Timer() = default;

    void start(void* callback, int interval);

    void stop();

private:
    SDL_TimerID m_timerId = 0;
};


#endif  //!__TIMER__H__