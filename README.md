## Player based on SDL

> SDL 是一个跨平台的开发库，主要提供对音频设备、图形设备、鼠标和键盘设备的访问。在媒体播放器、图像渲染、模拟器、游戏开发方面等有广泛的用途。

SDL 渲染图像流程：

- SDL_CreateWindow : 创建渲染窗口
- SDL_CreateRender : 创建渲染器
- SDL_CreateTexture : 创建图像纹理
- SDL_RenderCopy : 上传纹理到GPU
- SDL_RenderPresent : 纹理显示

SDL 音频播放，采用主动 Pull 方式，通过声卡驱动主动回调的方式传递数据，这个特性适合把音频设备时钟作为音视频同步的主时钟：

- SDL_OpenAudioDevice : 打开音频设备
- SDL_AudioSpec : 配置音频播放参数，包括回调音频数据
- SDL_PauseAudioDevice : 开始或者暂停设备
- 声卡驱动回调捕获数据
- 声音开始播放

SDL 事件循环，通过查询事件队列，可以得到已经发生的事件，从而响应。所有事件类型使用 SDL_Event union 结构体，表示事件类型。

SDL 定时器，集成了一个精确的定时器，支持事件循环和一次性定时任务，用于定时刷新等定时任务。


### 播放器架构

- 时间循环主线程
- demux 线程
- 视频解码线程
- 音频播放线程



其中视频解码线程流程为：

- 获取 AVPacket
- avcodec_send_packet 发送解码数据
- avcodec_receive_frame 获取解码数据
- synchronize_video 矫正 PTS
- 等待有足够空间时，写入 pictq 队列
- 等待同步显示



音频解码线程流程：

- getAudioData()
- avcodec_send_packet 发送数据
- avcodec_receive_frame 获取解码数据
- swr_convert 重采样音频数据
- 更新音频时钟
- 复制到 SDL 缓冲区
- 开始播放



### 音视频同步

使用时钟 clock 来解决音视频不同步的问题。

- 音频时钟：主流播放器均采用。以音频时间为基准，将视频同步到音频。稳定性更好。
- 视频时钟：较少使用。以视频时间为基准，将音频同步到视频。
- 外部时钟：较少使用。用一个外部时钟线作为时钟，把音视频同步到外部时钟之上。



视频驱动并显示：

- schedule_refresh 调度刷新，添加 Timer 并设置回调
- 在回调中，SDL_PushEvent 发送刷新事件
- 检查队列是否为空
- 队列不为空时，取出下一帧
- 音视频同步，计算下一次刷新事件
- 显示取出的帧


### BUG fixing
- Player context 类中成员变量初始化
- std::atomic 在低版本 g++ 中内存对齐问题
- 音视频同步逻辑，对 audio clock 的更新问题
- 视频帧渲染调度问题
- CMake 配置 FFmpeg 动态库问题



### TODO

- 按照时间精确 seek ，而不是在关键帧上 seek
- 视频滤镜，对解码后的图像进行后处理
- 特效处理，人脸识别、动作识别等
- 音频处理，如音频增强、噪声抑制等



