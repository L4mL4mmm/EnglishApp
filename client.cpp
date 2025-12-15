/**
 * ============================================================================
 * ENGLISH LEARNING APP - CLIENT (Enhanced Version with Real-time Chat)
 * ============================================================================
 * Client với giao diện console tương tác, hỗ trợ đầy đủ các chức năng
 *
 * [FIX] Thêm background thread để nhận message từ server bất cứ lúc nào
 * [FIX] Sử dụng message queue để đồng bộ giữa receive thread và main thread
 * [FIX] Non-blocking receive cho real-time chat notification
 *
 * Compile: g++ -std=c++17 -pthread -o client client.cpp
 * Run: ./client [server_ip] [port]
 * ============================================================================
 */

#include <iostream>
#include <string>
#include <cstring>
#include <thread>
#include <atomic>
#include <mutex>
#include <chrono>
#include <sstream>
#include <algorithm>
#include <vector>
#include <queue>
#include <condition_variable>
#include <map>

// POSIX socket headers
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <signal.h>
#include <poll.h>
#include <fcntl.h>

// ============================================================================
// CẤU HÌNH CLIENT
// ============================================================================
#define DEFAULT_SERVER "127.0.0.1"
#define DEFAULT_PORT 8888
#define BUFFER_SIZE 65536

// ============================================================================
// BIẾN TOÀN CỤC
// ============================================================================
int clientSocket = -1;
std::string sessionToken = "";
std::string currentUserId = "";
std::string currentUserName = "";
std::string currentLevel = "beginner";
std::atomic<bool> running(true);
std::atomic<bool> loggedIn(false);
std::atomic<bool> inChatMode(false);

// [FIX] Mutex để bảo vệ socket khi gửi dữ liệu
std::mutex socketMutex;
std::mutex printMutex;

// [FIX] Queue để nhận response từ server (cho request-response pattern)
std::queue<std::string> responseQueue;
std::mutex responseQueueMutex;
std::condition_variable responseCondition;

// [FIX] Biến để lưu thông tin tin nhắn mới
std::atomic<bool> hasNewNotification(false);
std::string pendingChatUserId = "";
std::string pendingChatUserName = "";
std::mutex notificationMutex;

// [FIX] Biến để kiểm soát việc hiển thị thông báo
std::atomic<bool> canShowNotification(true);

// [FIX] Lưu ID người đang chat để hiển thị tin nhắn trực tiếp thay vì popup
std::string currentChatPartnerId = "";
std::string currentChatPartnerName = "";
std::mutex chatPartnerMutex;

// ============================================================================
// HÀM TIỆN ÍCH
// ============================================================================

long long getCurrentTimestamp() {
    return std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::system_clock::now().time_since_epoch()
    ).count();
}

std::string generateMessageId() {
    static int counter = 0;
    return "msg_" + std::to_string(++counter);
}

// [FIX] Hàm in có màu với mutex protection
void printColored(const std::string& text, const std::string& color = "") {
    std::lock_guard<std::mutex> lock(printMutex);
    if (color == "green") std::cout << "\033[32m";
    else if (color == "red") std::cout << "\033[31m";
    else if (color == "yellow") std::cout << "\033[33m";
    else if (color == "blue") std::cout << "\033[34m";
    else if (color == "cyan") std::cout << "\033[36m";
    else if (color == "magenta") std::cout << "\033[35m";
    else if (color == "bold") std::cout << "\033[1m";

    std::cout << text;

    if (!color.empty()) std::cout << "\033[0m";
    std::cout << std::flush;
}

// [FIX] Hàm in không lock mutex (dùng khi đã có lock)
void printColoredNoLock(const std::string& text, const std::string& color = "") {
    if (color == "green") std::cout << "\033[32m";
    else if (color == "red") std::cout << "\033[31m";
    else if (color == "yellow") std::cout << "\033[33m";
    else if (color == "blue") std::cout << "\033[34m";
    else if (color == "cyan") std::cout << "\033[36m";
    else if (color == "magenta") std::cout << "\033[35m";
    else if (color == "bold") std::cout << "\033[1m";

    std::cout << text;

    if (!color.empty()) std::cout << "\033[0m";
    std::cout << std::flush;
}

void clearScreen() {
    std::cout << "\033[2J\033[1;1H" << std::flush;
}

void waitEnter() {
    printColored("\nPress Enter to continue...", "cyan");
    std::cin.get();
}

// ============================================================================
// JSON PARSER
// ============================================================================

std::string getJsonValue(const std::string& json, const std::string& key) {
    std::string searchKey = "\"" + key + "\"";
    size_t keyPos = json.find(searchKey);
    if (keyPos == std::string::npos) return "";

    size_t colonPos = json.find(':', keyPos);
    if (colonPos == std::string::npos) return "";

    size_t valueStart = colonPos + 1;
    while (valueStart < json.length() && (json[valueStart] == ' ' || json[valueStart] == '\t' || json[valueStart] == '\n')) {
        valueStart++;
    }

    if (valueStart >= json.length()) return "";

    if (json[valueStart] == '"') {
        size_t valueEnd = valueStart + 1;
        while (valueEnd < json.length()) {
            if (json[valueEnd] == '"' && json[valueEnd - 1] != '\\') break;
            valueEnd++;
        }
        if (valueEnd < json.length()) {
            return json.substr(valueStart + 1, valueEnd - valueStart - 1);
        }
    } else {
        size_t valueEnd = json.find_first_of(",}\n]", valueStart);
        if (valueEnd != std::string::npos) {
            std::string value = json.substr(valueStart, valueEnd - valueStart);
            value.erase(0, value.find_first_not_of(" \t\n\r"));
            if (!value.empty()) value.erase(value.find_last_not_of(" \t\n\r") + 1);
            return value;
        }
    }

    return "";
}

std::string getJsonObject(const std::string& json, const std::string& key) {
    std::string searchKey = "\"" + key + "\"";
    size_t keyPos = json.find(searchKey);
    if (keyPos == std::string::npos) return "";

    size_t colonPos = json.find(':', keyPos);
    if (colonPos == std::string::npos) return "";

    size_t bracePos = json.find('{', colonPos);
    if (bracePos == std::string::npos) return "";

    int braceCount = 1;
    size_t endPos = bracePos + 1;
    while (endPos < json.length() && braceCount > 0) {
        if (json[endPos] == '{') braceCount++;
        else if (json[endPos] == '}') braceCount--;
        endPos++;
    }

    return json.substr(bracePos, endPos - bracePos);
}

std::string getJsonArray(const std::string& json, const std::string& key) {
    std::string searchKey = "\"" + key + "\"";
    size_t keyPos = json.find(searchKey);
    if (keyPos == std::string::npos) return "";

    size_t colonPos = json.find(':', keyPos);
    if (colonPos == std::string::npos) return "";

    size_t bracketPos = json.find('[', colonPos);
    if (bracketPos == std::string::npos) return "";

    int bracketCount = 1;
    size_t endPos = bracketPos + 1;
    while (endPos < json.length() && bracketCount > 0) {
        if (json[endPos] == '[') bracketCount++;
        else if (json[endPos] == ']') bracketCount--;
        endPos++;
    }

    return json.substr(bracketPos, endPos - bracketPos);
}

std::vector<std::string> parseJsonArray(const std::string& arrayStr) {
    std::vector<std::string> result;
    if (arrayStr.empty() || arrayStr[0] != '[') return result;

    size_t pos = 1;
    while (pos < arrayStr.length()) {
        while (pos < arrayStr.length() && (arrayStr[pos] == ' ' || arrayStr[pos] == '\n' || arrayStr[pos] == '\t' || arrayStr[pos] == ',')) {
            pos++;
        }

        if (pos >= arrayStr.length() || arrayStr[pos] == ']') break;

        if (arrayStr[pos] == '{') {
            int braceCount = 1;
            size_t start = pos;
            pos++;
            while (pos < arrayStr.length() && braceCount > 0) {
                if (arrayStr[pos] == '{') braceCount++;
                else if (arrayStr[pos] == '}') braceCount--;
                pos++;
            }
            result.push_back(arrayStr.substr(start, pos - start));
        } else {
            pos++;
        }
    }

    return result;
}

std::string unescapeJson(const std::string& str) {
    std::string result;
    for (size_t i = 0; i < str.length(); i++) {
        if (str[i] == '\\' && i + 1 < str.length()) {
            switch (str[i + 1]) {
                case 'n': result += '\n'; i++; break;
                case 'r': result += '\r'; i++; break;
                case 't': result += '\t'; i++; break;
                case '"': result += '"'; i++; break;
                case '\\': result += '\\'; i++; break;
                default: result += str[i];
            }
        } else {
            result += str[i];
        }
    }
    return result;
}

std::string escapeJson(const std::string& str) {
    std::string result;
    for (char c : str) {
        switch (c) {
            case '"': result += "\\\""; break;
            case '\\': result += "\\\\"; break;
            case '\n': result += "\\n"; break;
            case '\r': result += "\\r"; break;
            case '\t': result += "\\t"; break;
            default: result += c;
        }
    }
    return result;
}

// ============================================================================
// [FIX] PUSH NOTIFICATION HANDLER
// Xử lý tin nhắn push từ server (tin nhắn chat real-time)
// ============================================================================

void handlePushNotification(const std::string& message) {
    std::string messageType = getJsonValue(message, "messageType");

    if (messageType == "RECEIVE_MESSAGE") {
        // Tin nhắn chat real-time từ user khác
        std::string payload = getJsonObject(message, "payload");
        std::string senderId = getJsonValue(payload, "senderId");
        std::string senderName = getJsonValue(payload, "senderName");
        std::string messageContent = getJsonValue(payload, "messageContent");

        // [FIX] Kiểm tra xem có đang chat với người này không
        bool isChattingWithSender = false;
        {
            std::lock_guard<std::mutex> lock(chatPartnerMutex);
            isChattingWithSender = (inChatMode && currentChatPartnerId == senderId);
        }

        if (isChattingWithSender) {
            // [FIX] Đang chat với người này -> hiển thị tin nhắn trực tiếp
            std::lock_guard<std::mutex> lock(printMutex);
            std::cout << "\n\033[33m" << senderName << ": \033[0m" << messageContent << "\n";
            std::cout << "\033[32mYou: \033[0m" << std::flush;
        } else {
            // Không đang chat với người này -> lưu thông báo và hiện popup
            {
                std::lock_guard<std::mutex> lock(notificationMutex);
                pendingChatUserId = senderId;
                pendingChatUserName = senderName;
                hasNewNotification = true;
            }

            // Hiển thị popup thông báo
            if (canShowNotification) {
                std::lock_guard<std::mutex> lock(printMutex);
                std::cout << "\n";
                std::cout << "\033[33m╔══════════════════════════════════════════╗\033[0m\n";
                std::cout << "\033[33m║  📬 NEW MESSAGE from \033[36m" << senderName << "\033[33m\033[0m\n";
                std::cout << "\033[33m║  \033[0m\"" << messageContent << "\"\n";
                std::cout << "\033[33m║  Press 'r' in menu to reply quickly      ║\033[0m\n";
                std::cout << "\033[33m╚══════════════════════════════════════════╝\033[0m\n";
                std::cout << std::flush;
            }
        }
    }
    else if (messageType == "UNREAD_MESSAGES_NOTIFICATION") {
        // Thông báo có tin nhắn chưa đọc khi mới login
        std::string payload = getJsonObject(message, "payload");
        std::string unreadCount = getJsonValue(payload, "unreadCount");
        std::string messagesArray = getJsonArray(payload, "messages");
        std::vector<std::string> messages = parseJsonArray(messagesArray);

        if (!messages.empty()) {
            // Lưu thông tin người gửi cuối cùng
            for (const std::string& msg : messages) {
                std::lock_guard<std::mutex> nlock(notificationMutex);
                pendingChatUserId = getJsonValue(msg, "senderId");
                pendingChatUserName = getJsonValue(msg, "senderName");
                hasNewNotification = true;
            }

            if (canShowNotification) {
                std::lock_guard<std::mutex> lock(printMutex);
                std::cout << "\n";
                std::cout << "\033[33m╔══════════════════════════════════════════╗\033[0m\n";
                std::cout << "\033[33m║  📬 You have \033[36m" << unreadCount << "\033[33m unread message(s)!          ║\033[0m\n";
                std::cout << "\033[33m╠══════════════════════════════════════════╣\033[0m\n";

                for (const std::string& msg : messages) {
                    std::string senderName = getJsonValue(msg, "senderName");
                    std::string content = getJsonValue(msg, "content");
                    std::string preview = content.length() > 25 ? content.substr(0, 25) + "..." : content;
                    std::cout << "\033[33m║  \033[36m" << senderName << "\033[0m: " << preview << "\n";
                }

                std::cout << "\033[33m╠══════════════════════════════════════════╣\033[0m\n";
                std::cout << "\033[33m║  Go to Chat (option 4) to reply!         ║\033[0m\n";
                std::cout << "\033[33m╚══════════════════════════════════════════╝\033[0m\n";
                std::cout << std::flush;
            }
        }
    }
}

// ============================================================================
// [FIX] BACKGROUND RECEIVE THREAD
// Thread chạy nền để liên tục nhận message từ server
// Phân loại message: response (đưa vào queue) hoặc push notification (hiển thị)
// ============================================================================

void receiveThreadFunc() {
    while (running && clientSocket >= 0) {
        // Sử dụng poll để kiểm tra có dữ liệu không (non-blocking check)
        struct pollfd pfd;
        pfd.fd = clientSocket;
        pfd.events = POLLIN;

        int ret = poll(&pfd, 1, 100); // timeout 100ms để check running flag

        if (ret < 0) {
            if (running) {
                printColored("\n[ERROR] Poll error\n", "red");
            }
            break;
        }

        if (ret == 0) {
            // Timeout, không có dữ liệu, tiếp tục vòng lặp
            continue;
        }

        if (pfd.revents & POLLIN) {
            // Có dữ liệu để đọc
            uint32_t msgLen = 0;
            ssize_t bytesRead = recv(clientSocket, &msgLen, sizeof(msgLen), MSG_WAITALL);

            if (bytesRead <= 0) {
                if (running) {
                    printColored("\n[INFO] Disconnected from server\n", "red");
                    running = false;
                }
                break;
            }

            msgLen = ntohl(msgLen);
            if (msgLen > BUFFER_SIZE - 1 || msgLen == 0) {
                continue;
            }

            std::string buffer(msgLen, '\0');
            size_t totalRead = 0;
            while (totalRead < msgLen) {
                ssize_t n = recv(clientSocket, &buffer[totalRead], msgLen - totalRead, 0);
                if (n <= 0) {
                    if (running) {
                        running = false;
                    }
                    return;
                }
                totalRead += n;
            }

            // Phân loại message
            std::string messageType = getJsonValue(buffer, "messageType");

            if (messageType == "RECEIVE_MESSAGE" || messageType == "UNREAD_MESSAGES_NOTIFICATION") {
                // [FIX] Đây là push notification, xử lý ngay
                handlePushNotification(buffer);
            } else {
                // [FIX] Đây là response cho request, đưa vào queue
                {
                    std::lock_guard<std::mutex> lock(responseQueueMutex);
                    responseQueue.push(buffer);
                }
                responseCondition.notify_one();
            }
        }

        if (pfd.revents & (POLLERR | POLLHUP | POLLNVAL)) {
            if (running) {
                printColored("\n[INFO] Connection error\n", "red");
                running = false;
            }
            break;
        }
    }
}

// ============================================================================
// [FIX] NETWORK FUNCTIONS - Sửa để hoạt động với background thread
// ============================================================================

// Gửi message qua socket (thread-safe)
bool sendMessage(const std::string& message) {
    std::lock_guard<std::mutex> lock(socketMutex);

    if (clientSocket < 0) return false;

    uint32_t len = htonl(message.length());
    if (send(clientSocket, &len, sizeof(len), 0) != sizeof(len)) {
        return false;
    }

    size_t totalSent = 0;
    while (totalSent < message.length()) {
        ssize_t sent = send(clientSocket, message.c_str() + totalSent, message.length() - totalSent, 0);
        if (sent <= 0) return false;
        totalSent += sent;
    }

    return true;
}

// [FIX] Chờ và lấy response từ queue (được đẩy vào bởi receive thread)
std::string waitForResponse(int timeoutMs = 10000) {
    std::unique_lock<std::mutex> lock(responseQueueMutex);

    // Chờ có response trong queue hoặc timeout
    bool success = responseCondition.wait_for(lock, std::chrono::milliseconds(timeoutMs), []() {
        return !responseQueue.empty() || !running;
    });

    if (!success || responseQueue.empty()) {
        return "";
    }

    std::string response = responseQueue.front();
    responseQueue.pop();
    return response;
}

// [FIX] Gửi request và chờ response (sử dụng queue thay vì blocking recv)
std::string sendAndReceive(const std::string& request) {
    if (!sendMessage(request)) {
        return "";
    }
    return waitForResponse();
}

// ============================================================================
// CÁC CHỨC NĂNG CHÍNH
// ============================================================================

// Forward declaration
void openChatWith(const std::string& recipientId, const std::string& recipientName);

void showMainMenu() {
    clearScreen();
    printColored("╔══════════════════════════════════════════╗\n", "cyan");
    printColored("║     ENGLISH LEARNING APP - MAIN MENU     ║\n", "cyan");
    printColored("╠══════════════════════════════════════════╣\n", "cyan");
    printColored("║  User: ", "cyan");
    printColored(currentUserName, "yellow");
    printColored(" | Level: ", "cyan");
    printColored(currentLevel, "green");
    printColored("\n", "");
    printColored("╠══════════════════════════════════════════╣\n", "cyan");
    printColored("║  1. Set English Level                    ║\n", "");
    printColored("║  2. View All Lessons                     ║\n", "");
    printColored("║  3. Take a Test                          ║\n", "");
    printColored("║  4. Do Exercises                         ║\n", "");
    printColored("║  5. Play Games                            ║\n", "");
    printColored("║  6. Chat with Others                     ║\n", "");
    printColored("║  7. Logout                               ║\n", "");
    printColored("║  0. Exit                                 ║\n", "");
    printColored("╚══════════════════════════════════════════╝\n", "cyan");

    // [FIX] Hiển thị thông báo tin nhắn mới nếu có
    {
        std::lock_guard<std::mutex> lock(notificationMutex);
        if (hasNewNotification && !pendingChatUserName.empty()) {
            printColored("╔══════════════════════════════════════════╗\n", "yellow");
            printColored("║  📬 New message from ", "yellow");
            printColored(pendingChatUserName, "cyan");
            printColored("!\n", "yellow");
            printColored("║  Press 'r' to reply quickly              ║\n", "yellow");
            printColored("╚══════════════════════════════════════════╝\n", "yellow");
        }
    }

    printColored("Enter your choice: ", "green");
}

// ============================================================================
// ĐĂNG KÝ & ĐĂNG NHẬP
// ============================================================================

bool registerAccount() {
    clearScreen();
    printColored("╔══════════════════════════════════════════╗\n", "cyan");
    printColored("║           REGISTER NEW ACCOUNT           ║\n", "cyan");
    printColored("╚══════════════════════════════════════════╝\n", "cyan");

    std::string fullname, email, password, confirmPassword;

    printColored("Full Name: ", "yellow");
    std::getline(std::cin, fullname);

    printColored("Email: ", "yellow");
    std::getline(std::cin, email);

    printColored("Password: ", "yellow");
    std::getline(std::cin, password);

    printColored("Confirm Password: ", "yellow");
    std::getline(std::cin, confirmPassword);

    std::string request = R"({"messageType":"REGISTER_REQUEST","messageId":")" + generateMessageId() +
                          R"(","timestamp":)" + std::to_string(getCurrentTimestamp()) +
                          R"(,"payload":{"fullname":")" + escapeJson(fullname) +
                          R"(","email":")" + escapeJson(email) +
                          R"(","password":")" + escapeJson(password) +
                          R"(","confirmPassword":")" + escapeJson(confirmPassword) + R"("}})";

    std::string response = sendAndReceive(request);
    if (response.empty()) {
        printColored("\n[ERROR] No response from server\n", "red");
        return false;
    }

    std::string status = getJsonValue(response, "status");
    std::string message = getJsonValue(response, "message");

    if (status == "success") {
        printColored("\n[SUCCESS] " + message + "\n", "green");
        printColored("You can now login with your email and password.\n", "");
        return true;
    } else {
        printColored("\n[ERROR] " + message + "\n", "red");
        return false;
    }
}

bool login() {
    clearScreen();
    printColored("╔══════════════════════════════════════════╗\n", "cyan");
    printColored("║                  LOGIN                   ║\n", "cyan");
    printColored("╚══════════════════════════════════════════╝\n", "cyan");

    std::string email, password;

    printColored("Email: ", "yellow");
    std::getline(std::cin, email);

    printColored("Password: ", "yellow");
    std::getline(std::cin, password);

    std::string request = R"({"messageType":"LOGIN_REQUEST","messageId":")" + generateMessageId() +
                          R"(","timestamp":)" + std::to_string(getCurrentTimestamp()) +
                          R"(,"payload":{"email":")" + escapeJson(email) +
                          R"(","password":")" + escapeJson(password) + R"("}})";

    std::string response = sendAndReceive(request);
    if (response.empty()) {
        printColored("\n[ERROR] No response from server\n", "red");
        return false;
    }

    std::string status = getJsonValue(response, "status");
    std::string message = getJsonValue(response, "message");

    if (status == "success") {
        std::string data = getJsonObject(response, "data");
        sessionToken = getJsonValue(data, "sessionToken");
        currentUserId = getJsonValue(data, "userId");
        currentUserName = getJsonValue(data, "fullname");
        currentLevel = getJsonValue(data, "level");

        printColored("\n[SUCCESS] " + message + "\n", "green");
        printColored("Welcome, " + currentUserName + "!\n", "yellow");
        loggedIn = true;
        return true;
    } else {
        printColored("\n[ERROR] " + message + "\n", "red");
        return false;
    }
}

// ============================================================================
// CHỌN MỨC ĐỘ
// ============================================================================

void setLevel() {
    clearScreen();
    printColored("╔══════════════════════════════════════════╗\n", "cyan");
    printColored("║         SET YOUR ENGLISH LEVEL           ║\n", "cyan");
    printColored("╚══════════════════════════════════════════╝\n", "cyan");
    printColored("Current level: ", "");
    printColored(currentLevel + "\n\n", "yellow");
    printColored("Available levels:\n", "");
    printColored("  1. beginner     - For new learners\n", "green");
    printColored("  2. intermediate - For those with basic knowledge\n", "yellow");
    printColored("  3. advanced     - For proficient speakers\n", "red");
    printColored("\nEnter level (1/2/3 or beginner/intermediate/advanced): ", "green");

    std::string input;
    std::getline(std::cin, input);

    std::string level;
    if (input == "1") level = "beginner";
    else if (input == "2") level = "intermediate";
    else if (input == "3") level = "advanced";
    else level = input;

    if (level != "beginner" && level != "intermediate" && level != "advanced") {
        printColored("\n[ERROR] Invalid level.\n", "red");
        waitEnter();
        return;
    }

    std::string request = R"({"messageType":"SET_LEVEL_REQUEST","messageId":")" + generateMessageId() +
                          R"(","timestamp":)" + std::to_string(getCurrentTimestamp()) +
                          R"(,"sessionToken":")" + sessionToken +
                          R"(","payload":{"level":")" + level + R"("}})";

    std::string response = sendAndReceive(request);
    std::string status = getJsonValue(response, "status");
    std::string message = getJsonValue(response, "message");

    if (status == "success") {
        currentLevel = level;
        printColored("\n[SUCCESS] " + message + "\n", "green");
    } else {
        printColored("\n[ERROR] " + message + "\n", "red");
    }

    waitEnter();
}

// ============================================================================
// XEM DANH SÁCH BÀI HỌC + XEM CHI TIẾT
// ============================================================================

void viewLessons() {
    while (true) {
        clearScreen();
        printColored("╔══════════════════════════════════════════╗\n", "cyan");
        printColored("║              VIEW LESSONS                ║\n", "cyan");
        printColored("╚══════════════════════════════════════════╝\n", "cyan");

        printColored("Filter by topic (press Enter for all):\n", "");
        printColored("  grammar | vocabulary | listening | speaking | reading | writing\n", "magenta");
        printColored("Topic: ", "green");

        std::string topic;
        std::getline(std::cin, topic);

        std::string request = R"({"messageType":"GET_LESSONS_REQUEST","messageId":")" + generateMessageId() +
                              R"(","timestamp":)" + std::to_string(getCurrentTimestamp()) +
                              R"(,"sessionToken":")" + sessionToken +
                              R"(","payload":{"level":"","topic":")" + topic + R"(","page":1,"limit":20}})";

        std::string response = sendAndReceive(request);
        std::string status = getJsonValue(response, "status");

        if (status != "success") {
            std::string message = getJsonValue(response, "message");
            printColored("\n[ERROR] " + message + "\n", "red");
            waitEnter();
            return;
        }

        std::string data = getJsonObject(response, "data");
        std::string lessonsArray = getJsonArray(data, "lessons");
        std::vector<std::string> lessonsList = parseJsonArray(lessonsArray);

        if (lessonsList.empty()) {
            printColored("\n[INFO] No lessons found.\n", "yellow");
            waitEnter();
            return;
        }

        // Lưu danh sách lesson IDs
        std::vector<std::string> lessonIds;

        printColored("\n┌────┬─────────────┬────────────────────────────────────┬──────────────┬──────────┐\n", "cyan");
        printColored("│ #  │ Lesson ID   │ Title                              │ Topic        │ Level    │\n", "cyan");
        printColored("├────┼─────────────┼────────────────────────────────────┼──────────────┼──────────┤\n", "cyan");

        int idx = 1;
        for (const std::string& lesson : lessonsList) {
            std::string lessonId = getJsonValue(lesson, "lessonId");
            std::string title = getJsonValue(lesson, "title");
            std::string lessonTopic = getJsonValue(lesson, "topic");
            std::string level = getJsonValue(lesson, "level");

            lessonIds.push_back(lessonId);

            // Truncate title if too long
            if (title.length() > 34) title = title.substr(0, 31) + "...";

            printf("│ %-2d │ %-11s │ %-34s │ %-12s │ %-8s │\n",
                   idx, lessonId.c_str(), title.c_str(), lessonTopic.c_str(), level.c_str());
            idx++;
        }

        printColored("└────┴─────────────┴────────────────────────────────────┴──────────────┴──────────┘\n", "cyan");

        printColored("\nOptions:\n", "yellow");
        printColored("  Enter lesson number (1-" + std::to_string(lessonsList.size()) + ") to study\n", "");
        printColored("  Enter 0 to go back to main menu\n", "");
        printColored("\nYour choice: ", "green");

        std::string choice;
        std::getline(std::cin, choice);

        if (choice == "0" || choice.empty()) {
            return;
        }

        try {
            int lessonNum = std::stoi(choice);
            if (lessonNum >= 1 && lessonNum <= (int)lessonIds.size()) {
                // Học bài học
                std::string selectedLessonId = lessonIds[lessonNum - 1];

                std::string detailRequest = R"({"messageType":"GET_LESSON_DETAIL_REQUEST","messageId":")" + generateMessageId() +
                                            R"(","timestamp":)" + std::to_string(getCurrentTimestamp()) +
                                            R"(,"sessionToken":")" + sessionToken +
                                            R"(","payload":{"lessonId":")" + selectedLessonId + R"("}})";

                std::string detailResponse = sendAndReceive(detailRequest);
                std::string detailStatus = getJsonValue(detailResponse, "status");

                if (detailStatus != "success") {
                    std::string message = getJsonValue(detailResponse, "message");
                    printColored("\n[ERROR] " + message + "\n", "red");
                    waitEnter();
                    continue;
                }

                std::string lessonData = getJsonObject(detailResponse, "data");
                std::string lessonTitle = getJsonValue(lessonData, "title");
                std::string lessonDesc = getJsonValue(lessonData, "description");
                std::string lessonLevel = getJsonValue(lessonData, "level");
                std::string lessonTopicDetail = getJsonValue(lessonData, "topic");
                std::string textContent = getJsonValue(lessonData, "textContent");

                clearScreen();
                printColored("╔══════════════════════════════════════════════════════════════════════════════╗\n", "cyan");
                printColored("║  ", "cyan");
                printColored(lessonTitle, "yellow");
                printColored("\n", "");
                printColored("╠══════════════════════════════════════════════════════════════════════════════╣\n", "cyan");
                printColored("║  Level: ", "cyan");
                printColored(lessonLevel, "green");
                printColored(" | Topic: ", "cyan");
                printColored(lessonTopicDetail, "magenta");
                printColored("\n", "");
                printColored("║  ", "cyan");
                printColored(lessonDesc, "");
                printColored("\n", "");
                printColored("╚══════════════════════════════════════════════════════════════════════════════╝\n\n", "cyan");

                // Hiển thị nội dung bài học
                std::string content = unescapeJson(textContent);
                std::cout << content << std::endl;

                printColored("\n════════════════════════════════════════════════════════════════════════════════\n", "cyan");
                printColored("[Video/Audio content would be displayed here in a real app]\n", "magenta");
                printColored("════════════════════════════════════════════════════════════════════════════════\n", "cyan");

                waitEnter();
            } else {
                printColored("\n[ERROR] Invalid choice.\n", "red");
                waitEnter();
            }
        } catch (...) {
            printColored("\n[ERROR] Invalid input.\n", "red");
            waitEnter();
        }
    }
}

// ============================================================================
// LÀM BÀI TEST
// ============================================================================

void takeTest() {
    clearScreen();
    printColored("╔══════════════════════════════════════════╗\n", "cyan");
    printColored("║              TAKE A TEST                 ║\n", "cyan");
    printColored("╚══════════════════════════════════════════╝\n", "cyan");

    printColored("Your current level: ", "");
    printColored(currentLevel + "\n\n", "yellow");
    printColored("The test will be based on your level.\n", "");
    printColored("Press Enter to start or type 'back' to go back: ", "green");

    std::string input;
    std::getline(std::cin, input);
    if (input == "back") return;

    // Lấy đề test
    std::string request = R"({"messageType":"GET_TEST_REQUEST","messageId":")" + generateMessageId() +
                          R"(","timestamp":)" + std::to_string(getCurrentTimestamp()) +
                          R"(,"sessionToken":")" + sessionToken +
                          R"(","payload":{"testType":"mixed","level":")" + currentLevel + R"(","topic":"grammar"}})";

    std::string response = sendAndReceive(request);
    std::string status = getJsonValue(response, "status");

    if (status != "success") {
        std::string message = getJsonValue(response, "message");
        printColored("\n[ERROR] " + message + "\n", "red");
        waitEnter();
        return;
    }

    std::string data = getJsonObject(response, "data");
    std::string testId = getJsonValue(data, "testId");
    std::string title = getJsonValue(data, "title");
    std::string questionsArray = getJsonArray(data, "questions");
    std::vector<std::string> questions = parseJsonArray(questionsArray);

    clearScreen();
    printColored("╔══════════════════════════════════════════╗\n", "cyan");
    printColored("║  ", "cyan");
    printColored(title, "yellow");
    printColored("\n", "");
    printColored("╠══════════════════════════════════════════╣\n", "cyan");
    printColored("║  Total questions: ", "cyan");
    printColored(std::to_string(questions.size()), "green");
    printColored(" | Level: ", "cyan");
    printColored(currentLevel, "yellow");
    printColored("\n", "");
    printColored("╚══════════════════════════════════════════╝\n\n", "cyan");

    std::vector<std::pair<std::string, std::string>> answers;

    for (size_t i = 0; i < questions.size(); i++) {
        const std::string& q = questions[i];
        std::string questionId = getJsonValue(q, "questionId");
        std::string type = getJsonValue(q, "type");
        std::string questionText = getJsonValue(q, "question");
        std::string points = getJsonValue(q, "points");

        printColored("────────────────────────────────────────────\n", "cyan");
        printColored("Question " + std::to_string(i + 1) + "/" + std::to_string(questions.size()) +
                     " (" + points + " points)\n", "magenta");
        printColored(questionText + "\n\n", "yellow");

        if (type == "multiple_choice") {
            std::string optionsArray = getJsonArray(q, "options");
            std::vector<std::string> options = parseJsonArray(optionsArray);

            for (const std::string& opt : options) {
                std::string optId = getJsonValue(opt, "id");
                std::string optText = getJsonValue(opt, "text");
                printColored("  " + optId + ") " + optText + "\n", "");
            }

            printColored("\nYour answer (a/b/c/d): ", "green");
        } else if (type == "sentence_order") {
            std::string wordsArray = getJsonArray(q, "words");
            std::vector<std::string> words = parseJsonArray(wordsArray);
            
            printColored("Words (in random order):\n", "magenta");
            for (size_t j = 0; j < words.size(); j++) {
                printColored("  " + std::to_string(j + 1) + ") " + words[j] + "\n", "");
            }
            printColored("\nType the sentence in correct order (separate words with spaces):\n", "yellow");
            printColored("Your answer: ", "green");
        } else {
            printColored("Type your answer: ", "green");
        }

        std::string answer;
        std::getline(std::cin, answer);
        answers.push_back({questionId, answer});
        printColored("\n", "");
    }

    // Nộp bài
    printColored("────────────────────────────────────────────\n", "cyan");
    printColored("Submitting your answers...\n", "yellow");

    std::stringstream answersJson;
    answersJson << "[";
    for (size_t i = 0; i < answers.size(); i++) {
        if (i > 0) answersJson << ",";
        answersJson << R"({"questionId":")" << answers[i].first << R"(","answer":")" << escapeJson(answers[i].second) << R"("})";
    }
    answersJson << "]";

    std::string submitRequest = R"({"messageType":"SUBMIT_TEST_REQUEST","messageId":")" + generateMessageId() +
                                R"(","timestamp":)" + std::to_string(getCurrentTimestamp()) +
                                R"(,"sessionToken":")" + sessionToken +
                                R"(","payload":{"testId":")" + testId +
                                R"(","timeSpent":600,"answers":)" + answersJson.str() + R"(}})";

    std::string submitResponse = sendAndReceive(submitRequest);
    std::string submitStatus = getJsonValue(submitResponse, "status");

    if (submitStatus != "success") {
        std::string message = getJsonValue(submitResponse, "message");
        printColored("\n[ERROR] " + message + "\n", "red");
        waitEnter();
        return;
    }

    // Hiển thị kết quả
    std::string resultData = getJsonObject(submitResponse, "data");
    std::string score = getJsonValue(resultData, "score");
    std::string percentage = getJsonValue(resultData, "percentage");
    std::string totalPoints = getJsonValue(resultData, "totalPoints");
    std::string correctAnswers = getJsonValue(resultData, "correctAnswers");
    std::string wrongAnswers = getJsonValue(resultData, "wrongAnswers");
    std::string passed = getJsonValue(resultData, "passed");
    std::string grade = getJsonValue(resultData, "grade");

    clearScreen();
    printColored("╔══════════════════════════════════════════╗\n", "cyan");
    printColored("║             TEST RESULTS                 ║\n", "cyan");
    printColored("╚══════════════════════════════════════════╝\n\n", "cyan");

    if (passed == "true") {
        printColored("    ★★★ CONGRATULATIONS! ★★★\n", "green");
        printColored("       You PASSED the test!\n\n", "green");
    } else {
        printColored("       Keep practicing!\n", "yellow");
        printColored("    You didn't pass this time.\n\n", "yellow");
    }

    printColored("┌────────────────────────────────────────┐\n", "cyan");
    printColored("│  Score: " + score + "/" + totalPoints + " points", "");
    printColored("                       │\n", "cyan");
    printColored("│  Percentage: " + percentage + "%", "");
    printColored("                        │\n", "cyan");
    printColored("│  Grade: ", "");
    printColored(grade, (grade == "A" || grade == "B") ? "green" : (grade == "C" || grade == "D") ? "yellow" : "red");
    printColored("                               │\n", "cyan");
    printColored("│  Correct: " + correctAnswers + " | Wrong: " + wrongAnswers, "");
    printColored("                   │\n", "cyan");
    printColored("└────────────────────────────────────────┘\n\n", "cyan");

    // Hiển thị chi tiết
    printColored("Detailed Results:\n", "yellow");

    std::string detailedArray = getJsonArray(resultData, "detailedResults");
    std::vector<std::string> detailedResults = parseJsonArray(detailedArray);

    for (size_t i = 0; i < detailedResults.size(); i++) {
        const std::string& result = detailedResults[i];
        std::string correct = getJsonValue(result, "correct");
        std::string userAnswer = getJsonValue(result, "userAnswer");
        std::string correctAnswer = getJsonValue(result, "correctAnswer");

        std::string icon = (correct == "true") ? "[✓]" : "[✗]";
        std::string color = (correct == "true") ? "green" : "red";

        printColored("Q" + std::to_string(i + 1) + " " + icon + " ", color);
        printColored("Your: \"" + userAnswer + "\" | Correct: \"" + correctAnswer + "\"\n", "");
    }

    waitEnter();
}

// ============================================================================
// DO EXERCISES
// ============================================================================

void doExercises() {
    clearScreen();
    printColored("╔══════════════════════════════════════════╗\n", "cyan");
    printColored("║              DO EXERCISES                ║\n", "cyan");
    printColored("╚══════════════════════════════════════════╝\n", "cyan");

    printColored("Exercise Types:\n", "yellow");
    printColored("  1. Sentence Rewrite (grammar practice)\n", "green");
    printColored("  2. Paragraph Writing (writing practice)\n", "green");
    printColored("  3. Topic Speaking (speaking practice)\n", "green");
    printColored("  0. Back to main menu\n\n", "");
    printColored("Your current level: ", "");
    printColored(currentLevel + "\n\n", "yellow");
    printColored("Select exercise type (1/2/3/0): ", "green");

    std::string typeChoice;
    std::getline(std::cin, typeChoice);

    std::string exerciseType;
    if (typeChoice == "1") {
        exerciseType = "sentence_rewrite";
    } else if (typeChoice == "2") {
        exerciseType = "paragraph_writing";
    } else if (typeChoice == "3") {
        exerciseType = "topic_speaking";
    } else {
        return;
    }

    // Get exercise
    std::string request = R"({"messageType":"GET_EXERCISE_REQUEST","messageId":")" + generateMessageId() +
                          R"(","timestamp":)" + std::to_string(getCurrentTimestamp()) +
                          R"(,"sessionToken":")" + sessionToken +
                          R"(","payload":{"exerciseType":")" + exerciseType +
                          R"(","level":")" + currentLevel + R"(","topic":""}})";

    std::string response = sendAndReceive(request);
    std::string status = getJsonValue(response, "status");

    if (status != "success") {
        std::string message = getJsonValue(response, "message");
        printColored("\n[ERROR] " + message + "\n", "red");
        waitEnter();
        return;
    }

    std::string data = getJsonObject(response, "data");
    std::string exerciseId = getJsonValue(data, "exerciseId");
    std::string title = getJsonValue(data, "title");
    std::string instructions = getJsonValue(data, "instructions");
    std::string duration = getJsonValue(data, "duration");

    clearScreen();
    printColored("╔══════════════════════════════════════════╗\n", "cyan");
    printColored("║  ", "cyan");
    printColored(title, "yellow");
    printColored("\n", "");
    printColored("╠══════════════════════════════════════════╣\n", "cyan");
    printColored("║  Duration: ", "cyan");
    printColored(duration + " minutes", "green");
    printColored("\n", "");
    printColored("╚══════════════════════════════════════════╝\n\n", "cyan");

    printColored("Instructions:\n", "yellow");
    std::string inst = unescapeJson(instructions);
    std::cout << inst << "\n\n";

    std::string content = "";

    if (exerciseType == "sentence_rewrite") {
        std::string promptsArray = getJsonArray(data, "prompts");
        std::vector<std::string> prompts = parseJsonArray(promptsArray);

        printColored("────────────────────────────────────────────\n", "cyan");
        printColored("Sentences to rewrite:\n\n", "magenta");

        for (size_t i = 0; i < prompts.size(); i++) {
            printColored(std::to_string(i + 1) + ". " + prompts[i] + "\n", "");
        }

        printColored("\n────────────────────────────────────────────\n", "cyan");
        printColored("Type your rewritten sentences (one per line, press Enter twice when done):\n", "yellow");
        printColored("(You can write all sentences together)\n\n", "");

        std::string line;
        std::vector<std::string> rewrittenSentences;
        while (std::getline(std::cin, line)) {
            if (line.empty() && !rewrittenSentences.empty()) break;
            if (!line.empty()) {
                rewrittenSentences.push_back(line);
            }
        }

        for (size_t i = 0; i < rewrittenSentences.size(); i++) {
            if (i > 0) content += "\n";
            content += rewrittenSentences[i];
        }
    } else if (exerciseType == "paragraph_writing") {
        std::string topicDesc = getJsonValue(data, "topicDescription");
        std::string requirementsArray = getJsonArray(data, "requirements");
        std::vector<std::string> requirements = parseJsonArray(requirementsArray);

        printColored("Topic:\n", "magenta");
        printColored(unescapeJson(topicDesc) + "\n\n", "");

        if (!requirements.empty()) {
            printColored("Requirements:\n", "magenta");
            for (const std::string& req : requirements) {
                printColored("  • " + unescapeJson(req) + "\n", "");
            }
            printColored("\n", "");
        }

        printColored("────────────────────────────────────────────\n", "cyan");
        printColored("Write your paragraph below (press Enter twice when done):\n", "yellow");

        std::string line;
        std::vector<std::string> lines;
        while (std::getline(std::cin, line)) {
            if (line.empty() && !lines.empty()) break;
            if (!line.empty()) {
                lines.push_back(line);
            }
        }

        for (size_t i = 0; i < lines.size(); i++) {
            if (i > 0) content += "\n";
            content += lines[i];
        }
    } else if (exerciseType == "topic_speaking") {
        std::string topicDesc = getJsonValue(data, "topicDescription");

        printColored("Topic:\n", "magenta");
        printColored(unescapeJson(topicDesc) + "\n\n", "");

        printColored("────────────────────────────────────────────\n", "cyan");
        printColored("Note: In a real application, you would record audio here.\n", "yellow");
        printColored("For now, please type a summary of what you would say:\n\n", "yellow");

        std::string line;
        std::vector<std::string> lines;
        while (std::getline(std::cin, line)) {
            if (line.empty() && !lines.empty()) break;
            if (!line.empty()) {
                lines.push_back(line);
            }
        }

        for (size_t i = 0; i < lines.size(); i++) {
            if (i > 0) content += "\n";
            content += lines[i];
        }
    }

    if (content.empty()) {
        printColored("\n[ERROR] No content submitted.\n", "red");
        waitEnter();
        return;
    }

    // Submit exercise
    printColored("\n────────────────────────────────────────────\n", "cyan");
    printColored("Submitting your exercise...\n", "yellow");

    std::string submitRequest = R"({"messageType":"SUBMIT_EXERCISE_REQUEST","messageId":")" + generateMessageId() +
                                R"(","timestamp":)" + std::to_string(getCurrentTimestamp()) +
                                R"(,"sessionToken":")" + sessionToken +
                                R"(","payload":{"exerciseId":")" + exerciseId +
                                R"(","exerciseType":")" + exerciseType +
                                R"(","content":")" + escapeJson(content) + R"("}})";

    std::string submitResponse = sendAndReceive(submitRequest);
    std::string submitStatus = getJsonValue(submitResponse, "status");

    if (submitStatus != "success") {
        std::string message = getJsonValue(submitResponse, "message");
        printColored("\n[ERROR] " + message + "\n", "red");
        waitEnter();
        return;
    }

    std::string submitData = getJsonObject(submitResponse, "data");
    std::string message = getJsonValue(submitResponse, "message");
    std::string submitMessage = getJsonValue(submitData, "message");

    clearScreen();
    printColored("╔══════════════════════════════════════════╗\n", "cyan");
    printColored("║         EXERCISE SUBMITTED                ║\n", "cyan");
    printColored("╚══════════════════════════════════════════╝\n\n", "cyan");

    printColored("[SUCCESS] " + message + "\n", "green");
    printColored(submitMessage + "\n", "yellow");
    printColored("\nA teacher will review your submission and provide feedback.\n", "");

    waitEnter();
}

// ============================================================================
// [FIX] CHAT - Sửa để hiển thị lịch sử và nhận tin nhắn real-time
// ============================================================================
void trim(std::string &s) {
    // Xóa ký tự trắng đầu
    s.erase(s.begin(), std::find_if(s.begin(), s.end(),
        [](unsigned char ch){ return !std::isspace(ch); }));
    // Xóa ký tự trắng cuối
    s.erase(std::find_if(s.rbegin(), s.rend(),
        [](unsigned char ch){ return !std::isspace(ch); }).base(), s.end());
}

// Hàm mở chat với một người dùng cụ thể
void openChatWith(const std::string& recipientId, const std::string& recipientName) {
    clearScreen();
    printColored("╔══════════════════════════════════════════╗\n", "cyan");
    printColored("║  Chatting with: ", "cyan");
    printColored(recipientName, "yellow");
    printColored("\n", "");
    printColored("╠══════════════════════════════════════════╣\n", "cyan");
    printColored("║  Type 'exit' to leave chat               ║\n", "magenta");
    printColored("╚══════════════════════════════════════════╝\n\n", "cyan");

    // Lấy lịch sử chat
    std::string historyRequest = R"({"messageType":"GET_CHAT_HISTORY_REQUEST","messageId":")" + generateMessageId() +
                                  R"(","timestamp":)" + std::to_string(getCurrentTimestamp()) +
                                  R"(,"sessionToken":")" + sessionToken +
                                  R"(","payload":{"recipientId":")" + recipientId + R"("}})";

    std::string historyResponse = sendAndReceive(historyRequest);
    std::string historyStatus = getJsonValue(historyResponse, "status");

    if (historyStatus == "success") {
        std::string historyData = getJsonObject(historyResponse, "data");
        std::string messagesArray = getJsonArray(historyData, "messages");
        std::vector<std::string> messages = parseJsonArray(messagesArray);

        if (!messages.empty()) {
            printColored("--- Chat History ---\n", "magenta");
            for (const std::string& msg : messages) {
                std::string msgSenderId = getJsonValue(msg, "senderId");
                std::string msgContent = getJsonValue(msg, "content");

                if (msgSenderId == currentUserId) {
                    printColored("You: ", "green");
                    printColored(msgContent + "\n", "");
                } else {
                    printColored(recipientName + ": ", "yellow");
                    printColored(msgContent + "\n", "");
                }
            }
            printColored("--- End of History ---\n\n", "magenta");
        }
    }

    // Đánh dấu tin nhắn đã đọc
    std::string markReadRequest = R"({"messageType":"MARK_MESSAGES_READ_REQUEST","messageId":")" + generateMessageId() +
                                   R"(","timestamp":)" + std::to_string(getCurrentTimestamp()) +
                                   R"(,"sessionToken":")" + sessionToken +
                                   R"(","payload":{"senderId":")" + recipientId + R"("}})";
    sendAndReceive(markReadRequest);

    // Reset thông báo pending
    {
        std::lock_guard<std::mutex> lock(notificationMutex);
        if (pendingChatUserId == recipientId) {
            hasNewNotification = false;
            pendingChatUserId = "";
            pendingChatUserName = "";
        }
    }

    // [FIX] Set người đang chat để hiển thị tin nhắn trực tiếp
    {
        std::lock_guard<std::mutex> lock(chatPartnerMutex);
        currentChatPartnerId = recipientId;
        currentChatPartnerName = recipientName;
    }

    inChatMode = true;
    canShowNotification = true; // Cho phép hiển thị thông báo trong chat mode

    while (inChatMode && running) {
        printColored("You: ", "green");
        std::string message;
        std::getline(std::cin, message);

        trim(message);  // [FIX] Loại bỏ khoảng trắng đầu/cuối
        if (message == "exit") {
            inChatMode = false;
            break;
        }
        if (message.empty()) continue;

        std::string chatRequest = R"({"messageType":"SEND_MESSAGE_REQUEST","messageId":")" + generateMessageId() +
                                  R"(","timestamp":)" + std::to_string(getCurrentTimestamp()) +
                                  R"(,"sessionToken":")" + sessionToken +
                                  R"(","payload":{"recipientId":")" + recipientId +
                                  R"(","messageContent":")" + escapeJson(message) +
                                  R"(","messageType":"text"}})";

        std::string chatResponse = sendAndReceive(chatRequest);
        std::string chatStatus = getJsonValue(chatResponse, "status");

        if (chatStatus == "success") {
            std::string chatData = getJsonObject(chatResponse, "data");
            std::string delivered = getJsonValue(chatData, "delivered");
            if (delivered == "true") {
                printColored("[Delivered ✓]\n", "green");
            } else {
                printColored("[Sent - User offline]\n", "yellow");
            }
        } else {
            std::string errorMsg = getJsonValue(chatResponse, "message");
            printColored("[ERROR] " + errorMsg + "\n", "red");
        }
    }

    // [FIX] Clear người đang chat khi thoát
    {
        std::lock_guard<std::mutex> lock(chatPartnerMutex);
        currentChatPartnerId = "";
        currentChatPartnerName = "";
    }
    inChatMode = false;
}

// ============================================================================
// PLAY GAMES
// ============================================================================

void playGames() {
    clearScreen();
    printColored("╔══════════════════════════════════════════╗\n", "cyan");
    printColored("║              PLAY GAMES                  ║\n", "cyan");
    printColored("╚══════════════════════════════════════════╝\n", "cyan");

    // Get game list
    std::string request = R"({"messageType":"GET_GAME_LIST_REQUEST","messageId":")" + generateMessageId() +
                          R"(","timestamp":)" + std::to_string(getCurrentTimestamp()) +
                          R"(,"sessionToken":")" + sessionToken +
                          R"(","payload":{"gameType":"all","level":")" + currentLevel + R"("}})";

    std::string response = sendAndReceive(request);
    std::string status = getJsonValue(response, "status");

    if (status != "success") {
        std::string message = getJsonValue(response, "message");
        printColored("\n[ERROR] " + message + "\n", "red");
        waitEnter();
        return;
    }

    std::string data = getJsonObject(response, "data");
    std::string gamesArray = getJsonArray(data, "games");
    std::vector<std::string> gamesList = parseJsonArray(gamesArray);

    if (gamesList.empty()) {
        printColored("\n[INFO] No games available for your level.\n", "yellow");
        waitEnter();
        return;
    }

    printColored("\nAvailable Games:\n", "yellow");
    printColored("┌────┬────────────────────────┬──────────────┬──────────┐\n", "cyan");
    printColored("│ #  │ Game Title              │ Type         │ Level    │\n", "cyan");
    printColored("├────┼────────────────────────┼──────────────┼──────────┤\n", "cyan");

    int idx = 1;
    std::vector<std::string> gameIds;

    for (const std::string& game : gamesList) {
        std::string gameId = getJsonValue(game, "gameId");
        std::string title = getJsonValue(game, "title");
        std::string gameType = getJsonValue(game, "gameType");
        std::string level = getJsonValue(game, "level");

        gameIds.push_back(gameId);

        std::string typeDisplay = gameType;
        if (gameType == "word_match") typeDisplay = "Word Match";
        else if (gameType == "sentence_match") typeDisplay = "Sentence Match";
        else if (gameType == "picture_match") typeDisplay = "Picture Match";

        if (title.length() > 22) title = title.substr(0, 19) + "...";

        printf("│ %-2d │ %-22s │ %-12s │ %-8s │\n",
               idx, title.c_str(), typeDisplay.c_str(), level.c_str());
        idx++;
    }

    printColored("└────┴────────────────────────┴──────────────┴──────────┘\n", "cyan");

    printColored("\nEnter game number to play (0 to go back): ", "green");

    std::string input;
    std::getline(std::cin, input);

    int choice;
    try {
        choice = std::stoi(input);
    } catch (...) {
        return;
    }

    if (choice <= 0 || choice > (int)gameIds.size()) {
        return;
    }

    std::string selectedGameId = gameIds[choice - 1];

    // Start game
    std::string startRequest = R"({"messageType":"START_GAME_REQUEST","messageId":")" + generateMessageId() +
                                R"(","timestamp":)" + std::to_string(getCurrentTimestamp()) +
                                R"(,"sessionToken":")" + sessionToken +
                                R"(","payload":{"gameId":")" + selectedGameId + R"("}})";

    std::string startResponse = sendAndReceive(startRequest);
    std::string startStatus = getJsonValue(startResponse, "status");

    if (startStatus != "success") {
        std::string message = getJsonValue(startResponse, "message");
        printColored("\n[ERROR] " + message + "\n", "red");
        waitEnter();
        return;
    }

    std::string gameData = getJsonObject(startResponse, "data");
    std::string gameSessionId = getJsonValue(gameData, "gameSessionId");
    std::string gameType = getJsonValue(gameData, "gameType");
    std::string title = getJsonValue(gameData, "title");
    std::string timeLimit = getJsonValue(gameData, "timeLimit");
    std::string pairsArray = getJsonArray(gameData, "pairs");

    clearScreen();
    printColored("╔══════════════════════════════════════════╗\n", "cyan");
    printColored("║  ", "cyan");
    printColored(title, "yellow");
    printColored("\n", "");
    printColored("╠══════════════════════════════════════════╣\n", "cyan");
    printColored("║  Time Limit: ", "cyan");
    printColored(timeLimit + " seconds", "green");
    printColored("\n", "");
    printColored("╚══════════════════════════════════════════╝\n\n", "cyan");

    // Display pairs and get matches
    std::vector<std::string> pairs = parseJsonArray(pairsArray);
    
    if (gameType == "word_match") {
        printColored("Match English words with Vietnamese meanings:\n\n", "yellow");
        
        std::vector<std::string> leftItems, rightItems;
        for (const std::string& pair : pairs) {
            leftItems.push_back(getJsonValue(pair, "left"));
            rightItems.push_back(getJsonValue(pair, "right"));
        }

        printColored("Left Column (English):\n", "magenta");
        for (size_t i = 0; i < leftItems.size(); i++) {
            printColored("  " + std::to_string(i + 1) + ". " + leftItems[i] + "\n", "");
        }

        printColored("\nRight Column (Vietnamese):\n", "magenta");
        for (size_t i = 0; i < rightItems.size(); i++) {
            printColored("  " + std::to_string(i + 1) + ". " + rightItems[i] + "\n", "");
        }

        printColored("\nEnter matches (format: left1-right1,left2-right2,...):\n", "yellow");
        printColored("Example: 1-3,2-1,3-5 means: English1-Vietnamese3, English2-Vietnamese1, etc.\n", "cyan");
        printColored("Your matches: ", "green");

        std::string matchesInput;
        std::getline(std::cin, matchesInput);

        // Parse matches
        std::stringstream matchesJson;
        matchesJson << "[";
        std::istringstream iss(matchesInput);
        std::string match;
        bool first = true;
        while (std::getline(iss, match, ',')) {
            size_t dashPos = match.find('-');
            if (dashPos != std::string::npos) {
                int leftIdx = std::stoi(match.substr(0, dashPos)) - 1;
                int rightIdx = std::stoi(match.substr(dashPos + 1)) - 1;
                if (leftIdx >= 0 && leftIdx < (int)leftItems.size() &&
                    rightIdx >= 0 && rightIdx < (int)rightItems.size()) {
                    if (!first) matchesJson << ",";
                    first = false;
                    matchesJson << R"({"left":")" << escapeJson(leftItems[leftIdx])
                                << R"(","right":")" << escapeJson(rightItems[rightIdx]) << R"("})";
                }
            }
        }
        matchesJson << "]";

        // Submit result
        std::string submitRequest = R"({"messageType":"SUBMIT_GAME_RESULT_REQUEST","messageId":")" + generateMessageId() +
                                    R"(","timestamp":)" + std::to_string(getCurrentTimestamp()) +
                                    R"(,"sessionToken":")" + sessionToken +
                                    R"(","payload":{"gameSessionId":")" + gameSessionId +
                                    R"(","gameId":")" + selectedGameId +
                                    R"(","matches":)" + matchesJson.str() + R"(}})";

        std::string submitResponse = sendAndReceive(submitRequest);
        std::string submitStatus = getJsonValue(submitResponse, "status");

        if (submitStatus == "success") {
            std::string resultData = getJsonObject(submitResponse, "data");
            std::string score = getJsonValue(resultData, "score");
            std::string maxScore = getJsonValue(resultData, "maxScore");
            std::string percentage = getJsonValue(resultData, "percentage");
            std::string grade = getJsonValue(resultData, "grade");
            std::string correctMatches = getJsonValue(resultData, "correctMatches");
            std::string totalPairs = getJsonValue(resultData, "totalPairs");

            clearScreen();
            printColored("╔══════════════════════════════════════════╗\n", "cyan");
            printColored("║            GAME RESULTS                  ║\n", "cyan");
            printColored("╚══════════════════════════════════════════╝\n\n", "cyan");

            printColored("Score: " + score + "/" + maxScore + " points\n", "green");
            printColored("Percentage: " + percentage + "%\n", "yellow");
            printColored("Grade: " + grade + "\n", (grade == "A" || grade == "B") ? "green" : "yellow");
            printColored("Correct Matches: " + correctMatches + "/" + totalPairs + "\n", "");

            if (percentage == "100") {
                printColored("\n    ★★★ PERFECT SCORE! ★★★\n", "green");
            }
        } else {
            std::string message = getJsonValue(submitResponse, "message");
            printColored("\n[ERROR] " + message + "\n", "red");
        }
    } else if (gameType == "sentence_match") {
        printColored("Match questions with answers:\n\n", "yellow");
        
        std::vector<std::string> leftItems, rightItems;
        for (const std::string& pair : pairs) {
            leftItems.push_back(getJsonValue(pair, "left"));
            rightItems.push_back(getJsonValue(pair, "right"));
        }

        printColored("Questions:\n", "magenta");
        for (size_t i = 0; i < leftItems.size(); i++) {
            printColored("  " + std::to_string(i + 1) + ". " + leftItems[i] + "\n", "");
        }

        printColored("\nAnswers:\n", "magenta");
        for (size_t i = 0; i < rightItems.size(); i++) {
            printColored("  " + std::to_string(i + 1) + ". " + rightItems[i] + "\n", "");
        }

        printColored("\nEnter matches (format: q1-a2,q2-a1,...): ", "green");
        std::string matchesInput;
        std::getline(std::cin, matchesInput);

        // Parse matches
        std::stringstream matchesJson;
        matchesJson << "[";
        std::istringstream iss(matchesInput);
        std::string match;
        bool first = true;
        while (std::getline(iss, match, ',')) {
            size_t dashPos = match.find('-');
            if (dashPos != std::string::npos) {
                int leftIdx = std::stoi(match.substr(0, dashPos)) - 1;
                int rightIdx = std::stoi(match.substr(dashPos + 1)) - 1;
                if (leftIdx >= 0 && leftIdx < (int)leftItems.size() &&
                    rightIdx >= 0 && rightIdx < (int)rightItems.size()) {
                    if (!first) matchesJson << ",";
                    first = false;
                    matchesJson << R"({"left":")" << escapeJson(leftItems[leftIdx])
                                << R"(","right":")" << escapeJson(rightItems[rightIdx]) << R"("})";
                }
            }
        }
        matchesJson << "]";

        std::string submitRequest = R"({"messageType":"SUBMIT_GAME_RESULT_REQUEST","messageId":")" + generateMessageId() +
                                    R"(","timestamp":)" + std::to_string(getCurrentTimestamp()) +
                                    R"(,"sessionToken":")" + sessionToken +
                                    R"(","payload":{"gameSessionId":")" + gameSessionId +
                                    R"(","gameId":")" + selectedGameId +
                                    R"(","matches":)" + matchesJson.str() + R"(}})";

        std::string submitResponse = sendAndReceive(submitRequest);
        std::string submitStatus = getJsonValue(submitResponse, "status");

        if (submitStatus == "success") {
            std::string resultData = getJsonObject(submitResponse, "data");
            std::string score = getJsonValue(resultData, "score");
            std::string maxScore = getJsonValue(resultData, "maxScore");
            std::string percentage = getJsonValue(resultData, "percentage");
            std::string grade = getJsonValue(resultData, "grade");

            clearScreen();
            printColored("╔══════════════════════════════════════════╗\n", "cyan");
            printColored("║            GAME RESULTS                  ║\n", "cyan");
            printColored("╚══════════════════════════════════════════╝\n\n", "cyan");

            printColored("Score: " + score + "/" + maxScore + " points\n", "green");
            printColored("Percentage: " + percentage + "%\n", "yellow");
            printColored("Grade: " + grade + "\n", (grade == "A" || grade == "B") ? "green" : "yellow");
        }
    } else if (gameType == "picture_match") {
        printColored("Match words with pictures:\n\n", "yellow");
        printColored("(Note: In a real app, images would be displayed here)\n\n", "magenta");
        
        std::vector<std::string> words, imageUrls;
        for (const std::string& pair : pairs) {
            words.push_back(getJsonValue(pair, "word"));
            imageUrls.push_back(getJsonValue(pair, "imageUrl"));
        }

        printColored("Words:\n", "magenta");
        for (size_t i = 0; i < words.size(); i++) {
            printColored("  " + std::to_string(i + 1) + ". " + words[i] + " (Image: " + imageUrls[i] + ")\n", "");
        }

        printColored("\nEnter matches (format: word1-img1,word2-img2,...): ", "green");
        std::string matchesInput;
        std::getline(std::cin, matchesInput);

        // Parse matches
        std::stringstream matchesJson;
        matchesJson << "[";
        std::istringstream iss(matchesInput);
        std::string match;
        bool first = true;
        while (std::getline(iss, match, ',')) {
            size_t dashPos = match.find('-');
            if (dashPos != std::string::npos) {
                int wordIdx = std::stoi(match.substr(0, dashPos)) - 1;
                int imgIdx = std::stoi(match.substr(dashPos + 1)) - 1;
                if (wordIdx >= 0 && wordIdx < (int)words.size() &&
                    imgIdx >= 0 && imgIdx < (int)imageUrls.size()) {
                    if (!first) matchesJson << ",";
                    first = false;
                    matchesJson << R"({"word":")" << escapeJson(words[wordIdx])
                                << R"(","imageUrl":")" << escapeJson(imageUrls[imgIdx]) << R"("})";
                }
            }
        }
        matchesJson << "]";

        std::string submitRequest = R"({"messageType":"SUBMIT_GAME_RESULT_REQUEST","messageId":")" + generateMessageId() +
                                    R"(","timestamp":)" + std::to_string(getCurrentTimestamp()) +
                                    R"(,"sessionToken":")" + sessionToken +
                                    R"(","payload":{"gameSessionId":")" + gameSessionId +
                                    R"(","gameId":")" + selectedGameId +
                                    R"(","matches":)" + matchesJson.str() + R"(}})";

        std::string submitResponse = sendAndReceive(submitRequest);
        std::string submitStatus = getJsonValue(submitResponse, "status");

        if (submitStatus == "success") {
            std::string resultData = getJsonObject(submitResponse, "data");
            std::string score = getJsonValue(resultData, "score");
            std::string maxScore = getJsonValue(resultData, "maxScore");
            std::string percentage = getJsonValue(resultData, "percentage");
            std::string grade = getJsonValue(resultData, "grade");

            clearScreen();
            printColored("╔══════════════════════════════════════════╗\n", "cyan");
            printColored("║            GAME RESULTS                  ║\n", "cyan");
            printColored("╚══════════════════════════════════════════╝\n\n", "cyan");

            printColored("Score: " + score + "/" + maxScore + " points\n", "green");
            printColored("Percentage: " + percentage + "%\n", "yellow");
            printColored("Grade: " + grade + "\n", (grade == "A" || grade == "B") ? "green" : "yellow");
        }
    }

    waitEnter();
}

void chat() {
    clearScreen();
    printColored("╔══════════════════════════════════════════╗\n", "cyan");
    printColored("║                  CHAT                    ║\n", "cyan");
    printColored("╚══════════════════════════════════════════╝\n", "cyan");

    // Kiểm tra xem có tin nhắn mới cần phản hồi không
    std::string quickReplyId;
    std::string quickReplyName;
    {
        std::lock_guard<std::mutex> lock(notificationMutex);
        if (hasNewNotification && !pendingChatUserId.empty()) {
            quickReplyId = pendingChatUserId;
            quickReplyName = pendingChatUserName;
        }
    }

    if (!quickReplyId.empty()) {
        printColored("\n📬 You have a pending message from ", "yellow");
        printColored(quickReplyName, "cyan");
        printColored("\n", "");
        printColored("Press 'r' to reply directly, or any other key to see contact list: ", "green");

        std::string input;
        std::getline(std::cin, input);

        if (input == "r" || input == "R") {
            openChatWith(quickReplyId, quickReplyName);
            return;
        }
    }

    // Lấy danh sách contacts
    std::string request = R"({"messageType":"GET_CONTACT_LIST_REQUEST","messageId":")" + generateMessageId() +
                          R"(","timestamp":)" + std::to_string(getCurrentTimestamp()) +
                          R"(,"sessionToken":")" + sessionToken +
                          R"(","payload":{"contactType":"all","online":false}})";

    std::string response = sendAndReceive(request);
    std::string status = getJsonValue(response, "status");

    if (status != "success") {
        std::string message = getJsonValue(response, "message");
        printColored("\n[ERROR] " + message + "\n", "red");
        waitEnter();
        return;
    }

    std::string data = getJsonObject(response, "data");
    std::string contactsArray = getJsonArray(data, "contacts");
    std::vector<std::string> contacts = parseJsonArray(contactsArray);

    if (contacts.empty()) {
        printColored("\nNo contacts available.\n", "yellow");
        waitEnter();
        return;
    }

    printColored("\nAvailable Contacts:\n", "yellow");
    printColored("┌────┬────────────────────────┬──────────────┬──────────┐\n", "cyan");
    printColored("│ #  │ Name                   │ Role         │ Status   │\n", "cyan");
    printColored("├────┼────────────────────────┼──────────────┼──────────┤\n", "cyan");

    int idx = 1;
    std::vector<std::pair<std::string, std::string>> contactList;

    for (const std::string& contact : contacts) {
        std::string contactId = getJsonValue(contact, "userId");
        std::string fullName = getJsonValue(contact, "fullName");
        std::string role = getJsonValue(contact, "role");
        std::string contactStatus = getJsonValue(contact, "status");

        contactList.push_back({contactId, fullName});

        std::string statusColor = (contactStatus == "online") ? "green" : "red";

        printf("│ %-2d │ %-22s │ %-12s │ ", idx, fullName.c_str(), role.c_str());
        printColored(contactStatus, statusColor);
        printf("%*s│\n", 8 - (int)contactStatus.length(), "");
        idx++;
    }

    printColored("└────┴────────────────────────┴──────────────┴──────────┘\n", "cyan");

    printColored("\nEnter contact number to chat (0 to go back): ", "green");

    std::string input;
    std::getline(std::cin, input);

    int choice;
    try {
        choice = std::stoi(input);
    } catch (...) {
        return;
    }

    if (choice <= 0 || choice > (int)contactList.size()) {
        return;
    }

    std::string recipientId = contactList[choice - 1].first;
    std::string recipientName = contactList[choice - 1].second;

    openChatWith(recipientId, recipientName);
}

// ============================================================================
// SIGNAL HANDLER & MAIN
// ============================================================================

void signalHandler(int sig) {
    printColored("\n[INFO] Disconnecting...\n", "yellow");
    running = false;
    if (clientSocket >= 0) {
        close(clientSocket);
    }
    exit(0);
}

int main_cli(int argc, char* argv[]) {
    std::string serverIP = DEFAULT_SERVER;
    int port = DEFAULT_PORT;

    if (argc > 1) serverIP = argv[1];
    if (argc > 2) port = std::stoi(argv[2]);

    signal(SIGINT, signalHandler);
    signal(SIGTERM, signalHandler);

    clientSocket = socket(AF_INET, SOCK_STREAM, 0);
    if (clientSocket < 0) {
        std::cerr << "[ERROR] Cannot create socket" << std::endl;
        return 1;
    }

    struct sockaddr_in serverAddr;
    memset(&serverAddr, 0, sizeof(serverAddr));
    serverAddr.sin_family = AF_INET;
    serverAddr.sin_port = htons(port);

    if (inet_pton(AF_INET, serverIP.c_str(), &serverAddr.sin_addr) <= 0) {
        std::cerr << "[ERROR] Invalid server address" << std::endl;
        close(clientSocket);
        return 1;
    }

    clearScreen();
    printColored("╔══════════════════════════════════════════╗\n", "cyan");
    printColored("║       ENGLISH LEARNING APP               ║\n", "cyan");
    printColored("╚══════════════════════════════════════════╝\n", "cyan");
    printColored("Connecting to server " + serverIP + ":" + std::to_string(port) + "...\n", "yellow");

    if (connect(clientSocket, (struct sockaddr*)&serverAddr, sizeof(serverAddr)) < 0) {
        std::cerr << "[ERROR] Cannot connect to server" << std::endl;
        close(clientSocket);
        return 1;
    }

    printColored("[SUCCESS] Connected to server!\n\n", "green");

    // [FIX] Khởi động background thread để nhận message
    std::thread recvThread(receiveThreadFunc);

    // Menu đăng nhập/đăng ký
    while (running && !loggedIn) {
        printColored("╔══════════════════════════════════════════╗\n", "cyan");
        printColored("║       ENGLISH LEARNING APP               ║\n", "cyan");
        printColored("╠══════════════════════════════════════════╣\n", "cyan");
        printColored("║  1. Login                                ║\n", "");
        printColored("║  2. Register                             ║\n", "");
        printColored("║  0. Exit                                 ║\n", "");
        printColored("╚══════════════════════════════════════════╝\n", "cyan");
        printColored("Enter your choice: ", "green");

        std::string input;
        std::getline(std::cin, input);

        if (input == "1") {
            if (login()) {
                // Login thành công
            }
            waitEnter();
        } else if (input == "2") {
            registerAccount();
            waitEnter();
        } else if (input == "0") {
            running = false;
            break;
        }
    }

    if (!loggedIn) {
        running = false;
        if (recvThread.joinable()) {
            recvThread.join();
        }
        close(clientSocket);
        return 0;
    }

    // Main menu loop
    while (running && loggedIn) {
        showMainMenu();

        std::string input;
        std::getline(std::cin, input);

        if (input == "1") {
            setLevel();
        } else if (input == "2") {
            viewLessons();
        } else if (input == "3") {
            takeTest();
        } else if (input == "4") {
            doExercises();
        } else if (input == "5") {
            playGames();
        } else if (input == "6") {
            chat();
        } else if (input == "7") {
            loggedIn = false;
            sessionToken = "";
            currentUserId = "";
            currentUserName = "";
            printColored("\n[INFO] Logged out successfully.\n", "yellow");
            waitEnter();
        } else if (input == "0") {
            running = false;
        } else if (input == "r" || input == "R") {
            // [FIX] Phản hồi nhanh tin nhắn mới
            std::string quickReplyId;
            std::string quickReplyName;
            {
                std::lock_guard<std::mutex> lock(notificationMutex);
                if (hasNewNotification && !pendingChatUserId.empty()) {
                    quickReplyId = pendingChatUserId;
                    quickReplyName = pendingChatUserName;
                }
            }
            if (!quickReplyId.empty()) {
                openChatWith(quickReplyId, quickReplyName);
            } else {
                printColored("\nNo pending messages to reply.\n", "yellow");
                waitEnter();
            }
        }
    }

    printColored("\nGoodbye! Thank you for using English Learning App.\n", "cyan");

    // [FIX] Chờ receive thread kết thúc
    running = false;
    if (recvThread.joinable()) {
        recvThread.join();
    }

    close(clientSocket);
    return 0;
}
// --- DÁN ĐOẠN NÀY VÀO CLIENT.CPP ---
bool connectToServer(const char* ip, int port) {
    // Tạo socket
    clientSocket = socket(AF_INET, SOCK_STREAM, 0);
    if (clientSocket < 0) {
        std::cerr << "[ERROR] Cannot create socket" << std::endl;
        return false;
    }

    // Cấu hình địa chỉ
    struct sockaddr_in serverAddr;
    memset(&serverAddr, 0, sizeof(serverAddr));
    serverAddr.sin_family = AF_INET;
    serverAddr.sin_port = htons(port);

    if (inet_pton(AF_INET, ip, &serverAddr.sin_addr) <= 0) {
        std::cerr << "[ERROR] Invalid server address" << std::endl;
        close(clientSocket);
        return false;
    }

    // Kết nối
    std::cout << "Connecting to " << ip << ":" << port << "..." << std::endl;
    if (connect(clientSocket, (struct sockaddr*)&serverAddr, sizeof(serverAddr)) < 0) {
        std::cerr << "[ERROR] Cannot connect to server" << std::endl;
        close(clientSocket);
        return false;
    }

    std::cout << "[SUCCESS] Connected!" << std::endl;
    return true;
}

// Entry point wrapper so linker finds main (skipped when embedded in GUI build)
#ifndef CLIENT_SKIP_MAIN
int main(int argc, char* argv[]) {
    return main_cli(argc, argv);
}
#endif