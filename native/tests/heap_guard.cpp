#include "heap_guard.h"

#include <cstdlib>
#include <new>

#if defined(_WIN32)
#include <malloc.h>
#endif

namespace {

thread_local std::size_t* watched = nullptr;

void count_operation() noexcept
{
    if (watched) {
        ++*watched;
    }
}

#if defined(_MSC_VER) && defined(_DEBUG)
int __cdecl count_crt_heap(int, void*, std::size_t, int block, long, const unsigned char*, int) noexcept
{
    if (block != _CRT_BLOCK) {
        count_operation();
    }
    return 1;
}
#endif

void* allocate_aligned(std::size_t size, std::size_t alignment)
{
    count_operation();
#if defined(_WIN32)
    if (auto* pointer = _aligned_malloc(size == 0 ? 1 : size, alignment)) {
        return pointer;
    }
#else
    void* pointer = nullptr;
    if (posix_memalign(&pointer, alignment, size == 0 ? 1 : size) == 0) {
        return pointer;
    }
#endif
    throw std::bad_alloc();
}

void free_aligned(void* pointer) noexcept
{
    if (!pointer) {
        return;
    }
    count_operation();
#if defined(_WIN32)
    _aligned_free(pointer);
#else
    std::free(pointer);
#endif
}

}

void* operator new(std::size_t size)
{
    count_operation();
    if (auto* pointer = std::malloc(size == 0 ? 1 : size)) {
        return pointer;
    }
    throw std::bad_alloc();
}

void* operator new[](std::size_t size) { return ::operator new(size); }
void* operator new(std::size_t size, std::align_val_t alignment)
{
    return allocate_aligned(size, static_cast<std::size_t>(alignment));
}
void* operator new[](std::size_t size, std::align_val_t alignment) { return ::operator new(size, alignment); }

void operator delete(void* pointer) noexcept
{
    if (pointer) {
        count_operation();
        std::free(pointer);
    }
}

void operator delete[](void* pointer) noexcept { ::operator delete(pointer); }
void operator delete(void* pointer, std::size_t) noexcept { ::operator delete(pointer); }
void operator delete[](void* pointer, std::size_t) noexcept { ::operator delete(pointer); }
void operator delete(void* pointer, std::align_val_t) noexcept { free_aligned(pointer); }
void operator delete[](void* pointer, std::align_val_t) noexcept { free_aligned(pointer); }
void operator delete(void* pointer, std::size_t, std::align_val_t) noexcept { free_aligned(pointer); }
void operator delete[](void* pointer, std::size_t, std::align_val_t) noexcept { free_aligned(pointer); }

namespace oae::test {

HeapGuard::HeapGuard() noexcept : previous_(watched)
{
#if defined(_MSC_VER) && defined(_DEBUG)
    previousHook_ = _CrtSetAllocHook(count_crt_heap);
#endif
    watched = &operations_;
}

HeapGuard::~HeapGuard()
{
    watched = previous_;
#if defined(_MSC_VER) && defined(_DEBUG)
    _CrtSetAllocHook(previousHook_);
#endif
}

std::size_t HeapGuard::operations() const noexcept
{
    return operations_;
}

}
