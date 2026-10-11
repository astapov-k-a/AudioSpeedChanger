#pragma once

#include <atomic>

class CancellationToken
{
public:
    void cancel()                  { m_cancelled.store(true,  std::memory_order_release); }
    void reset()                   { m_cancelled.store(false, std::memory_order_release); }
    bool isCancelled() const       { return m_cancelled.load(std::memory_order_acquire); }
private:
    std::atomic_bool m_cancelled{ false };
};
