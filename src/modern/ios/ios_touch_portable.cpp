#include "ios_touch.hpp"
#include "FrontEndGlobals.hpp"
#include "GameplayGlobals.hpp"
#include "PhotoGameTask.hpp"
#ifndef NDEBUG
#include "PhotoPlayerRuntime.hpp"
#include "PhotoEnemyManager.hpp"
#endif
#include "ios_gl_legacy.hpp"
#include <SDL_ttf.h>
#include <map>
#include <string>
#include <vector>
#include <cstdio>

#include <algorithm>
#include <cmath>

namespace th095::modern::ios
{
namespace
{
u16 g_buttons = 0;
PresentationLayout g_layout = {640, 480, 0, 0, 640, 480};
f32 g_dragX = 0.0f;
f32 g_dragY = 0.0f;
f32 g_shakeX = 0.0f;
f32 g_shakeY = 0.0f;
bool g_invincible = false;
bool g_autoBomb = false;
Uint32 g_tapUntil = 0;
Uint32 g_commandUntil = 0;
u16 g_command = 0;
struct Finger { u16 mask; bool movement; bool joystick = false; };
std::map<SDL_FingerID, Finger> g_fingers;
bool g_zToggle = false, g_sToggle = false, g_zLatched = false, g_sLatched = false;
bool g_portrait = true, g_settings = false, g_loadedSettings = false;
float g_sensitivity = 1.0f;
float g_stickX = 0, g_stickY = 0;
bool Gameplay() { return g_RuntimePlayerOwner && g_RuntimeGlobalStateOwner; }
bool Battle() { return Gameplay() && !static_cast<PhotoGameTaskView *>(g_RuntimeGlobalStateOwner)->resultScreenActive; }
std::string SettingsPath()
{
    char *base = SDL_GetPrefPath("th095", "mobile");
    std::string result = base ? base : "";
    SDL_free(base);
    return result + "controls.txt";
}
void LoadSettings()
{
    if (g_loadedSettings) return;
    g_loadedSettings = true;
    if (FILE *file = fopen(SettingsPath().c_str(), "r"))
    {
        int z = 0, s = 0, portrait = 1;
        if (fscanf(file, "%d %d %d %f", &z, &s, &portrait, &g_sensitivity) == 4)
        { g_zToggle = z != 0; g_sToggle = s != 0; g_portrait = portrait != 0; }
        fclose(file);
        g_sensitivity = std::max(0.5f, std::min(2.0f, g_sensitivity));
    }
}
void SaveSettings()
{
    if (FILE *file = fopen(SettingsPath().c_str(), "w"))
    {
        fprintf(file, "%d %d %d %.2f\n", g_zToggle, g_sToggle, g_portrait, g_sensitivity);
        fclose(file);
    }
}
struct Button { float x,y,w,h; const char *label; u16 mask; int action; bool circular = false; };
struct StickGeometry { float x,y,r; };
StickGeometry Stick()
{
    const float w = g_layout.drawableWidth, h = g_layout.drawableHeight;
    const float unit = ControlScale(w,h);
    const float radius = 48*unit;
    return {w > h ? 82*unit : w*.23f, h-100*unit,radius};
}
void UpdateStick(float px,float py)
{
    const auto stick = Stick();
    float dx=(px-stick.x)/stick.r, dy=(py-stick.y)/stick.r;
    const float length=std::sqrt(dx*dx+dy*dy);
    if (length>1) {dx/=length;dy/=length;}
    g_stickX=dx; g_stickY=dy;
}
std::vector<Button> Buttons()
{
    const float w = g_layout.drawableWidth, h = g_layout.drawableHeight;
    const float unit = ControlScale(w,h);
    std::vector<Button> result;
    if (g_settings)
    {
        float bw=std::min(w*.84f,420*unit),x=(w-bw)/2,bh=std::min(45*unit,h*.11f),y=(h-bh*5.8f)/2;
        result.push_back({x,y,bw,bh,g_zToggle ? "Z TOGGLE: ON" : "Z TOGGLE: OFF",0,2});
        result.push_back({x,y+bh*1.2f,bw,bh,g_sToggle ? "S TOGGLE: ON" : "S TOGGLE: OFF",0,3});
        result.push_back({x,y+bh*2.4f,bw,bh,g_portrait ? "PORTRAIT BATTLE: ON" : "PORTRAIT BATTLE: OFF",0,4});
        result.push_back({x,y+bh*3.6f,bw,bh,g_sensitivity < .9f ? "TOUCH SPEED: SLOW" : g_sensitivity > 1.1f ? "TOUCH SPEED: FAST" : "TOUCH SPEED: NORMAL",0,5});
        result.push_back({x,y+bh*4.8f,bw,bh,"DONE",0,1});
        return result;
    }
    const float top=h>w ? 37*unit : 16*unit;
    result.push_back({w-53*unit,top,36*unit,36*unit,"...",0,1,true});
    if (Gameplay()) result.push_back({w-99*unit,top,36*unit,36*unit,"II",TH_BUTTON_MENU,0,true});
    const float right=w-48*unit, bottom=h-66*unit;
    result.push_back({right-31*unit,bottom-95*unit,62*unit,62*unit,"Z",TH_BUTTON_SHOOT,0,true});
    result.push_back({right-101*unit,bottom-124*unit,54*unit,54*unit,"S",TH_BUTTON_FOCUS,0,true});
    result.push_back({right-86*unit,bottom-36*unit,58*unit,58*unit,"X",TH_BUTTON_BOMB,0,true});
    return result;
}
void Circle(float x,float y,float radius,unsigned char r,unsigned char g,unsigned char b,unsigned char a)
{
    glColor4ub(r,g,b,a); glBegin(GL_TRIANGLE_FAN); glVertex2f(x,y);
    for(int i=0;i<=64;++i)
    {float angle=i*6.28318530718f/64; glVertex2f(x+std::cos(angle)*radius,y+std::sin(angle)*radius);}
    glEnd();
}
void Rectangle(float x,float y,float w,float h, unsigned char r,unsigned char g,unsigned char b,unsigned char a)
{
    glColor4ub(r,g,b,a);
    glBegin(GL_TRIANGLE_STRIP);
    glTexCoord2f(0,0); glVertex2f(x,y);
    glTexCoord2f(1,0); glVertex2f(x+w,y);
    glTexCoord2f(0,1); glVertex2f(x,y+h);
    glTexCoord2f(1,1); glVertex2f(x+w,y+h);
    glEnd();
}
void Label(const Button &button)
{
    struct Text { GLuint texture; int w,h; };
    static std::map<std::string,Text> cache;
    static TTF_Font *font = nullptr;
    if (!font)
    {
        if (!TTF_WasInit()) TTF_Init();
        font = TTF_OpenFont("/System/Library/Fonts/CoreUI/SFUI.ttf",24);
        if (!font) font = TTF_OpenFont("/System/Library/Fonts/PingFang.ttc",24);
        if (!font) return;
    }
    auto it = cache.find(button.label);
    if (it == cache.end())
    {
        SDL_Surface *raw = TTF_RenderUTF8_Blended(font,button.label,{255,255,255,255});
        if (!raw) return;
        SDL_Surface *rgba = SDL_ConvertSurfaceFormat(raw,SDL_PIXELFORMAT_ABGR8888,0);
        SDL_FreeSurface(raw);
        if (!rgba) return;
        Text t = {0,rgba->w,rgba->h};
        glGenTextures(1,&t.texture); glBindTexture(GL_TEXTURE_2D,t.texture);
        glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MIN_FILTER,GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MAG_FILTER,GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_S,GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_T,GL_CLAMP_TO_EDGE);
        glTexImage2D(GL_TEXTURE_2D,0,GL_RGBA,t.w,t.h,0,GL_RGBA,GL_UNSIGNED_BYTE,rgba->pixels);
        SDL_FreeSurface(rgba);
        it = cache.emplace(button.label,t).first;
    }
    const Text &t = it->second;
    float scale = std::min(button.w*.83f/t.w,button.h*(button.circular ? .64f : .38f)/t.h);
    glBindTexture(GL_TEXTURE_2D,t.texture); IosLegacySetTextureUsage(GL_TRUE,GL_TRUE);
    Rectangle(button.x+(button.w-t.w*scale)/2,button.y+(button.h-t.h*scale)/2,t.w*scale,t.h*scale,255,255,255,255);
}
}

IosLanguage GetLanguage() { return IOS_LANGUAGE_JP; }
const char *LanguageName() { return "Japanese"; }
void CycleLanguage() {}
u32 LanguageRevision() { return 0; }
bool ConsumeLanguageRestartRequest() { return false; }
bool IsLanguageRestartInProgress() { return false; }
void CompleteLanguageRestart() {}
const char *PauseMenuText(int index) { return index == 0 ? "Resume" : "Return"; }
const char *RetryMenuText(int index) { return index == 0 ? "Retry" : "Title"; }

void SetPresentationLayout(const PresentationLayout &layout)
{
    // A finger's old position must not become a new direction after rotation.
    if (layout.drawableWidth != g_layout.drawableWidth ||
        layout.drawableHeight != g_layout.drawableHeight) ResetTouchState();
    g_layout = layout;
}
PresentationLayout GetPresentationLayout() { return g_layout; }
bool UsePortraitBattle() { LoadSettings(); return g_portrait; }
float ControlScale(int drawableWidth, int drawableHeight)
{
    int width=drawableWidth,height=drawableHeight;
    if (SDL_Window *window=SDL_GL_GetCurrentWindow()) SDL_GetWindowSize(window,&width,&height);
    if (width<=0 || height<=0) return 1.f;
    const float density=static_cast<float>(drawableWidth)/width;
    return density*std::min(1.6f,std::min(width,height)/390.f);
}

static void SetKey(SDL_Scancode key, u16 button, const SDL_Event &event)
{
    if (event.key.keysym.scancode == key)
    {
        if (event.type == SDL_KEYDOWN) g_buttons |= button;
        if (event.type == SDL_KEYUP) g_buttons &= static_cast<u16>(~button);
    }
}

u16 PollButtons()
{
    LoadSettings();
    u16 regressionButtons = 0;
#ifndef NDEBUG
    static bool resultTapDone=false;
    const char *resultItem=SDL_getenv("TH095_IOS_TEST_RESULT_ITEM");
    if (resultItem && !resultTapDone && Gameplay() && !Battle())
    {
        float x,y;
        if (ResultScreenTouchPointForItem(atoi(resultItem),&x,&y))
        {
            const bool cropped=UsePortraitBattle() && g_layout.drawableHeight>g_layout.drawableWidth;
            SDL_Event event={}; event.type=SDL_FINGERDOWN; event.tfinger.fingerId=9010;
            event.tfinger.x=(g_layout.viewportX+(x-(cropped?128.f:0.f))*g_layout.viewportWidth/(cropped?384.f:640.f))/g_layout.drawableWidth;
            event.tfinger.y=(g_layout.viewportY+(y-(cropped?16.f:0.f))*g_layout.viewportHeight/(cropped?448.f:480.f))/g_layout.drawableHeight;
            ProcessEvent(event); event.type=SDL_FINGERUP; ProcessEvent(event);
            resultTapDone=true; modern::LogStartup("test: result menu finger tap dispatched");
        }
    }
    // Exercise capture through ordinary movement and keyboard input. This
    // changes no player health, charge, score, unlocks, or enemy state.
    if (SDL_getenv("TH095_IOS_TEST_CAPTURE") && Battle() && g_RuntimeEnemyManagerOwner)
    {
        auto *player = static_cast<PhotoPlayerRuntimeView *>(g_RuntimePlayerOwner);
        auto *enemies = static_cast<PhotoEnemyManagerView *>(g_RuntimeEnemyManagerOwner);
        static unsigned frame = 0;
        static int capturePhase = 0, previousPhoto = -1;
        ++frame;
        if (player->mode == PHOTO_PLAYER_MODE_ACTIVE && enemies->activeEnemyCount > 0)
        {
            const auto &enemy = enemies->enemyPool[0];
            const bool retreat = player->camera.mode == PHOTO_CAMERA_CAPTURED || player->camera.mode == PHOTO_CAMERA_RECOVERING;
            const float targetX=retreat ? (player->playerPosition.x < 0 ? -150.f : 150.f) : enemy.position.x;
            const float targetY=retreat ? 410.f : enemy.position.y+90;
            const float dx=targetX-player->playerPosition.x;
            const float dy=targetY-player->playerPosition.y;
            g_dragX += std::max(-3.f,std::min(3.f,dx));
            g_dragY += std::max(-3.f,std::min(3.f,dy));
            if (capturePhase == 0)
            {
                regressionButtons = TH_BUTTON_FOCUS | TH_BUTTON_SHOOT;
                if (player->camera.charge >= 1.f && std::fabs(dx)<15 && std::fabs(dy)<15)
                    capturePhase=1;
            }
            else if (capturePhase == 1) capturePhase=2; // Release focus before Z.
            else if (capturePhase == 2)
            {
                regressionButtons = TH_BUTTON_SHOOT;
                if (player->camera.mode == PHOTO_CAMERA_CHARGING) capturePhase=3;
            }
            else capturePhase=0; // Z release takes the photograph.
        }
        if (frame%120==0 || player->camera.photoIndex != previousPhoto)
        {
            char message[200];
            snprintf(message,sizeof(message),"test-capture: frame=%u player=%d camera=%d charge=%.2f photos=%d/%d pos=%.1f,%.1f",frame,player->mode,player->camera.mode,player->camera.charge,player->camera.photoIndex,player->camera.photoLimit,player->playerPosition.x,player->playerPosition.y);
            modern::LogStartup(message);
            previousPhoto=player->camera.photoIndex;
        }
    }
    if (SDL_getenv("TH095_IOS_TEST_PAUSE") && Gameplay())
    {
        static unsigned pauseFrame = 0;
        if (++pauseFrame == 90)
        {
            g_command = TH_BUTTON_MENU; g_commandUntil = SDL_GetTicks() + 150;
            modern::LogStartup("test: pause requested");
        }
        if (pauseFrame == 240 && SDL_getenv("TH095_IOS_TEST_RESUME"))
        {
            g_command = TH_BUTTON_SHOOT; g_commandUntil = SDL_GetTicks() + 150;
            modern::LogStartup("test: resume requested");
        }
    }
    // Simulator regression driver uses the same hit testing as a finger tap.
    static bool testTapDone = false;
    static bool testBattleDone = false;
    const char *testTap = SDL_getenv("TH095_IOS_TEST_TAP");
    if (!testTapDone && testTap != nullptr)
    {
        float x, y;
        if (SDL_sscanf(testTap, "%f,%f", &x, &y) == 2 && FrontEndTapMainMenu(x, y))
        {
            testTapDone = true;
            g_tapUntil = SDL_GetTicks() + 150;
        }
    }
    if (!testBattleDone && SDL_getenv("TH095_IOS_TEST_BATTLE") != nullptr &&
        FrontEndSceneSelectReady())
    {
        testBattleDone = true;
        g_tapUntil = SDL_GetTicks() + 150;
        modern::LogStartup("test: starting selected scene");
    }
    if (SDL_getenv("TH095_IOS_TEST_CONTROLS") && Battle())
    {
        static unsigned frame = 0;
        ++frame;
        if (frame == 30 || frame == 40 || frame == 50)
        {
            const auto stick = Stick();
            SDL_Event e = {};
            e.type = frame == 30 ? SDL_FINGERDOWN : frame == 40 ? SDL_FINGERMOTION : SDL_FINGERUP;
            e.tfinger.fingerId = 9003;
            e.tfinger.x = (stick.x + stick.r * .6f) / g_layout.drawableWidth;
            e.tfinger.y = stick.y / g_layout.drawableHeight;
            ProcessEvent(e);
            const bool active = g_stickX > .16f && std::fabs(g_stickY) < .01f;
            modern::LogStartup((frame == 50 ? !active : active) ?
                "test: joystick direction/release PASS" : "test: joystick direction/release FAIL");
        }
        if (frame == 90 || frame == 91 || frame == 92)
        {
            SDL_Event e = {};
            e.type = frame == 90 ? SDL_FINGERDOWN : frame == 91 ? SDL_FINGERMOTION : SDL_FINGERUP;
            e.tfinger.fingerId = 9001; e.tfinger.x = .45f; e.tfinger.y = .55f;
            e.tfinger.dx = .10f;
            ProcessEvent(e);
            modern::LogStartup("test: relative touch movement event");
        }
        if (frame == 180 || frame == 190 || frame == 300 || frame == 310)
        {
            for (const auto &button : Buttons()) if (button.mask == TH_BUTTON_SHOOT)
            {
                SDL_Event e = {};
                e.type = frame == 180 || frame == 300 ? SDL_FINGERDOWN : SDL_FINGERUP;
                e.tfinger.fingerId = 9002;
                e.tfinger.x = (button.x+button.w/2)/g_layout.drawableWidth;
                e.tfinger.y = (button.y+button.h/2)/g_layout.drawableHeight;
                ProcessEvent(e);
                modern::LogStartup("test: photo button touch event");
            }
        }
    }
#endif
    u16 held = g_buttons | regressionButtons;
    for (const auto &finger : g_fingers) held |= finger.second.mask;
    if (Battle())
    { if (g_zLatched) held |= TH_BUTTON_SHOOT; if (g_sLatched) held |= TH_BUTTON_FOCUS; }
    else { g_zLatched = g_sLatched = false; g_dragX = g_dragY = 0; }
    if (!g_settings && std::sqrt(g_stickX*g_stickX+g_stickY*g_stickY) > .16f)
    {
        // Eight sectors with one radial dead zone; diagonal input uses the
        // game's existing diagonal speed normalization.
        const float ax = std::fabs(g_stickX), ay = std::fabs(g_stickY);
        if (ax >= ay * .41421356f)
            held |= g_stickX < 0 ? TH_BUTTON_LEFT : TH_BUTTON_RIGHT;
        if (ay >= ax * .41421356f)
            held |= g_stickY < 0 ? TH_BUTTON_UP : TH_BUTTON_DOWN;
    }
    if (static_cast<Sint32>(g_commandUntil-SDL_GetTicks()) > 0) held |= g_command;
    return held | (static_cast<Sint32>(g_tapUntil - SDL_GetTicks()) > 0 ? TH_BUTTON_SHOOT : 0);
}

void ProcessEvent(const SDL_Event &event)
{
    if (event.type == SDL_KEYDOWN || event.type == SDL_KEYUP)
    {
        SetKey(SDL_SCANCODE_Z, TH_BUTTON_SHOOT, event);
        SetKey(SDL_SCANCODE_X, TH_BUTTON_BOMB, event);
        SetKey(SDL_SCANCODE_LSHIFT, TH_BUTTON_FOCUS, event);
        SetKey(SDL_SCANCODE_ESCAPE, TH_BUTTON_MENU, event);
        SetKey(SDL_SCANCODE_RETURN, TH_BUTTON_ENTER, event);
        SetKey(SDL_SCANCODE_UP, TH_BUTTON_UP, event);
        SetKey(SDL_SCANCODE_DOWN, TH_BUTTON_DOWN, event);
        SetKey(SDL_SCANCODE_LEFT, TH_BUTTON_LEFT, event);
        SetKey(SDL_SCANCODE_RIGHT, TH_BUTTON_RIGHT, event);
        SetKey(SDL_SCANCODE_S, TH_BUTTON_S, event);
    }
    else if (event.type == SDL_FINGERDOWN && g_layout.viewportWidth > 0 && g_layout.viewportHeight > 0)
    {
        const float px = event.tfinger.x*g_layout.drawableWidth, py = event.tfinger.y*g_layout.drawableHeight;
        for (const auto &button : Buttons())
        {
            if (px < button.x || px > button.x+button.w || py < button.y || py > button.y+button.h) continue;
            if (button.circular)
            {float dx=px-button.x-button.w/2,dy=py-button.y-button.h/2;
             if(dx*dx+dy*dy>button.w*button.w*.25f)continue;}
            if (button.action)
            {
                if (button.action == 1)
                {
                    if (!g_settings && Battle()) { g_command = TH_BUTTON_MENU; g_commandUntil = SDL_GetTicks()+150; }
                    g_settings = !g_settings; g_fingers.clear(); g_zLatched = g_sLatched = false; g_stickX=g_stickY=0;
                }
                if (button.action == 2) g_zToggle = !g_zToggle;
                if (button.action == 3) g_sToggle = !g_sToggle;
                if (button.action == 4) g_portrait = !g_portrait;
                if (button.action == 5) g_sensitivity = g_sensitivity < .9f ? 1.f : g_sensitivity < 1.1f ? 1.5f : .65f;
                SaveSettings();
            }
            else if (Battle() && button.mask == TH_BUTTON_SHOOT && g_zToggle) g_zLatched = !g_zLatched;
            else if (Battle() && button.mask == TH_BUTTON_FOCUS && g_sToggle) g_sLatched = !g_sLatched;
            else g_fingers[event.tfinger.fingerId] = {button.mask,false};
            return;
        }
        if (g_settings) return;
        const auto stick=Stick();
        if(std::hypot(px-stick.x,py-stick.y)<stick.r*1.4f)
        {
            for(const auto &finger:g_fingers) if(finger.second.joystick)return;
            g_fingers[event.tfinger.fingerId]={0,false,true};UpdateStick(px,py);return;
        }
        const bool cropped = Gameplay() && UsePortraitBattle() && g_layout.drawableHeight > g_layout.drawableWidth;
        const float x = (px - g_layout.viewportX) * (cropped ? 384.f : 640.f) / g_layout.viewportWidth + (cropped ? 128.f : 0.f);
        const float y = (py - g_layout.viewportY) * (cropped ? 448.f : 480.f) / g_layout.viewportHeight + (cropped ? 16.f : 0.f);
        if (FrontEndTapMainMenu(x, y)) g_tapUntil = SDL_GetTicks() + 150;
        else
        {
            const int menuAction = Gameplay() ? ResultScreenTapMenu(x,y) : FrontEndTapSubmenu(x,y);
            if (menuAction)
            {
                if (menuAction == 2) g_tapUntil = SDL_GetTicks() + 150;
                return;
            }
            if (!Battle()) return;
            bool hasMovement = false;
            for (const auto &finger : g_fingers) hasMovement |= finger.second.movement;
            if (!hasMovement) g_fingers[event.tfinger.fingerId] = {0,true};
        }
    }
    else if (event.type == SDL_FINGERMOTION)
    {
        auto finger = g_fingers.find(event.tfinger.fingerId);
        if(finger!=g_fingers.end() && finger->second.joystick)
        {UpdateStick(event.tfinger.x*g_layout.drawableWidth,event.tfinger.y*g_layout.drawableHeight);return;}
        if (finger == g_fingers.end() || !finger->second.movement || !Battle()) return;
        const bool portrait = UsePortraitBattle() && g_layout.drawableHeight > g_layout.drawableWidth;
        g_dragX += event.tfinger.dx*g_layout.drawableWidth*(portrait ? 384.f : 640.f)/g_layout.viewportWidth*g_sensitivity;
        g_dragY += event.tfinger.dy*g_layout.drawableHeight*(portrait ? 448.f : 480.f)/g_layout.viewportHeight*g_sensitivity;
    }
    else if (event.type == SDL_FINGERUP)
    {
        auto finger=g_fingers.find(event.tfinger.fingerId);
        if(finger!=g_fingers.end() && finger->second.joystick)g_stickX=g_stickY=0;
        g_fingers.erase(event.tfinger.fingerId);
    }
    else if (event.type == SDL_APP_WILLENTERBACKGROUND || event.type == SDL_APP_DIDENTERBACKGROUND ||
             (event.type == SDL_WINDOWEVENT && event.window.event == SDL_WINDOWEVENT_FOCUS_LOST)) ResetTouchState();
}

void ResetTouchState() { g_buttons = 0; g_tapUntil = g_commandUntil = 0; g_command = 0; g_dragX = g_dragY = 0.0f; g_fingers.clear(); g_zLatched = g_sLatched = false; g_stickX=g_stickY=0; }
void DrawVirtualControls()
{
    glViewport(0,0,g_layout.drawableWidth,g_layout.drawableHeight);
    glDisable(GL_DEPTH_TEST); glDisable(GL_SCISSOR_TEST); glDisable(GL_CULL_FACE);
    glEnable(GL_BLEND); glBlendFunc(GL_SRC_ALPHA,GL_ONE_MINUS_SRC_ALPHA);
    IosLegacySetFog(GL_FALSE,0,0,0,0,1); IosLegacySetAlphaThreshold(-1); IosLegacySetForceAlphaDiscard(GL_FALSE);
    if (g_settings)
    {
        glBindTexture(GL_TEXTURE_2D,0); IosLegacySetTextureUsage(GL_FALSE,GL_FALSE);
        Rectangle(0,0,g_layout.drawableWidth,g_layout.drawableHeight,8,12,20,235);
    }
    for (const auto &button : Buttons())
    {
        bool held = (button.mask == TH_BUTTON_SHOOT && g_zLatched) || (button.mask == TH_BUTTON_FOCUS && g_sLatched);
        for (const auto &finger : g_fingers) held |= button.mask && finger.second.mask == button.mask;
        glBindTexture(GL_TEXTURE_2D,0); IosLegacySetTextureUsage(GL_FALSE,GL_FALSE);
        if(button.circular)
        {
            const float cx=button.x+button.w/2,cy=button.y+button.h/2,r=button.w/2;
            Circle(cx,cy,r,160,144,173,held ? 130 : 32);
            Circle(cx,cy,r-2,held ? 118 : 32,held ? 64 : 27,held ? 99 : 43,held ? 160 : 110);
        }
        else Rectangle(button.x,button.y,button.w,button.h,held ? 130 : 32,held ? 65 : 39,held ? 100 : 52,210);
        Label(button);
    }
    if(!g_settings)
    {
        const auto s=Stick();glBindTexture(GL_TEXTURE_2D,0);IosLegacySetTextureUsage(GL_FALSE,GL_FALSE);
        Circle(s.x,s.y,s.r,18,22,34,110);Circle(s.x,s.y,s.r*.78f,48,53,68,120);
        Circle(s.x+g_stickX*s.r*.48f,s.y+g_stickY*s.r*.48f,s.r*.35f,181,189,202,160);
    }
}
bool IsDeveloperInvincible() { return g_invincible; }
bool IsAutoBombEnabled() { return g_autoBomb; }

bool ConsumeDragDelta(f32 *dx, f32 *dy)
{
    if (dx != nullptr) *dx = g_dragX;
    if (dy != nullptr) *dy = g_dragY;
    const bool moved = std::fabs(g_dragX) > 0.001f || std::fabs(g_dragY) > 0.001f;
    g_dragX = g_dragY = 0.0f;
    return moved;
}

void SetPresentationShake(f32 x, f32 y) { g_shakeX = x; g_shakeY = y; }
void GetPresentationShake(f32 *x, f32 *y)
{
    if (x != nullptr) *x = g_shakeX;
    if (y != nullptr) *y = g_shakeY;
}
} // namespace th095::modern::ios
