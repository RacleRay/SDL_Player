#ifndef __THREADBASE__H__
#define __THREADBASE__H__

#include <atomic>
#include <thread>


class ThreadBase {
  public:
    ThreadBase() = default;

    virtual void run() = 0;

    void stop();

    void start();

    virtual ~ThreadBase() = default;

  private:
    std::thread *m_th = nullptr;

  protected:
    std::atomic_bool m_stop {false};
};

#endif //!__THREADBASE__H__