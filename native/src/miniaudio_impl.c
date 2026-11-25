#if defined(_MSC_VER)
#pragma warning(push)
#pragma warning(disable : 4701 4244 4456 4267 4457)
#endif

#define STB_VORBIS_HEADER_ONLY
#include <stb_vorbis.c>
#undef STB_VORBIS_HEADER_ONLY

#define MINIAUDIO_IMPLEMENTATION
#include <miniaudio.h>

#include <stb_vorbis.c>

#if defined(_MSC_VER)
#pragma warning(pop)
#endif


void oae_ma_enable_pitch(ma_sound* sound)
{
    ma_atomic_exchange_explicit_32(&sound->engineNode.isPitchDisabled, MA_FALSE, ma_atomic_memory_order_release);
}
