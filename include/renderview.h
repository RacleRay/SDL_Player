#ifndef __RENDERVIEW__H__
#define __RENDERVIEW__H__

#include <SDL.h>
#include <list>
#include <mutex>

/**
显示模块封装
*/

struct RenderItem {
    SDL_Texture *texture;
    SDL_Rect srcRect;
    SDL_Rect dstRect;
};

class RenderView {
  public:
    explicit RenderView() = default;

    void setNativeHandle(void *handle);

    int initSDL();

    RenderItem *createRGB24Texture(int w, int h);

    void updateTexture(RenderItem *item, unsigned char *pixelData, int rows);

    void onRefresh();

  private:
    SDL_Window *m_sdlWindow = nullptr;
    SDL_Renderer *m_sdlRender = nullptr;

    void *m_nativeHandle = nullptr;

    std::list<RenderItem *> m_items;
    std::mutex m_updateMutex;
};

#endif //!__RENDERVIEW__H__