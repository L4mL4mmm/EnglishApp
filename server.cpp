/**
 * ============================================================================
 * ENGLISH LEARNING APP - SERVER (Enhanced Version)
 * ============================================================================
 * Server xử lý các request từ client theo giao thức đã thiết kế
 * Sử dụng POSIX socket, multithreading để xử lý nhiều client đồng thời
 *
 * Compile: g++ -std=c++17 -pthread -o server server.cpp
 * Run: ./server [port]
 * ============================================================================
 */

#include <iostream>
#include <string>
#include <cstring>
#include <map>
#include <vector>
#include <thread>
#include <mutex>
#include <chrono>
#include <sstream>
#include <algorithm>
#include <fstream>
#include <ctime>
#include <random>
#include <unordered_map>

// POSIX socket headers
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <signal.h>

// ============================================================================
// CẤU HÌNH SERVER
// ============================================================================
#define DEFAULT_PORT 8888
#define BUFFER_SIZE 65536
#define MAX_CLIENTS 100

// ============================================================================
// CORE DOMAIN MODELS (Refactored to include/core/)
// ============================================================================
#include "include/core/all.h"

// Type aliases for backward compatibility with existing code
using User = english_learning::core::User;
using Session = english_learning::core::Session;
using Lesson = english_learning::core::Lesson;
using TestQuestion = english_learning::core::TestQuestion;
using Test = english_learning::core::Test;
using ChatMessage = english_learning::core::ChatMessage;
using Exercise = english_learning::core::Exercise;
using ExerciseSubmission = english_learning::core::ExerciseSubmission;
using Game = english_learning::core::Game;
using GameSession = english_learning::core::GameSession;

// ============================================================================
// PROTOCOL LAYER (Refactored to include/protocol/)
// ============================================================================
#include "include/protocol/all.h"

// ============================================================================
// SERVICE LAYER (Refactored architecture)
// ============================================================================
#include "src/repository/bridge/bridge_repositories.h"
#include "src/repository/bridge/bridge_repositories_ext.h"
#include "src/service/all.h"

// Using declarations for protocol utilities
using english_learning::protocol::getJsonValue;
using english_learning::protocol::getJsonObject;
using english_learning::protocol::getJsonArray;
using english_learning::protocol::parseJsonArray;
using english_learning::protocol::escapeJson;
using english_learning::protocol::utils::getCurrentTimestamp;
using english_learning::protocol::utils::generateId;
using english_learning::protocol::utils::generateSessionToken;
namespace MessageType = english_learning::protocol::MessageType;

// ============================================================================
// BIẾN TOÀN CỤC VÀ MUTEX
// ============================================================================
std::map<std::string, User> users;              // email -> User (changed for easier lookup)
std::map<std::string, User*> userById;          // userId -> User*
std::map<std::string, Session> sessions;        // sessionToken -> Session
std::map<std::string, Lesson> lessons;          // lessonId -> Lesson
std::map<std::string, Test> tests;              // testId -> Test
std::map<std::string, Exercise> exercises;      // exerciseId -> Exercise
std::vector<ExerciseSubmission> exerciseSubmissions;  // Danh sách bài nộp
std::map<std::string, Game> games;              // gameId -> Game
std::map<std::string, GameSession> gameSessions;  // sessionId -> GameSession
std::vector<ChatMessage> chatMessages;          // Danh sách tin nhắn
std::map<int, std::string> clientSessions;      // socket -> sessionToken

// Voice Call type alias
using VoiceCallSession = english_learning::core::VoiceCallSession;
std::map<std::string, VoiceCallSession> voiceCalls;  // callId -> VoiceCallSession

std::mutex usersMutex;
std::mutex sessionsMutex;
std::mutex chatMutex;
std::mutex logMutex;
std::mutex exercisesMutex;
std::mutex gamesMutex;
std::mutex voiceCallMutex;

int serverSocket = -1;
bool running = true;

// ============================================================================
// SERVICE LAYER INTEGRATION
// ============================================================================
// Bridge repositories and service container (initialized in main())
// These wrap the global data structures above, enabling gradual migration
// to the service layer while maintaining backward compatibility.
namespace bridge = english_learning::repository::bridge;
namespace service = english_learning::service;

std::unique_ptr<service::ServiceContainer> serviceContainer;

// ============================================================================
// HÀM TIỆN ÍCH
// ============================================================================
bool isAdmin(const std::string& userId);
bool isTeacher(const std::string& userId);

// NOTE: getCurrentTimestamp(), generateId(), generateSessionToken()
// are now provided by include/protocol/utils.h

// Ghi log
void logMessage(const std::string& direction, const std::string& clientInfo, const std::string& message) {
    std::lock_guard<std::mutex> lock(logMutex);

    auto now = std::chrono::system_clock::now();
    auto time = std::chrono::system_clock::to_time_t(now);
    std::string timeStr = std::ctime(&time);
    timeStr.pop_back();

    std::cout << "[" << timeStr << "] " << direction << " " << clientInfo << ": ";

    if (message.length() > 200) {
        std::cout << message.substr(0, 200) << "..." << std::endl;
    } else {
        std::cout << message << std::endl;
    }

    std::ofstream logFile("server.log", std::ios::app);
    if (logFile.is_open()) {
        logFile << "[" << timeStr << "] " << direction << " " << clientInfo << ": " << message << std::endl;
        logFile.close();
    }
}

// NOTE: escapeJson(), getJsonValue(), getJsonObject(), getJsonArray(), parseJsonArray()
// are now provided by include/protocol/json_parser.h

// ============================================================================
// KHỞI TẠO DỮ LIỆU MẪU - PHONG PHÚ
// ============================================================================

void initSampleData() {
    // ========== TẠO USERS ==========

    // Teacher 1
    User teacher1;
    teacher1.userId = "teacher_001";
    teacher1.fullname = "Ms. Sarah Johnson";
    teacher1.email = "sarah@example.com";
    teacher1.password = "teacher123";
    teacher1.role = "teacher";
    teacher1.level = "advanced";
    teacher1.createdAt = getCurrentTimestamp();
    teacher1.online = false;
    teacher1.clientSocket = -1;
    users[teacher1.email] = teacher1;
    userById[teacher1.userId] = &users[teacher1.email];

    // Teacher 2
    User teacher2;
    teacher2.userId = "teacher_002";
    teacher2.fullname = "Mr. John Smith";
    teacher2.email = "john@example.com";
    teacher2.password = "teacher123";
    teacher2.role = "teacher";
    teacher2.level = "advanced";
    teacher2.createdAt = getCurrentTimestamp();
    teacher2.online = false;
    teacher2.clientSocket = -1;
    users[teacher2.email] = teacher2;
    userById[teacher2.userId] = &users[teacher2.email];

    // Student 1
    User student1;
    student1.userId = "student_001";
    student1.fullname = "Nguyen Van A";
    student1.email = "student@example.com";
    student1.password = "student123";
    student1.role = "student";
    student1.level = "beginner";
    student1.createdAt = getCurrentTimestamp();
    student1.online = false;
    student1.clientSocket = -1;
    users[student1.email] = student1;
    userById[student1.userId] = &users[student1.email];

    // Student 2
    User student2;
    student2.userId = "student_002";
    student2.fullname = "Tran Thi B";
    student2.email = "student2@example.com";
    student2.password = "student123";
    student2.role = "student";
    student2.level = "intermediate";
    student2.createdAt = getCurrentTimestamp();
    student2.online = false;
    student2.clientSocket = -1;
    users[student2.email] = student2;
    userById[student2.userId] = &users[student2.email];

    // Student 3
    User student3;
    student3.userId = "student_003";
    student3.fullname = "Le Van C";
    student3.email = "student3@example.com";
    student3.password = "student123";
    student3.role = "student";
    student3.level = "advanced";
    student3.createdAt = getCurrentTimestamp();
    student3.online = false;
    student3.clientSocket = -1;
    users[student3.email] = student3;
    userById[student3.userId] = &users[student3.email];

    // Admin user
    User admin;
    admin.userId = "admin_001";
    admin.fullname = "System Administrator";
    admin.email = "admin@example.com";
    admin.password = "admin123";
    admin.role = "admin";
    admin.level = "advanced";
    admin.createdAt = getCurrentTimestamp();
    admin.online = false;
    admin.clientSocket = -1;
    users[admin.email] = admin;
    userById[admin.userId] = &users[admin.email];

    // ========== TẠO BÀI HỌC - BEGINNER ==========

    // Lesson 1: Present Simple
    Lesson lesson1;
    lesson1.lessonId = "lesson_001";
    lesson1.title = "Present Simple Tense";
    lesson1.description = "Learn how to use present simple tense in English";
    lesson1.topic = "grammar";
    lesson1.level = "beginner";
    lesson1.duration = 30;
    lesson1.videoUrl = "https://www.youtube.com/watch?v=dQw4w9WgXcQ";  // Sample video
    lesson1.audioUrl = "/usr/share/sounds/freedesktop/stereo/complete.oga";  // Sample system audio
    lesson1.textContent = R"(
========================================
        PRESENT SIMPLE TENSE
========================================

1. USAGE (Cách dùng)
--------------------
The present simple tense is used to describe:
- Habits and routines (Thói quen)
- General truths and facts (Sự thật chung)
- Fixed schedules (Lịch trình cố định)

2. STRUCTURE (Cấu trúc)
-----------------------
(+) Affirmative: Subject + V(s/es)
    - I/You/We/They + V
    - He/She/It + V(s/es)

(-) Negative: Subject + do/does + not + V
    - I/You/We/They + do not (don't) + V
    - He/She/It + does not (doesn't) + V

(?) Question: Do/Does + Subject + V?
    - Do + I/you/we/they + V?
    - Does + he/she/it + V?

3. EXAMPLES (Ví dụ)
-------------------
(+) I work in an office.
    He works in an office.
    She plays tennis every Sunday.

(-) I don't work on Sundays.
    She doesn't like coffee.
    They don't speak French.

(?) Do you work here?
    Does he play football?
    Do they live in London?

4. TIME EXPRESSIONS (Trạng từ thời gian)
----------------------------------------
- always (luôn luôn)
- usually (thường)
- often (thường xuyên)
- sometimes (thỉnh thoảng)
- rarely (hiếm khi)
- never (không bao giờ)
- every day/week/month/year

5. SPELLING RULES (Quy tắc chính tả)
------------------------------------
For he/she/it:
- Most verbs: add -s (work -> works)
- Verbs ending in -s, -sh, -ch, -x, -o: add -es
  (watch -> watches, go -> goes)
- Verbs ending in consonant + y: change y to -ies
  (study -> studies, fly -> flies)

6. COMMON MISTAKES (Lỗi thường gặp)
-----------------------------------
X He don't like coffee.
V He doesn't like coffee.

X She work in a bank.
V She works in a bank.

X Do she speak English?
V Does she speak English?
)";
    lessons[lesson1.lessonId] = lesson1;

    // Lesson 2: Common Daily Vocabulary
    Lesson lesson2;
    lesson2.lessonId = "lesson_002";
    lesson2.title = "Common Daily Vocabulary";
    lesson2.description = "Essential vocabulary for daily conversation";
    lesson2.topic = "vocabulary";
    lesson2.level = "beginner";
    lesson2.duration = 25;
    lesson2.videoUrl = "";  // No video for this lesson
    lesson2.audioUrl = "/usr/share/sounds/freedesktop/stereo/message.oga";  // Audio pronunciation
    lesson2.textContent = R"(
========================================
     COMMON DAILY VOCABULARY
========================================

1. GREETINGS (Chào hỏi)
-----------------------
- Hello /həˈloʊ/ - Xin chào
- Hi /haɪ/ - Chào (thân mật)
- Good morning - Chào buổi sáng
- Good afternoon - Chào buổi chiều
- Good evening - Chào buổi tối
- Goodbye /ˌɡʊdˈbaɪ/ - Tạm biệt
- See you later - Hẹn gặp lại
- Good night - Chúc ngủ ngon

2. BASIC EXPRESSIONS (Câu giao tiếp cơ bản)
-------------------------------------------
- Thank you / Thanks - Cảm ơn
- You're welcome - Không có gì
- Please - Làm ơn / Xin vui lòng
- Sorry / Excuse me - Xin lỗi
- Yes - Vâng / Có
- No - Không

3. QUESTIONS (Câu hỏi)
----------------------
- How are you? - Bạn khỏe không?
- What's your name? - Tên bạn là gì?
- Where are you from? - Bạn đến từ đâu?
- What time is it? - Mấy giờ rồi?
- How old are you? - Bạn bao nhiêu tuổi?
- What do you do? - Bạn làm nghề gì?

4. NUMBERS (Số đếm)
-------------------
1 - one      6 - six
2 - two      7 - seven
3 - three    8 - eight
4 - four     9 - nine
5 - five     10 - ten

11 - eleven       20 - twenty
12 - twelve       30 - thirty
13 - thirteen     40 - forty
14 - fourteen     50 - fifty
15 - fifteen      100 - one hundred

5. COLORS (Màu sắc)
-------------------
- Red /red/ - Đỏ
- Blue /bluː/ - Xanh dương
- Green /ɡriːn/ - Xanh lá
- Yellow /ˈjeloʊ/ - Vàng
- Orange /ˈɔːrɪndʒ/ - Cam
- Purple /ˈpɜːrpl/ - Tím
- Pink /pɪŋk/ - Hồng
- White /waɪt/ - Trắng
- Black /blæk/ - Đen
- Brown /braʊn/ - Nâu
- Gray /ɡreɪ/ - Xám

6. DAYS OF THE WEEK (Các ngày trong tuần)
-----------------------------------------
- Monday - Thứ Hai
- Tuesday - Thứ Ba
- Wednesday - Thứ Tư
- Thursday - Thứ Năm
- Friday - Thứ Sáu
- Saturday - Thứ Bảy
- Sunday - Chủ Nhật

7. FAMILY MEMBERS (Thành viên gia đình)
---------------------------------------
- Father / Dad - Bố
- Mother / Mom - Mẹ
- Brother - Anh/Em trai
- Sister - Chị/Em gái
- Grandfather - Ông
- Grandmother - Bà
- Uncle - Chú/Bác/Cậu
- Aunt - Cô/Dì/Thím
)";
    lessons[lesson2.lessonId] = lesson2;

    // Lesson 3: Basic Listening Skills
    Lesson lesson3;
    lesson3.lessonId = "lesson_003";
    lesson3.title = "Introduction to Listening";
    lesson3.description = "Basic listening skills and strategies for beginners";
    lesson3.topic = "listening";
    lesson3.level = "beginner";
    lesson3.duration = 20;
    lesson3.videoUrl = "https://www.youtube.com/watch?v=JGwWNGJdvx8";  // English listening practice
    lesson3.audioUrl = "/usr/share/sounds/freedesktop/stereo/dialog-information.oga";
    lesson3.textContent = R"(
========================================
    INTRODUCTION TO LISTENING
========================================

1. WHY IS LISTENING IMPORTANT?
------------------------------
- Understanding native speakers
- Improving pronunciation
- Learning natural expressions
- Building confidence in communication

2. LISTENING STRATEGIES
-----------------------
a) Before listening:
   - Read the questions first
   - Predict what you might hear
   - Focus on key words

b) While listening:
   - Don't panic if you miss something
   - Focus on main ideas first
   - Listen for key words
   - Pay attention to intonation

c) After listening:
   - Check your answers
   - Listen again if possible
   - Note new vocabulary

3. COMMON LISTENING SITUATIONS
------------------------------
- Introducing yourself
- Asking for directions
- Ordering food
- Shopping
- Making phone calls

4. PRACTICE DIALOGUE
--------------------
A: Hello! My name is Tom. What's your name?
B: Hi Tom! I'm Lisa. Nice to meet you.
A: Nice to meet you too. Where are you from?
B: I'm from Vietnam. How about you?
A: I'm from the USA. Do you like it here?
B: Yes, I do. It's very nice!

5. KEY PHRASES TO LISTEN FOR
----------------------------
- "My name is..." (Tên tôi là...)
- "I'm from..." (Tôi đến từ...)
- "Nice to meet you" (Rất vui được gặp bạn)
- "How are you?" (Bạn khỏe không?)
- "I'm fine, thank you" (Tôi khỏe, cảm ơn)

6. TIPS FOR IMPROVEMENT
-----------------------
- Listen to English every day (5-10 minutes)
- Watch English videos with subtitles
- Listen to slow, clear recordings first
- Repeat what you hear
- Don't translate word by word
)";
    lessons[lesson3.lessonId] = lesson3;

    // ========== TẠO BÀI HỌC - INTERMEDIATE ==========

    // Lesson 4: Past Tenses
    Lesson lesson4;
    lesson4.lessonId = "lesson_004";
    lesson4.title = "Past Tenses";
    lesson4.description = "Master past simple and past continuous tenses";
    lesson4.topic = "grammar";
    lesson4.level = "intermediate";
    lesson4.duration = 45;
    lesson4.textContent = R"(
========================================
          PAST TENSES
========================================

1. PAST SIMPLE TENSE
--------------------
Used for completed actions in the past.

Structure:
(+) Subject + V2 (past form)
(-) Subject + did not (didn't) + V
(?) Did + Subject + V?

Regular verbs: add -ed
- work -> worked
- play -> played
- study -> studied

Irregular verbs (must memorize):
- go -> went
- see -> saw
- eat -> ate
- buy -> bought
- come -> came
- take -> took
- make -> made
- give -> gave
- find -> found
- know -> knew

Examples:
- I worked late yesterday.
- She didn't go to the party.
- Did you see the movie?

2. PAST CONTINUOUS TENSE
------------------------
Used for ongoing actions in the past.

Structure:
(+) Subject + was/were + V-ing
(-) Subject + was/were + not + V-ing
(?) Was/Were + Subject + V-ing?

Examples:
- I was studying at 8 PM.
- They were playing football.
- Was she working when you called?

3. PAST SIMPLE vs PAST CONTINUOUS
---------------------------------
Use Past Continuous for longer/background actions
Use Past Simple for shorter/interrupting actions

Example:
"I was cooking when the phone rang."
- was cooking = longer action (past continuous)
- rang = shorter, interrupting action (past simple)

More examples:
- While she was sleeping, someone knocked on the door.
- They were watching TV when the power went out.
- I met her while I was walking in the park.

4. TIME EXPRESSIONS
-------------------
Past Simple:
- yesterday
- last week/month/year
- two days ago
- in 2020

Past Continuous:
- at 8 o'clock yesterday
- this time last week
- while
- when

5. COMMON MISTAKES
------------------
X I was go to school yesterday.
V I went to school yesterday.

X She didn't went to the party.
V She didn't go to the party.

X When I was walking home, I was seeing a cat.
V When I was walking home, I saw a cat.
)";
    lessons[lesson4.lessonId] = lesson4;

    // Lesson 5: Business Vocabulary
    Lesson lesson5;
    lesson5.lessonId = "lesson_005";
    lesson5.title = "Business English Vocabulary";
    lesson5.description = "Essential vocabulary for the workplace";
    lesson5.topic = "vocabulary";
    lesson5.level = "intermediate";
    lesson5.duration = 35;
    lesson5.textContent = R"(
========================================
    BUSINESS ENGLISH VOCABULARY
========================================

1. OFFICE VOCABULARY
--------------------
- Meeting - Cuộc họp
- Deadline - Hạn chót
- Report - Báo cáo
- Presentation - Bài thuyết trình
- Conference - Hội nghị
- Department - Phòng ban
- Manager - Quản lý
- Employee - Nhân viên
- Colleague - Đồng nghiệp
- Client - Khách hàng

2. EMAIL EXPRESSIONS
--------------------
Opening:
- Dear Mr./Ms. [Name],
- Hello [Name],
- Good morning/afternoon,

Body:
- I am writing to inform you...
- Please find attached...
- I would like to request...
- Thank you for your email.
- Regarding your inquiry...

Closing:
- Best regards,
- Kind regards,
- Sincerely,
- Looking forward to hearing from you.

3. MEETING PHRASES
------------------
Starting:
- Let's get started.
- The purpose of this meeting is...
- Today we're going to discuss...

Opinions:
- In my opinion...
- I think/believe that...
- From my point of view...

Agreeing:
- I agree with you.
- That's a good point.
- Exactly!

Disagreeing:
- I see your point, but...
- I'm not sure I agree.
- I have a different opinion.

Ending:
- To summarize...
- Let's wrap up.
- Any final questions?

4. PHONE CONVERSATIONS
----------------------
Answering:
- Hello, [Company name], how may I help you?
- [Name] speaking.

Asking to speak:
- May I speak to [Name], please?
- Could you put me through to...?
- Is [Name] available?

Leaving a message:
- Could you take a message?
- Please tell him/her that...
- Could you ask him/her to call me back?

5. COMMON BUSINESS VERBS
------------------------
- negotiate - đàm phán
- collaborate - hợp tác
- implement - triển khai
- analyze - phân tích
- delegate - ủy quyền
- prioritize - ưu tiên
- schedule - lên lịch
- confirm - xác nhận
)";
    lessons[lesson5.lessonId] = lesson5;

    // ========== TẠO BÀI HỌC - ADVANCED ==========

    // Lesson 6: Advanced Grammar - Conditionals
    Lesson lesson6;
    lesson6.lessonId = "lesson_006";
    lesson6.title = "Conditional Sentences";
    lesson6.description = "Master all types of conditional sentences";
    lesson6.topic = "grammar";
    lesson6.level = "advanced";
    lesson6.duration = 50;
    lesson6.textContent = R"(
========================================
      CONDITIONAL SENTENCES
========================================

1. ZERO CONDITIONAL (Type 0)
----------------------------
Use: General truths, scientific facts

Structure: If + present simple, present simple

Examples:
- If you heat water to 100°C, it boils.
- If it rains, the grass gets wet.
- Plants die if they don't get water.

2. FIRST CONDITIONAL (Type 1)
-----------------------------
Use: Real/possible situations in the future

Structure: If + present simple, will + V

Examples:
- If it rains tomorrow, I will stay home.
- If you study hard, you will pass the exam.
- She will be angry if you don't call her.

3. SECOND CONDITIONAL (Type 2)
------------------------------
Use: Unreal/hypothetical situations now

Structure: If + past simple, would + V

Examples:
- If I won the lottery, I would buy a house.
- If I were you, I would accept the job.
- She would travel more if she had more money.

Note: Use "were" for all subjects (formal)
- If I were rich... (not "was")
- If she were here...

4. THIRD CONDITIONAL (Type 3)
-----------------------------
Use: Unreal situations in the past

Structure: If + past perfect, would have + V3

Examples:
- If I had studied harder, I would have passed.
- If she had left earlier, she wouldn't have missed the train.
- They would have won if they had practiced more.

5. MIXED CONDITIONALS
---------------------
Type 3 + Type 2 (Past -> Present):
If + past perfect, would + V
- If I had taken that job, I would be rich now.

Type 2 + Type 3 (Present -> Past):
If + past simple, would have + V3
- If I were braver, I would have asked her out.

6. ALTERNATIVE STRUCTURES
-------------------------
Unless = If...not
- Unless you hurry, you'll be late.
- (= If you don't hurry, you'll be late.)

Provided that / As long as
- I'll help you provided that you help me too.

In case
- Take an umbrella in case it rains.

7. COMMON MISTAKES
------------------
X If I will see him, I will tell him.
V If I see him, I will tell him.

X If I would have money, I would buy it.
V If I had money, I would buy it.

X If I would have known, I would have helped.
V If I had known, I would have helped.
)";
    lessons[lesson6.lessonId] = lesson6;

    // Lesson 7: IELTS Speaking
    Lesson lesson7;
    lesson7.lessonId = "lesson_007";
    lesson7.title = "IELTS Speaking Skills";
    lesson7.description = "Advanced speaking techniques for IELTS exam";
    lesson7.topic = "speaking";
    lesson7.level = "advanced";
    lesson7.duration = 60;
    lesson7.textContent = R"(
========================================
      IELTS SPEAKING SKILLS
========================================

1. PART 1: INTRODUCTION (4-5 minutes)
-------------------------------------
Topics: Home, work, studies, hobbies, etc.

Tips:
- Give extended answers (2-3 sentences)
- Don't memorize scripts
- Be natural and confident

Example:
Q: Do you work or study?
A: I'm currently working as a software developer
   at a tech company in Ho Chi Minh City. I've been
   in this position for about two years, and I find
   it quite challenging but rewarding.

2. PART 2: LONG TURN (3-4 minutes)
----------------------------------
You get a cue card with a topic.
1 minute to prepare, 2 minutes to speak.

Structure:
- Introduction: What it is
- Description: Details
- Explanation: Why/How
- Conclusion: Your feelings/opinions

Example topic: Describe a book you enjoyed reading.
- What book it was
- When you read it
- What it was about
- Why you enjoyed it

3. PART 3: DISCUSSION (4-5 minutes)
-----------------------------------
Abstract questions related to Part 2.

Tips:
- Express and justify opinions
- Give examples
- Consider different perspectives
- Use advanced vocabulary

4. USEFUL PHRASES
-----------------
Giving opinions:
- From my perspective...
- As far as I'm concerned...
- I would argue that...
- In my view...

Explaining:
- The main reason is that...
- This is primarily because...
- What I mean by that is...

Giving examples:
- For instance...
- A good example would be...
- To illustrate this point...

Contrasting:
- On the other hand...
- Having said that...
- Nevertheless...

5. FLUENCY TECHNIQUES
---------------------
- Use fillers naturally: "Well...", "Let me think..."
- Paraphrase if you forget a word
- Self-correct naturally
- Maintain eye contact
- Speak at a natural pace

6. VOCABULARY ENHANCEMENT
-------------------------
Instead of "good" -> excellent, outstanding, remarkable
Instead of "bad" -> terrible, dreadful, appalling
Instead of "big" -> enormous, massive, substantial
Instead of "small" -> tiny, minute, negligible
Instead of "important" -> crucial, vital, significant
)";
    lessons[lesson7.lessonId] = lesson7;

    // ========== TẠO BÀI TEST ==========

    // Test 1: Beginner Grammar
    Test test1;
    test1.testId = "test_001";
    test1.testType = "mixed";
    test1.level = "beginner";
    test1.topic = "grammar";
    test1.title = "Present Simple Tense Test";

    TestQuestion q1;
    q1.questionId = "q_001";
    q1.type = "multiple_choice";
    q1.question = "She ____ to school every day.";
    q1.options = {"go", "goes", "going", "went"};
    q1.correctAnswer = "b";
    q1.points = 10;
    test1.questions.push_back(q1);

    TestQuestion q2;
    q2.questionId = "q_002";
    q2.type = "multiple_choice";
    q2.question = "They ____ like spicy food.";
    q2.options = {"doesn't", "don't", "isn't", "aren't"};
    q2.correctAnswer = "b";
    q2.points = 10;
    test1.questions.push_back(q2);

    TestQuestion q3;
    q3.questionId = "q_003";
    q3.type = "fill_blank";
    q3.question = "I ____ English every day. (study)";
    q3.correctAnswer = "study";
    q3.points = 10;
    test1.questions.push_back(q3);

    TestQuestion q4;
    q4.questionId = "q_004";
    q4.type = "multiple_choice";
    q4.question = "Choose the correct sentence:";
    q4.options = {"He don't like coffee", "He doesn't likes coffee", "He doesn't like coffee", "He not like coffee"};
    q4.correctAnswer = "c";
    q4.points = 10;
    test1.questions.push_back(q4);

    TestQuestion q5;
    q5.questionId = "q_005";
    q5.type = "fill_blank";
    q5.question = "My mother ____ (cook) dinner every evening.";
    q5.correctAnswer = "cooks";
    q5.points = 10;
    test1.questions.push_back(q5);

    TestQuestion q6;
    q6.questionId = "q_006";
    q6.type = "multiple_choice";
    q6.question = "____ your brother work here?";
    q6.options = {"Do", "Does", "Is", "Are"};
    q6.correctAnswer = "b";
    q6.points = 10;
    test1.questions.push_back(q6);

    TestQuestion q7;
    q7.questionId = "q_007";
    q7.type = "multiple_choice";
    q7.question = "Water ____ at 100 degrees Celsius.";
    q7.options = {"boil", "boils", "boiling", "boiled"};
    q7.correctAnswer = "b";
    q7.points = 10;
    test1.questions.push_back(q7);

    TestQuestion q8;
    q8.questionId = "q_008";
    q8.type = "fill_blank";
    q8.question = "She always ____ (arrive) on time.";
    q8.correctAnswer = "arrives";
    q8.points = 10;
    test1.questions.push_back(q8);

    TestQuestion q9;
    q9.questionId = "q_009";
    q9.type = "sentence_order";
    q9.question = "Arrange the words to make a correct sentence:";
    q9.words = {"goes", "to", "school", "every", "day", "She"};
    q9.correctAnswer = "She goes to school every day";
    q9.points = 15;
    test1.questions.push_back(q9);

    tests[test1.testId] = test1;

    // Test 2: Intermediate Grammar
    Test test2;
    test2.testId = "test_002";
    test2.testType = "mixed";
    test2.level = "intermediate";
    test2.topic = "grammar";
    test2.title = "Past Tenses Test";

    TestQuestion q2_1;
    q2_1.questionId = "q2_001";
    q2_1.type = "multiple_choice";
    q2_1.question = "I ____ to the cinema yesterday.";
    q2_1.options = {"go", "went", "gone", "going"};
    q2_1.correctAnswer = "b";
    q2_1.points = 10;
    test2.questions.push_back(q2_1);

    TestQuestion q2_2;
    q2_2.questionId = "q2_002";
    q2_2.type = "multiple_choice";
    q2_2.question = "While I ____ TV, the phone rang.";
    q2_2.options = {"watch", "watched", "was watching", "am watching"};
    q2_2.correctAnswer = "c";
    q2_2.points = 10;
    test2.questions.push_back(q2_2);

    TestQuestion q2_3;
    q2_3.questionId = "q2_003";
    q2_3.type = "fill_blank";
    q2_3.question = "She ____ (not/come) to the party last night.";
    q2_3.correctAnswer = "didn't come";
    q2_3.points = 10;
    test2.questions.push_back(q2_3);

    TestQuestion q2_4;
    q2_4.questionId = "q2_004";
    q2_4.type = "multiple_choice";
    q2_4.question = "They ____ football when it started to rain.";
    q2_4.options = {"played", "play", "were playing", "are playing"};
    q2_4.correctAnswer = "c";
    q2_4.points = 10;
    test2.questions.push_back(q2_4);

    TestQuestion q2_5;
    q2_5.questionId = "q2_005";
    q2_5.type = "fill_blank";
    q2_5.question = "What ____ you ____ (do) at 8pm yesterday?";
    q2_5.correctAnswer = "were doing";
    q2_5.points = 10;
    test2.questions.push_back(q2_5);

    TestQuestion q2_6;
    q2_6.questionId = "q2_006";
    q2_6.type = "sentence_order";
    q2_6.question = "Arrange the words to make a correct sentence:";
    q2_6.words = {"was", "I", "when", "cooking", "rang", "the", "phone"};
    q2_6.correctAnswer = "I was cooking when the phone rang";
    q2_6.points = 15;
    test2.questions.push_back(q2_6);

    tests[test2.testId] = test2;

    // Test 3: Advanced Grammar - Conditionals
    Test test3;
    test3.testId = "test_003";
    test3.testType = "mixed";
    test3.level = "advanced";
    test3.topic = "grammar";
    test3.title = "Conditional Sentences Test";

    TestQuestion q3_1;
    q3_1.questionId = "q3_001";
    q3_1.type = "multiple_choice";
    q3_1.question = "If I ____ rich, I would buy a big house.";
    q3_1.options = {"am", "was", "were", "will be"};
    q3_1.correctAnswer = "c";
    q3_1.points = 10;
    test3.questions.push_back(q3_1);

    TestQuestion q3_2;
    q3_2.questionId = "q3_002";
    q3_2.type = "multiple_choice";
    q3_2.question = "If you heat ice, it ____.";
    q3_2.options = {"melts", "will melt", "would melt", "melted"};
    q3_2.correctAnswer = "a";
    q3_2.points = 10;
    test3.questions.push_back(q3_2);

    TestQuestion q3_3;
    q3_3.questionId = "q3_003";
    q3_3.type = "fill_blank";
    q3_3.question = "If I had studied harder, I ____ (pass) the exam.";
    q3_3.correctAnswer = "would have passed";
    q3_3.points = 15;
    test3.questions.push_back(q3_3);

    TestQuestion q3_4;
    q3_4.questionId = "q3_004";
    q3_4.type = "multiple_choice";
    q3_4.question = "If it rains tomorrow, we ____ the picnic.";
    q3_4.options = {"cancel", "will cancel", "would cancel", "cancelled"};
    q3_4.correctAnswer = "b";
    q3_4.points = 10;
    test3.questions.push_back(q3_4);

    TestQuestion q3_5;
    q3_5.questionId = "q3_005";
    q3_5.type = "fill_blank";
    q3_5.question = "I wish I ____ (know) the answer.";
    q3_5.correctAnswer = "knew";
    q3_5.points = 15;
    test3.questions.push_back(q3_5);

    tests[test3.testId] = test3;

    // ========== TẠO BÀI TẬP ==========

    // Exercise 1: Sentence Rewrite - Passive Voice
    Exercise ex1;
    ex1.exerciseId = "ex_001";
    ex1.exerciseType = "sentence_rewrite";
    ex1.title = "Rewrite Sentences in Passive Voice";
    ex1.description = "Practice converting active sentences to passive voice";
    ex1.instructions = "Rewrite the following sentences in passive voice. Make sure to use the correct verb forms.";
    ex1.level = "intermediate";
    ex1.topic = "grammar";
    ex1.prompts = {
        "People speak English all over the world.",
        "The teacher corrected the homework.",
        "They built this house in 2020.",
        "Someone stole my bicycle yesterday."
    };
    ex1.duration = 20;
    exercises[ex1.exerciseId] = ex1;

    // Exercise 2: Paragraph Writing
    Exercise ex2;
    ex2.exerciseId = "ex_002";
    ex2.exerciseType = "paragraph_writing";
    ex2.title = "Write About Your Daily Routine";
    ex2.description = "Practice writing descriptive paragraphs";
    ex2.instructions = "Write a paragraph (150-200 words) describing your daily routine. Include what you do from morning to evening.";
    ex2.level = "beginner";
    ex2.topic = "writing";
    ex2.topicDescription = "Describe your typical day from when you wake up until you go to bed.";
    ex2.requirements = {
        "Use present simple tense",
        "Include at least 5 activities",
        "Use time expressions (in the morning, at noon, etc.)",
        "Proper paragraph structure"
    };
    ex2.duration = 30;
    exercises[ex2.exerciseId] = ex2;

    // Exercise 3: Topic Speaking
    Exercise ex3;
    ex3.exerciseId = "ex_003";
    ex3.exerciseType = "topic_speaking";
    ex3.title = "Speak About Environmental Issues";
    ex3.description = "Practice speaking on important topics";
    ex3.instructions = "Record yourself speaking about environmental issues for 2-3 minutes. Discuss causes, effects, and solutions.";
    ex3.level = "advanced";
    ex3.topic = "speaking";
    ex3.topicDescription = "Environmental issues are becoming more serious every day. Discuss:\n- Main environmental problems\n- Their causes\n- Possible solutions\n- What individuals can do";
    ex3.duration = 5;
    exercises[ex3.exerciseId] = ex3;

    // Exercise 4: Sentence Rewrite - Reported Speech
    Exercise ex4;
    ex4.exerciseId = "ex_004";
    ex4.exerciseType = "sentence_rewrite";
    ex4.title = "Rewrite Sentences in Reported Speech";
    ex4.description = "Practice converting direct speech to reported speech";
    ex4.instructions = "Rewrite the following sentences in reported speech. Change pronouns and verb tenses appropriately.";
    ex4.level = "intermediate";
    ex4.topic = "grammar";
    ex4.prompts = {
        "She said: 'I am studying English.'",
        "He asked: 'Where do you live?'",
        "They told me: 'We will come tomorrow.'",
        "I said: 'I have finished my homework.'"
    };
    ex4.duration = 25;
    exercises[ex4.exerciseId] = ex4;

    // ========== TẠO TRÒ CHƠI ==========

    // Game 1: Word Matching - Daily Vocabulary
    Game game1;
    game1.gameId = "game_001";
    game1.gameType = "word_match";
    game1.title = "Daily Vocabulary Matching";
    game1.description = "Match English words with Vietnamese meanings";
    game1.level = "beginner";
    game1.topic = "vocabulary";
    game1.pairs = {
        {"Hello", "Xin chào"},
        {"Thank you", "Cảm ơn"},
        {"Good morning", "Chào buổi sáng"},
        {"Please", "Làm ơn"},
        {"Sorry", "Xin lỗi"},
        {"Yes", "Vâng"},
        {"No", "Không"},
        {"Water", "Nước"}
    };
    game1.timeLimit = 120;
    game1.maxScore = 100;
    games[game1.gameId] = game1;

    // Game 2: Word Matching - Intermediate
    Game game2;
    game2.gameId = "game_002";
    game2.gameType = "word_match";
    game2.title = "Business Vocabulary Matching";
    game2.description = "Match business terms with definitions";
    game2.level = "intermediate";
    game2.topic = "vocabulary";
    game2.pairs = {
        {"Meeting", "Cuộc họp"},
        {"Deadline", "Hạn chót"},
        {"Report", "Báo cáo"},
        {"Manager", "Quản lý"},
        {"Employee", "Nhân viên"},
        {"Client", "Khách hàng"},
        {"Contract", "Hợp đồng"},
        {"Budget", "Ngân sách"}
    };
    game2.timeLimit = 150;
    game2.maxScore = 100;
    games[game2.gameId] = game2;

    // Game 3: Sentence Matching
    Game game3;
    game3.gameId = "game_003";
    game3.gameType = "sentence_match";
    game3.title = "Question-Answer Matching";
    game3.description = "Match questions with correct answers";
    game3.level = "beginner";
    game3.topic = "grammar";
    game3.sentencePairs = {
        {"What's your name?", "My name is John."},
        {"How are you?", "I'm fine, thank you."},
        {"Where are you from?", "I'm from Vietnam."},
        {"How old are you?", "I'm 25 years old."},
        {"What do you do?", "I'm a student."},
        {"Do you like coffee?", "Yes, I do."}
    };
    game3.timeLimit = 180;
    game3.maxScore = 100;
    games[game3.gameId] = game3;

    // Game 4: Picture Matching (using text descriptions as placeholder)
    Game game4;
    game4.gameId = "game_004";
    game4.gameType = "picture_match";
    game4.title = "Picture-Word Matching";
    game4.description = "Match words with pictures";
    game4.level = "beginner";
    game4.topic = "vocabulary";
    game4.picturePairs = {
        {"Apple", "https://example.com/images/apple.jpg"},
        {"Cat", "https://example.com/images/cat.jpg"},
        {"Book", "https://example.com/images/book.jpg"},
        {"Car", "https://example.com/images/car.jpg"},
        {"House", "https://example.com/images/house.jpg"},
        {"Tree", "https://example.com/images/tree.jpg"}
    };
    game4.timeLimit = 150;
    game4.maxScore = 100;
    games[game4.gameId] = game4;

    std::cout << "[INFO] Sample data initialized: "
              << users.size() << " users, "
              << lessons.size() << " lessons, "
              << tests.size() << " tests, "
              << exercises.size() << " exercises, "
              << games.size() << " games" << std::endl;
}

// ============================================================================
// XỬ LÝ CÁC LOẠI REQUEST
// ============================================================================

// Xử lý REGISTER_REQUEST
std::string handleRegister(const std::string& json) {
    std::string payload = getJsonObject(json, "payload");
    std::string messageId = getJsonValue(json, "messageId");
    std::string fullname = getJsonValue(payload, "fullname");
    std::string email = getJsonValue(payload, "email");
    std::string password = getJsonValue(payload, "password");
    std::string confirmPassword = getJsonValue(payload, "confirmPassword");

    if (password != confirmPassword) {
        return R"({"messageType":"REGISTER_RESPONSE","messageId":")" + messageId +
               R"(","timestamp":)" + std::to_string(getCurrentTimestamp()) +
               R"(,"payload":{"status":"error","message":"Passwords do not match"}})";
    }

    {
        std::lock_guard<std::mutex> lock(usersMutex);
        if (users.find(email) != users.end()) {
            return R"({"messageType":"REGISTER_RESPONSE","messageId":")" + messageId +
                   R"(","timestamp":)" + std::to_string(getCurrentTimestamp()) +
                   R"(,"payload":{"status":"error","message":"Email already exists"}})";
        }

        User newUser;
        newUser.userId = generateId("user");
        newUser.fullname = fullname;
        newUser.email = email;
        newUser.password = password;
        newUser.role = "student";
        newUser.level = "beginner";
        newUser.createdAt = getCurrentTimestamp();
        newUser.online = false;
        newUser.clientSocket = -1;

        users[email] = newUser;
        userById[newUser.userId] = &users[email];

        return R"({"messageType":"REGISTER_RESPONSE","messageId":")" + messageId +
               R"(","timestamp":)" + std::to_string(getCurrentTimestamp()) +
               R"(,"payload":{"status":"success","message":"Register successfully","data":{"userId":")" +
               newUser.userId + R"(","fullname":")" + escapeJson(newUser.fullname) +
               R"(","email":")" + newUser.email + R"(","createdAt":)" + std::to_string(newUser.createdAt) + R"(}}})";
    }
}

// Gửi thông báo tin nhắn chưa đọc khi user login
void sendUnreadMessagesNotification(int clientSocket, const std::string& userId) {
    std::vector<ChatMessage> unreadMessages;

    {
        std::lock_guard<std::mutex> lock(chatMutex);
        for (auto& msg : chatMessages) {
            if (msg.recipientId == userId && !msg.read) {
                unreadMessages.push_back(msg);
            }
        }
    }

    if (unreadMessages.empty()) return;

    // Tạo danh sách tin nhắn chưa đọc với tên người gửi
    std::stringstream messagesJson;
    messagesJson << "[";
    bool first = true;

    std::lock_guard<std::mutex> userLock(usersMutex);
    for (const auto& msg : unreadMessages) {
        std::string senderName = "Unknown";
        auto senderIt = userById.find(msg.senderId);
        if (senderIt != userById.end()) {
            senderName = senderIt->second->fullname;
        }

        if (!first) messagesJson << ",";
        first = false;

        messagesJson << R"({"messageId":")" << msg.messageId
                     << R"(","senderId":")" << msg.senderId
                     << R"(","senderName":")" << escapeJson(senderName)
                     << R"(","content":")" << escapeJson(msg.content)
                     << R"(","timestamp":)" << msg.timestamp << "}";
    }
    messagesJson << "]";

    std::string notification = R"({"messageType":"UNREAD_MESSAGES_NOTIFICATION","messageId":")" +
                               generateId("notif") +
                               R"(","timestamp":)" + std::to_string(getCurrentTimestamp()) +
                               R"(,"payload":{"unreadCount":)" + std::to_string(unreadMessages.size()) +
                               R"(,"messages":)" + messagesJson.str() + R"(}})";

    uint32_t len = htonl(notification.length());
    send(clientSocket, &len, sizeof(len), 0);
    send(clientSocket, notification.c_str(), notification.length(), 0);

    logMessage("SEND", "Client:" + std::to_string(clientSocket), "UNREAD_MESSAGES_NOTIFICATION");
}

// Xử lý LOGIN_REQUEST
std::string handleLogin(const std::string& json, int clientSocket) {
    std::string payload = getJsonObject(json, "payload");
    std::string messageId = getJsonValue(json, "messageId");
    std::string email = getJsonValue(payload, "email");
    std::string password = getJsonValue(payload, "password");

    std::string userId;
    std::string response;

    {
        std::lock_guard<std::mutex> lock(usersMutex);

        auto it = users.find(email);
        if (it == users.end() || it->second.password != password) {
            return R"({"messageType":"LOGIN_RESPONSE","messageId":")" + messageId +
                   R"(","timestamp":)" + std::to_string(getCurrentTimestamp()) +
                   R"(,"payload":{"status":"error","message":"Invalid email or password"}})";
        }

        User& user = it->second;
        user.online = true;
        user.clientSocket = clientSocket;
        userId = user.userId;

        std::string sessionToken = generateSessionToken();
        Session session;
        session.sessionToken = sessionToken;
        session.userId = user.userId;
        session.expiresAt = getCurrentTimestamp() + 3600000;

        {
            std::lock_guard<std::mutex> slock(sessionsMutex);
            sessions[sessionToken] = session;
            clientSessions[clientSocket] = sessionToken;
        }

        response = R"({"messageType":"LOGIN_RESPONSE","messageId":")" + messageId +
               R"(","timestamp":)" + std::to_string(getCurrentTimestamp()) +
               R"(,"payload":{"status":"success","message":"Login successfully","data":{"userId":")" +
               user.userId + R"(","fullname":")" + escapeJson(user.fullname) + R"(","email":")" +
               user.email + R"(","level":")" + user.level + R"(","sessionToken":")" +
               sessionToken + R"(","expiresAt":)" + std::to_string(session.expiresAt) + R"(}}})";
    }

    // Gửi response trước
    uint32_t respLen = htonl(response.length());
    send(clientSocket, &respLen, sizeof(respLen), 0);
    send(clientSocket, response.c_str(), response.length(), 0);
    logMessage("SEND", "Client:" + std::to_string(clientSocket), response);

    // Sau đó gửi thông báo tin nhắn chưa đọc (nếu có)
    sendUnreadMessagesNotification(clientSocket, userId);

    // Trả về chuỗi rỗng để báo hiệu đã xử lý response
    return "";
}

// ============================================================================
// SERVICE LAYER DEMONSTRATION - handleLoginV2
// ============================================================================
// This is a demonstration of how handlers can be migrated to use the service
// layer instead of directly manipulating global data structures.
//
// Benefits:
// - Business logic is encapsulated in AuthService
// - Handler only deals with JSON parsing and response formatting
// - Easier to unit test (service can be mocked)
// - Thread safety handled by bridge repositories
// ============================================================================
std::string handleLoginV2(const std::string& json, int clientSocket) {
    std::string payload = getJsonObject(json, "payload");
    std::string messageId = getJsonValue(json, "messageId");
    std::string email = getJsonValue(payload, "email");
    std::string password = getJsonValue(payload, "password");

    // Use the service layer for authentication
    auto result = serviceContainer->auth().login(email, password, clientSocket);

    std::string response;
    if (!result.isSuccess()) {
        // Error case - service provides the error message
        response = R"({"messageType":"LOGIN_RESPONSE","messageId":")" + messageId +
                   R"(","timestamp":)" + std::to_string(getCurrentTimestamp()) +
                   R"(,"payload":{"status":"error","message":")" + escapeJson(result.getMessage()) + R"("}})";
    } else {
        // Success case - build response from LoginResult DTO
        const auto& data = result.getData();
        response = R"({"messageType":"LOGIN_RESPONSE","messageId":")" + messageId +
                   R"(","timestamp":)" + std::to_string(getCurrentTimestamp()) +
                   R"(,"payload":{"status":"success","message":"Login successfully","data":{"userId":")" +
                   data.userId + R"(","fullname":")" + escapeJson(data.fullname) + R"(","email":")" +
                   data.email + R"(","level":")" + data.level + R"(","sessionToken":")" +
                   data.sessionToken + R"(","expiresAt":)" + std::to_string(data.expiresAt) + R"(}}})";
    }

    // Send response
    uint32_t respLen = htonl(response.length());
    send(clientSocket, &respLen, sizeof(respLen), 0);
    send(clientSocket, response.c_str(), response.length(), 0);
    logMessage("SEND", "Client:" + std::to_string(clientSocket), response);

    // Send unread messages notification if login successful
    if (result.isSuccess()) {
        sendUnreadMessagesNotification(clientSocket, result.getData().userId);
    }

    return "";  // Response already sent
}

// Kiểm tra session hợp lệ
std::string validateSession(const std::string& sessionToken) {
    std::lock_guard<std::mutex> lock(sessionsMutex);
    auto it = sessions.find(sessionToken);
    if (it == sessions.end()) return "";
    if (it->second.expiresAt < getCurrentTimestamp()) {
        sessions.erase(it);
        return "";
    }
    return it->second.userId;
}

// Xử lý GET_LESSONS_REQUEST
std::string handleGetLessons(const std::string& json) {
    std::string payload = getJsonObject(json, "payload");
    std::string messageId = getJsonValue(json, "messageId");
    std::string sessionToken = getJsonValue(json, "sessionToken");
    std::string level = getJsonValue(payload, "level");
    std::string topic = getJsonValue(payload, "topic");

    std::string userId = validateSession(sessionToken);
    if (userId.empty()) {
        return R"({"messageType":"GET_LESSONS_RESPONSE","messageId":")" + messageId +
               R"(","timestamp":)" + std::to_string(getCurrentTimestamp()) +
               R"(,"payload":{"status":"error","message":"Invalid or expired session"}})";
    }

    std::stringstream lessonsJson;
    lessonsJson << "[";
    bool first = true;
    int count = 0;

    for (const auto& pair : lessons) {
        const Lesson& lesson = pair.second;

        // Lọc theo topic nếu có
        if (!topic.empty() && lesson.topic != topic) continue;
        // Lọc theo level nếu có
        if (!level.empty() && lesson.level != level) continue;

        if (!first) lessonsJson << ",";
        first = false;

        lessonsJson << R"({"lessonId":")" << lesson.lessonId
                    << R"(","title":")" << escapeJson(lesson.title)
                    << R"(","description":")" << escapeJson(lesson.description)
                    << R"(","topic":")" << lesson.topic
                    << R"(","level":")" << lesson.level
                    << R"(","duration":)" << lesson.duration
                    << R"(,"completionStatus":false,"progress":0})";
        count++;
    }
    lessonsJson << "]";

    return R"({"messageType":"GET_LESSONS_RESPONSE","messageId":")" + messageId +
           R"(","timestamp":)" + std::to_string(getCurrentTimestamp()) +
           R"(,"payload":{"status":"success","message":"Retrieved lessons successfully","data":{"lessons":)" +
           lessonsJson.str() + R"(,"pagination":{"currentPage":1,"totalPages":1,"totalLessons":)" +
           std::to_string(count) + R"(}}}})";
}

// Xử lý GET_LESSON_DETAIL_REQUEST
std::string handleGetLessonDetail(const std::string& json) {
    std::string payload = getJsonObject(json, "payload");
    std::string messageId = getJsonValue(json, "messageId");
    std::string sessionToken = getJsonValue(json, "sessionToken");
    std::string lessonId = getJsonValue(payload, "lessonId");

    std::string userId = validateSession(sessionToken);
    if (userId.empty()) {
        return R"({"messageType":"GET_LESSON_DETAIL_RESPONSE","messageId":")" + messageId +
               R"(","timestamp":)" + std::to_string(getCurrentTimestamp()) +
               R"(,"payload":{"status":"error","message":"Invalid or expired session"}})";
    }

    auto it = lessons.find(lessonId);
    if (it == lessons.end()) {
        return R"({"messageType":"GET_LESSON_DETAIL_RESPONSE","messageId":")" + messageId +
               R"(","timestamp":)" + std::to_string(getCurrentTimestamp()) +
               R"(,"payload":{"status":"error","message":"Lesson not found"}})";
    }

    const Lesson& lesson = it->second;

    return R"({"messageType":"GET_LESSON_DETAIL_RESPONSE","messageId":")" + messageId +
           R"(","timestamp":)" + std::to_string(getCurrentTimestamp()) +
           R"(,"payload":{"status":"success","data":{"lessonId":")" + lesson.lessonId +
           R"(","title":")" + escapeJson(lesson.title) +
           R"(","description":")" + escapeJson(lesson.description) +
           R"(","level":")" + lesson.level +
           R"(","topic":")" + lesson.topic +
           R"(","duration":)" + std::to_string(lesson.duration) +
           R"(,"content":")" + escapeJson(lesson.textContent) +
           R"(","textContent":")" + escapeJson(lesson.textContent) +
           R"(","videoUrl":")" + escapeJson(lesson.videoUrl) +
           R"(","audioUrl":")" + escapeJson(lesson.audioUrl) + R"("}}})";
}

// Xử lý GET_TEST_REQUEST
std::string handleGetTest(const std::string& json) {
    std::string payload = getJsonObject(json, "payload");
    std::string messageId = getJsonValue(json, "messageId");
    std::string sessionToken = getJsonValue(json, "sessionToken");
    std::string level = getJsonValue(payload, "level");

    std::string userId = validateSession(sessionToken);
    if (userId.empty()) {
        return R"({"messageType":"GET_TEST_RESPONSE","messageId":")" + messageId +
               R"(","timestamp":)" + std::to_string(getCurrentTimestamp()) +
               R"(,"payload":{"status":"error","message":"Invalid or expired session"}})";
    }

    // Tìm test phù hợp với level
    const Test* selectedTest = nullptr;
    for (const auto& pair : tests) {
        if (pair.second.level == level) {
            selectedTest = &pair.second;
            break;
        }
    }

    // Nếu không tìm thấy, lấy test đầu tiên
    if (!selectedTest && !tests.empty()) {
        selectedTest = &tests.begin()->second;
    }

    if (!selectedTest) {
        return R"({"messageType":"GET_TEST_RESPONSE","messageId":")" + messageId +
               R"(","timestamp":)" + std::to_string(getCurrentTimestamp()) +
               R"(,"payload":{"status":"error","message":"No tests available"}})";
    }

    std::stringstream questionsJson;
    questionsJson << "[";
    for (size_t i = 0; i < selectedTest->questions.size(); i++) {
        const TestQuestion& q = selectedTest->questions[i];
        if (i > 0) questionsJson << ",";

        questionsJson << R"({"questionId":")" << q.questionId
                      << R"(","type":")" << q.type
                      << R"(","order":)" << (i + 1)
                      << R"(,"question":")" << escapeJson(q.question)
                      << R"(","points":)" << q.points;

        if (!q.options.empty()) {
            questionsJson << R"(,"options":[)";
            for (size_t j = 0; j < q.options.size(); j++) {
                if (j > 0) questionsJson << ",";
                char optionId = 'a' + j;
                questionsJson << R"({"id":")" << optionId << R"(","text":")" << escapeJson(q.options[j]) << R"("})";
            }
            questionsJson << "]";
        }

        if (q.type == "sentence_order" && !q.words.empty()) {
            questionsJson << R"(,"words":[)";
            for (size_t j = 0; j < q.words.size(); j++) {
                if (j > 0) questionsJson << ",";
                questionsJson << R"(")" << escapeJson(q.words[j]) << R"(")";
            }
            questionsJson << "]";
        }
        questionsJson << "}";
    }
    questionsJson << "]";

    return R"({"messageType":"GET_TEST_RESPONSE","messageId":")" + messageId +
           R"(","timestamp":)" + std::to_string(getCurrentTimestamp()) +
           R"(,"payload":{"status":"success","data":{"testId":")" + selectedTest->testId +
           R"(","testType":")" + selectedTest->testType +
           R"(","level":")" + selectedTest->level +
           R"(","topic":")" + selectedTest->topic +
           R"(","title":")" + escapeJson(selectedTest->title) +
           R"(","duration":1800,"totalQuestions":)" + std::to_string(selectedTest->questions.size()) +
           R"(,"passingScore":60,"questions":)" + questionsJson.str() +
           R"(,"instructions":"Read each question carefully. Answer all questions."}}})";
}

// Xử lý SUBMIT_TEST_REQUEST
std::string handleSubmitTest(const std::string& json) {
    std::string payload = getJsonObject(json, "payload");
    std::string messageId = getJsonValue(json, "messageId");
    std::string sessionToken = getJsonValue(json, "sessionToken");
    std::string testId = getJsonValue(payload, "testId");

    std::string userId = validateSession(sessionToken);
    if (userId.empty()) {
        return R"({"messageType":"SUBMIT_TEST_RESPONSE","messageId":")" + messageId +
               R"(","timestamp":)" + std::to_string(getCurrentTimestamp()) +
               R"(,"payload":{"status":"error","message":"Invalid or expired session"}})";
    }

    auto it = tests.find(testId);
    if (it == tests.end()) {
        return R"({"messageType":"SUBMIT_TEST_RESPONSE","messageId":")" + messageId +
               R"(","timestamp":)" + std::to_string(getCurrentTimestamp()) +
               R"(,"payload":{"status":"error","message":"Test not found"}})";
    }

    const Test& test = it->second;
    std::string answersArray = getJsonArray(json, "answers");

    int totalPoints = 0;
    int earnedPoints = 0;
    int correctCount = 0;
    int wrongCount = 0;

    std::stringstream detailedResults;
    detailedResults << "[";

    for (size_t i = 0; i < test.questions.size(); i++) {
        const TestQuestion& q = test.questions[i];
        totalPoints += q.points;

        // Tìm câu trả lời của user cho câu hỏi này
        std::string userAnswer = "";
        size_t qPos = answersArray.find(q.questionId);
        if (qPos != std::string::npos) {
            size_t answerPos = answersArray.find("\"answer\"", qPos);
            if (answerPos != std::string::npos) {
                size_t colonPos = answersArray.find(':', answerPos);
                size_t valueStart = answersArray.find('"', colonPos);
                size_t valueEnd = answersArray.find('"', valueStart + 1);
                if (valueStart != std::string::npos && valueEnd != std::string::npos) {
                    userAnswer = answersArray.substr(valueStart + 1, valueEnd - valueStart - 1);
                }
            }
        }

        bool isCorrect = false;
        if (q.type == "multiple_choice") {
            isCorrect = (userAnswer == q.correctAnswer);
        } else if (q.type == "sentence_order") {
            // sentence_order: userAnswer is comma-separated word indices or sentence
            // For simplicity, compare the sentence directly (normalized)
            std::string lowerUser = userAnswer;
            std::string lowerCorrect = q.correctAnswer;
            std::transform(lowerUser.begin(), lowerUser.end(), lowerUser.begin(), ::tolower);
            std::transform(lowerCorrect.begin(), lowerCorrect.end(), lowerCorrect.begin(), ::tolower);
            // Remove extra spaces and punctuation for comparison
            lowerUser.erase(std::remove_if(lowerUser.begin(), lowerUser.end(), 
                [](char c) { return !std::isalnum(c) && c != ' '; }), lowerUser.end());
            lowerCorrect.erase(std::remove_if(lowerCorrect.begin(), lowerCorrect.end(), 
                [](char c) { return !std::isalnum(c) && c != ' '; }), lowerCorrect.end());
            // Trim
            lowerUser.erase(0, lowerUser.find_first_not_of(" \t"));
            lowerUser.erase(lowerUser.find_last_not_of(" \t") + 1);
            lowerCorrect.erase(0, lowerCorrect.find_first_not_of(" \t"));
            lowerCorrect.erase(lowerCorrect.find_last_not_of(" \t") + 1);
            // Normalize multiple spaces
            std::string normalizedUser, normalizedCorrect;
            for (size_t i = 0; i < lowerUser.length(); i++) {
                if (lowerUser[i] == ' ' && (normalizedUser.empty() || normalizedUser.back() != ' ')) {
                    normalizedUser += ' ';
                } else if (lowerUser[i] != ' ') {
                    normalizedUser += lowerUser[i];
                }
            }
            for (size_t i = 0; i < lowerCorrect.length(); i++) {
                if (lowerCorrect[i] == ' ' && (normalizedCorrect.empty() || normalizedCorrect.back() != ' ')) {
                    normalizedCorrect += ' ';
                } else if (lowerCorrect[i] != ' ') {
                    normalizedCorrect += lowerCorrect[i];
                }
            }
            isCorrect = (normalizedUser == normalizedCorrect);
        } else {
            // fill_blank: so sánh không phân biệt hoa thường và khoảng trắng thừa
            std::string lowerUser = userAnswer;
            std::string lowerCorrect = q.correctAnswer;
            std::transform(lowerUser.begin(), lowerUser.end(), lowerUser.begin(), ::tolower);
            std::transform(lowerCorrect.begin(), lowerCorrect.end(), lowerCorrect.begin(), ::tolower);
            // Trim
            lowerUser.erase(0, lowerUser.find_first_not_of(" \t"));
            lowerUser.erase(lowerUser.find_last_not_of(" \t") + 1);
            lowerCorrect.erase(0, lowerCorrect.find_first_not_of(" \t"));
            lowerCorrect.erase(lowerCorrect.find_last_not_of(" \t") + 1);
            isCorrect = (lowerUser == lowerCorrect);
        }

        if (isCorrect) {
            earnedPoints += q.points;
            correctCount++;
        } else {
            wrongCount++;
        }

        if (i > 0) detailedResults << ",";
        detailedResults << R"({"questionId":")" << q.questionId
                        << R"(","correct":)" << (isCorrect ? "true" : "false")
                        << R"(,"userAnswer":")" << escapeJson(userAnswer)
                        << R"(","correctAnswer":")" << escapeJson(q.correctAnswer)
                        << R"(","points":)" << q.points
                        << R"(,"earnedPoints":)" << (isCorrect ? q.points : 0) << "}";
    }
    detailedResults << "]";

    int percentage = (totalPoints > 0) ? (earnedPoints * 100 / totalPoints) : 0;
    bool passed = (percentage >= 60);
    std::string grade;
    if (percentage >= 90) grade = "A";
    else if (percentage >= 80) grade = "B";
    else if (percentage >= 70) grade = "C";
    else if (percentage >= 60) grade = "D";
    else grade = "F";

    return R"({"messageType":"SUBMIT_TEST_RESPONSE","messageId":")" + messageId +
           R"(","timestamp":)" + std::to_string(getCurrentTimestamp()) +
           R"(,"payload":{"status":"success","data":{"testId":")" + testId +
           R"(","submissionId":")" + generateId("sub") +
           R"(","score":)" + std::to_string(earnedPoints) +
           R"(,"percentage":)" + std::to_string(percentage) +
           R"(,"totalPoints":)" + std::to_string(totalPoints) +
           R"(,"earnedPoints":)" + std::to_string(earnedPoints) +
           R"(,"totalQuestions":)" + std::to_string(test.questions.size()) +
           R"(,"correctAnswers":)" + std::to_string(correctCount) +
           R"(,"wrongAnswers":)" + std::to_string(wrongCount) +
           R"(,"passed":)" + (passed ? "true" : "false") +
           R"(,"grade":")" + grade +
           R"(","detailedResults":)" + detailedResults.str() + R"(}}})";
}

// Xử lý GET_CONTACT_LIST_REQUEST
std::string handleGetContactList(const std::string& json) {
    std::string messageId = getJsonValue(json, "messageId");
    std::string sessionToken = getJsonValue(json, "sessionToken");

    std::string currentUserId = validateSession(sessionToken);
    if (currentUserId.empty()) {
        return R"({"messageType":"GET_CONTACT_LIST_RESPONSE","messageId":")" + messageId +
               R"(","timestamp":)" + std::to_string(getCurrentTimestamp()) +
               R"(,"payload":{"status":"error","message":"Invalid or expired session"}})";
    }

    std::stringstream contactsJson;
    contactsJson << "[";
    bool first = true;
    int onlineCount = 0;

    {
        std::lock_guard<std::mutex> lock(usersMutex);
        for (const auto& pair : users) {
            const User& user = pair.second;
            if (user.userId == currentUserId) continue;

            if (!first) contactsJson << ",";
            first = false;

            contactsJson << R"({"userId":")" << user.userId
                         << R"(","fullName":")" << escapeJson(user.fullname)
                         << R"(","role":")" << user.role
                         << R"(","status":")" << (user.online ? "online" : "offline")
                         << R"(","level":")" << user.level << R"("})";

            if (user.online) onlineCount++;
        }
    }
    contactsJson << "]";

    return R"({"messageType":"GET_CONTACT_LIST_RESPONSE","messageId":")" + messageId +
           R"(","timestamp":)" + std::to_string(getCurrentTimestamp()) +
           R"(,"payload":{"status":"success","data":{"contacts":)" + contactsJson.str() +
           R"(,"onlineCount":)" + std::to_string(onlineCount) +
           R"(,"totalContacts":)" + std::to_string(users.size() - 1) + R"(}}})";
}

// Xử lý SEND_MESSAGE_REQUEST
std::string handleSendMessage(const std::string& json, int senderSocket) {
    std::string payload = getJsonObject(json, "payload");
    std::string messageId = getJsonValue(json, "messageId");
    std::string sessionToken = getJsonValue(json, "sessionToken");
    std::string recipientId = getJsonValue(payload, "recipientId");
    std::string messageContent = getJsonValue(payload, "messageContent");

    std::string senderId = validateSession(sessionToken);
    if (senderId.empty()) {
        return R"({"messageType":"SEND_MESSAGE_RESPONSE","messageId":")" + messageId +
               R"(","timestamp":)" + std::to_string(getCurrentTimestamp()) +
               R"(,"payload":{"status":"error","message":"Invalid or expired session"}})";
    }

    User* recipient = nullptr;
    std::string senderName;
    {
        std::lock_guard<std::mutex> lock(usersMutex);
        auto it = userById.find(recipientId);
        if (it != userById.end()) {
            recipient = it->second;
        }
        auto senderIt = userById.find(senderId);
        if (senderIt != userById.end()) {
            senderName = senderIt->second->fullname;
        }
    }

    if (!recipient) {
        return R"({"messageType":"SEND_MESSAGE_RESPONSE","messageId":")" + messageId +
               R"(","timestamp":)" + std::to_string(getCurrentTimestamp()) +
               R"(,"payload":{"status":"error","message":"Recipient not found"}})";
    }

    ChatMessage msg;
    msg.messageId = generateId("chatmsg");
    msg.senderId = senderId;
    msg.recipientId = recipientId;
    msg.content = messageContent;
    msg.timestamp = getCurrentTimestamp();
    msg.read = false;

    {
        std::lock_guard<std::mutex> lock(chatMutex);
        chatMessages.push_back(msg);
    }

    bool delivered = false;
    if (recipient->online && recipient->clientSocket > 0) {
        std::string notification = R"({"messageType":"RECEIVE_MESSAGE","messageId":")" + msg.messageId +
                                   R"(","timestamp":)" + std::to_string(msg.timestamp) +
                                   R"(,"payload":{"messageId":")" + msg.messageId +
                                   R"(","senderId":")" + senderId +
                                   R"(","senderName":")" + escapeJson(senderName) +
                                   R"(","messageContent":")" + escapeJson(messageContent) +
                                   R"(","sentAt":)" + std::to_string(msg.timestamp) + R"(}})";

        uint32_t len = htonl(notification.length());
        if (send(recipient->clientSocket, &len, sizeof(len), 0) > 0) {
            if (send(recipient->clientSocket, notification.c_str(), notification.length(), 0) > 0) {
                delivered = true;
                logMessage("SEND", "Client:" + std::to_string(recipient->clientSocket), notification);
            }
        }
    }

    return R"({"messageType":"SEND_MESSAGE_RESPONSE","messageId":")" + messageId +
           R"(","timestamp":)" + std::to_string(getCurrentTimestamp()) +
           R"(,"payload":{"status":"success","data":{"messageId":")" + msg.messageId +
           R"(","recipientId":")" + recipientId +
           R"(","messageContent":")" + escapeJson(messageContent) +
           R"(","sentAt":)" + std::to_string(msg.timestamp) +
           R"(,"delivered":)" + (delivered ? "true" : "false") + R"(}}})";
}

// Xử lý MARK_MESSAGES_READ_REQUEST - đánh dấu tin nhắn đã đọc
std::string handleMarkMessagesRead(const std::string& json) {
    std::string payload = getJsonObject(json, "payload");
    std::string messageId = getJsonValue(json, "messageId");
    std::string sessionToken = getJsonValue(json, "sessionToken");
    std::string senderId = getJsonValue(payload, "senderId");

    std::string userId = validateSession(sessionToken);
    if (userId.empty()) {
        return R"({"messageType":"MARK_MESSAGES_READ_RESPONSE","messageId":")" + messageId +
               R"(","timestamp":)" + std::to_string(getCurrentTimestamp()) +
               R"(,"payload":{"status":"error","message":"Invalid or expired session"}})";
    }

    int markedCount = 0;
    {
        std::lock_guard<std::mutex> lock(chatMutex);
        for (auto& msg : chatMessages) {
            if (msg.recipientId == userId && msg.senderId == senderId && !msg.read) {
                msg.read = true;
                markedCount++;
            }
        }
    }

    return R"({"messageType":"MARK_MESSAGES_READ_RESPONSE","messageId":")" + messageId +
           R"(","timestamp":)" + std::to_string(getCurrentTimestamp()) +
           R"(,"payload":{"status":"success","data":{"markedCount":)" + std::to_string(markedCount) + R"(}}})";
}

// Xử lý GET_CHAT_HISTORY_REQUEST
std::string handleGetChatHistory(const std::string& json) {
    std::string payload = getJsonObject(json, "payload");
    std::string messageId = getJsonValue(json, "messageId");
    std::string sessionToken = getJsonValue(json, "sessionToken");
    std::string recipientId = getJsonValue(payload, "recipientId");

    std::string userId = validateSession(sessionToken);
    if (userId.empty()) {
        return R"({"messageType":"GET_CHAT_HISTORY_RESPONSE","messageId":")" + messageId +
               R"(","timestamp":)" + std::to_string(getCurrentTimestamp()) +
               R"(,"payload":{"status":"error","message":"Invalid or expired session"}})";
    }

    std::stringstream messagesJson;
    messagesJson << "[";
    bool first = true;

    {
        std::lock_guard<std::mutex> lock(chatMutex);
        for (const auto& msg : chatMessages) {
            if ((msg.senderId == userId && msg.recipientId == recipientId) ||
                (msg.senderId == recipientId && msg.recipientId == userId)) {
                if (!first) messagesJson << ",";
                first = false;

                messagesJson << R"({"messageId":")" << msg.messageId
                             << R"(","senderId":")" << msg.senderId
                             << R"(","content":")" << escapeJson(msg.content)
                             << R"(","timestamp":)" << msg.timestamp << "}";
            }
        }
    }
    messagesJson << "]";

    return R"({"messageType":"GET_CHAT_HISTORY_RESPONSE","messageId":")" + messageId +
           R"(","timestamp":)" + std::to_string(getCurrentTimestamp()) +
           R"(,"payload":{"status":"success","data":{"messages":)" + messagesJson.str() + R"(}}})";
}

// Xử lý GET_EXERCISE_REQUEST
std::string handleGetExercise(const std::string& json) {
    std::string payload = getJsonObject(json, "payload");
    std::string messageId = getJsonValue(json, "messageId");
    std::string sessionToken = getJsonValue(json, "sessionToken");
    std::string exerciseType = getJsonValue(payload, "exerciseType");
    std::string level = getJsonValue(payload, "level");
    std::string topic = getJsonValue(payload, "topic");

    std::string userId = validateSession(sessionToken);
    if (userId.empty()) {
        return R"({"messageType":"GET_EXERCISE_RESPONSE","messageId":")" + messageId +
               R"(","timestamp":)" + std::to_string(getCurrentTimestamp()) +
               R"(,"payload":{"status":"error","message":"Invalid or expired session"}})";
    }

    // Tìm exercise phù hợp
    const Exercise* selectedExercise = nullptr;
    for (const auto& pair : exercises) {
        const Exercise& ex = pair.second;
        bool typeMatch = exerciseType.empty() || ex.exerciseType == exerciseType;
        bool levelMatch = level.empty() || ex.level == level;
        bool topicMatch = topic.empty() || ex.topic == topic;

        if (typeMatch && levelMatch && topicMatch) {
            selectedExercise = &ex;
            break;
        }
    }

    if (!selectedExercise && !exercises.empty()) {
        selectedExercise = &exercises.begin()->second;
    }

    if (!selectedExercise) {
        return R"({"messageType":"GET_EXERCISE_RESPONSE","messageId":")" + messageId +
               R"(","timestamp":)" + std::to_string(getCurrentTimestamp()) +
               R"(,"payload":{"status":"error","message":"No exercises available"}})";
    }

    const Exercise& ex = *selectedExercise;
    std::stringstream responseJson;
    responseJson << R"({"messageType":"GET_EXERCISE_RESPONSE","messageId":")" << messageId
                 << R"(","timestamp":)" << getCurrentTimestamp()
                 << R"(,"payload":{"status":"success","data":{"exerciseId":")" << ex.exerciseId
                 << R"(","exerciseType":")" << ex.exerciseType
                 << R"(","title":")" << escapeJson(ex.title)
                 << R"(","description":")" << escapeJson(ex.description)
                 << R"(","instructions":")" << escapeJson(ex.instructions)
                 << R"(","level":")" << ex.level
                 << R"(","topic":")" << ex.topic
                 << R"(","duration":)" << ex.duration;

    if (ex.exerciseType == "sentence_rewrite" && !ex.prompts.empty()) {
        responseJson << R"(,"prompts":[)";
        for (size_t i = 0; i < ex.prompts.size(); i++) {
            if (i > 0) responseJson << ",";
            responseJson << R"(")" << escapeJson(ex.prompts[i]) << R"(")";
        }
        responseJson << "]";
    } else if (ex.exerciseType == "paragraph_writing") {
        responseJson << R"(,"topicDescription":")" << escapeJson(ex.topicDescription) << R"(")";
        if (!ex.requirements.empty()) {
            responseJson << R"(,"requirements":[)";
            for (size_t i = 0; i < ex.requirements.size(); i++) {
                if (i > 0) responseJson << ",";
                responseJson << R"(")" << escapeJson(ex.requirements[i]) << R"(")";
            }
            responseJson << "]";
        }
    } else if (ex.exerciseType == "topic_speaking") {
        responseJson << R"(,"topicDescription":")" << escapeJson(ex.topicDescription) << R"(")";
    }

    responseJson << R"(}}})";
    return responseJson.str();
}

// Xử lý SUBMIT_EXERCISE_REQUEST
std::string handleSubmitExercise(const std::string& json) {
    std::string payload = getJsonObject(json, "payload");
    std::string messageId = getJsonValue(json, "messageId");
    std::string sessionToken = getJsonValue(json, "sessionToken");
    std::string exerciseId = getJsonValue(payload, "exerciseId");
    std::string exerciseType = getJsonValue(payload, "exerciseType");
    std::string content = getJsonValue(payload, "content");

    std::string userId = validateSession(sessionToken);
    if (userId.empty()) {
        return R"({"messageType":"SUBMIT_EXERCISE_RESPONSE","messageId":")" + messageId +
               R"(","timestamp":)" + std::to_string(getCurrentTimestamp()) +
               R"(,"payload":{"status":"error","message":"Invalid or expired session"}})";
    }

    auto it = exercises.find(exerciseId);
    if (it == exercises.end()) {
        return R"({"messageType":"SUBMIT_EXERCISE_RESPONSE","messageId":")" + messageId +
               R"(","timestamp":)" + std::to_string(getCurrentTimestamp()) +
               R"(,"payload":{"status":"error","message":"Exercise not found"}})";
    }

    ExerciseSubmission submission;
    submission.submissionId = generateId("sub");
    submission.exerciseId = exerciseId;
    submission.userId = userId;
    submission.exerciseType = exerciseType;
    submission.content = content;
    submission.status = "pending";
    submission.submittedAt = getCurrentTimestamp();
    submission.teacherId = "";
    submission.teacherFeedback = "";
    submission.teacherScore = 0;
    submission.reviewedAt = 0;

    {
        std::lock_guard<std::mutex> lock(exercisesMutex);
        exerciseSubmissions.push_back(submission);
    }

    return R"({"messageType":"SUBMIT_EXERCISE_RESPONSE","messageId":")" + messageId +
           R"(","timestamp":)" + std::to_string(getCurrentTimestamp()) +
           R"(,"payload":{"status":"success","message":"Exercise submitted successfully","data":{"submissionId":")" +
           submission.submissionId +
           R"(","status":"pending","message":"Your submission will be reviewed by a teacher soon."}}})";
}

// Xử lý GET_PENDING_SUBMISSIONS_REQUEST (Teacher only)
std::string handleGetPendingSubmissions(const std::string& json) {
    std::string messageId = getJsonValue(json, "messageId");
    std::string sessionToken = getJsonValue(json, "sessionToken");

    std::string userId = validateSession(sessionToken);
    if (userId.empty() || !isTeacher(userId)) {
        return R"({"messageType":"GET_PENDING_SUBMISSIONS_RESPONSE","messageId":")" + messageId +
               R"(","timestamp":)" + std::to_string(getCurrentTimestamp()) +
               R"(,"payload":{"status":"error","message":"Unauthorized: Teacher access required"}})";
    }

    std::stringstream submissionsJson;
    submissionsJson << "[";
    bool first = true;
    int count = 0;

    {
        std::lock_guard<std::mutex> lock(exercisesMutex);
        for (const auto& submission : exerciseSubmissions) {
            if (submission.status == "pending") {
                if (!first) submissionsJson << ",";
                first = false;

                std::string studentName = "Unknown";
                {
                    std::lock_guard<std::mutex> userLock(usersMutex);
                    auto it = userById.find(submission.userId);
                    if (it != userById.end()) {
                        studentName = it->second->fullname;
                    }
                }

                submissionsJson << R"({"submissionId":")" << submission.submissionId
                               << R"(","exerciseId":")" << submission.exerciseId
                               << R"(","userId":")" << submission.userId
                               << R"(","studentName":")" << escapeJson(studentName)
                               << R"(","exerciseType":")" << submission.exerciseType
                               << R"(","content":")" << escapeJson(submission.content)
                               << R"(","submittedAt":)" << submission.submittedAt << "}";
                count++;
            }
        }
    }
    submissionsJson << "]";

    return R"({"messageType":"GET_PENDING_SUBMISSIONS_RESPONSE","messageId":")" + messageId +
           R"(","timestamp":)" + std::to_string(getCurrentTimestamp()) +
           R"(,"payload":{"status":"success","data":{"submissions":)" + submissionsJson.str() +
           R"(,"totalPending":)" + std::to_string(count) + R"(}}})";
}

#if 0
// Xử lý REVIEW_EXERCISE_REQUEST (Teacher only)
std::string handleReviewExercise(const std::string& json) {
    std::string payload = getJsonObject(json, "payload");
    std::string messageId = getJsonValue(json, "messageId");
    std::string sessionToken = getJsonValue(json, "sessionToken");
    std::string submissionId = getJsonValue(payload, "submissionId");
    std::string feedback = getJsonValue(payload, "feedback");
    std::string scoreStr = getJsonValue(payload, "score");

    std::string userId = validateSession(sessionToken);
    if (userId.empty() || !isTeacher(userId)) {
        return R"({"messageType":"REVIEW_EXERCISE_RESPONSE","messageId":")" + messageId +
               R"(","timestamp":)" + std::to_string(getCurrentTimestamp()) +
               R"(,"payload":{"status":"error","message":"Unauthorized: Teacher access required"}})";
    }

    int score = scoreStr.empty() ? 0 : std::stoi(scoreStr);
    if (score < 0 || score > 100) {
        return R"({"messageType":"REVIEW_EXERCISE_RESPONSE","messageId":")" + messageId +
               R"(","timestamp":)" + std::to_string(getCurrentTimestamp()) +
               R"(,"payload":{"status":"error","message":"Score must be between 0 and 100"}})";
    }

    {
        std::lock_guard<std::mutex> lock(exercisesMutex);
        for (auto& submission : exerciseSubmissions) {
            if (submission.submissionId == submissionId) {
                submission.status = "reviewed";
                submission.teacherId = userId;
                submission.teacherFeedback = feedback;
                submission.teacherScore = score;
                submission.reviewedAt = getCurrentTimestamp();

                // Send notification to student if online
                {
                    std::lock_guard<std::mutex> userLock(usersMutex);
                    auto it = userById.find(submission.userId);
                    if (it != userById.end() && it->second->online && it->second->clientSocket > 0) {
                        std::string notification = R"({"messageType":"EXERCISE_FEEDBACK_NOTIFICATION","messageId":")" +
                                                   generateId("notif") +
                                                   R"(","timestamp":)" + std::to_string(getCurrentTimestamp()) +
                                                   R"(,"payload":{"submissionId":")" + submissionId +
                                                   R"(","exerciseId":")" + submission.exerciseId +
                                                   R"(","feedback":")" + escapeJson(feedback) +
                                                   R"(","score":)" + std::to_string(score) + R"(}})";

                        uint32_t len = htonl(notification.length());
                        send(it->second->clientSocket, &len, sizeof(len), 0);
                        send(it->second->clientSocket, notification.c_str(), notification.length(), 0);
                        logMessage("SEND", "Client:" + std::to_string(it->second->clientSocket), "EXERCISE_FEEDBACK_NOTIFICATION");
                    }
                }

                return R"({"messageType":"REVIEW_EXERCISE_RESPONSE","messageId":")" + messageId +
                       R"(","timestamp":)" + std::to_string(getCurrentTimestamp()) +
                       R"(,"payload":{"status":"success","message":"Exercise reviewed successfully"}}})";
            }
        }
    }

    return R"({"messageType":"REVIEW_EXERCISE_RESPONSE","messageId":")" + messageId +
           R"(","timestamp":)" + std::to_string(getCurrentTimestamp()) +
           R"(,"payload":{"status":"error","message":"Submission not found"}}})";
}
#endif

#if 0
// Xử lý GET_FEEDBACK_REQUEST (Student)
std::string handleGetFeedback(const std::string& json) {
    std::string payload = getJsonObject(json, "payload");
    std::string messageId = getJsonValue(json, "messageId");
    std::string sessionToken = getJsonValue(json, "sessionToken");
    std::string submissionId = getJsonValue(payload, "submissionId");

    std::string userId = validateSession(sessionToken);
    if (userId.empty()) {
        return R"({"messageType":"GET_FEEDBACK_RESPONSE","messageId":")" + messageId +
               R"(","timestamp":)" + std::to_string(getCurrentTimestamp()) +
               R"(,"payload":{"status":"error","message":"Invalid or expired session"}})";
    }

    {
        std::lock_guard<std::mutex> lock(exercisesMutex);
        for (const auto& submission : exerciseSubmissions) {
            if (submission.submissionId == submissionId && submission.userId == userId) {
                if (submission.status == "pending") {
                    return R"({"messageType":"GET_FEEDBACK_RESPONSE","messageId":")" + messageId +
                           R"(","timestamp":)" + std::to_string(getCurrentTimestamp()) +
                           R"(,"payload":{"status":"success","data":{"status":"pending","message":"Your submission is still being reviewed"}}})";
                }

                std::string teacherName = "Unknown";
                {
                    std::lock_guard<std::mutex> userLock(usersMutex);
                    auto it = userById.find(submission.teacherId);
                    if (it != userById.end()) {
                        teacherName = it->second->fullname;
                    }
                }

                return R"({"messageType":"GET_FEEDBACK_RESPONSE","messageId":")" + messageId +
                       R"(","timestamp":)" + std::to_string(getCurrentTimestamp()) +
                       R"(,"payload":{"status":"success","data":{"submissionId":")" + submission.submissionId +
                       R"(","exerciseId":")" + submission.exerciseId +
                       R"(","status":"reviewed","teacherName":")" + escapeJson(teacherName) +
                       R"(","feedback":")" + escapeJson(submission.teacherFeedback) +
                       R"(","score":)" + std::to_string(submission.teacherScore) +
                       R"(,"reviewedAt":)" + std::to_string(submission.reviewedAt) + R"(}}})";
            }
        }
    }

    return R"({"messageType":"GET_FEEDBACK_RESPONSE","messageId":")" + messageId +
           R"(","timestamp":)" + std::to_string(getCurrentTimestamp()) +
           R"(,"payload":{"status":"error","message":"Submission not found"}}})";
}
#endif

// Xử lý GET_GAME_LIST_REQUEST
std::string handleGetGameList(const std::string& json) {
    std::string payload = getJsonObject(json, "payload");
    std::string messageId = getJsonValue(json, "messageId");
    std::string sessionToken = getJsonValue(json, "sessionToken");
    std::string gameType = getJsonValue(payload, "gameType");
    std::string level = getJsonValue(payload, "level");

    std::string userId = validateSession(sessionToken);
    if (userId.empty()) {
        return R"({"messageType":"GET_GAME_LIST_RESPONSE","messageId":")" + messageId +
               R"(","timestamp":)" + std::to_string(getCurrentTimestamp()) +
               R"(,"payload":{"status":"error","message":"Invalid or expired session"}})";
    }

    std::stringstream gamesJson;
    gamesJson << "[";
    bool first = true;
    int count = 0;

    for (const auto& pair : games) {
        const Game& game = pair.second;
        bool typeMatch = gameType.empty() || gameType == "all" || game.gameType == gameType;
        bool levelMatch = level.empty() || game.level == level;

        if (typeMatch && levelMatch) {
            if (!first) gamesJson << ",";
            first = false;

            gamesJson << R"({"gameId":")" << game.gameId
                      << R"(","gameType":")" << game.gameType
                      << R"(","title":")" << escapeJson(game.title)
                      << R"(","description":")" << escapeJson(game.description)
                      << R"(","level":")" << game.level
                      << R"(","topic":")" << game.topic
                      << R"(","timeLimit":)" << game.timeLimit
                      << R"(,"maxScore":)" << game.maxScore << "}";
            count++;
        }
    }
    gamesJson << "]";

    return R"({"messageType":"GET_GAME_LIST_RESPONSE","messageId":")" + messageId +
           R"(","timestamp":)" + std::to_string(getCurrentTimestamp()) +
           R"(,"payload":{"status":"success","data":{"games":)" + gamesJson.str() +
           R"(,"totalGames":)" + std::to_string(count) + R"(}}})";
}

// Xử lý START_GAME_REQUEST
std::string handleStartGame(const std::string& json) {
    std::string payload = getJsonObject(json, "payload");
    std::string messageId = getJsonValue(json, "messageId");
    std::string sessionToken = getJsonValue(json, "sessionToken");
    std::string gameId = getJsonValue(payload, "gameId");

    std::string userId = validateSession(sessionToken);
    if (userId.empty()) {
        return R"({"messageType":"START_GAME_RESPONSE","messageId":")" + messageId +
               R"(","timestamp":)" + std::to_string(getCurrentTimestamp()) +
               R"(,"payload":{"status":"error","message":"Invalid or expired session"}})";
    }

    auto it = games.find(gameId);
    if (it == games.end()) {
        return R"({"messageType":"START_GAME_RESPONSE","messageId":")" + messageId +
               R"(","timestamp":)" + std::to_string(getCurrentTimestamp()) +
               R"(,"payload":{"status":"error","message":"Game not found"}})";
    }

    const Game& game = it->second;
    std::string sessionId = generateId("gs");

    GameSession session;
    session.sessionId = sessionId;
    session.gameId = gameId;
    session.userId = userId;
    session.startTime = getCurrentTimestamp();
    session.endTime = 0;
    session.score = 0;
    session.maxScore = game.maxScore;
    session.completed = false;

    {
        std::lock_guard<std::mutex> lock(gamesMutex);
        gameSessions[sessionId] = session;
    }

    std::stringstream gameDataJson;
    gameDataJson << R"({"gameSessionId":")" << sessionId
                 << R"(","gameId":")" << game.gameId
                 << R"(","gameType":")" << game.gameType
                 << R"(","title":")" << escapeJson(game.title)
                 << R"(","timeLimit":)" << game.timeLimit
                 << R"(","maxScore":)" << game.maxScore;

    if (game.gameType == "word_match") {
        gameDataJson << R"(,"pairs":[)";
        for (size_t i = 0; i < game.pairs.size(); i++) {
            if (i > 0) gameDataJson << ",";
            gameDataJson << R"({"left":")" << escapeJson(game.pairs[i].first)
                         << R"(","right":")" << escapeJson(game.pairs[i].second) << R"("})";
        }
        gameDataJson << "]";
    } else if (game.gameType == "sentence_match") {
        gameDataJson << R"(,"pairs":[)";
        for (size_t i = 0; i < game.sentencePairs.size(); i++) {
            if (i > 0) gameDataJson << ",";
            gameDataJson << R"({"left":")" << escapeJson(game.sentencePairs[i].first)
                         << R"(","right":")" << escapeJson(game.sentencePairs[i].second) << R"("})";
        }
        gameDataJson << "]";
    } else if (game.gameType == "picture_match") {
        gameDataJson << R"(,"pairs":[)";
        for (size_t i = 0; i < game.picturePairs.size(); i++) {
            if (i > 0) gameDataJson << ",";
            gameDataJson << R"({"word":")" << escapeJson(game.picturePairs[i].first)
                         << R"(","imageUrl":")" << escapeJson(game.picturePairs[i].second) << R"("})";
        }
        gameDataJson << "]";
    }

    gameDataJson << "}";

    return R"({"messageType":"START_GAME_RESPONSE","messageId":")" + messageId +
           R"(","timestamp":)" + std::to_string(getCurrentTimestamp()) +
           R"(,"payload":{"status":"success","data":)" + gameDataJson.str() + R"(}}})";
}

// Xử lý SUBMIT_GAME_RESULT_REQUEST
std::string handleSubmitGameResult(const std::string& json) {
    std::string payload = getJsonObject(json, "payload");
    std::string messageId = getJsonValue(json, "messageId");
    std::string sessionToken = getJsonValue(json, "sessionToken");
    std::string gameSessionId = getJsonValue(payload, "gameSessionId");
    std::string gameId = getJsonValue(payload, "gameId");
    std::string matchesArray = getJsonArray(payload, "matches");

    std::string userId = validateSession(sessionToken);
    if (userId.empty()) {
        return R"({"messageType":"SUBMIT_GAME_RESULT_RESPONSE","messageId":")" + messageId +
               R"(","timestamp":)" + std::to_string(getCurrentTimestamp()) +
               R"(,"payload":{"status":"error","message":"Invalid or expired session"}})";
    }

    auto sessionIt = gameSessions.find(gameSessionId);
    if (sessionIt == gameSessions.end()) {
        return R"({"messageType":"SUBMIT_GAME_RESULT_RESPONSE","messageId":")" + messageId +
               R"(","timestamp":)" + std::to_string(getCurrentTimestamp()) +
               R"(,"payload":{"status":"error","message":"Game session not found"}})";
    }

    auto gameIt = games.find(gameId);
    if (gameIt == games.end()) {
        return R"({"messageType":"SUBMIT_GAME_RESULT_RESPONSE","messageId":")" + messageId +
               R"(","timestamp":)" + std::to_string(getCurrentTimestamp()) +
               R"(,"payload":{"status":"error","message":"Game not found"}})";
    }

    const Game& game = gameIt->second;
    GameSession& session = sessionIt->second;

    // Parse matches and calculate score
    int correctMatches = 0;
    int totalPairs = 0;

    if (game.gameType == "word_match") {
        totalPairs = game.pairs.size();
        std::vector<std::string> matches = parseJsonArray(matchesArray);
        for (const std::string& match : matches) {
            std::string left = getJsonValue(match, "left");
            std::string right = getJsonValue(match, "right");
            for (const auto& pair : game.pairs) {
                if (pair.first == left && pair.second == right) {
                    correctMatches++;
                    break;
                }
            }
        }
    } else if (game.gameType == "sentence_match") {
        totalPairs = game.sentencePairs.size();
        std::vector<std::string> matches = parseJsonArray(matchesArray);
        for (const std::string& match : matches) {
            std::string left = getJsonValue(match, "left");
            std::string right = getJsonValue(match, "right");
            for (const auto& pair : game.sentencePairs) {
                if (pair.first == left && pair.second == right) {
                    correctMatches++;
                    break;
                }
            }
        }
    } else if (game.gameType == "picture_match") {
        totalPairs = game.picturePairs.size();
        std::vector<std::string> matches = parseJsonArray(matchesArray);
        for (const std::string& match : matches) {
            std::string word = getJsonValue(match, "word");
            std::string imageUrl = getJsonValue(match, "imageUrl");
            for (const auto& pair : game.picturePairs) {
                if (pair.first == word && pair.second == imageUrl) {
                    correctMatches++;
                    break;
                }
            }
        }
    }

    int score = (totalPairs > 0) ? (correctMatches * game.maxScore / totalPairs) : 0;
    session.score = score;
    session.endTime = getCurrentTimestamp();
    session.completed = true;

    int percentage = (totalPairs > 0) ? (correctMatches * 100 / totalPairs) : 0;
    std::string grade;
    if (percentage >= 90) grade = "A";
    else if (percentage >= 80) grade = "B";
    else if (percentage >= 70) grade = "C";
    else if (percentage >= 60) grade = "D";
    else grade = "F";

    return R"({"messageType":"SUBMIT_GAME_RESULT_RESPONSE","messageId":")" + messageId +
           R"(","timestamp":)" + std::to_string(getCurrentTimestamp()) +
           R"(,"payload":{"status":"success","data":{"gameSessionId":")" + gameSessionId +
           R"(","gameId":")" + gameId +
           R"(","score":)" + std::to_string(score) +
           R"(,"maxScore":)" + std::to_string(game.maxScore) +
           R"(,"correctMatches":)" + std::to_string(correctMatches) +
           R"(,"totalPairs":)" + std::to_string(totalPairs) +
           R"(,"percentage":)" + std::to_string(percentage) +
           R"(,"grade":")" + grade +
           R"(","timeSpent":)" + std::to_string((session.endTime - session.startTime) / 1000) + R"(}}})";
}

// Helper function to check if user is admin
bool isAdmin(const std::string& userId) {
    std::lock_guard<std::mutex> lock(usersMutex);
    auto it = userById.find(userId);
    if (it != userById.end()) {
        return it->second->role == "admin";
    }
    return false;
}

// Helper function to check if user is teacher
bool isTeacher(const std::string& userId) {
    std::lock_guard<std::mutex> lock(usersMutex);
    auto it = userById.find(userId);
    if (it != userById.end()) {
        return it->second->role == "teacher";
    }
    return false;
}

// Xử lý ADD_GAME_REQUEST (Admin only)
std::string handleAddGame(const std::string& json) {
    std::string payload = getJsonObject(json, "payload");
    std::string messageId = getJsonValue(json, "messageId");
    std::string sessionToken = getJsonValue(json, "sessionToken");

    std::string userId = validateSession(sessionToken);
    if (userId.empty() || !isAdmin(userId)) {
        return R"({"messageType":"ADD_GAME_RESPONSE","messageId":")" + messageId +
               R"(","timestamp":)" + std::to_string(getCurrentTimestamp()) +
               R"(,"payload":{"status":"error","message":"Unauthorized: Admin access required"}})";
    }

    std::string gameType = getJsonValue(payload, "gameType");
    std::string title = getJsonValue(payload, "title");
    std::string description = getJsonValue(payload, "description");
    std::string level = getJsonValue(payload, "level");
    std::string topic = getJsonValue(payload, "topic");
    std::string timeLimitStr = getJsonValue(payload, "timeLimit");
    std::string maxScoreStr = getJsonValue(payload, "maxScore");

    Game newGame;
    newGame.gameId = generateId("game");
    newGame.gameType = gameType;
    newGame.title = title;
    newGame.description = description;
    newGame.level = level;
    newGame.topic = topic;
    newGame.timeLimit = timeLimitStr.empty() ? 120 : std::stoi(timeLimitStr);
    newGame.maxScore = maxScoreStr.empty() ? 100 : std::stoi(maxScoreStr);

    // Parse pairs based on game type
    if (gameType == "word_match") {
        std::string pairsArray = getJsonArray(payload, "pairs");
        std::vector<std::string> pairs = parseJsonArray(pairsArray);
        for (const std::string& pair : pairs) {
            std::string left = getJsonValue(pair, "left");
            std::string right = getJsonValue(pair, "right");
            newGame.pairs.push_back({left, right});
        }
    } else if (gameType == "sentence_match") {
        std::string pairsArray = getJsonArray(payload, "pairs");
        std::vector<std::string> pairs = parseJsonArray(pairsArray);
        for (const std::string& pair : pairs) {
            std::string left = getJsonValue(pair, "left");
            std::string right = getJsonValue(pair, "right");
            newGame.sentencePairs.push_back({left, right});
        }
    } else if (gameType == "picture_match") {
        std::string pairsArray = getJsonArray(payload, "pairs");
        std::vector<std::string> pairs = parseJsonArray(pairsArray);
        for (const std::string& pair : pairs) {
            std::string word = getJsonValue(pair, "word");
            std::string imageUrl = getJsonValue(pair, "imageUrl");
            newGame.picturePairs.push_back({word, imageUrl});
        }
    }

    {
        std::lock_guard<std::mutex> lock(gamesMutex);
        games[newGame.gameId] = newGame;
    }

    return R"({"messageType":"ADD_GAME_RESPONSE","messageId":")" + messageId +
           R"(","timestamp":)" + std::to_string(getCurrentTimestamp()) +
           R"(,"payload":{"status":"success","message":"Game added successfully","data":{"gameId":")" +
           newGame.gameId + R"("}}})";
}

// Xử lý UPDATE_GAME_REQUEST (Admin only)
std::string handleUpdateGame(const std::string& json) {
    std::string payload = getJsonObject(json, "payload");
    std::string messageId = getJsonValue(json, "messageId");
    std::string sessionToken = getJsonValue(json, "sessionToken");
    std::string gameId = getJsonValue(payload, "gameId");

    std::string userId = validateSession(sessionToken);
    if (userId.empty() || !isAdmin(userId)) {
        return R"({"messageType":"UPDATE_GAME_RESPONSE","messageId":")" + messageId +
               R"(","timestamp":)" + std::to_string(getCurrentTimestamp()) +
               R"(,"payload":{"status":"error","message":"Unauthorized: Admin access required"}})";
    }

    {
        std::lock_guard<std::mutex> lock(gamesMutex);
        auto it = games.find(gameId);
        if (it == games.end()) {
            return R"({"messageType":"UPDATE_GAME_RESPONSE","messageId":")" + messageId +
                   R"(","timestamp":)" + std::to_string(getCurrentTimestamp()) +
                   R"(,"payload":{"status":"error","message":"Game not found"}})";
        }

        Game& game = it->second;
        std::string title = getJsonValue(payload, "title");
        std::string description = getJsonValue(payload, "description");
        if (!title.empty()) game.title = title;
        if (!description.empty()) game.description = description;
    }

    return R"({"messageType":"UPDATE_GAME_RESPONSE","messageId":")" + messageId +
           R"(","timestamp":)" + std::to_string(getCurrentTimestamp()) +
           R"(,"payload":{"status":"success","message":"Game updated successfully"}}})";
}

// Xử lý DELETE_GAME_REQUEST (Admin only)
std::string handleDeleteGame(const std::string& json) {
    std::string payload = getJsonObject(json, "payload");
    std::string messageId = getJsonValue(json, "messageId");
    std::string sessionToken = getJsonValue(json, "sessionToken");
    std::string gameId = getJsonValue(payload, "gameId");

    std::string userId = validateSession(sessionToken);
    if (userId.empty() || !isAdmin(userId)) {
        return R"({"messageType":"DELETE_GAME_RESPONSE","messageId":")" + messageId +
               R"(","timestamp":)" + std::to_string(getCurrentTimestamp()) +
               R"(,"payload":{"status":"error","message":"Unauthorized: Admin access required"}})";
    }

    {
        std::lock_guard<std::mutex> lock(gamesMutex);
        auto it = games.find(gameId);
        if (it == games.end()) {
            return R"({"messageType":"DELETE_GAME_RESPONSE","messageId":")" + messageId +
                   R"(","timestamp":)" + std::to_string(getCurrentTimestamp()) +
                   R"(,"payload":{"status":"error","message":"Game not found"}})";
        }
        games.erase(it);
    }

    return R"({"messageType":"DELETE_GAME_RESPONSE","messageId":")" + messageId +
           R"(","timestamp":)" + std::to_string(getCurrentTimestamp()) +
           R"(,"payload":{"status":"success","message":"Game deleted successfully"}}})";
}

// Xử lý GET_ADMIN_GAMES_REQUEST (Admin only)
std::string handleGetAdminGames(const std::string& json) {
    std::string messageId = getJsonValue(json, "messageId");
    std::string sessionToken = getJsonValue(json, "sessionToken");

    std::string userId = validateSession(sessionToken);
    if (userId.empty() || !isAdmin(userId)) {
        return R"({"messageType":"GET_ADMIN_GAMES_RESPONSE","messageId":")" + messageId +
               R"(","timestamp":)" + std::to_string(getCurrentTimestamp()) +
               R"(,"payload":{"status":"error","message":"Unauthorized: Admin access required"}})";
    }

    std::stringstream gamesJson;
    gamesJson << "[";
    bool first = true;

    {
        std::lock_guard<std::mutex> lock(gamesMutex);
        for (const auto& pair : games) {
            const Game& game = pair.second;
            if (!first) gamesJson << ",";
            first = false;

            gamesJson << R"({"gameId":")" << game.gameId
                      << R"(","gameType":")" << game.gameType
                      << R"(","title":")" << escapeJson(game.title)
                      << R"(","description":")" << escapeJson(game.description)
                      << R"(","level":")" << game.level
                      << R"(","topic":")" << game.topic
                      << R"(","timeLimit":)" << game.timeLimit
                      << R"(,"maxScore":)" << game.maxScore;

            if (game.gameType == "word_match") {
                gamesJson << R"(,"pairs":[)";
                for (size_t i = 0; i < game.pairs.size(); i++) {
                    if (i > 0) gamesJson << ",";
                    gamesJson << R"({"left":")" << escapeJson(game.pairs[i].first)
                              << R"(","right":")" << escapeJson(game.pairs[i].second) << R"("})";
                }
                gamesJson << "]";
            } else if (game.gameType == "sentence_match") {
                gamesJson << R"(,"pairs":[)";
                for (size_t i = 0; i < game.sentencePairs.size(); i++) {
                    if (i > 0) gamesJson << ",";
                    gamesJson << R"({"left":")" << escapeJson(game.sentencePairs[i].first)
                              << R"(","right":")" << escapeJson(game.sentencePairs[i].second) << R"("})";
                }
                gamesJson << "]";
            } else if (game.gameType == "picture_match") {
                gamesJson << R"(,"pairs":[)";
                for (size_t i = 0; i < game.picturePairs.size(); i++) {
                    if (i > 0) gamesJson << ",";
                    gamesJson << R"({"word":")" << escapeJson(game.picturePairs[i].first)
                              << R"(","imageUrl":")" << escapeJson(game.picturePairs[i].second) << R"("})";
                }
                gamesJson << "]";
            }

            gamesJson << "}";
        }
    }
    gamesJson << "]";

    return R"({"messageType":"GET_ADMIN_GAMES_RESPONSE","messageId":")" + messageId +
           R"(","timestamp":)" + std::to_string(getCurrentTimestamp()) +
           R"(,"payload":{"status":"success","data":{"games":)" + gamesJson.str() + R"(}}})";
}


// Xử lý REVIEW_EXERCISE_REQUEST (Teacher only)
std::string handleReviewExercise(const std::string& json) {
    std::string payload = getJsonObject(json, "payload");
    std::string messageId = getJsonValue(json, "messageId");
    std::string sessionToken = getJsonValue(json, "sessionToken");
    std::string submissionId = getJsonValue(payload, "submissionId");
    std::string feedback = getJsonValue(payload, "feedback");
    std::string scoreStr = getJsonValue(payload, "score");

    std::string userId = validateSession(sessionToken);
    if (userId.empty() || !isTeacher(userId)) {
        return R"({"messageType":"REVIEW_EXERCISE_RESPONSE","messageId":")" + messageId +
               R"(","timestamp":)" + std::to_string(getCurrentTimestamp()) +
               R"(,"payload":{"status":"error","message":"Unauthorized: Teacher access required"}})";
    }

    int score = scoreStr.empty() ? 0 : std::stoi(scoreStr);
    if (score < 0 || score > 100) {
        return R"({"messageType":"REVIEW_EXERCISE_RESPONSE","messageId":")" + messageId +
               R"(","timestamp":)" + std::to_string(getCurrentTimestamp()) +
               R"(,"payload":{"status":"error","message":"Score must be between 0 and 100"}})";
    }

    {
        std::lock_guard<std::mutex> lock(exercisesMutex);
        for (auto& submission : exerciseSubmissions) {
            if (submission.submissionId == submissionId) {
                submission.status = "reviewed";
                submission.teacherId = userId;
                submission.teacherFeedback = feedback;
                submission.teacherScore = score;
                submission.reviewedAt = getCurrentTimestamp();

                // Send notification to student if online
                {
                    std::lock_guard<std::mutex> userLock(usersMutex);
                    auto it = userById.find(submission.userId);
                    if (it != userById.end() && it->second->online && it->second->clientSocket > 0) {
                        std::string notification = R"({"messageType":"EXERCISE_FEEDBACK_NOTIFICATION","messageId":")" +
                                                   generateId("notif") +
                                                   R"(","timestamp":)" + std::to_string(getCurrentTimestamp()) +
                                                   R"(,"payload":{"submissionId":")" + submissionId +
                                                   R"(","exerciseId":")" + submission.exerciseId +
                                                   R"(","feedback":")" + escapeJson(feedback) +
                                                   R"(","score":)" + std::to_string(score) + R"(}})";

                        uint32_t len = htonl(notification.length());
                        send(it->second->clientSocket, &len, sizeof(len), 0);
                        send(it->second->clientSocket, notification.c_str(), notification.length(), 0);
                        logMessage("SEND", "Client:" + std::to_string(it->second->clientSocket), "EXERCISE_FEEDBACK_NOTIFICATION");
                    }
                }

                return R"({"messageType":"REVIEW_EXERCISE_RESPONSE","messageId":")" + messageId +
                       R"(","timestamp":)" + std::to_string(getCurrentTimestamp()) +
                       R"(,"payload":{"status":"success","message":"Exercise reviewed successfully"}}})";
            }
        }
    }

    return R"({"messageType":"REVIEW_EXERCISE_RESPONSE","messageId":")" + messageId +
           R"(","timestamp":)" + std::to_string(getCurrentTimestamp()) +
           R"(,"payload":{"status":"error","message":"Submission not found"}}})";
}

// Xử lý GET_FEEDBACK_REQUEST (Student)
std::string handleGetFeedback(const std::string& json) {
    std::string payload = getJsonObject(json, "payload");
    std::string messageId = getJsonValue(json, "messageId");
    std::string sessionToken = getJsonValue(json, "sessionToken");
    std::string submissionId = getJsonValue(payload, "submissionId");

    std::string userId = validateSession(sessionToken);
    if (userId.empty()) {
        return R"({"messageType":"GET_FEEDBACK_RESPONSE","messageId":")" + messageId +
               R"(","timestamp":)" + std::to_string(getCurrentTimestamp()) +
               R"(,"payload":{"status":"error","message":"Invalid or expired session"}})";
    }

    {
        std::lock_guard<std::mutex> lock(exercisesMutex);
        for (const auto& submission : exerciseSubmissions) {
            if (submission.submissionId == submissionId && submission.userId == userId) {
                if (submission.status == "pending") {
                    return R"({"messageType":"GET_FEEDBACK_RESPONSE","messageId":")" + messageId +
                           R"(","timestamp":)" + std::to_string(getCurrentTimestamp()) +
                           R"(,"payload":{"status":"success","data":{"status":"pending","message":"Your submission is still being reviewed"}}})";
                }

                std::string teacherName = "Unknown";
                {
                    std::lock_guard<std::mutex> userLock(usersMutex);
                    auto it = userById.find(submission.teacherId);
                    if (it != userById.end()) {
                        teacherName = it->second->fullname;
                    }
                }

                return R"({"messageType":"GET_FEEDBACK_RESPONSE","messageId":")" + messageId +
                       R"(","timestamp":)" + std::to_string(getCurrentTimestamp()) +
                       R"(,"payload":{"status":"success","data":{"submissionId":")" + submission.submissionId +
                       R"(","exerciseId":")" + submission.exerciseId +
                       R"(","status":"reviewed","teacherName":")" + escapeJson(teacherName) +
                       R"(","feedback":")" + escapeJson(submission.teacherFeedback) +
                       R"(","score":)" + std::to_string(submission.teacherScore) +
                       R"(,"reviewedAt":)" + std::to_string(submission.reviewedAt) + R"(}}})";
            }
        }
    }

    return R"({"messageType":"GET_FEEDBACK_RESPONSE","messageId":")" + messageId +
           R"(","timestamp":)" + std::to_string(getCurrentTimestamp()) +
           R"(,"payload":{"status":"error","message":"Submission not found"}}})";
}

// Xử lý GET_USER_SUBMISSIONS_REQUEST (Student views all their submissions with feedback)
std::string handleGetUserSubmissions(const std::string& json) {
    std::string messageId = getJsonValue(json, "messageId");
    std::string sessionToken = getJsonValue(json, "sessionToken");

    std::string userId = validateSession(sessionToken);
    if (userId.empty()) {
        return R"({"messageType":"GET_USER_SUBMISSIONS_RESPONSE","messageId":")" + messageId +
               R"(","timestamp":)" + std::to_string(getCurrentTimestamp()) +
               R"(,"payload":{"status":"error","message":"Invalid or expired session"}})";
    }

    std::string submissionsJson = "[";
    bool first = true;

    {
        std::lock_guard<std::mutex> lock(exercisesMutex);
        for (const auto& submission : exerciseSubmissions) {
            if (submission.userId == userId) {
                if (!first) submissionsJson += ",";
                first = false;

                // Get exercise title
                std::string exerciseTitle = "Unknown Exercise";
                auto exIt = exercises.find(submission.exerciseId);
                if (exIt != exercises.end()) {
                    exerciseTitle = exIt->second.title;
                }

                // Get teacher name if reviewed
                std::string teacherName = "";
                if (submission.status == "reviewed" && !submission.teacherId.empty()) {
                    std::lock_guard<std::mutex> userLock(usersMutex);
                    auto it = userById.find(submission.teacherId);
                    if (it != userById.end()) {
                        teacherName = it->second->fullname;
                    }
                }

                submissionsJson += R"({"submissionId":")" + submission.submissionId +
                                   R"(","exerciseId":")" + submission.exerciseId +
                                   R"(","exerciseTitle":")" + escapeJson(exerciseTitle) +
                                   R"(","exerciseType":")" + submission.exerciseType +
                                   R"(","status":")" + submission.status +
                                   R"(","submittedAt":)" + std::to_string(submission.submittedAt);

                if (submission.status == "reviewed") {
                    submissionsJson += R"(,"teacherId":")" + submission.teacherId +
                                       R"(","teacherName":")" + escapeJson(teacherName) +
                                       R"(","feedback":")" + escapeJson(submission.teacherFeedback) +
                                       R"(","score":)" + std::to_string(submission.teacherScore) +
                                       R"(,"reviewedAt":)" + std::to_string(submission.reviewedAt);
                }
                submissionsJson += "}";
            }
        }
    }
    submissionsJson += "]";

    return R"({"messageType":"GET_USER_SUBMISSIONS_RESPONSE","messageId":")" + messageId +
           R"(","timestamp":)" + std::to_string(getCurrentTimestamp()) +
           R"(,"payload":{"status":"success","submissions":)" + submissionsJson + R"(}})";
}

// Xử lý GET_PENDING_REVIEWS_REQUEST (Teacher views submissions pending review)
std::string handleGetPendingReviews(const std::string& json) {
    std::string messageId = getJsonValue(json, "messageId");
    std::string sessionToken = getJsonValue(json, "sessionToken");

    std::string userId = validateSession(sessionToken);
    if (userId.empty() || !isTeacher(userId)) {
        return R"({"messageType":"GET_PENDING_REVIEWS_RESPONSE","messageId":")" + messageId +
               R"(","timestamp":)" + std::to_string(getCurrentTimestamp()) +
               R"(,"payload":{"status":"error","message":"Unauthorized: Teacher access required"}})";
    }

    std::string submissionsJson = "[";
    bool first = true;

    {
        std::lock_guard<std::mutex> lock(exercisesMutex);
        for (const auto& submission : exerciseSubmissions) {
            if (submission.status == "pending") {
                if (!first) submissionsJson += ",";
                first = false;

                // Get exercise title
                std::string exerciseTitle = "Unknown Exercise";
                auto exIt = exercises.find(submission.exerciseId);
                if (exIt != exercises.end()) {
                    exerciseTitle = exIt->second.title;
                }

                // Get student name
                std::string studentName = "Unknown";
                {
                    std::lock_guard<std::mutex> userLock(usersMutex);
                    auto it = userById.find(submission.userId);
                    if (it != userById.end()) {
                        studentName = it->second->fullname;
                    }
                }

                submissionsJson += R"({"submissionId":")" + submission.submissionId +
                                   R"(","exerciseId":")" + submission.exerciseId +
                                   R"(","exerciseTitle":")" + escapeJson(exerciseTitle) +
                                   R"(","exerciseType":")" + submission.exerciseType +
                                   R"(","studentId":")" + submission.userId +
                                   R"(","studentName":")" + escapeJson(studentName) +
                                   R"(","content":")" + escapeJson(submission.content) +
                                   R"(","submittedAt":)" + std::to_string(submission.submittedAt) + "}";
            }
        }
    }
    submissionsJson += "]";

    return R"({"messageType":"GET_PENDING_REVIEWS_RESPONSE","messageId":")" + messageId +
           R"(","timestamp":)" + std::to_string(getCurrentTimestamp()) +
           R"(,"payload":{"status":"success","submissions":)" + submissionsJson + R"(}})";
}

// Xử lý SET_LEVEL_REQUEST
std::string handleSetLevel(const std::string& json) {
    std::string payload = getJsonObject(json, "payload");
    std::string messageId = getJsonValue(json, "messageId");
    std::string sessionToken = getJsonValue(json, "sessionToken");
    std::string level = getJsonValue(payload, "level");

    std::string userId = validateSession(sessionToken);
    if (userId.empty()) {
        return R"({"messageType":"SET_LEVEL_RESPONSE","messageId":")" + messageId +
               R"(","timestamp":)" + std::to_string(getCurrentTimestamp()) +
               R"(,"payload":{"status":"error","message":"Invalid or expired session"}})";
    }

    if (level != "beginner" && level != "intermediate" && level != "advanced") {
        return R"({"messageType":"SET_LEVEL_RESPONSE","messageId":")" + messageId +
               R"(","timestamp":)" + std::to_string(getCurrentTimestamp()) +
               R"(,"payload":{"status":"error","message":"Invalid level"}})";
    }

    {
        std::lock_guard<std::mutex> lock(usersMutex);
        auto it = userById.find(userId);
        if (it != userById.end()) {
            it->second->level = level;
        }
    }

    return R"({"messageType":"SET_LEVEL_RESPONSE","messageId":")" + messageId +
           R"(","timestamp":)" + std::to_string(getCurrentTimestamp()) +
           R"(,"payload":{"status":"success","message":"Level updated successfully","data":{"level":")" + level + R"("}}})";
}

// ============================================================================
// VOICE CALL HANDLERS
// ============================================================================

// Helper: Send push notification to a user's socket
void sendPushToUser(const std::string& userId, const std::string& message) {
    std::lock_guard<std::mutex> userLock(usersMutex);
    auto it = userById.find(userId);
    if (it != userById.end() && it->second->online && it->second->clientSocket >= 0) {
        int socket = it->second->clientSocket;
        uint32_t len = htonl(message.length());
        send(socket, &len, sizeof(len), 0);
        send(socket, message.c_str(), message.length(), 0);
    }
}

// Handle VOICE_CALL_INITIATE_REQUEST
std::string handleVoiceCallInitiate(const std::string& json) {
    std::string payload = getJsonObject(json, "payload");
    std::string messageId = getJsonValue(json, "messageId");
    std::string sessionToken = getJsonValue(json, "sessionToken");
    std::string receiverId = getJsonValue(payload, "receiverId");
    std::string audioSource = getJsonValue(payload, "audioSource");
    if (audioSource.empty()) audioSource = "microphone";

    std::string callerId = validateSession(sessionToken);
    if (callerId.empty()) {
        return R"({"messageType":"VOICE_CALL_INITIATE_RESPONSE","messageId":")" + messageId +
               R"(","timestamp":)" + std::to_string(getCurrentTimestamp()) +
               R"(,"payload":{"status":"error","message":"Invalid or expired session"}})";
    }

    // Check if receiver exists and is online
    User* receiver = nullptr;
    std::string callerName;
    {
        std::lock_guard<std::mutex> lock(usersMutex);
        auto it = userById.find(receiverId);
        if (it != userById.end()) {
            receiver = it->second;
        }
        auto callerIt = userById.find(callerId);
        if (callerIt != userById.end()) {
            callerName = callerIt->second->fullname;
        }
    }

    if (!receiver) {
        return R"({"messageType":"VOICE_CALL_INITIATE_RESPONSE","messageId":")" + messageId +
               R"(","timestamp":)" + std::to_string(getCurrentTimestamp()) +
               R"(,"payload":{"status":"error","message":"Receiver not found"}})";
    }

    if (!receiver->online) {
        return R"({"messageType":"VOICE_CALL_INITIATE_RESPONSE","messageId":")" + messageId +
               R"(","timestamp":)" + std::to_string(getCurrentTimestamp()) +
               R"(,"payload":{"status":"error","message":"Receiver is offline"}})";
    }

    // Check for existing active/pending calls
    {
        std::lock_guard<std::mutex> lock(voiceCallMutex);
        for (const auto& p : voiceCalls) {
            if (p.second.involvesUser(callerId) &&
                (p.second.isActive() || p.second.isPending())) {
                return R"({"messageType":"VOICE_CALL_INITIATE_RESPONSE","messageId":")" + messageId +
                       R"(","timestamp":)" + std::to_string(getCurrentTimestamp()) +
                       R"(,"payload":{"status":"error","message":"You are already in a call"}})";
            }
            if (p.second.involvesUser(receiverId) && p.second.isActive()) {
                return R"({"messageType":"VOICE_CALL_INITIATE_RESPONSE","messageId":")" + messageId +
                       R"(","timestamp":)" + std::to_string(getCurrentTimestamp()) +
                       R"(,"payload":{"status":"error","message":"Receiver is already in a call"}})";
            }
        }
    }

    // Create call session
    VoiceCallSession call;
    call.callId = generateId("call");
    call.callerId = callerId;
    call.receiverId = receiverId;
    call.status = english_learning::core::VoiceCallStatus::Pending;
    call.audioSource = audioSource;
    call.startTime = getCurrentTimestamp();

    {
        std::lock_guard<std::mutex> lock(voiceCallMutex);
        voiceCalls[call.callId] = call;
    }

    // Send push notification to receiver
    std::string incomingNotification = R"({"messageType":"VOICE_CALL_INCOMING","timestamp":)" +
        std::to_string(getCurrentTimestamp()) +
        R"(,"payload":{"callId":")" + call.callId +
        R"(","callerId":")" + callerId +
        R"(","callerName":")" + escapeJson(callerName) +
        R"(","audioSource":")" + audioSource + R"("}})";
    sendPushToUser(receiverId, incomingNotification);

    return R"({"messageType":"VOICE_CALL_INITIATE_RESPONSE","messageId":")" + messageId +
           R"(","timestamp":)" + std::to_string(getCurrentTimestamp()) +
           R"(,"payload":{"status":"success","data":{"callId":")" + call.callId +
           R"(","receiverId":")" + receiverId +
           R"(","receiverName":")" + escapeJson(receiver->fullname) +
           R"(","callStatus":"pending"}}})";
}

// Handle VOICE_CALL_ACCEPT_REQUEST
std::string handleVoiceCallAccept(const std::string& json) {
    std::string payload = getJsonObject(json, "payload");
    std::string messageId = getJsonValue(json, "messageId");
    std::string sessionToken = getJsonValue(json, "sessionToken");
    std::string callId = getJsonValue(payload, "callId");

    std::string userId = validateSession(sessionToken);
    if (userId.empty()) {
        return R"({"messageType":"VOICE_CALL_ACCEPT_RESPONSE","messageId":")" + messageId +
               R"(","timestamp":)" + std::to_string(getCurrentTimestamp()) +
               R"(,"payload":{"status":"error","message":"Invalid or expired session"}})";
    }

    VoiceCallSession* call = nullptr;
    std::string callerName, receiverName;
    {
        std::lock_guard<std::mutex> lock(voiceCallMutex);
        auto it = voiceCalls.find(callId);
        if (it != voiceCalls.end()) {
            call = &it->second;
        }
    }

    if (!call) {
        return R"({"messageType":"VOICE_CALL_ACCEPT_RESPONSE","messageId":")" + messageId +
               R"(","timestamp":)" + std::to_string(getCurrentTimestamp()) +
               R"(,"payload":{"status":"error","message":"Call not found"}})";
    }

    if (call->receiverId != userId) {
        return R"({"messageType":"VOICE_CALL_ACCEPT_RESPONSE","messageId":")" + messageId +
               R"(","timestamp":)" + std::to_string(getCurrentTimestamp()) +
               R"(,"payload":{"status":"error","message":"Only the receiver can accept the call"}})";
    }

    if (!call->isPending()) {
        return R"({"messageType":"VOICE_CALL_ACCEPT_RESPONSE","messageId":")" + messageId +
               R"(","timestamp":)" + std::to_string(getCurrentTimestamp()) +
               R"(,"payload":{"status":"error","message":"Call is not pending"}})";
    }

    // Accept the call
    {
        std::lock_guard<std::mutex> lock(voiceCallMutex);
        call->accept(getCurrentTimestamp());
    }

    // Get names
    {
        std::lock_guard<std::mutex> lock(usersMutex);
        auto callerIt = userById.find(call->callerId);
        if (callerIt != userById.end()) callerName = callerIt->second->fullname;
        auto receiverIt = userById.find(call->receiverId);
        if (receiverIt != userById.end()) receiverName = receiverIt->second->fullname;
    }

    // Notify caller
    std::string acceptNotification = R"({"messageType":"VOICE_CALL_ACCEPTED","timestamp":)" +
        std::to_string(getCurrentTimestamp()) +
        R"(,"payload":{"callId":")" + callId +
        R"(","receiverId":")" + userId +
        R"(","receiverName":")" + escapeJson(receiverName) + R"("}})";
    sendPushToUser(call->callerId, acceptNotification);

    return R"({"messageType":"VOICE_CALL_ACCEPT_RESPONSE","messageId":")" + messageId +
           R"(","timestamp":)" + std::to_string(getCurrentTimestamp()) +
           R"(,"payload":{"status":"success","data":{"callId":")" + callId +
           R"(","callStatus":"active","callerId":")" + call->callerId +
           R"(","callerName":")" + escapeJson(callerName) + R"("}}})";
}

// Handle VOICE_CALL_REJECT_REQUEST
std::string handleVoiceCallReject(const std::string& json) {
    std::string payload = getJsonObject(json, "payload");
    std::string messageId = getJsonValue(json, "messageId");
    std::string sessionToken = getJsonValue(json, "sessionToken");
    std::string callId = getJsonValue(payload, "callId");

    std::string userId = validateSession(sessionToken);
    if (userId.empty()) {
        return R"({"messageType":"VOICE_CALL_REJECT_RESPONSE","messageId":")" + messageId +
               R"(","timestamp":)" + std::to_string(getCurrentTimestamp()) +
               R"(,"payload":{"status":"error","message":"Invalid or expired session"}})";
    }

    VoiceCallSession* call = nullptr;
    std::string callerName;
    {
        std::lock_guard<std::mutex> lock(voiceCallMutex);
        auto it = voiceCalls.find(callId);
        if (it != voiceCalls.end()) {
            call = &it->second;
        }
    }

    if (!call) {
        return R"({"messageType":"VOICE_CALL_REJECT_RESPONSE","messageId":")" + messageId +
               R"(","timestamp":)" + std::to_string(getCurrentTimestamp()) +
               R"(,"payload":{"status":"error","message":"Call not found"}})";
    }

    if (call->receiverId != userId) {
        return R"({"messageType":"VOICE_CALL_REJECT_RESPONSE","messageId":")" + messageId +
               R"(","timestamp":)" + std::to_string(getCurrentTimestamp()) +
               R"(,"payload":{"status":"error","message":"Only the receiver can reject the call"}})";
    }

    if (!call->isPending()) {
        return R"({"messageType":"VOICE_CALL_REJECT_RESPONSE","messageId":")" + messageId +
               R"(","timestamp":)" + std::to_string(getCurrentTimestamp()) +
               R"(,"payload":{"status":"error","message":"Call is not pending"}})";
    }

    // Reject the call
    {
        std::lock_guard<std::mutex> lock(voiceCallMutex);
        call->reject(getCurrentTimestamp());
    }

    // Get caller name
    {
        std::lock_guard<std::mutex> lock(usersMutex);
        auto callerIt = userById.find(call->callerId);
        if (callerIt != userById.end()) callerName = callerIt->second->fullname;
    }

    // Notify caller
    std::string rejectNotification = R"({"messageType":"VOICE_CALL_REJECTED","timestamp":)" +
        std::to_string(getCurrentTimestamp()) +
        R"(,"payload":{"callId":")" + callId +
        R"(","receiverId":")" + userId + R"("}})";
    sendPushToUser(call->callerId, rejectNotification);

    return R"({"messageType":"VOICE_CALL_REJECT_RESPONSE","messageId":")" + messageId +
           R"(","timestamp":)" + std::to_string(getCurrentTimestamp()) +
           R"(,"payload":{"status":"success","data":{"callId":")" + callId +
           R"(","callStatus":"rejected"}}})";
}

// Handle VOICE_CALL_END_REQUEST
std::string handleVoiceCallEnd(const std::string& json) {
    std::string payload = getJsonObject(json, "payload");
    std::string messageId = getJsonValue(json, "messageId");
    std::string sessionToken = getJsonValue(json, "sessionToken");
    std::string callId = getJsonValue(payload, "callId");

    std::string userId = validateSession(sessionToken);
    if (userId.empty()) {
        return R"({"messageType":"VOICE_CALL_END_RESPONSE","messageId":")" + messageId +
               R"(","timestamp":)" + std::to_string(getCurrentTimestamp()) +
               R"(,"payload":{"status":"error","message":"Invalid or expired session"}})";
    }

    VoiceCallSession* call = nullptr;
    {
        std::lock_guard<std::mutex> lock(voiceCallMutex);
        auto it = voiceCalls.find(callId);
        if (it != voiceCalls.end()) {
            call = &it->second;
        }
    }

    if (!call) {
        return R"({"messageType":"VOICE_CALL_END_RESPONSE","messageId":")" + messageId +
               R"(","timestamp":)" + std::to_string(getCurrentTimestamp()) +
               R"(,"payload":{"status":"error","message":"Call not found"}})";
    }

    if (!call->involvesUser(userId)) {
        return R"({"messageType":"VOICE_CALL_END_RESPONSE","messageId":")" + messageId +
               R"(","timestamp":)" + std::to_string(getCurrentTimestamp()) +
               R"(,"payload":{"status":"error","message":"You are not a participant of this call"}})";
    }

    if (call->hasEnded()) {
        return R"({"messageType":"VOICE_CALL_END_RESPONSE","messageId":")" + messageId +
               R"(","timestamp":)" + std::to_string(getCurrentTimestamp()) +
               R"(,"payload":{"status":"error","message":"Call has already ended"}})";
    }

    std::string otherUserId = (call->callerId == userId) ? call->receiverId : call->callerId;
    int64_t duration = 0;

    // End the call
    {
        std::lock_guard<std::mutex> lock(voiceCallMutex);
        call->end(getCurrentTimestamp());
        duration = call->getDurationSeconds();
    }

    // Notify other participant
    std::string endNotification = R"({"messageType":"VOICE_CALL_ENDED","timestamp":)" +
        std::to_string(getCurrentTimestamp()) +
        R"(,"payload":{"callId":")" + callId +
        R"(","endedBy":")" + userId +
        R"(","duration":)" + std::to_string(duration) + R"(}})";
    sendPushToUser(otherUserId, endNotification);

    return R"({"messageType":"VOICE_CALL_END_RESPONSE","messageId":")" + messageId +
           R"(","timestamp":)" + std::to_string(getCurrentTimestamp()) +
           R"(,"payload":{"status":"success","data":{"callId":")" + callId +
           R"(","callStatus":"ended","duration":)" + std::to_string(duration) + R"(}}})";
}

// Handle VOICE_CALL_GET_STATUS_REQUEST
std::string handleVoiceCallGetStatus(const std::string& json) {
    std::string payload = getJsonObject(json, "payload");
    std::string messageId = getJsonValue(json, "messageId");
    std::string sessionToken = getJsonValue(json, "sessionToken");
    std::string callId = getJsonValue(payload, "callId");

    std::string userId = validateSession(sessionToken);
    if (userId.empty()) {
        return R"({"messageType":"VOICE_CALL_GET_STATUS_RESPONSE","messageId":")" + messageId +
               R"(","timestamp":)" + std::to_string(getCurrentTimestamp()) +
               R"(,"payload":{"status":"error","message":"Invalid or expired session"}})";
    }

    VoiceCallSession call;
    bool found = false;
    {
        std::lock_guard<std::mutex> lock(voiceCallMutex);
        auto it = voiceCalls.find(callId);
        if (it != voiceCalls.end()) {
            call = it->second;
            found = true;
        }
    }

    if (!found) {
        return R"({"messageType":"VOICE_CALL_GET_STATUS_RESPONSE","messageId":")" + messageId +
               R"(","timestamp":)" + std::to_string(getCurrentTimestamp()) +
               R"(,"payload":{"status":"error","message":"Call not found"}})";
    }

    std::string callerName, receiverName;
    {
        std::lock_guard<std::mutex> lock(usersMutex);
        auto callerIt = userById.find(call.callerId);
        if (callerIt != userById.end()) callerName = callerIt->second->fullname;
        auto receiverIt = userById.find(call.receiverId);
        if (receiverIt != userById.end()) receiverName = receiverIt->second->fullname;
    }

    std::string statusStr = english_learning::core::voiceCallStatusToString(call.status);

    return R"({"messageType":"VOICE_CALL_GET_STATUS_RESPONSE","messageId":")" + messageId +
           R"(","timestamp":)" + std::to_string(getCurrentTimestamp()) +
           R"(,"payload":{"status":"success","data":{"callId":")" + call.callId +
           R"(","callerId":")" + call.callerId +
           R"(","callerName":")" + escapeJson(callerName) +
           R"(","receiverId":")" + call.receiverId +
           R"(","receiverName":")" + escapeJson(receiverName) +
           R"(","callStatus":")" + statusStr +
           R"(","startTime":)" + std::to_string(call.startTime) +
           R"(,"acceptTime":)" + std::to_string(call.acceptTime) +
           R"(,"endTime":)" + std::to_string(call.endTime) +
           R"(,"duration":)" + std::to_string(call.getDurationSeconds()) + R"(}}})";
}

// ============================================================================
// XỬ LÝ CLIENT
// ============================================================================

void handleClient(int clientSocket, struct sockaddr_in clientAddr) {
    char clientIP[INET_ADDRSTRLEN];
    inet_ntop(AF_INET, &clientAddr.sin_addr, clientIP, INET_ADDRSTRLEN);
    std::string clientInfo = std::string(clientIP) + ":" + std::to_string(ntohs(clientAddr.sin_port));

    std::cout << "[INFO] New connection from " << clientInfo << std::endl;

    char buffer[BUFFER_SIZE];

    while (running) {
        uint32_t msgLen = 0;
        ssize_t bytesRead = recv(clientSocket, &msgLen, sizeof(msgLen), MSG_WAITALL);

        if (bytesRead <= 0) {
            std::cout << "[INFO] Client " << clientInfo << " disconnected" << std::endl;
            break;
        }

        msgLen = ntohl(msgLen);
        if (msgLen > BUFFER_SIZE - 1) {
            std::cout << "[ERROR] Message too long from " << clientInfo << std::endl;
            continue;
        }

        memset(buffer, 0, BUFFER_SIZE);
        bytesRead = recv(clientSocket, buffer, msgLen, MSG_WAITALL);

        if (bytesRead <= 0) {
            std::cout << "[INFO] Client " << clientInfo << " disconnected" << std::endl;
            break;
        }

        std::string message(buffer, bytesRead);
        logMessage("RECV", clientInfo, message);

        std::string messageType = getJsonValue(message, "messageType");
        std::string response;

        if (messageType == "REGISTER_REQUEST") {
            response = handleRegister(message);
        }
        else if (messageType == "LOGIN_REQUEST") {
            response = handleLogin(message, clientSocket);
            // handleLogin đã tự gửi response nên không cần gửi lại
            if (response.empty()) {
                continue;
            }
        }
        else if (messageType == "GET_LESSONS_REQUEST") {
            response = handleGetLessons(message);
        }
        else if (messageType == "GET_LESSON_DETAIL_REQUEST") {
            response = handleGetLessonDetail(message);
        }
        else if (messageType == "GET_TEST_REQUEST") {
            response = handleGetTest(message);
        }
        else if (messageType == "SUBMIT_TEST_REQUEST") {
            response = handleSubmitTest(message);
        }
        else if (messageType == "GET_EXERCISE_REQUEST") {
            response = handleGetExercise(message);
        }
        else if (messageType == "SUBMIT_EXERCISE_REQUEST") {
            response = handleSubmitExercise(message);
        }
        else if (messageType == "GET_USER_SUBMISSIONS_REQUEST") {
            response = handleGetUserSubmissions(message);
        }
        else if (messageType == "GET_FEEDBACK_REQUEST") {
            response = handleGetFeedback(message);
        }
        else if (messageType == "GET_PENDING_REVIEWS_REQUEST") {
            response = handleGetPendingReviews(message);
        }
        else if (messageType == "REVIEW_EXERCISE_REQUEST") {
            response = handleReviewExercise(message);
        }
        else if (messageType == "GET_GAME_LIST_REQUEST") {
            response = handleGetGameList(message);
        }
        else if (messageType == "START_GAME_REQUEST") {
            response = handleStartGame(message);
        }
        else if (messageType == "SUBMIT_GAME_RESULT_REQUEST") {
            response = handleSubmitGameResult(message);
        }
        else if (messageType == "GET_CONTACT_LIST_REQUEST") {
            response = handleGetContactList(message);
        }
        else if (messageType == "SEND_MESSAGE_REQUEST") {
            response = handleSendMessage(message, clientSocket);
        }
        else if (messageType == "GET_CHAT_HISTORY_REQUEST") {
            response = handleGetChatHistory(message);
        }
        else if (messageType == "SET_LEVEL_REQUEST") {
            response = handleSetLevel(message);
        }
        else if (messageType == "ADD_GAME_REQUEST") {
            response = handleAddGame(message);
        }
        else if (messageType == "UPDATE_GAME_REQUEST") {
            response = handleUpdateGame(message);
        }
        else if (messageType == "DELETE_GAME_REQUEST") {
            response = handleDeleteGame(message);
        }
        else if (messageType == "GET_ADMIN_GAMES_REQUEST") {
            response = handleGetAdminGames(message);
        }
        else if (messageType == "MARK_MESSAGES_READ_REQUEST") {
            response = handleMarkMessagesRead(message);
        }
        // Voice Call handlers
        else if (messageType == "VOICE_CALL_INITIATE_REQUEST") {
            response = handleVoiceCallInitiate(message);
        }
        else if (messageType == "VOICE_CALL_ACCEPT_REQUEST") {
            response = handleVoiceCallAccept(message);
        }
        else if (messageType == "VOICE_CALL_REJECT_REQUEST") {
            response = handleVoiceCallReject(message);
        }
        else if (messageType == "VOICE_CALL_END_REQUEST") {
            response = handleVoiceCallEnd(message);
        }
        else if (messageType == "VOICE_CALL_GET_STATUS_REQUEST") {
            response = handleVoiceCallGetStatus(message);
        }
        else {
            response = R"({"messageType":"ERROR_RESPONSE","timestamp":)" +
                       std::to_string(getCurrentTimestamp()) +
                       R"(,"payload":{"status":"error","message":"Unknown message type"}})";
        }

        uint32_t respLen = htonl(response.length());
        send(clientSocket, &respLen, sizeof(respLen), 0);
        send(clientSocket, response.c_str(), response.length(), 0);

        logMessage("SEND", clientInfo, response);
    }

    // Cleanup
    {
        std::lock_guard<std::mutex> lock(sessionsMutex);
        auto it = clientSessions.find(clientSocket);
        if (it != clientSessions.end()) {
            std::string token = it->second;
            auto sessionIt = sessions.find(token);
            if (sessionIt != sessions.end()) {
                std::string uid = sessionIt->second.userId;
                std::lock_guard<std::mutex> userLock(usersMutex);
                auto userIt = userById.find(uid);
                if (userIt != userById.end()) {
                    userIt->second->online = false;
                    userIt->second->clientSocket = -1;
                }
            }
            clientSessions.erase(it);
        }
    }

    close(clientSocket);
}

// ============================================================================
// SIGNAL HANDLER & MAIN
// ============================================================================

void signalHandler(int signal) {
    std::cout << "\n[INFO] Shutting down server..." << std::endl;
    running = false;
    if (serverSocket >= 0) {
        close(serverSocket);
    }
    exit(0);
}

int main(int argc, char* argv[]) {
    int port = DEFAULT_PORT;
    if (argc > 1) {
        port = std::stoi(argv[1]);
    }

    signal(SIGINT, signalHandler);
    signal(SIGTERM, signalHandler);
    signal(SIGPIPE, SIG_IGN);

    initSampleData();

    // ========================================================================
    // INITIALIZE SERVICE LAYER
    // ========================================================================
    // Create bridge repositories that wrap the global data structures
    static bridge::BridgeUserRepository userRepo(users, userById, usersMutex);
    static bridge::BridgeSessionRepository sessionRepo(sessions, clientSessions, sessionsMutex);
    static bridge::BridgeLessonRepository lessonRepo(lessons);
    static bridge::BridgeTestRepository testRepo(tests);
    static bridge::BridgeChatRepository chatRepo(chatMessages, chatMutex);
    static bridge::BridgeExerciseRepository exerciseRepo(exercises, exerciseSubmissions, exercisesMutex);
    static bridge::BridgeGameRepository gameRepo(games, gameSessions, gamesMutex);
    static bridge::BridgeVoiceCallRepository voiceCallRepo(voiceCalls, voiceCallMutex);

    // Create service container with dependency injection
    serviceContainer = std::make_unique<service::ServiceContainer>(
        userRepo, sessionRepo, lessonRepo, testRepo, chatRepo, exerciseRepo, gameRepo, voiceCallRepo);

    std::cout << "[INFO] Service layer initialized" << std::endl;
    // ========================================================================

    serverSocket = socket(AF_INET, SOCK_STREAM, 0);
    if (serverSocket < 0) {
        std::cerr << "[ERROR] Cannot create socket" << std::endl;
        return 1;
    }

    int opt = 1;
    setsockopt(serverSocket, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    struct sockaddr_in serverAddr;
    memset(&serverAddr, 0, sizeof(serverAddr));
    serverAddr.sin_family = AF_INET;
    serverAddr.sin_addr.s_addr = INADDR_ANY;
    serverAddr.sin_port = htons(port);

    if (bind(serverSocket, (struct sockaddr*)&serverAddr, sizeof(serverAddr)) < 0) {
        std::cerr << "[ERROR] Cannot bind to port " << port << std::endl;
        close(serverSocket);
        return 1;
    }

    if (listen(serverSocket, MAX_CLIENTS) < 0) {
        std::cerr << "[ERROR] Listen failed" << std::endl;
        close(serverSocket);
        return 1;
    }

    std::cout << "============================================" << std::endl;
    std::cout << "   ENGLISH LEARNING APP - SERVER" << std::endl;
    std::cout << "============================================" << std::endl;
    std::cout << "[INFO] Server started on port " << port << std::endl;
    std::cout << "[INFO] Press Ctrl+C to stop" << std::endl;
    std::cout << "--------------------------------------------" << std::endl;
    std::cout << "Sample accounts:" << std::endl;
    std::cout << "  - student@example.com / student123" << std::endl;
    std::cout << "  - student2@example.com / student123" << std::endl;
    std::cout << "  - sarah@example.com / teacher123" << std::endl;
    std::cout << "--------------------------------------------" << std::endl;
    std::cout << "Lessons: " << lessons.size() << " | Tests: " << tests.size() << std::endl;
    std::cout << "--------------------------------------------" << std::endl;

    while (running) {
        struct sockaddr_in clientAddr;
        socklen_t clientLen = sizeof(clientAddr);

        int clientSock = accept(serverSocket, (struct sockaddr*)&clientAddr, &clientLen);

        if (clientSock < 0) {
            if (running) std::cerr << "[ERROR] Accept failed" << std::endl;
            continue;
        }

        std::thread clientThread(handleClient, clientSock, clientAddr);
        clientThread.detach();
    }

    close(serverSocket);
    return 0;
}
