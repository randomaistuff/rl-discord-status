#include "pch.h"
#include "RLDiscordStatus.h"

BAKKESMOD_PLUGIN(RLDiscordStatus, "RL Discord Status", PLUGIN_VERSION, PLUGINTYPE_THREADED)

// -----------------------------------------------------------------------
// Lifecycle
// -----------------------------------------------------------------------

void RLDiscordStatus::onLoad()
{
    showStadium  = std::make_shared<bool>(true);
    showScore    = std::make_shared<bool>(true);
    showTime     = std::make_shared<bool>(true);
    showMode     = std::make_shared<bool>(true);
    showRank     = std::make_shared<bool>(true);
    showElapsed  = std::make_shared<bool>(true);
    scoreStyle   = std::make_shared<int>(0);

    cvarManager->registerCvar("rlds_show_stadium",  "1", "Show stadium name",             true, true, 0, true, 1).bindTo(showStadium);
    cvarManager->registerCvar("rlds_show_score",    "1", "Show score",                    true, true, 0, true, 1).bindTo(showScore);
    cvarManager->registerCvar("rlds_show_time",     "1", "Show time remaining",           true, true, 0, true, 1).bindTo(showTime);
    cvarManager->registerCvar("rlds_show_mode",     "1", "Show game mode",               true, true, 0, true, 1).bindTo(showMode);
    cvarManager->registerCvar("rlds_show_rank",     "1", "Show rank (ranked modes only)", true, true, 0, true, 1).bindTo(showRank);
    cvarManager->registerCvar("rlds_show_elapsed",  "1", "Show elapsed match timer",      true, true, 0, true, 1).bindTo(showElapsed);
    cvarManager->registerCvar("rlds_score_style",   "0", "Score style (0=Full, 1=Minimal, 2=Win/Loss)", true, true, 0, true, 2).bindTo(scoreStyle);

    if (discord.Connect(DISCORD_APP_ID))
        cvarManager->log("RLDiscordStatus: Discord connected!");
    else
        cvarManager->log("RLDiscordStatus: Discord not running, will retry on next update.");

    UpdateMenuPresence();

    // In-game hooks
    gameWrapper->HookEvent("Function TAGame.GameEvent_Soccar_TA.OnGameTimeUpdated",
        [this](std::string e) { OnGameTimeUpdated(e); });
    gameWrapper->HookEvent("Function TAGame.GameEvent_Soccar_TA.EventGoalScored",
        [this](std::string e) { OnGoalScored(e); });
    gameWrapper->HookEvent("Function TAGame.GameEvent_Soccar_TA.EventMatchEnded",
        [this](std::string e) { OnMatchEnded(e); });
    gameWrapper->HookEvent("Function GameEvent_TA.Countdown.BeginState",
        [this](std::string e) { OnMatchStarted(e); });
    gameWrapper->HookEvent("Function TAGame.GameEvent_Soccar_TA.InitGame",
        [this](std::string e) { OnMatchStarted(e); });
    gameWrapper->HookEvent("Function TAGame.GFxHUD_TA.Destroyed",
        [this](std::string e) { OnMatchEnded(e); });

    // Queue hooks
    gameWrapper->HookEvent("Function TAGame.GFxData_Matchmaking_TA.StartSearching",
        [this](std::string e) { OnQueueStarted(e); });
    gameWrapper->HookEvent("Function TAGame.GFxData_Matchmaking_TA.StopSearching",
        [this](std::string e) { OnQueueStopped(e); });
    // Backup queue hook (fires when match is found / user cancels)
    gameWrapper->HookEvent("Function TAGame.GameEvent_TA.OnMatchmakingStarted",
        [this](std::string e) { OnQueueStarted(e); });

    cvarManager->log("RLDiscordStatus v" + std::string(PLUGIN_VERSION) + " loaded!");
}

void RLDiscordStatus::onUnload()
{
    gameWrapper->UnhookEvent("Function TAGame.GameEvent_Soccar_TA.OnGameTimeUpdated");
    gameWrapper->UnhookEvent("Function TAGame.GameEvent_Soccar_TA.EventGoalScored");
    gameWrapper->UnhookEvent("Function TAGame.GameEvent_Soccar_TA.EventMatchEnded");
    gameWrapper->UnhookEvent("Function GameEvent_TA.Countdown.BeginState");
    gameWrapper->UnhookEvent("Function TAGame.GameEvent_Soccar_TA.InitGame");
    gameWrapper->UnhookEvent("Function TAGame.GFxHUD_TA.Destroyed");
    gameWrapper->UnhookEvent("Function TAGame.GFxData_Matchmaking_TA.StartSearching");
    gameWrapper->UnhookEvent("Function TAGame.GFxData_Matchmaking_TA.StopSearching");
    gameWrapper->UnhookEvent("Function TAGame.GameEvent_TA.OnMatchmakingStarted");

    discord.ClearActivity();
    discord.Disconnect();
    cvarManager->log("RLDiscordStatus unloaded!");
}

// -----------------------------------------------------------------------
// Presence Updates
// -----------------------------------------------------------------------

void RLDiscordStatus::UpdateMenuPresence()
{
    if (!discord.IsConnected())
        discord.Connect(DISCORD_APP_ID);

    std::string details;
    std::string state;

    if (inQueue)
    {
        details = "Searching for Match...";
        state   = GetGameModeName(queuedPlaylist);
    }
    else
    {
        details = "In Menus";
        state   = "Rocket League";
    }

    // Party size in menus
    int partySize = GetPartySize();
    if (partySize > 1)
    {
        details += "  |  Party of " + std::to_string(partySize);
    }

    discord.SetActivity(details, state, "rocket_league", "Rocket League", "", 0);
}

void RLDiscordStatus::UpdateDiscordPresence()
{
    if (!gameWrapper->IsInGame() && !gameWrapper->IsInOnlineGame()) return;

    if (!discord.IsConnected())
    {
        if (!discord.Connect(DISCORD_APP_ID))
            return;
    }

    std::string mapName   = GetCurrentMapName();
    int   playlistId      = GetCurrentPlaylistId();
    std::string gameMode  = GetGameModeName(playlistId);
    int   timeRemaining   = GetTimeRemaining();
    bool  isFreeplay      = !gameWrapper->IsInOnlineGame();

    int localTeam = GetLocalTeamNum();
    int myScore   = (localTeam == 1) ? GetOrangeScore() : GetBlueScore();
    int oppScore  = (localTeam == 1) ? GetBlueScore()   : GetOrangeScore();

    // --- Details line (top): stadium + rank ---
    std::string details;
    if (*showStadium && !mapName.empty())
        details = GetMapDisplayName(mapName);
    else
        details = "Rocket League";

    if (*showRank && !isFreeplay)
    {
        std::string rank = GetRankDisplay(playlistId);
        if (!rank.empty())
            details += "  |  " + rank;
    }

    // --- State line (bottom): score | mode | time ---
    std::string state;

    if (*showScore && !isFreeplay)
    {
        int style = *scoreStyle;
        if (style == 1)
        {
            // Minimal: "2 - 1"
            state += std::to_string(myScore) + " - " + std::to_string(oppScore);
        }
        else if (style == 2)
        {
            // Context: Winning / Losing / Tied
            if (myScore > oppScore)
                state += "Winning " + std::to_string(myScore) + "-" + std::to_string(oppScore);
            else if (myScore < oppScore)
                state += "Losing " + std::to_string(myScore) + "-" + std::to_string(oppScore);
            else
                state += "Tied " + std::to_string(myScore) + "-" + std::to_string(oppScore);
        }
        else
        {
            // Full: "Blue 2 - 1 Orange" (your team first)
            std::string myLabel  = (localTeam == 1) ? "Orange" : "Blue";
            std::string oppLabel = (localTeam == 1) ? "Blue"   : "Orange";
            state += myLabel + " " + std::to_string(myScore) +
                     " - " + std::to_string(oppScore) + " " + oppLabel;
        }
    }

    if (*showMode && !gameMode.empty())
    {
        if (!state.empty()) state += "  |  ";
        state += gameMode;
    }

    if (*showTime)
    {
        if (timeRemaining > 0)
        {
            if (!state.empty()) state += "  |  ";
            state += FormatTime(timeRemaining);
        }
        else if (timeRemaining == 0 && inGame && !isFreeplay)
        {
            if (!state.empty()) state += "  |  ";
            state += "Overtime!";
        }
    }

    if (state.empty()) state = isFreeplay ? "Freeplay" : "Playing";

    int64_t elapsed = (*showElapsed) ? matchStartTime : 0;
    discord.SetActivity(details, state, "rocket_league", "Rocket League", "", elapsed);
}

// -----------------------------------------------------------------------
// Event Handlers
// -----------------------------------------------------------------------

void RLDiscordStatus::OnMatchStarted(std::string eventName)
{
    inGame        = true;
    inQueue       = false;
    matchStartTime = static_cast<int64_t>(time(nullptr));

    gameWrapper->SetTimeout([this](GameWrapper* gw) {
        UpdateDiscordPresence();
    }, 2.0f);
}

void RLDiscordStatus::OnMatchEnded(std::string eventName)
{
    inGame         = false;
    matchStartTime = 0;
    UpdateMenuPresence();
}

void RLDiscordStatus::OnGoalScored(std::string eventName)
{
    gameWrapper->SetTimeout([this](GameWrapper* gw) {
        UpdateDiscordPresence();
    }, 1.0f);
}

void RLDiscordStatus::OnGameTimeUpdated(std::string eventName)
{
    int t = GetTimeRemaining();
    if (abs(t - lastTimeRemaining) >= 2 || t == 0)
    {
        lastTimeRemaining = t;
        UpdateDiscordPresence();
    }
}

void RLDiscordStatus::OnQueueStarted(std::string eventName)
{
    if (inGame) return; // don't override in-game presence
    inQueue        = true;
    queuedPlaylist = GetCurrentPlaylistId();
    UpdateMenuPresence();
}

void RLDiscordStatus::OnQueueStopped(std::string eventName)
{
    if (inGame) return;
    inQueue = false;
    UpdateMenuPresence();
}

// -----------------------------------------------------------------------
// Game Data
// -----------------------------------------------------------------------

int RLDiscordStatus::GetBlueScore()
{
    if (!gameWrapper->IsInGame() && !gameWrapper->IsInOnlineGame()) return 0;
    ServerWrapper sw = gameWrapper->IsInOnlineGame()
        ? gameWrapper->GetOnlineGame() : gameWrapper->GetGameEventAsServer();
    if (sw.IsNull()) return 0;
    auto teams = sw.GetTeams();
    return (teams.Count() >= 1) ? teams.Get(0).GetScore() : 0;
}

int RLDiscordStatus::GetOrangeScore()
{
    if (!gameWrapper->IsInGame() && !gameWrapper->IsInOnlineGame()) return 0;
    ServerWrapper sw = gameWrapper->IsInOnlineGame()
        ? gameWrapper->GetOnlineGame() : gameWrapper->GetGameEventAsServer();
    if (sw.IsNull()) return 0;
    auto teams = sw.GetTeams();
    return (teams.Count() >= 2) ? teams.Get(1).GetScore() : 0;
}

int RLDiscordStatus::GetTimeRemaining()
{
    if (!gameWrapper->IsInGame() && !gameWrapper->IsInOnlineGame()) return 0;
    ServerWrapper sw = gameWrapper->IsInOnlineGame()
        ? gameWrapper->GetOnlineGame() : gameWrapper->GetGameEventAsServer();
    if (sw.IsNull()) return 0;
    return sw.GetSecondsRemaining();
}

int RLDiscordStatus::GetLocalTeamNum()
{
    auto car = gameWrapper->GetLocalCar();
    if (car.IsNull()) return -1;
    auto pri = car.GetPRI();
    if (pri.IsNull()) return -1;
    return pri.GetTeamNum();
}

std::string RLDiscordStatus::GetCurrentMapName()
{
    return gameWrapper->GetCurrentMap();
}

int RLDiscordStatus::GetCurrentPlaylistId()
{
    if (!gameWrapper->IsInOnlineGame()) return 0;
    ServerWrapper sw = gameWrapper->GetOnlineGame();
    if (sw.IsNull()) return 0;
    auto playlist = sw.GetPlaylist();
    if (playlist.IsNull()) return 0;
    return playlist.GetPlaylistId();
}

int RLDiscordStatus::GetPartySize()
{
    // Party size detection via GameWrapper isn't directly exposed in this SDK version.
    // Returns 1 (solo) as a safe default.
    return 1;
}

std::string RLDiscordStatus::GetRankDisplay(int playlistId)
{
    // Only show rank for competitive playlists
    static const int rankedPlaylists[] = { 10, 11, 13, 22, 27, 28, 29, 34 };
    bool isRanked = false;
    for (int p : rankedPlaylists)
        if (p == playlistId) { isRanked = true; break; }
    if (!isRanked) return "";

    auto mw  = gameWrapper->GetMMRWrapper();
    auto uid = gameWrapper->GetUniqueID();

    auto rank = mw.GetPlayerRank(uid, playlistId);
    int tier     = rank.Tier;
    int division = rank.Division;

    static const char* tiers[] = {
        "Unranked",
        "Bronze I",    "Bronze II",    "Bronze III",
        "Silver I",    "Silver II",    "Silver III",
        "Gold I",      "Gold II",      "Gold III",
        "Platinum I",  "Platinum II",  "Platinum III",
        "Diamond I",   "Diamond II",   "Diamond III",
        "Champion I",  "Champion II",  "Champion III",
        "Grand Champ I","Grand Champ II","Grand Champ III",
        "Supersonic Legend"
    };

    if (tier < 0 || tier > 22) return "";
    if (tier == 0)  return "Unranked";
    if (tier == 22) return "Supersonic Legend";

    return std::string(tiers[tier]) + " Div " + std::to_string(division + 1);
}

// -----------------------------------------------------------------------
// Helpers
// -----------------------------------------------------------------------

std::string RLDiscordStatus::FormatTime(int seconds)
{
    char buf[16];
    snprintf(buf, sizeof(buf), "%d:%02d", seconds / 60, seconds % 60);
    return std::string(buf);
}

std::string RLDiscordStatus::GetGameModeName(int playlistId)
{
    switch (playlistId)
    {
        case 1:  return "Casual 1v1";
        case 2:  return "Casual 2v2";
        case 3:  return "Casual 3v3";
        case 4:  return "Casual 4v4";
        case 6:  return "Private Match";
        case 10: return "Ranked 1v1";
        case 11: return "Ranked 2v2";
        case 13: return "Ranked 3v3";
        case 22: return "Ranked Hoops";
        case 27: return "Ranked Rumble";
        case 28: return "Ranked Dropshot";
        case 29: return "Ranked Snowday";
        case 34: return "Tournament";
        case 44: return "Duels";
        default: return playlistId > 0 ? "Playlist #" + std::to_string(playlistId) : "Freeplay";
    }
}

std::string RLDiscordStatus::GetMapDisplayName(std::string mapName)
{
    static const std::unordered_map<std::string, std::string> mapNames = {
        {"Stadium_P",              "DFH Stadium"},
        {"Stadium_Race_Day_P",     "DFH Stadium (Day)"},
        {"Stadium_Winter_P",       "DFH Stadium (Snowy)"},
        {"Stadium_Foggy_P",        "DFH Stadium (Stormy)"},
        {"Park_P",                 "Beckwith Park"},
        {"Park_Night_P",           "Beckwith Park (Midnight)"},
        {"Park_Rainy_P",           "Beckwith Park (Stormy)"},
        {"TrainStation_P",         "Urban Central"},
        {"TrainStation_Night_P",   "Urban Central (Night)"},
        {"TrainStation_Dawn_P",    "Urban Central (Dawn)"},
        {"Wasteland_P",            "Wasteland"},
        {"Wasteland_Night_P",      "Wasteland (Night)"},
        {"NeoTokyo_P",             "Tokyo Underpass"},
        {"Utopia_P",               "Utopia Coliseum"},
        {"Utopia_Retro_P",         "Utopia Coliseum (Dusk)"},
        {"Utopia_Snow_P",          "Utopia Coliseum (Snowy)"},
        {"EuropaCenter_P",         "Mannfield"},
        {"EuropaCenter_Night_P",   "Mannfield (Night)"},
        {"EuropaCenter_Stormy_P",  "Mannfield (Stormy)"},
        {"Underpass_P",            "AquaDome"},
        {"ShatterShot_P",          "Dropshot Arena"},
        {"HoopsStadium_P",         "Dunk House"},
        {"CS_Day_P",               "Champions Field"},
        {"Farmstead_P",            "Farmstead"},
        {"Farmstead_Night_P",      "Farmstead (Night)"},
        {"Forest_Night_P",         "Forbidden Temple"},
        {"Beach_P",                "Salty Shores"},
        {"Beach_Night_P",          "Salty Shores (Night)"},
        {"Throwback_Stadium_P",    "Throwback Stadium"},
        {"Rivals_P",               "Rivals Arena"},
        {"Street_P",               "Sovereign Heights"},
        {"Haunted_P",              "Haunted Hallows"},
        {"Musical_P",              "Neon Fields"},
        {"Starbase_Arc_P",         "Starbase ARC"},
        {"Outlaw_P",               "Deadeye Canyon"},
        {"Corridor_P",             "Fort Lawrence"},
    };

    auto it = mapNames.find(mapName);
    if (it != mapNames.end()) return it->second;

    std::string result = mapName;
    if (result.size() > 2 && result.substr(result.size() - 2) == "_P")
        result = result.substr(0, result.size() - 2);
    for (char& c : result) if (c == '_') c = ' ';
    return result;
}
