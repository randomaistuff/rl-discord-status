#pragma once
/** @file discord_register.h
 * Discord Rich Presence - Application Registration Helper
 * Source: https://github.com/discord/discord-rpc/releases/tag/v3.4.0
 */

#ifdef __cplusplus
extern "C" {
#endif

void Discord_Register(const char* applicationId, const char* command);
void Discord_RegisterSteamGame(const char* applicationId, const char* steamId);

#ifdef __cplusplus
} /* extern "C" */
#endif
