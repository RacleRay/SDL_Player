#ifndef __SDLAPP__H__
#define __SDLAPP__H__

#include <functional>
#include <map>
#include <memory>

extern "C" {
#include <SDL.h>
}

#define sdlApp (SDLApp::instance())

class SDLApp {
public:
    SDLApp();

public:
    int exec();

    static void quit();

    void registerEvent(Uint32 eventType, const std::function<void(SDL_Event*)> &callback);

public:
    static SDLApp *instance();

private:
    std::map<uint32_t, std::function<void(SDL_Event*)>> m_registeredEventMaps;
};

#endif //!__SDLAPP__H__