#pragma once
#include <Windows.h>
#include <string>
#include <cstdint>

// -----------------------------------------------------------------------
// DiscordIPC - Direct Discord Rich Presence via Windows Named Pipe
// Implements the discord-rpc wire protocol internally.
// No external DLL required.
// -----------------------------------------------------------------------

class DiscordIPC
{
public:
    DiscordIPC() : pipe(INVALID_HANDLE_VALUE), connected(false), nonce(0) {}
    ~DiscordIPC() { Disconnect(); }

    bool Connect(const char* appId)
    {
        clientId = appId;
        // Try discord-ipc-0 through discord-ipc-9
        for (int i = 0; i < 10; i++)
        {
            std::string pipeName = "\\\\.\\pipe\\discord-ipc-" + std::to_string(i);
            pipe = CreateFileA(
                pipeName.c_str(),
                GENERIC_READ | GENERIC_WRITE,
                0, nullptr,
                OPEN_EXISTING,
                FILE_FLAG_OVERLAPPED,
                nullptr);

            if (pipe != INVALID_HANDLE_VALUE)
                break;
        }

        if (pipe == INVALID_HANDLE_VALUE)
            return false;

        // Send handshake
        std::string handshake = "{\"v\":1,\"client_id\":\"" + clientId + "\"}";
        if (!WriteFrame(0, handshake)) // op 0 = HANDSHAKE
        {
            Disconnect();
            return false;
        }

        // Read the ready response (fire-and-forget, we don't validate)
        ReadFrame();

        connected = true;
        return true;
    }

    void Disconnect()
    {
        if (pipe != INVALID_HANDLE_VALUE)
        {
            CloseHandle(pipe);
            pipe = INVALID_HANDLE_VALUE;
        }
        connected = false;
    }

    bool IsConnected() const { return connected && pipe != INVALID_HANDLE_VALUE; }

    void SetActivity(
        const std::string& details,
        const std::string& state,
        const std::string& largeImageKey,
        const std::string& largeImageText,
        const std::string& smallImageText,
        int64_t startTimestamp)
    {
        if (!IsConnected()) return;

        nonce++;
        std::string nonceStr = std::to_string(nonce);

        // Build activity JSON manually (avoids json library dependency)
        std::string activity = "{";
        activity += "\"details\":\"" + EscapeJson(details) + "\",";
        activity += "\"state\":\"" + EscapeJson(state) + "\",";

        if (startTimestamp > 0)
            activity += "\"timestamps\":{\"start\":" + std::to_string(startTimestamp) + "},";

        activity += "\"assets\":{";
        activity += "\"large_image\":\"" + EscapeJson(largeImageKey) + "\",";
        activity += "\"large_text\":\"" + EscapeJson(largeImageText) + "\"";
        if (!smallImageText.empty())
        {
            activity += ",\"small_image\":\"clock\"";
            activity += ",\"small_text\":\"" + EscapeJson(smallImageText) + "\"";
        }
        activity += "}";
        activity += "}";

        DWORD pid = GetCurrentProcessId();
        std::string payload = "{\"cmd\":\"SET_ACTIVITY\",\"args\":{\"pid\":" +
            std::to_string(pid) + ",\"activity\":" + activity + "},\"nonce\":\"" +
            nonceStr + "\"}";

        if (!WriteFrame(1, payload)) // op 1 = FRAME
        {
            Disconnect();
        }

        // Drain any pending reads
        ReadFrame();
    }

    void ClearActivity()
    {
        if (!IsConnected()) return;

        nonce++;
        DWORD pid = GetCurrentProcessId();
        std::string payload = "{\"cmd\":\"SET_ACTIVITY\",\"args\":{\"pid\":" +
            std::to_string(pid) + ",\"activity\":null},\"nonce\":\"" +
            std::to_string(nonce) + "\"}";

        if (!WriteFrame(1, payload))
            Disconnect();

        ReadFrame();
    }

private:
    HANDLE pipe;
    bool connected;
    std::string clientId;
    uint32_t nonce;

    bool WriteFrame(uint32_t opcode, const std::string& data)
    {
        uint32_t length = static_cast<uint32_t>(data.size());

        // Header: opcode (4 bytes LE) + length (4 bytes LE)
        uint8_t header[8];
        header[0] = opcode & 0xFF;
        header[1] = (opcode >> 8) & 0xFF;
        header[2] = (opcode >> 16) & 0xFF;
        header[3] = (opcode >> 24) & 0xFF;
        header[4] = length & 0xFF;
        header[5] = (length >> 8) & 0xFF;
        header[6] = (length >> 16) & 0xFF;
        header[7] = (length >> 24) & 0xFF;

        DWORD written = 0;
        if (!WriteFile(pipe, header, 8, &written, nullptr) || written != 8)
            return false;
        if (!WriteFile(pipe, data.c_str(), length, &written, nullptr) || written != length)
            return false;
        return true;
    }

    void ReadFrame()
    {
        // Non-blocking drain — just read and discard whatever Discord sent back
        uint8_t header[8] = {};
        DWORD available = 0;
        if (!PeekNamedPipe(pipe, nullptr, 0, nullptr, &available, nullptr) || available < 8)
            return;

        DWORD read = 0;
        ReadFile(pipe, header, 8, &read, nullptr);
        if (read < 8) return;

        uint32_t length = header[4] | (header[5] << 8) | (header[6] << 16) | (header[7] << 24);
        if (length > 0 && length < 65536)
        {
            std::string buf(length, '\0');
            ReadFile(pipe, &buf[0], length, &read, nullptr);
        }
    }

    std::string EscapeJson(const std::string& s)
    {
        std::string out;
        out.reserve(s.size());
        for (char c : s)
        {
            if (c == '"')  out += "\\\"";
            else if (c == '\\') out += "\\\\";
            else if (c == '\n') out += "\\n";
            else if (c == '\r') out += "\\r";
            else if (c == '\t') out += "\\t";
            else out += c;
        }
        return out;
    }
};
