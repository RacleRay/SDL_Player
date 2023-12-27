#include <functional>

#include "SDL.h"

#include "sdlapp.h"


static SDLApp* g_app_instance = nullptr;


SDLApp::SDLApp()
{
    SDL_Init(SDL_INIT_EVERYTHING);

    if (!g_app_instance) {
        g_app_instance = this;
    } else {
        (void)fprintf(stderr, "SDLApp::SDLApp() - already initialized\n");
        exit(1);
    }
}

int SDLApp::exec()
{
    SDL_Event event;

    for (;;) {
        SDL_WaitEventTimeout(&event, 1);

        switch (event.type) {
        case SDL_QUIT: {
            SDL_Quit();
            return 0;
        }
        case SDL_USEREVENT: {
            std::function<void()> cb = *(std::function<void()>*)event.user.data1;
            cb();
            break;
        }
        default: {
            auto iter = m_eventMap.find(event.type);
            if (iter != m_eventMap.end()) {
                auto on_event_cb = iter->second;
                on_event_cb(&event);
            }
            break;
        }
        }
    }
}

void SDLApp::quit()
{
    SDL_Event event;
    event.type = SDL_QUIT;
    SDL_PushEvent(&event);
}

void SDLApp::registerEvent(uint32_t eventType, const std::function<void(SDL_Event*)> &callback)
{
    m_eventMap[eventType] = callback;
}

SDLApp* SDLApp::instance()
{
    return g_app_instance;
}
