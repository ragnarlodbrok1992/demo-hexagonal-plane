#ifndef _H_WIN_UTILS
#define _H_WIN_UTILS

#include <WinError.h>
#include <stdexcept>

// Helper function for HRESULT checking
inline void ThrowIfFailed(HRESULT hr) {
    if (FAILED(hr)) {
        throw std::runtime_error("HRESULT failed");
    }
}

#endif /* _H_WIN_UTILS */

