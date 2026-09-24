#pragma once

#include <cstdint>
#include <string>
#include <vector>

class TH095Runtime {
public:
    static TH095Runtime &shared();
    void start(const std::string &documents);
    void tick(double dt);
    void setMove(float x, float y);
    void setActionZ(bool down);
    void setActionS(bool down);
    void toggleSettings();
    void setZMode(bool enabled);
    void setSMode(bool enabled);
    void activatePrimaryMenu();
    bool settingsVisible() const { return settingsVisible_; }
    bool zMode() const { return zMode_; }
    bool sMode() const { return sMode_; }
    bool zToggle() const { return zToggle_; }
    bool sToggle() const { return sToggle_; }
    const std::string &logPath() const { return logPath_; }
    const std::string &dataPath() const { return dataPath_; }
    float playerX() const { return playerX_; }
    float playerY() const { return playerY_; }
    uint64_t frames() const { return frames_; }
    bool assetsReady() const { return assetsReady_; }
    const std::string &assetStatus() const { return assetStatus_; }
    int titleWidth() const { return titleWidth_; }
    int titleHeight() const { return titleHeight_; }
    const std::vector<std::uint8_t> &titleRgba() const { return titleRgba_; }
    const std::vector<std::uint8_t> &worldRgba() const { return worldRgba_; }
    int worldWidth() const { return worldWidth_; }
    int worldHeight() const { return worldHeight_; }
    bool gameplayVisible() const { return gameplayVisible_; }
private:
    TH095Runtime() = default;
    void log(const char *message);
    std::string documents_;
    std::string logPath_;
    std::string dataPath_;
    float moveX_ = 0.0f, moveY_ = 0.0f;
    float playerX_ = 0.5f, playerY_ = 0.5f;
    bool zToggle_ = false, sToggle_ = false;
    bool zMode_ = false, sMode_ = false, settingsVisible_ = false;
    uint64_t frames_ = 0;
    bool assetsReady_ = false;
    std::string assetStatus_;
    int titleWidth_ = 0, titleHeight_ = 0;
    std::vector<std::uint8_t> titleRgba_;
    std::vector<std::uint8_t> worldRgba_;
    int worldWidth_ = 0, worldHeight_ = 0;
    bool gameplayVisible_ = false;
};
