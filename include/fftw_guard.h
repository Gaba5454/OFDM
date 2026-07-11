#pragma once

#include <mutex>

inline std::mutex& fftw_global_mutex()
{
    static std::mutex mutex;
    return mutex;
}
