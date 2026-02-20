#pragma once

#include "bakkesmod/plugin/bakkesmodplugin.h"
#include "DiscordIPC.h"

#include <string>
#include <unordered_map>
#include <ctime>

#define PLUGIN_VERSION "1.1.0"
#define DISCORD_APP_ID "1474318889440772238"

class RLDiscordStatus : public BakkesMod::Plugin::BakkesModPlugin
{
public:
    void onLoad() override;
    void onUnload() override;

private:
    // Presence updates
    void UpdateDiscordPresence();
    void UpdateMenuPresence();

    // Game event hooks
    void OnMatchStarted(std::string eventName);
    void OnMatchEnded(std::string eventName);
    void OnGoalScored(std::string eventName);
    void OnGameTimeUpdated(std::string eventName);
    void OnQueueStarted(std::string eventName);
    void OnQueueStopped(std::string eventName);

    // Game data
    int  GetBlueScore();
    int  GetOrangeScore();
    int  GetTimeRemaining();
    int  GetLocalTeamNum();      // 0=blue, 1=orange, -1=unknown
    int  GetCurrentPlaylistId();
    int  GetPartySize();
    std::string GetCurrentMapName();
    std::string GetRankDisplay(int playlistId);
    std::string GetGameModeName(int playlistId);
    std::string GetMapDisplayName(std::string mapName);
    std::string FormatTime(int seconds);

    // Discord
    DiscordIPC discord;

    // State
    bool    inGame          = false;
    bool    inQueue         = false;
    int     queuedPlaylist  = 0;
    int64_t matchStartTime  = 0;
    int     lastTimeRemaining = 0;

    // Settings
    std::shared_ptr<bool> showStadium;
    std::shared_ptr<bool> showScore;
    std::shared_ptr<bool> showTime;
    std::shared_ptr<bool> showMode;
    std::shared_ptr<bool> showRank;
    std::shared_ptr<bool> showElapsed;
    std::shared_ptr<int>  scoreStyle;  // 0=Full, 1=Minimal, 2=Win/Loss
};
