#pragma once

#include <cstdint>
#include <string>
#include <vector>

struct TH095TextureImage {
    int width = 0;
    int height = 0;
    std::vector<std::uint8_t> rgba;
};

class TH095AssetReader {
public:
    bool open(const std::string &archivePath, std::string *error);
    bool readEntry(const std::string &name, std::vector<std::uint8_t> *out,
                   std::string *error) const;
    bool readAnmTexture(const std::string &name, TH095TextureImage *out,
                        std::string *error) const;
    bool readTitleImage(TH095TextureImage *out, std::string *error) const;
    std::size_t entryCount() const { return entries_.size(); }

private:
    struct Entry {
        std::string name;
        std::uint32_t dataOffset = 0;
        std::uint32_t decompressedSize = 0;
    };

    std::string archivePath_;
    std::vector<std::uint8_t> archive_;
    std::vector<Entry> entries_;
    std::size_t tableOffset_ = 0;
};
