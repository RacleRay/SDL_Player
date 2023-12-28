#include "threadbase.h"

// invoke subclass of ThreadBase ::run() method in a thread
static void ThreadEntry(void *arg) {
    auto *th = (ThreadBase *)arg;
    th->run();
}

//=================================================================================
// ThreadBase class

void ThreadBase::stop() {
    m_stop = true;
    if (m_th) { m_th->join(); }
}

void ThreadBase::start() {
    m_stop = false;
    if (m_th) { return; }

    m_th = new std::thread(ThreadEntry, this);
}
