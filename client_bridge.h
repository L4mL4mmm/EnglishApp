#ifndef CLIENT_BRIDGE_H
#define CLIENT_BRIDGE_H

#include <atomic>
#include <iostream>
#include <string>

// --- BIẾN DÙNG CHUNG (Shared Variables) ---
// Khai báo extern để gui_main.cpp hiểu là các biến này đang nằm bên client.cpp
extern std::string sessionToken; // Token đăng nhập
extern std::string currentLevel; // Level hiện tại (beginner/intermediate...)
extern bool running;             // Trạng thái chương trình
extern bool hasIncomingCall;     // Có cuộc gọi đến
extern std::string pendingCallId;
extern std::string pendingCallerId;
extern std::string pendingCallerName;
extern int pendingCallUdpPort;       // UDP port của người gọi
extern int activeCallUdpPort;        // UDP port của người nhận (khi mình gọi)
extern std::atomic<bool> inCallMode; // Trạng thái đang gọi
extern std::string activeCallId;     // ID cuộc gọi đang diễn ra

// --- CÁC HÀM DÙNG CHUNG (Shared Functions) ---
bool connectToServer(const char *ip, int port);
bool sendMessage(const std::string &message);
std::string waitForResponse(int timeoutMs = 3000);
void receiveThreadFunc();
std::string getJsonValue(const std::string &json, const std::string &key);

#endif