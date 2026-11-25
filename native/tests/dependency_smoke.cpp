#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include <doctest.h>
#include <miniaudio.h>
#include <nlohmann/json.hpp>

#include <array>

TEST_CASE("Decode an in-memory PCM WAV without an audio device")
{
    const auto format = nlohmann::json::parse(R"({"sample_rate":8000,"channels":1})");
    const auto config = ma_decoder_config_init(
        ma_format_s16, format.at("channels").get<ma_uint32>(),
        format.at("sample_rate").get<ma_uint32>());

    constexpr std::array<ma_uint8, 48> wav{
        'R', 'I', 'F', 'F', 40, 0, 0, 0, 'W', 'A', 'V', 'E',
        'f', 'm', 't', ' ', 16, 0, 0, 0, 1, 0, 1, 0,
        0x40, 0x1f, 0, 0, 0x80, 0x3e, 0, 0, 2, 0, 16, 0,
        'd', 'a', 't', 'a', 4, 0, 0, 0, 0, 0x40, 0, 0xc0
    };

    ma_decoder decoder{};
    REQUIRE(ma_decoder_init_memory(wav.data(), wav.size(), &config, &decoder) == MA_SUCCESS);

    std::array<ma_int16, 2> samples{};
    ma_uint64 framesRead = 0;
    const auto result = ma_decoder_read_pcm_frames(&decoder, samples.data(), samples.size(), &framesRead);
    CHECK(ma_decoder_uninit(&decoder) == MA_SUCCESS);

    CHECK(result == MA_SUCCESS);
    CHECK(framesRead == samples.size());
    CHECK(samples[0] == 16384);
    CHECK(samples[1] == -16384);
}
