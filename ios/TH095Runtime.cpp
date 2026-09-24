#include "TH095Runtime.hpp"
#include "TH095AssetReader.hpp"

#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <vector>

TH095Runtime &TH095Runtime::shared() {
    static TH095Runtime runtime;
    return runtime;
}

void TH095Runtime::start(const std::string &documents) {
    documents_ = documents;
    std::filesystem::create_directories(documents_ + "/TH095Data");
    logPath_ = documents_ + "/startup.log";
    dataPath_ = documents_ + "/TH095Data";
    assetsReady_ = false;
    assetStatus_.clear();
    titleRgba_.clear();
    worldRgba_.clear();
    gameplayVisible_ = false;
    const std::vector<std::string> archiveCandidates = {
        dataPath_ + "/th095.dat",
        documents_ + "/th095.dat",
    };
    for (const std::string &archivePath : archiveCandidates) {
        if (!std::filesystem::exists(archivePath))
            continue;
        TH095AssetReader reader;
        std::string error;
        if (reader.open(archivePath, &error)) {
            TH095TextureImage title;
            TH095TextureImage world;
            if (reader.readTitleImage(&title, &error) &&
                reader.readAnmTexture("world01.anm", &world, &error)) {
                titleWidth_ = title.width;
                titleHeight_ = title.height;
                titleRgba_ = std::move(title.rgba);
                worldWidth_ = world.width;
                worldHeight_ = world.height;
                worldRgba_ = std::move(world.rgba);
                assetsReady_ = true;
                std::vector<std::uint8_t> probe;
                const char *probeNames[] = {
                    "world01.std", "world01.anm", "ecl1_a.ecl",
                    "front.anm", "title.anm",
                };
                int probes = 0;
                for (const char *probeName : probeNames) {
                    if (reader.readEntry(probeName, &probe, nullptr))
                        ++probes;
                }
                assetStatus_ = "archive entries=" + std::to_string(reader.entryCount()) +
                                " probes=" + std::to_string(probes) +
                                " title=" + std::to_string(title.width) + "x" +
                                std::to_string(title.height);
                if (std::getenv("TH095_TEST_GAMEPLAY") != nullptr)
                    gameplayVisible_ = true;
                break;
            }
        }
        assetStatus_ = error.empty() ? "archive load failed" : error;
    }
    std::FILE *settings = std::fopen((dataPath_ + "/settings.ini").c_str(), "rb");
    if (settings) {
        int z = 0, s = 0;
        if (std::fscanf(settings, "z_mode=%d\ns_mode=%d", &z, &s) == 2) {
            zMode_ = z != 0; sMode_ = s != 0;
        }
        std::fclose(settings);
    }
    std::FILE *file = std::fopen(logPath_.c_str(), "wb");
    if (file) {
        std::fputs("TH095 iOS runtime\n", file);
        std::fputs("platform=ios arm64 deployment=14.0\n", file);
        std::fputs(assetsReady_ ? "phase=asset-title\n" : "phase=ui-shell\n", file);
        if (gameplayVisible_)
            std::fputs("mode=gameplay-resource-test\n", file);
        if (!assetStatus_.empty())
            std::fprintf(file, "assets=%s\n", assetStatus_.c_str());
        std::fclose(file);
    }
    log("runtime initialized");
}

void TH095Runtime::log(const char *message) {
    if (logPath_.empty()) return;
    std::FILE *file = std::fopen(logPath_.c_str(), "ab");
    if (!file) return;
    std::fprintf(file, "frame=%llu %s\n", static_cast<unsigned long long>(frames_), message);
    std::fclose(file);
}

void TH095Runtime::tick(double dt) {
    const float speed = 0.65f * static_cast<float>(dt);
    playerX_ += moveX_ * speed;
    playerY_ += moveY_ * speed;
    if (playerX_ < 0.03f) playerX_ = 0.03f;
    if (playerX_ > 0.97f) playerX_ = 0.97f;
    if (playerY_ < 0.05f) playerY_ = 0.05f;
    if (playerY_ > 0.95f) playerY_ = 0.95f;
    ++frames_;
    if ((frames_ % 600) == 0) log("heartbeat");
}

void TH095Runtime::setMove(float x, float y) { moveX_ = x; moveY_ = y; }
void TH095Runtime::setActionZ(bool down) { zToggle_ = down; if (down) log("action=Z"); }
void TH095Runtime::setActionS(bool down) { sToggle_ = down; if (down) log("action=S"); }

void TH095Runtime::toggleSettings() {
    settingsVisible_ = !settingsVisible_;
    log(settingsVisible_ ? "settings=open" : "settings=close");
}

void TH095Runtime::setZMode(bool enabled) {
    zMode_ = enabled;
    std::FILE *file = std::fopen((dataPath_ + "/settings.ini").c_str(), "wb");
    if (file) { std::fprintf(file, "z_mode=%d\ns_mode=%d\n", zMode_ ? 1 : 0, sMode_ ? 1 : 0); std::fclose(file); }
    log(zMode_ ? "settings=z-toggle" : "settings=z-hold");
}

void TH095Runtime::setSMode(bool enabled) {
    sMode_ = enabled;
    std::FILE *file = std::fopen((dataPath_ + "/settings.ini").c_str(), "wb");
    if (file) { std::fprintf(file, "z_mode=%d\ns_mode=%d\n", zMode_ ? 1 : 0, sMode_ ? 1 : 0); std::fclose(file); }
    log(sMode_ ? "settings=s-toggle" : "settings=s-hold");
}

void TH095Runtime::activatePrimaryMenu() {
    if (!assetsReady_)
        return;
    gameplayVisible_ = true;
    log("menu=game-start resource=world01.anm");
}
