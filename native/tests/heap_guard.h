#pragma once

#include <cstddef>

#if defined(_MSC_VER) && defined(_DEBUG)
#include <crtdbg.h>
#endif

namespace oae::test {

class HeapGuard {
public:
    HeapGuard() noexcept;
    ~HeapGuard();
    HeapGuard(const HeapGuard&) = delete;
    HeapGuard& operator=(const HeapGuard&) = delete;
    std::size_t operations() const noexcept;

private:
    std::size_t operations_ = 0;
    std::size_t* previous_;
#if defined(_MSC_VER) && defined(_DEBUG)
    _CRT_ALLOC_HOOK previousHook_ = nullptr;
#endif
};

}
