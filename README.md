# RL Discord Status — BakkesMod Plugin

A BakkesMod plugin for Rocket League that shows your current **stadium, score, match time, and game mode** as your Discord Rich Presence status.

---

## ✅ Prerequisites

- **Rocket League** installed
- **BakkesMod** installed and injected
- **Visual Studio 2022** with the **Desktop development with C++** workload
- **BakkesModSDK** NuGet package (installed via NuGet in your VS project)
- **Discord** running on your PC

---

## Step 1 — Create a Discord Developer Application

1. Go to [discord.com/developers/applications](https://discord.com/developers/applications)
2. Click **"New Application"**
3. Name it **"Rocket League Status"** → click **Create**
4. On the **General Information** page, copy your **APPLICATION ID** (a long number)
5. *(Optional but recommended)* Go to **Rich Presence → Art Assets** in the left sidebar:
   - Upload a Rocket League image and name the key exactly: `rocket_league`
   - This image will appear next to your status in Discord

---

## Step 2 — Add Your Application ID to the Plugin

Open `RLDiscordStatus.h` and find this line near the top:

```cpp
#define DISCORD_APP_ID "YOUR_DISCORD_APP_ID_HERE"
```

Replace `YOUR_DISCORD_APP_ID_HERE` with your **Application ID** from Step 1, for example:

```cpp
#define DISCORD_APP_ID "1234567890123456789"
```

---

## Step 3 — Download discord-rpc DLL & LIB

The plugin needs two files from the **discord-rpc v3.4.0** release:

1. Download from: https://github.com/discord/discord-rpc/releases/tag/v3.4.0
2. Download `discord-rpc-win.zip`
3. Extract and find these files in `win64-dynamic/`:
   - `discord-rpc.dll` → copy to this project folder
   - `discord-rpc.lib` → copy to this project folder

---

## Step 4 — Build in Visual Studio 2022

1. Open **Visual Studio 2022**
2. Choose **File → Open → Project/Solution**
3. Open `RLDiscordStatus.vcxproj` from this folder
4. In Visual Studio, open **Tools → NuGet Package Manager → Package Manager Console** and run:
   ```
   Install-Package BakkesModSDK
   ```
5. Set configuration to **Release | x64**
6. Press **Ctrl+Shift+B** to build

The output DLL will be at: `x64\Release\RLDiscordStatus.dll`

---

## Step 5 — Install the Plugin

Copy these two files to your BakkesMod plugins folder:

```
%appdata%\bakkesmod\bakkesmod\plugins\
```

- `x64\Release\RLDiscordStatus.dll` → copy here as `RLDiscordStatus.dll`
- `discord-rpc.dll` → copy here too (must be in the same folder!)

---

## Step 6 — Load the Plugin

In Rocket League with BakkesMod injected, open the BakkesMod console (default: F6) and type:

```
plugin load rldiscordstatus
```

Or place the plugin in the **Plugin Manager** tab in BakkesMod (F2 → Plugins).

---

## What it Shows on Discord

| Field | Example |
|-------|---------|
| Details | 🏟️ DFH Stadium |
| State | 🟦 2 - 1 🟧  \|  Ranked 3v3 |
| Timer | Elapsed time since match started |
| Small text | ⏱️ 3:42 remaining |

---

## Settings

Open BakkesMod (F2) → Plugins → **RL Discord Status** to toggle:
- Show/hide stadium name
- Show/hide score
- Show/hide time remaining
- Show/hide game mode

---

## Supported Game Modes

- Casual & Ranked: 1v1, 2v2, 3v3
- Ranked Hoops, Rumble, Dropshot, Snowday
- Private Match, Tournaments
- Freeplay / Training

---

## Troubleshooting

**Status not showing?**
- Make sure Discord is running before Rocket League
- Verify `discord-rpc.dll` is in the plugins folder alongside the plugin DLL
- Check your Application ID is correctly set in `RLDiscordStatus.h`
- In BakkesMod console, type `plugin load rldiscordstatus` to reload

**Build errors about missing BakkesModSDK?**
- Right-click the project in VS → Manage NuGet Packages → search for `BakkesModSDK` → Install

**Build errors about `discord-rpc.lib` not found?**
- Ensure `discord-rpc.lib` is in the same folder as `RLDiscordStatus.vcxproj`
