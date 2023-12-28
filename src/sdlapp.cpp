#include <functional>

#include "SDL.h"

#include "sdlapp.h"


#define SDL_APP_EVENT_TIMEOUT (1)

static SDLApp* globalInstance = nullptr;

SDLApp::SDLApp()
{
    SDL_Init(SDL_INIT_EVERYTHING);

    if (!globalInstance) {
        globalInstance = this;
    } else {
        fprintf(stderr, "only one instance allowed\n");
        exit(1);
    }
}

int SDLApp::exec()
{
    SDL_Event event;
    for (;;) {
        // 带超时的事件等待，阻塞调用
        SDL_WaitEventTimeout(&event, SDL_APP_EVENT_TIMEOUT);
        // SDL_PollEvent : 使用传统的事件轮询机制，非阻塞调用，有事件返回1，否则返回0
        switch(event.type) {
        case SDL_QUIT:
            SDL_Quit();
            return 0;
        case SDL_USEREVENT:
        {
            // Timer 设置的 callback ，比如 刷新窗口
            std::function<void()> cb = *(std::function<void()>*)event.user.data1;
            cb();
        }
            break;
        default:
            auto iter = m_userEventMaps.find(event.type);
            // 只响应 m_userEventMaps 中注册的事件，并执行 callback
            if (iter != m_userEventMaps.end()) {
                auto onEventCb = iter->second;
                onEventCb(&event);
            }
            break;
        }
    }
}

void SDLApp::quit()
{
    SDL_Event event;
    event.type = SDL_QUIT;
    SDL_PushEvent(&event);
}

// 注册事件及相应的回调函数
void SDLApp::registerEvent(Uint32 type, const std::function<void (SDL_Event *)> &cb)
{
    m_userEventMaps[type] = cb;
}

SDLApp *SDLApp::instance()
{
    return globalInstance;
}
