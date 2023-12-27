#include "timer.h"


// 返回 Timer 设置的 interval 值。若返回 0 ，则取消此定时器；否则返回下一次 interval 的值
// NOTE:
//  若 SDL 定时器在另一个线程上执行，并且要将回调函数和事件循环在一个线程内执行，
//  则需要在 callback 中将事件 post 给事件循环所在线程。
static Uint32 callbackfunc(Uint32 interval, void* param) {
    SDL_Event event;
    SDL_UserEvent userevent;

    userevent.type = SDL_USEREVENT;
    userevent.code = 0;
    userevent.data1 = param;
    userevent.data2 = nullptr;

    event.type = SDL_USEREVENT;
    event.user = userevent;

    SDL_PushEvent(&event);

    return interval;
}


// callback 由 callbackfunc 中 userevent.data1 保存，并在事件触发时回调
void Timer::start(void* callback, int interval)
{
    // timer already started
    if (m_timerId != 0) {
        return;
    }

    // add new timer : 精度依赖于操作系统的调度精度，属于典型的非实时定时器
    // 设置超时时间和超时后的回调函数. 回调函数中再将 callback 关联到 USEREVENT 事件
    SDL_TimerID timerId = SDL_AddTimer(interval, callbackfunc, callback);
    if (timerId == 0) {
        return;
    }
}

void Timer::stop()
{
    if (m_timerId != 0) {
        SDL_RemoveTimer(m_timerId);
        m_timerId = 0;
    }
}
