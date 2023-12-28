#include "renderview.h"

#define SDL_WINDOW_DEFAULT_WIDTH  (1280)
#define SDL_WINDOW_DEFAULT_HEIGHT (720)

static SDL_Rect makeRect(int x, int y, int w, int h)
{
    SDL_Rect r;
    r.x = x;
    r.y = y;
    r.w = w;
    r.h = h;

    return r;
}

// RenderView::RenderView()
// {

// }

void RenderView::setNativeHandle(void *handle)
{
    m_nativeHandle = handle;
}

// 创建窗口和渲染器部分
int RenderView::initSDL()
{
    if (m_nativeHandle) {  // 从窗口句柄创建渲染上下文，可集成其他 UI 系统
        m_sdlWindow = SDL_CreateWindowFrom(m_nativeHandle);
    } else {  // 创建一个平台独立的渲染窗口
        m_sdlWindow = SDL_CreateWindow("ffmpeg-simple-player",
                                       SDL_WINDOWPOS_CENTERED,
                                       SDL_WINDOWPOS_CENTERED,
                                       SDL_WINDOW_DEFAULT_WIDTH,
                                       SDL_WINDOW_DEFAULT_HEIGHT,
                                       SDL_WINDOW_RESIZABLE);
    }

    if (!m_sdlWindow) {
        return -1;
    }

    // 创建渲染器，使用后记得删除渲染器 SDL_DestroyRender()
    // flags : 
    // SDL_RENDERER_SOFTWARE 软渲染，在显卡驱动异常时使用，降级方案，不推荐
    // SDL_RENDERER_ACCELERATED GPU 加速渲染
    // SDL_RENDERER_PRESENTVSYNC 垂直同步渲染，游戏场景中使用，将渲染引擎输出结果频率和显示器刷新频率同步
    // SDL_RENDERER_TARGETTEXTURE 离屏渲染，在屏幕外，也在显存外，单独进行渲染计算，通常是有其它目的

    m_sdlRender = SDL_CreateRenderer(m_sdlWindow, -1, SDL_RENDERER_ACCELERATED);
    if (!m_sdlRender) {
        return -2;
    }

    // 渲染窗口逻辑大小
    SDL_RenderSetLogicalSize(m_sdlRender,
                             SDL_WINDOW_DEFAULT_WIDTH, SDL_WINDOW_DEFAULT_HEIGHT);

    // 设置反锯齿特性等
    SDL_SetHint(SDL_HINT_RENDER_SCALE_QUALITY, "1");

    return 0;
}

// 创建图像纹理
RenderItem *RenderView::createRGB24Texture(int w, int h)
{
    m_updateMutex.lock();

    // 纹理包装类，支持多个纹理同时渲染，比如字幕、logo等，只需要给每个纹理设置 源区域srcRect 和 目的区域dstRect
    RenderItem *ret = new RenderItem;
    // 指定纹理像素格式、纹理大小等参数
    // SDL_TEXTUREACCESS_STREAMING 指定纹理为易变纹理，视频播放就属于易变纹理场景
    SDL_Texture *tex = SDL_CreateTexture(m_sdlRender, SDL_PIXELFORMAT_RGB24, SDL_TEXTUREACCESS_STREAMING, w, h);
    ret->texture = tex;
    ret->srcRect = makeRect(0, 0, w, h);
    ret->dstRect = makeRect(0, 0, SDL_WINDOW_DEFAULT_WIDTH, SDL_WINDOW_DEFAULT_HEIGHT);

    m_items.push_back(ret);

    m_updateMutex.unlock();

    return ret;
}

// 更新纹理
void RenderView::updateTexture(RenderItem *item, unsigned char *pixelData, int rows)
{
    m_updateMutex.lock();

    void *pixels = nullptr;
    int pitch;
    // 先锁定纹理
    SDL_LockTexture(item->texture, nullptr, &pixels, &pitch);
    // 复制像素数据
    memcpy(pixels, pixelData, pitch * rows);
    // 解锁纹理
    SDL_UnlockTexture(item->texture);

    std::list<RenderItem *>::iterator iter;
    SDL_RenderClear(m_sdlRender);  // 清除脏数据
    for (iter = m_items.begin(); iter != m_items.end(); iter++)
    {
        // 将纹理数据，复制给渲染器
        RenderItem *item = *iter;
        SDL_RenderCopy(m_sdlRender, item->texture, &item->srcRect, &item->dstRect);
    }

    m_updateMutex.unlock();
}

// 定时调用 onRefresh
void RenderView::onRefresh()
{
    m_updateMutex.lock();

    if (m_sdlRender) {
        // SDL 使用多缓冲渲染机制，当调用一个渲染函数时，先把数据放到缓冲区中
        // 调用 SDL_RenderPresent 时，才会把缓冲区的数据刷新到屏幕上
        SDL_RenderPresent(m_sdlRender);
    }

    m_updateMutex.unlock();
}
