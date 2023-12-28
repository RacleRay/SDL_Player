#include "timer.h"

// 返回 Timer 设置的 interval 值。若返回 0 ，则取消此定时器；否则返回下一次
// interval 的值 NOTE:
//     若 SDL
//     定时器在另一个线程上执行，并且要将回调函数和事件循环在一个线程内执行，
//     则需要在 callback 中将事件 post 给事件循环所在线程。
static Uint32 registerUsereventCb(Uint32 interval, void *cb_ptr) {
    SDL_Event event;
    SDL_UserEvent userevent;

    userevent.type = SDL_USEREVENT;
    userevent.code = 0;
    userevent.data1 = cb_ptr;
    userevent.data2 = nullptr;

    event.type = SDL_USEREVENT;
    event.user = userevent;

    SDL_PushEvent(&event);

    return interval;
}

//=================================================================================
// Timer class

void Timer::start(void *cb, int interval) {
    if (m_timerId != 0) { return; }

    // add new timer : 精度依赖于操作系统的调度精度，属于典型的非实时定时器
    //      callbackfunc 内将注册 SDL_USEREVENT，并关联 cb 参数
    //      当 SDL_USEREVENT 触发时，调用 cb
    SDL_TimerID timerId = SDL_AddTimer(interval, registerUsereventCb, cb);
    if (timerId == 0) { return; }

    m_timerId = timerId;
}

void Timer::stop() {
    if (m_timerId != 0) {
        SDL_RemoveTimer(m_timerId);
        m_timerId = 0;
    }
}