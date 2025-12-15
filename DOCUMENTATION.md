# English Learning App - Technical Documentation

## Mục lục
1. [Tổng quan hệ thống](#1-tổng-quan-hệ-thống)
2. [Kiến trúc Server](#2-kiến-trúc-server)
3. [Kiến trúc Client](#3-kiến-trúc-client)
4. [Giao thức truyền thông](#4-giao-thức-truyền-thông)
5. [Các tính năng chính](#5-các-tính-năng-chính)
6. [Luồng xử lý chi tiết](#6-luồng-xử-lý-chi-tiết)
7. [Cơ chế Chat Real-time](#7-cơ-chế-chat-real-time)
8. [Bảo mật và Session](#8-bảo-mật-và-session)
9. [Hướng dẫn biên dịch và chạy](#9-hướng-dẫn-biên-dịch-và-chạy)

---

## 1. Tổng quan hệ thống

### 1.1 Mô tả
English Learning App là ứng dụng học tiếng Anh với kiến trúc Client-Server, sử dụng TCP Socket để giao tiếp. Hệ thống hỗ trợ:
- Đăng ký/Đăng nhập tài khoản
- Xem bài học theo cấp độ
- Làm bài kiểm tra
- Chat real-time giữa các user

### 1.2 Công nghệ sử dụng
- **Ngôn ngữ**: C++17
- **Giao thức mạng**: TCP/IP (POSIX Socket)
- **Threading**: std::thread, std::mutex, std::condition_variable
- **Định dạng dữ liệu**: JSON

### 1.3 Sơ đồ kiến trúc tổng quan

```
┌─────────────────┐         TCP Socket          ┌─────────────────┐
│                 │◄──────────────────────────►│                 │
│    Client 1     │                             │                 │
│  (Background    │         Port 8888           │     SERVER      │
│    Thread)      │                             │                 │
└─────────────────┘                             │  ┌───────────┐  │
                                                │  │  Thread   │  │
┌─────────────────┐                             │  │  Pool     │  │
│                 │◄──────────────────────────►│  └───────────┘  │
│    Client 2     │                             │                 │
│                 │                             │  ┌───────────┐  │
└─────────────────┘                             │  │  Memory   │  │
                                                │  │  Storage  │  │
┌─────────────────┐                             │  └───────────┘  │
│                 │◄──────────────────────────►│                 │
│    Client N     │                             └─────────────────┘
│                 │
└─────────────────┘
```

---

## 2. Kiến trúc Server

### 2.1 Cấu trúc file `server.cpp`

```
server.cpp
├── Includes & Defines (1-45)
├── Data Structures (47-120)
│   ├── User
│   ├── Session
│   ├── Lesson
│   ├── Question
│   ├── Test
│   └── ChatMessage
├── Global Variables (122-140)
├── Utility Functions (142-300)
│   ├── getCurrentTimestamp()
│   ├── generateId()
│   ├── generateSessionToken()
│   ├── escapeJson()
│   └── logMessage()
├── JSON Parser Functions (302-500)
├── Data Initialization (502-800)
│   ├── initializeLessons()
│   └── initializeTests()
├── Request Handlers (802-1800)
│   ├── handleRegister()
│   ├── handleLogin()
│   ├── handleGetLessons()
│   ├── handleGetLessonDetail()
│   ├── handleGetTest()
│   ├── handleSubmitTest()
│   ├── handleSetLevel()
│   ├── handleGetContactList()
│   ├── handleSendMessage()
│   ├── handleGetChatHistory()
│   └── handleMarkMessagesRead()
├── Client Handler (1802-1950)
│   └── handleClient()
└── Main Function (1952-2000)
```

### 2.2 Cấu trúc dữ liệu chính

#### User Structure
```cpp
struct User {
    std::string userId;      // ID duy nhất: "user_xxx"
    std::string fullname;    // Họ tên
    std::string email;       // Email (unique, dùng làm key)
    std::string password;    // Mật khẩu (plain text - demo)
    std::string level;       // beginner/intermediate/advanced
    std::string role;        // student/teacher
    long long createdAt;     // Timestamp tạo tài khoản
    bool online;             // Trạng thái online
    int clientSocket;        // Socket descriptor khi online
};
```

#### Session Structure
```cpp
struct Session {
    std::string sessionToken;  // Token ngẫu nhiên 32 ký tự
    std::string userId;        // ID user sở hữu session
    long long expiresAt;       // Thời điểm hết hạn (1 giờ)
};
```

#### ChatMessage Structure
```cpp
struct ChatMessage {
    std::string messageId;    // ID tin nhắn: "chatmsg_xxx"
    std::string senderId;     // ID người gửi
    std::string recipientId;  // ID người nhận
    std::string content;      // Nội dung tin nhắn
    long long timestamp;      // Thời điểm gửi
    bool read;                // Đã đọc chưa
};
```

### 2.3 Lưu trữ dữ liệu (In-Memory)

```cpp
// User storage - Map theo email
std::map<std::string, User> users;

// User lookup by ID
std::map<std::string, User*> userById;

// Session storage - Map theo token
std::map<std::string, Session> sessions;

// Client socket to session mapping
std::map<int, std::string> clientSessions;

// Lessons storage
std::vector<Lesson> lessons;

// Tests storage
std::vector<Test> tests;

// Chat messages storage
std::vector<ChatMessage> chatMessages;

// Mutex cho thread-safety
std::mutex usersMutex;
std::mutex sessionsMutex;
std::mutex chatMutex;
std::mutex logMutex;
```

### 2.4 Xử lý đa client (Multi-threading)

```cpp
void main() {
    // Tạo socket server
    int serverSocket = socket(AF_INET, SOCK_STREAM, 0);

    // Bind và Listen
    bind(serverSocket, ...);
    listen(serverSocket, 10);

    while (running) {
        // Accept connection mới
        int clientSocket = accept(serverSocket, ...);

        // Tạo thread mới cho mỗi client
        std::thread clientThread(handleClient, clientSocket);
        clientThread.detach();  // Detach để chạy độc lập
    }
}
```

**Sơ đồ xử lý đa client:**
```
                    ┌─────────────────┐
                    │   Main Thread   │
                    │   (Accept)      │
                    └────────┬────────┘
                             │
            ┌────────────────┼────────────────┐
            ▼                ▼                ▼
    ┌───────────────┐ ┌───────────────┐ ┌───────────────┐
    │ Client Thread │ │ Client Thread │ │ Client Thread │
    │   (Client 1)  │ │   (Client 2)  │ │   (Client N)  │
    └───────────────┘ └───────────────┘ └───────────────┘
```

### 2.5 Hàm handleClient - Xử lý request từ client

```cpp
void handleClient(int clientSocket) {
    while (running) {
        // 1. Đọc độ dài message (4 bytes, network byte order)
        uint32_t msgLen;
        recv(clientSocket, &msgLen, sizeof(msgLen), MSG_WAITALL);
        msgLen = ntohl(msgLen);

        // 2. Đọc nội dung message
        std::string message(msgLen, '\0');
        recv(clientSocket, &message[0], msgLen, 0);

        // 3. Parse messageType và dispatch
        std::string messageType = getJsonValue(message, "messageType");
        std::string response;

        if (messageType == "LOGIN_REQUEST") {
            response = handleLogin(message, clientSocket);
        } else if (messageType == "REGISTER_REQUEST") {
            response = handleRegister(message);
        } else if (messageType == "SEND_MESSAGE_REQUEST") {
            response = handleSendMessage(message, clientSocket);
        }
        // ... các handler khác

        // 4. Gửi response
        uint32_t respLen = htonl(response.length());
        send(clientSocket, &respLen, sizeof(respLen), 0);
        send(clientSocket, response.c_str(), response.length(), 0);
    }

    // Cleanup khi client disconnect
    close(clientSocket);
}
```

---

## 3. Kiến trúc Client

### 3.1 Cấu trúc file `client.cpp`

```
client.cpp
├── Includes & Defines (1-45)
├── Global Variables (46-80)
│   ├── Socket & Session info
│   ├── Threading primitives
│   └── Notification state
├── Utility Functions (82-135)
├── JSON Parser Functions (137-285)
├── Push Notification Handler (287-370)
├── Background Receive Thread (372-450)
├── Network Functions (452-495)
│   ├── sendMessage()
│   ├── waitForResponse()
│   └── sendAndReceive()
├── UI Functions (497-540)
├── Feature Functions (542-1140)
│   ├── registerAccount()
│   ├── login()
│   ├── setLevel()
│   ├── viewLessons()
│   ├── takeTest()
│   ├── openChatWith()
│   └── chat()
└── Main Function (1142-1380)
```

### 3.2 Mô hình Threading của Client

```
┌─────────────────────────────────────────────────────────┐
│                      CLIENT                              │
│                                                          │
│  ┌──────────────────┐      ┌──────────────────┐         │
│  │   Main Thread    │      │  Receive Thread  │         │
│  │                  │      │   (Background)   │         │
│  │  - UI/Menu       │      │                  │         │
│  │  - User Input    │      │  - poll() socket │         │
│  │  - Send Request  │      │  - Recv message  │         │
│  │  - Wait Response │◄────►│  - Classify msg  │         │
│  │                  │      │  - Push to queue │         │
│  └──────────────────┘      │    or handle     │         │
│           │                │    notification  │         │
│           ▼                └──────────────────┘         │
│  ┌──────────────────┐                                   │
│  │  Response Queue  │                                   │
│  │  (thread-safe)   │                                   │
│  └──────────────────┘                                   │
│                                                          │
└─────────────────────────────────────────────────────────┘
```

### 3.3 Background Receive Thread

```cpp
void receiveThreadFunc() {
    while (running && clientSocket >= 0) {
        // 1. Sử dụng poll() để non-blocking check
        struct pollfd pfd;
        pfd.fd = clientSocket;
        pfd.events = POLLIN;

        int ret = poll(&pfd, 1, 100);  // timeout 100ms

        if (ret > 0 && (pfd.revents & POLLIN)) {
            // 2. Đọc message từ server
            uint32_t msgLen;
            recv(clientSocket, &msgLen, sizeof(msgLen), MSG_WAITALL);
            msgLen = ntohl(msgLen);

            std::string buffer(msgLen, '\0');
            recv(clientSocket, &buffer[0], msgLen, 0);

            // 3. Phân loại message
            std::string messageType = getJsonValue(buffer, "messageType");

            if (messageType == "RECEIVE_MESSAGE" ||
                messageType == "UNREAD_MESSAGES_NOTIFICATION") {
                // Push notification -> xử lý ngay
                handlePushNotification(buffer);
            } else {
                // Response cho request -> đưa vào queue
                {
                    std::lock_guard<std::mutex> lock(responseQueueMutex);
                    responseQueue.push(buffer);
                }
                responseCondition.notify_one();
            }
        }
    }
}
```

### 3.4 Cơ chế Send/Receive với Queue

```cpp
// Gửi message (thread-safe)
bool sendMessage(const std::string& message) {
    std::lock_guard<std::mutex> lock(socketMutex);

    uint32_t len = htonl(message.length());
    send(clientSocket, &len, sizeof(len), 0);
    send(clientSocket, message.c_str(), message.length(), 0);
    return true;
}

// Chờ response từ queue
std::string waitForResponse(int timeoutMs = 10000) {
    std::unique_lock<std::mutex> lock(responseQueueMutex);

    // Chờ có response hoặc timeout
    bool success = responseCondition.wait_for(
        lock,
        std::chrono::milliseconds(timeoutMs),
        []() { return !responseQueue.empty() || !running; }
    );

    if (!success || responseQueue.empty()) return "";

    std::string response = responseQueue.front();
    responseQueue.pop();
    return response;
}

// Request-Response pattern
std::string sendAndReceive(const std::string& request) {
    if (!sendMessage(request)) return "";
    return waitForResponse();
}
```

---

## 4. Giao thức truyền thông

### 4.1 Message Format

Mỗi message được truyền theo format:
```
┌──────────────────┬─────────────────────────────────┐
│  Length (4 bytes)│       JSON Payload              │
│  Network Order   │       (Variable length)         │
│  (Big Endian)    │                                 │
└──────────────────┴─────────────────────────────────┘
```

### 4.2 Request/Response Structure

**Request Format:**
```json
{
    "messageType": "REQUEST_TYPE",
    "messageId": "msg_123",
    "timestamp": 1701432000000,
    "sessionToken": "abc123...",
    "payload": {
        // Request-specific data
    }
}
```

**Response Format:**
```json
{
    "messageType": "RESPONSE_TYPE",
    "messageId": "msg_123",
    "timestamp": 1701432000001,
    "payload": {
        "status": "success|error",
        "message": "Description",
        "data": {
            // Response-specific data
        }
    }
}
```

### 4.3 Danh sách Message Types

| Request Type | Response Type | Mô tả |
|--------------|---------------|-------|
| `REGISTER_REQUEST` | `REGISTER_RESPONSE` | Đăng ký tài khoản |
| `LOGIN_REQUEST` | `LOGIN_RESPONSE` | Đăng nhập |
| `SET_LEVEL_REQUEST` | `SET_LEVEL_RESPONSE` | Đặt cấp độ |
| `GET_LESSONS_REQUEST` | `GET_LESSONS_RESPONSE` | Lấy danh sách bài học |
| `GET_LESSON_DETAIL_REQUEST` | `GET_LESSON_DETAIL_RESPONSE` | Chi tiết bài học |
| `GET_TEST_REQUEST` | `GET_TEST_RESPONSE` | Lấy đề test |
| `SUBMIT_TEST_REQUEST` | `SUBMIT_TEST_RESPONSE` | Nộp bài test |
| `GET_CONTACT_LIST_REQUEST` | `GET_CONTACT_LIST_RESPONSE` | Danh sách contacts |
| `SEND_MESSAGE_REQUEST` | `SEND_MESSAGE_RESPONSE` | Gửi tin nhắn |
| `GET_CHAT_HISTORY_REQUEST` | `GET_CHAT_HISTORY_RESPONSE` | Lịch sử chat |
| `MARK_MESSAGES_READ_REQUEST` | `MARK_MESSAGES_READ_RESPONSE` | Đánh dấu đã đọc |

### 4.4 Push Notification Types (Server -> Client)

| Type | Mô tả |
|------|-------|
| `RECEIVE_MESSAGE` | Tin nhắn mới từ user khác |
| `UNREAD_MESSAGES_NOTIFICATION` | Thông báo tin nhắn chưa đọc khi login |

---

## 5. Các tính năng chính

### 5.1 Đăng ký tài khoản

**Flow:**
```
Client                          Server
   │                               │
   │  REGISTER_REQUEST             │
   │  {fullname, email, password}  │
   │──────────────────────────────►│
   │                               │ Validate:
   │                               │ - Email format
   │                               │ - Password match
   │                               │ - Email unique
   │                               │ Create User
   │  REGISTER_RESPONSE            │
   │  {status, userId}             │
   │◄──────────────────────────────│
   │                               │
```

### 5.2 Đăng nhập

**Flow:**
```
Client                          Server
   │                               │
   │  LOGIN_REQUEST                │
   │  {email, password}            │
   │──────────────────────────────►│
   │                               │ Validate credentials
   │                               │ Create session
   │                               │ Mark user online
   │  LOGIN_RESPONSE               │
   │  {sessionToken, userData}     │
   │◄──────────────────────────────│
   │                               │
   │  UNREAD_MESSAGES_NOTIFICATION │
   │  (if any unread messages)     │
   │◄──────────────────────────────│
   │                               │
```

### 5.3 Xem bài học

**Flow:**
```
Client                          Server
   │                               │
   │  GET_LESSONS_REQUEST          │
   │  {level, topic, page}         │
   │──────────────────────────────►│
   │                               │ Filter lessons
   │  GET_LESSONS_RESPONSE         │
   │  {lessons[]}                  │
   │◄──────────────────────────────│
   │                               │
   │  GET_LESSON_DETAIL_REQUEST    │
   │  {lessonId}                   │
   │──────────────────────────────►│
   │                               │
   │  GET_LESSON_DETAIL_RESPONSE   │
   │  {title, content, ...}        │
   │◄──────────────────────────────│
   │                               │
```

### 5.4 Làm bài test

**Flow:**
```
Client                          Server
   │                               │
   │  GET_TEST_REQUEST             │
   │  {level, topic, testType}     │
   │──────────────────────────────►│
   │                               │ Generate test
   │  GET_TEST_RESPONSE            │
   │  {testId, questions[]}        │
   │◄──────────────────────────────│
   │                               │
   │  (User answers questions)     │
   │                               │
   │  SUBMIT_TEST_REQUEST          │
   │  {testId, answers[]}          │
   │──────────────────────────────►│
   │                               │ Grade test
   │  SUBMIT_TEST_RESPONSE         │
   │  {score, percentage, grade,   │
   │   detailedResults[]}          │
   │◄──────────────────────────────│
   │                               │
```

---

## 6. Luồng xử lý chi tiết

### 6.1 Login Handler (Server)

```cpp
std::string handleLogin(const std::string& json, int clientSocket) {
    // 1. Parse request
    std::string email = getJsonValue(payload, "email");
    std::string password = getJsonValue(payload, "password");

    // 2. Validate credentials
    auto it = users.find(email);
    if (it == users.end() || it->second.password != password) {
        return errorResponse("Invalid email or password");
    }

    // 3. Update user status
    User& user = it->second;
    user.online = true;
    user.clientSocket = clientSocket;

    // 4. Create session
    std::string sessionToken = generateSessionToken();
    Session session;
    session.sessionToken = sessionToken;
    session.userId = user.userId;
    session.expiresAt = getCurrentTimestamp() + 3600000; // 1 hour

    sessions[sessionToken] = session;
    clientSessions[clientSocket] = sessionToken;

    // 5. Send login response
    sendResponse(clientSocket, loginResponse);

    // 6. Send unread messages notification
    sendUnreadMessagesNotification(clientSocket, user.userId);

    return ""; // Empty = already sent
}
```

### 6.2 Send Message Handler (Server)

```cpp
std::string handleSendMessage(const std::string& json, int clientSocket) {
    // 1. Validate session
    std::string userId = validateSession(sessionToken);
    if (userId.empty()) return errorResponse("Invalid session");

    // 2. Create message
    ChatMessage msg;
    msg.messageId = generateId("chatmsg");
    msg.senderId = userId;
    msg.recipientId = recipientId;
    msg.content = messageContent;
    msg.timestamp = getCurrentTimestamp();
    msg.read = false;

    // 3. Store message
    chatMessages.push_back(msg);

    // 4. Try to deliver in real-time
    bool delivered = false;
    User* recipient = userById[recipientId];

    if (recipient->online && recipient->clientSocket > 0) {
        // Build push notification
        std::string notification = buildReceiveMessageNotification(msg);

        // Send to recipient
        uint32_t len = htonl(notification.length());
        send(recipient->clientSocket, &len, sizeof(len), 0);
        send(recipient->clientSocket, notification.c_str(), ...);
        delivered = true;
    }

    // 5. Return response with delivery status
    return buildResponse(msg.messageId, delivered);
}
```

### 6.3 Push Notification Handler (Client)

```cpp
void handlePushNotification(const std::string& message) {
    std::string messageType = getJsonValue(message, "messageType");

    if (messageType == "RECEIVE_MESSAGE") {
        std::string senderId = getJsonValue(payload, "senderId");
        std::string senderName = getJsonValue(payload, "senderName");
        std::string content = getJsonValue(payload, "messageContent");

        // Check if currently chatting with sender
        bool isChattingWithSender = (inChatMode && currentChatPartnerId == senderId);

        if (isChattingWithSender) {
            // Display message directly in chat
            std::cout << senderName << ": " << content << "\n";
            std::cout << "You: " << std::flush;
        } else {
            // Save for quick reply
            pendingChatUserId = senderId;
            pendingChatUserName = senderName;
            hasNewNotification = true;

            // Show popup notification
            displayNotificationPopup(senderName, content);
        }
    }
}
```

---

## 7. Cơ chế Chat Real-time

### 7.1 Tổng quan

```
┌──────────┐                    ┌──────────┐                    ┌──────────┐
│ Client A │                    │  SERVER  │                    │ Client B │
│ (Online) │                    │          │                    │ (Online) │
└────┬─────┘                    └────┬─────┘                    └────┬─────┘
     │                               │                               │
     │  1. SEND_MESSAGE_REQUEST      │                               │
     │  {recipientId: B, content}    │                               │
     │──────────────────────────────►│                               │
     │                               │                               │
     │                               │  2. Store message             │
     │                               │  3. Check B online? YES       │
     │                               │                               │
     │                               │  4. RECEIVE_MESSAGE           │
     │                               │  {senderId: A, content}       │
     │                               │──────────────────────────────►│
     │                               │                               │
     │                               │                               │ 5. Display
     │                               │                               │    message
     │  6. SEND_MESSAGE_RESPONSE     │                               │
     │  {delivered: true}            │                               │
     │◄──────────────────────────────│                               │
     │                               │                               │
```

### 7.2 Khi recipient offline

```
┌──────────┐                    ┌──────────┐                    ┌──────────┐
│ Client A │                    │  SERVER  │                    │ Client B │
│ (Online) │                    │          │                    │(Offline) │
└────┬─────┘                    └────┬─────┘                    └────┬─────┘
     │                               │                               │
     │  1. SEND_MESSAGE_REQUEST      │                               │
     │──────────────────────────────►│                               │
     │                               │  2. Store message             │
     │                               │     (read = false)            │
     │                               │  3. Check B online? NO        │
     │  4. SEND_MESSAGE_RESPONSE     │                               │
     │  {delivered: false}           │                               │
     │◄──────────────────────────────│                               │
     │                               │                               │
     │                               │         ... Later ...         │
     │                               │                               │
     │                               │  5. LOGIN_REQUEST             │
     │                               │◄──────────────────────────────│
     │                               │                               │
     │                               │  6. LOGIN_RESPONSE            │
     │                               │──────────────────────────────►│
     │                               │                               │
     │                               │  7. UNREAD_MESSAGES_NOTIF     │
     │                               │  {messages: [{from: A, ...}]} │
     │                               │──────────────────────────────►│
     │                               │                               │
```

### 7.3 Xử lý khi cả 2 đang trong cửa sổ chat

```cpp
// Client A và B đều đang trong openChatWith() với nhau

// Khi A gửi tin nhắn:
// 1. A gọi sendAndReceive(SEND_MESSAGE_REQUEST)
// 2. Server nhận, lưu message, gửi RECEIVE_MESSAGE đến B
// 3. B's receive thread nhận RECEIVE_MESSAGE
// 4. B's handlePushNotification() check:
//    - inChatMode = true
//    - currentChatPartnerId = A's ID
//    => isChattingWithSender = true
// 5. B hiển thị tin nhắn trực tiếp: "A: Hello"
// 6. A nhận SEND_MESSAGE_RESPONSE {delivered: true}
```

### 7.4 Quản lý trạng thái chat

```cpp
// Global variables
std::atomic<bool> inChatMode(false);        // Đang trong chat mode?
std::string currentChatPartnerId = "";       // ID người đang chat
std::string currentChatPartnerName = "";     // Tên người đang chat
std::mutex chatPartnerMutex;                 // Mutex bảo vệ

// Khi mở chat
void openChatWith(recipientId, recipientName) {
    // Set current chat partner
    {
        std::lock_guard<std::mutex> lock(chatPartnerMutex);
        currentChatPartnerId = recipientId;
        currentChatPartnerName = recipientName;
    }
    inChatMode = true;

    // Chat loop...

    // Cleanup khi thoát
    {
        std::lock_guard<std::mutex> lock(chatPartnerMutex);
        currentChatPartnerId = "";
        currentChatPartnerName = "";
    }
    inChatMode = false;
}
```

---

## 8. Bảo mật và Session

### 8.1 Session Token

```cpp
std::string generateSessionToken() {
    static const char charset[] =
        "abcdefghijklmnopqrstuvwxyz"
        "ABCDEFGHIJKLMNOPQRSTUVWXYZ"
        "0123456789";

    std::string token;
    token.reserve(32);

    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<> dis(0, sizeof(charset) - 2);

    for (int i = 0; i < 32; i++) {
        token += charset[dis(gen)];
    }

    return token;
}
```

### 8.2 Session Validation

```cpp
std::string validateSession(const std::string& sessionToken) {
    std::lock_guard<std::mutex> lock(sessionsMutex);

    auto it = sessions.find(sessionToken);
    if (it == sessions.end()) {
        return "";  // Session not found
    }

    if (it->second.expiresAt < getCurrentTimestamp()) {
        sessions.erase(it);  // Session expired
        return "";
    }

    return it->second.userId;  // Valid session
}
```

### 8.3 Thread Safety

Các mutex được sử dụng để bảo vệ shared resources:

| Mutex | Bảo vệ |
|-------|--------|
| `usersMutex` | `users`, `userById` maps |
| `sessionsMutex` | `sessions`, `clientSessions` maps |
| `chatMutex` | `chatMessages` vector |
| `logMutex` | File logging |
| `socketMutex` (client) | Socket send operations |
| `responseQueueMutex` (client) | Response queue |
| `printMutex` (client) | Console output |
| `chatPartnerMutex` (client) | Current chat partner info |
| `notificationMutex` (client) | Pending notification info |

---

## 9. Hướng dẫn biên dịch và chạy

### 9.1 Yêu cầu hệ thống

- **OS**: Linux/Unix (hoặc WSL trên Windows)
- **Compiler**: g++ với hỗ trợ C++17
- **Libraries**: pthread (POSIX threads)

### 9.2 Biên dịch

```bash
# Biên dịch server
g++ -std=c++17 -pthread -o server server.cpp

# Biên dịch client
g++ -std=c++17 -pthread -o client client.cpp
```

### 9.3 Chạy ứng dụng

```bash
# Terminal 1: Khởi động server
./server
# Server listening on port 8888...

# Terminal 2: Khởi động client 1
./client
# Hoặc với custom server/port:
./client 192.168.1.100 9999

# Terminal 3: Khởi động client 2
./client
```

### 9.4 Tài khoản mặc định (Demo)

| Email | Password | Role |
|-------|----------|------|
| `student@example.com` | `student123` | Student |
| `student2@example.com` | `student123` | Student |
| `teacher@example.com` | `teacher123` | Teacher |

### 9.5 Test Chat Real-time

1. Mở 2 terminal, chạy 2 client
2. Login với 2 tài khoản khác nhau
3. Client 1: Vào Chat (4) -> Chọn contact -> Gửi tin nhắn
4. Client 2: Thấy notification popup
5. Client 2: Nhấn 'r' hoặc vào Chat để reply
6. Cả 2 vào chat với nhau -> tin nhắn hiển thị trực tiếp

---

## Phụ lục: Sơ đồ luồng dữ liệu tổng hợp

```
┌─────────────────────────────────────────────────────────────────────────┐
│                              CLIENT                                      │
│  ┌─────────────┐     ┌─────────────┐     ┌─────────────────────────┐   │
│  │    Main     │     │   Receive   │     │      Response Queue     │   │
│  │   Thread    │     │   Thread    │     │   ┌─────┬─────┬─────┐   │   │
│  │             │     │             │     │   │resp1│resp2│ ... │   │   │
│  │  ┌───────┐  │     │  ┌───────┐  │     │   └─────┴─────┴─────┘   │   │
│  │  │ Send  │──┼─────┼──┼►Socket│  │     │                         │   │
│  │  │Request│  │     │  │ recv()│──┼─────┼────► Push to Queue      │   │
│  │  └───────┘  │     │  └───────┘  │     │         OR              │   │
│  │      │      │     │      │      │     │   Handle Notification   │   │
│  │      ▼      │     │      │      │     │                         │   │
│  │  ┌───────┐  │     │      │      │     │                         │   │
│  │  │ Wait  │◄─┼─────┼──────┼──────┼─────┼──── Pop from Queue      │   │
│  │  │Response│ │     │      │      │     │                         │   │
│  │  └───────┘  │     │      ▼      │     └─────────────────────────┘   │
│  │             │     │  ┌───────┐  │                                    │
│  │             │     │  │Display│  │                                    │
│  │             │     │  │Notif. │  │                                    │
│  │             │     │  └───────┘  │                                    │
│  └─────────────┘     └─────────────┘                                    │
└─────────────────────────────────────────────────────────────────────────┘
                              │
                              │ TCP Socket
                              │
                              ▼
┌─────────────────────────────────────────────────────────────────────────┐
│                              SERVER                                      │
│  ┌─────────────┐     ┌─────────────┐     ┌─────────────────────────┐   │
│  │    Main     │     │   Client    │     │      Data Storage       │   │
│  │   Thread    │     │   Thread    │     │                         │   │
│  │             │     │             │     │   ┌───────────────────┐ │   │
│  │  ┌───────┐  │     │  ┌───────┐  │     │   │      Users        │ │   │
│  │  │Accept │──┼────►│  │Handle │  │     │   ├───────────────────┤ │   │
│  │  │Connect│  │     │  │Request│◄─┼─────┼──►│     Sessions      │ │   │
│  │  └───────┘  │     │  └───────┘  │     │   ├───────────────────┤ │   │
│  │             │     │      │      │     │   │     Lessons       │ │   │
│  │             │     │      ▼      │     │   ├───────────────────┤ │   │
│  │             │     │  ┌───────┐  │     │   │      Tests        │ │   │
│  │             │     │  │ Send  │  │     │   ├───────────────────┤ │   │
│  │             │     │  │Response│ │     │   │   ChatMessages    │ │   │
│  │             │     │  └───────┘  │     │   └───────────────────┘ │   │
│  │             │     │      │      │     │                         │   │
│  │             │     │      ▼      │     │                         │   │
│  │             │     │  ┌───────┐  │     │                         │   │
│  │             │     │  │ Push  │──┼─────┼──► To other clients     │   │
│  │             │     │  │ Notif │  │     │                         │   │
│  │             │     │  └───────┘  │     │                         │   │
│  └─────────────┘     └─────────────┘     └─────────────────────────┘   │
└─────────────────────────────────────────────────────────────────────────┘
```

---

*Document created: December 2024*
*Version: 1.0*
