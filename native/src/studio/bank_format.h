#pragma once

#include <oae_audio.h>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <string>
#include <vector>

namespace oae {

inline uint32_t calc_crc32(const uint8_t* data, size_t size) noexcept {
    uint32_t value = 0xFFFFFFFFu;
    while (size--) {
        value ^= *data++;
        for (int bit = 0; bit < 8; ++bit) {
            value = (value >> 1) ^ (0xEDB88320u & (0u - (value & 1u)));
        }
    }
    return ~value;
}

#pragma pack(push, 1)
struct BankHeaderWire {
    char magic[4];
    uint16_t version;
    uint16_t flags;
    uint32_t chunk_count;
};

struct ChunkDirEntryWire {
    char tag[4];
    uint64_t offset;
    uint64_t length;
    uint32_t crc32;
};
#pragma pack(pop)

static_assert(sizeof(BankHeaderWire) == 12, "BankHeaderWire must be exactly 12 bytes");
static_assert(sizeof(ChunkDirEntryWire) == 24, "ChunkDirEntryWire must be exactly 24 bytes");

class ByteReader {
public:
    ByteReader(const uint8_t* data, size_t size) noexcept
        : data_(data), size_(size), pos_(0) {}

    size_t remaining() const noexcept { return pos_ <= size_ ? size_ - pos_ : 0; }
    size_t position() const noexcept { return pos_; }

    bool can_read(size_t bytes) const noexcept {
        return pos_ + bytes <= size_ && pos_ + bytes >= pos_;
    }

    template <typename T>
    bool read(T& val) noexcept {
        static_assert(std::is_trivially_copyable_v<T>, "T must be trivially copyable");
        if (!can_read(sizeof(T))) return false;
        std::memcpy(&val, data_ + pos_, sizeof(T));
        pos_ += sizeof(T);
        return true;
    }

    bool read_bool(bool& b) noexcept {
        uint8_t byte = 0;
        if (!read(byte)) return false;
        if (byte > 1) return false;
        b = (byte == 1);
        return true;
    }

    bool read_bytes(void* dest, size_t count) noexcept {
        if (!can_read(count)) return false;
        std::memcpy(dest, data_ + pos_, count);
        pos_ += count;
        return true;
    }

    const uint8_t* current_ptr() const noexcept { return data_ + pos_; }

    bool skip(size_t bytes) noexcept {
        if (!can_read(bytes)) return false;
        pos_ += bytes;
        return true;
    }

private:
    const uint8_t* data_;
    size_t size_;
    size_t pos_;
};

class ByteWriter {
public:
    void write_bytes(const void* src, size_t count) {
        const auto* p = static_cast<const uint8_t*>(src);
        buffer_.insert(buffer_.end(), p, p + count);
    }

    template <typename T>
    void write(const T& val) {
        static_assert(std::is_trivially_copyable_v<T>, "T must be trivially copyable");
        write_bytes(&val, sizeof(T));
    }

    void write_bool(bool b) {
        uint8_t byte = b ? 1 : 0;
        write(byte);
    }

    void align_to(size_t alignment) {
        while (buffer_.size() % alignment != 0) {
            buffer_.push_back(0);
        }
    }

    size_t size() const noexcept { return buffer_.size(); }
    const uint8_t* data() const noexcept { return buffer_.data(); }
    const std::vector<uint8_t>& buffer() const noexcept { return buffer_; }
    std::vector<uint8_t>& buffer() noexcept { return buffer_; }

private:
    std::vector<uint8_t> buffer_;
};

}
