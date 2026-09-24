#include "TH095AssetReader.hpp"

#include <algorithm>
#include <cstring>
#include <fstream>

namespace {

constexpr std::uint8_t kProfiles[8][4] = {
    {0x1b, 0x37, 0x40, 0x00}, {0x51, 0xe9, 0x40, 0x00},
    {0xc1, 0x51, 0x80, 0x00}, {0x03, 0x19, 0x00, 0x04},
    {0xab, 0xcd, 0x00, 0x02}, {0x12, 0x34, 0x80, 0x00},
    {0x35, 0x97, 0x80, 0x00}, {0x99, 0x37, 0x00, 0x04},
};
constexpr std::uint32_t kProfilesMax[8] = {
    0x2800, 0x3000, 0x3200, 0x7800,
    0x2800, 0x3200, 0x2800, 0x2000,
};

std::uint32_t u32(const std::uint8_t *p) {
    return static_cast<std::uint32_t>(p[0]) |
           (static_cast<std::uint32_t>(p[1]) << 8) |
           (static_cast<std::uint32_t>(p[2]) << 16) |
           (static_cast<std::uint32_t>(p[3]) << 24);
}

std::uint16_t u16(const std::uint8_t *p) {
    return static_cast<std::uint16_t>(p[0] | (p[1] << 8));
}

void decrypt(std::vector<std::uint8_t> *bytes, std::uint8_t xorValue,
             std::uint8_t increment, std::uint32_t chunkSize,
             std::uint32_t maxBytes) {
    const std::size_t size = bytes->size();
    const std::size_t remainder = size % chunkSize;
    std::size_t untouched = remainder < chunkSize / 4 ? remainder : 0;
    untouched += size & 1;
    const std::size_t copySize = std::min<std::size_t>(size, maxBytes);
    std::vector<std::uint8_t> temporary(bytes->begin(), bytes->begin() + copySize);
    std::size_t cursor = 0;
    std::size_t outputBase = 0;
    std::size_t remaining = size - untouched;
    std::size_t allowed = maxBytes;
    while (remaining > 0 && allowed > 0) {
        const std::size_t block = std::min<std::size_t>(remaining, chunkSize);
        for (std::size_t i = 0; i < (block + 1) / 2; ++i) {
            (*bytes)[outputBase + block - 1 - i * 2] = temporary[cursor++] ^ xorValue;
            xorValue = static_cast<std::uint8_t>(xorValue + increment);
        }
        for (std::size_t i = 0; i < block / 2; ++i) {
            (*bytes)[outputBase + block - 2 - i * 2] = temporary[cursor++] ^ xorValue;
            xorValue = static_cast<std::uint8_t>(xorValue + increment);
        }
        remaining -= block;
        allowed -= block;
        outputBase += block;
    }
}

bool decompress(const std::vector<std::uint8_t> &input, std::size_t outputSize,
                std::vector<std::uint8_t> *output) {
    std::uint8_t ring[8192] = {};
    std::size_t ringHead = 1;
    std::size_t cursor = 0;
    std::uint8_t current = 0;
    std::uint8_t mask = 0;
    output->clear();
    output->reserve(outputSize);
    auto bit = [&]() -> unsigned {
        if (mask == 0) {
            current = cursor < input.size() ? input[cursor++] : 0;
            mask = 0x80;
        }
        const unsigned value = (current & mask) != 0;
        mask >>= 1;
        return value;
    };
    auto bits = [&](unsigned count) -> std::uint32_t {
        std::uint32_t value = 0;
        for (unsigned i = 0; i < count; ++i)
            value = (value << 1) | bit();
        return value;
    };
    while (output->size() < outputSize) {
        if (bit()) {
            if (output->size() >= outputSize)
                return false;
            const std::uint8_t value = static_cast<std::uint8_t>(bits(8));
            output->push_back(value);
            ring[ringHead] = value;
            ringHead = (ringHead + 1) & 0x1fff;
        } else {
            const std::uint32_t offset = bits(13);
            if (offset == 0)
                break;
            const std::uint32_t length = bits(4) + 3;
            for (std::uint32_t i = 0; i < length && output->size() < outputSize; ++i) {
                const std::uint8_t value = ring[(offset + i) & 0x1fff];
                output->push_back(value);
                ring[ringHead] = value;
                ringHead = (ringHead + 1) & 0x1fff;
            }
        }
    }
    return output->size() == outputSize;
}

std::uint8_t checksum(const std::string &name) {
    std::uint8_t result = 0;
    for (unsigned char c : name)
        result = static_cast<std::uint8_t>(result + c);
    return result;
}

void setError(std::string *error, const char *message) {
    if (error)
        *error = message;
}

} // namespace

bool TH095AssetReader::open(const std::string &archivePath, std::string *error) {
    std::ifstream file(archivePath, std::ios::binary | std::ios::ate);
    if (!file) {
        setError(error, "archive open failed");
        return false;
    }
    const std::streamsize size = file.tellg();
    if (size < 16) {
        setError(error, "archive is too small");
        return false;
    }
    file.seekg(0);
    archive_.resize(static_cast<std::size_t>(size));
    if (!file.read(reinterpret_cast<char *>(archive_.data()), size)) {
        setError(error, "archive read failed");
        return false;
    }

    std::vector<std::uint8_t> header(archive_.begin(), archive_.begin() + 16);
    decrypt(&header, 0x1b, 0x37, 16, 16);
    if (std::memcmp(header.data(), "THA1", 4) != 0) {
        setError(error, "unsupported archive header");
        return false;
    }
    const std::uint32_t tableSize = u32(header.data() + 4) - 123456789u;
    const std::uint32_t compressedSize = u32(header.data() + 8) - 987654321u;
    const std::uint32_t entryCount = u32(header.data() + 12) - 135792468u;
    if (compressedSize > archive_.size() || entryCount == 0 || entryCount > 4096) {
        setError(error, "invalid archive table bounds");
        return false;
    }
    const std::size_t tableOffset = archive_.size() - compressedSize;
    tableOffset_ = tableOffset;
    std::vector<std::uint8_t> compressed(archive_.begin() + tableOffset, archive_.end());
    decrypt(&compressed, 0x3e, 0x9b, 0x80, compressedSize);
    std::vector<std::uint8_t> table;
    if (!decompress(compressed, tableSize, &table)) {
        setError(error, "archive table decompression failed");
        return false;
    }
    entries_.clear();
    std::size_t cursor = 0;
    for (std::uint32_t i = 0; i < entryCount; ++i) {
        if (cursor >= table.size()) {
            setError(error, "archive table entry is truncated");
            return false;
        }
        const void *terminator = std::memchr(table.data() + cursor, 0, table.size() - cursor);
        if (terminator == nullptr) {
            setError(error, "archive table filename is truncated");
            return false;
        }
        const std::size_t end = static_cast<const std::uint8_t *>(terminator) - table.data();
        Entry entry;
        entry.name.assign(reinterpret_cast<const char *>(table.data() + cursor), end - cursor);
        cursor = (end + 4) & ~std::size_t(3);
        if (cursor + 12 > table.size()) {
            setError(error, "archive table metadata is truncated");
            return false;
        }
        entry.dataOffset = u32(table.data() + cursor);
        entry.decompressedSize = u32(table.data() + cursor + 4);
        cursor += 12;
        entries_.push_back(entry);
    }
    archivePath_ = archivePath;
    return true;
}

bool TH095AssetReader::readEntry(const std::string &name,
                                 std::vector<std::uint8_t> *out,
                                 std::string *error) const {
    for (std::size_t i = 0; i < entries_.size(); ++i) {
        if (entries_[i].name != name)
            continue;
        const std::uint32_t end = i + 1 < entries_.size()
            ? entries_[i + 1].dataOffset
            : static_cast<std::uint32_t>(tableOffset_);
        const Entry &entry = entries_[i];
        if (entry.dataOffset > end || end > archive_.size()) {
            setError(error, "archive entry bounds are invalid");
            return false;
        }
        std::vector<std::uint8_t> compressed(archive_.begin() + entry.dataOffset,
                                              archive_.begin() + end);
        const std::size_t profile = checksum(entry.name) % 8;
        const auto &p = kProfiles[profile];
        const std::uint32_t chunk = static_cast<std::uint32_t>(p[2]) |
                                    (static_cast<std::uint32_t>(p[3]) << 8);
        decrypt(&compressed, p[0], p[1], chunk, kProfilesMax[profile]);
        if (compressed.size() == entry.decompressedSize) {
            *out = std::move(compressed);
            return true;
        }
        if (!decompress(compressed, entry.decompressedSize, out)) {
            setError(error, "archive entry decompression failed");
            return false;
        }
        return true;
    }
    setError(error, "archive entry not found");
    return false;
}

bool TH095AssetReader::readAnmTexture(const std::string &name,
                                      TH095TextureImage *out,
                                      std::string *error) const {
    std::vector<std::uint8_t> anm;
    if (!readEntry(name, &anm, error))
        return false;
    if (anm.size() < 0x40) {
        setError(error, "animation is truncated");
        return false;
    }
    const std::uint32_t textureOffset = u32(anm.data() + 0x30);
    if (textureOffset + 16 > anm.size() || std::memcmp(anm.data() + textureOffset, "THTX", 4) != 0) {
        setError(error, "texture header is invalid");
        return false;
    }
    const std::uint16_t format = u16(anm.data() + textureOffset + 6);
    const int width = static_cast<int>(u16(anm.data() + textureOffset + 8));
    const int height = static_cast<int>(u16(anm.data() + textureOffset + 10));
    const int bpp = format == 4 ? 3 : (format == 0 ? 4 : 2);
    const std::size_t pixels = static_cast<std::size_t>(width) * height * bpp;
    if (width <= 0 || height <= 0 || textureOffset + 16 + pixels > anm.size()) {
        setError(error, "texture pixels are invalid");
        return false;
    }
    out->width = width;
    out->height = height;
    out->rgba.resize(static_cast<std::size_t>(width) * height * 4);
    const std::uint8_t *src = anm.data() + textureOffset + 16;
    for (int y = 0; y < height; ++y) {
        for (int x = 0; x < width; ++x) {
            std::uint8_t r = 0, g = 0, b = 0, a = 255;
            if (format == 5) {
                const std::uint16_t p = u16(src);
                src += 2;
                a = static_cast<std::uint8_t>(((p >> 12) & 0xf) * 17);
                r = static_cast<std::uint8_t>(((p >> 8) & 0xf) * 17);
                g = static_cast<std::uint8_t>(((p >> 4) & 0xf) * 17);
                b = static_cast<std::uint8_t>((p & 0xf) * 17);
            } else if (format == 1) {
                b = *src++; g = *src++; r = *src++; a = *src++;
            } else if (format == 2) {
                const std::uint16_t p = u16(src); src += 2;
                a = (p & 0x8000) ? 255 : 0;
                r = static_cast<std::uint8_t>(((p >> 10) & 0x1f) * 255 / 31);
                g = static_cast<std::uint8_t>(((p >> 5) & 0x1f) * 255 / 31);
                b = static_cast<std::uint8_t>((p & 0x1f) * 255 / 31);
            } else if (format == 3) {
                const std::uint16_t p = u16(src); src += 2;
                r = static_cast<std::uint8_t>(((p >> 11) & 0x1f) * 255 / 31);
                g = static_cast<std::uint8_t>(((p >> 5) & 0x3f) * 255 / 63);
                b = static_cast<std::uint8_t>((p & 0x1f) * 255 / 31);
            } else if (format == 4) {
                r = *src++; g = *src++; b = *src++;
            } else {
                setError(error, "unsupported texture format");
                return false;
            }
            const std::size_t dst = (static_cast<std::size_t>(y) * width + x) * 4;
            out->rgba[dst + 0] = r;
            out->rgba[dst + 1] = g;
            out->rgba[dst + 2] = b;
            out->rgba[dst + 3] = a;
        }
    }
    return true;
}

bool TH095AssetReader::readTitleImage(TH095TextureImage *out, std::string *error) const {
    return readAnmTexture("title.anm", out, error);
}
