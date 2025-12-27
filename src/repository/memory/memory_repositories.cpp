#include "memory_repositories.h"

namespace english_learning {
namespace repository {
namespace memory {

// ============================================================================
// MemoryLessonRepository
// ============================================================================

bool MemoryLessonRepository::add(const core::Lesson& lesson) {
    std::lock_guard<std::mutex> lock(mutex_);
    if (lessons_.find(lesson.lessonId) != lessons_.end()) return false;
    lessons_[lesson.lessonId] = lesson;
    return true;
}

std::optional<core::Lesson> MemoryLessonRepository::findById(const std::string& lessonId) const {
    std::lock_guard<std::mutex> lock(mutex_);
    auto it = lessons_.find(lessonId);
    return (it != lessons_.end()) ? std::optional(it->second) : std::nullopt;
}

std::vector<core::Lesson> MemoryLessonRepository::findAll() const {
    std::lock_guard<std::mutex> lock(mutex_);
    std::vector<core::Lesson> result;
    for (const auto& p : lessons_) result.push_back(p.second);
    return result;
}

std::vector<core::Lesson> MemoryLessonRepository::findByLevel(const std::string& level) const {
    std::lock_guard<std::mutex> lock(mutex_);
    std::vector<core::Lesson> result;
    for (const auto& p : lessons_) {
        if (p.second.level == level) result.push_back(p.second);
    }
    return result;
}

std::vector<core::Lesson> MemoryLessonRepository::findByTopic(const std::string& topic) const {
    std::lock_guard<std::mutex> lock(mutex_);
    std::vector<core::Lesson> result;
    for (const auto& p : lessons_) {
        if (p.second.topic == topic) result.push_back(p.second);
    }
    return result;
}

std::vector<core::Lesson> MemoryLessonRepository::findByLevelAndTopic(
    const std::string& level, const std::string& topic) const {
    std::lock_guard<std::mutex> lock(mutex_);
    std::vector<core::Lesson> result;
    for (const auto& p : lessons_) {
        if (p.second.level == level && (topic.empty() || p.second.topic == topic)) {
            result.push_back(p.second);
        }
    }
    return result;
}

bool MemoryLessonRepository::exists(const std::string& lessonId) const {
    std::lock_guard<std::mutex> lock(mutex_);
    return lessons_.find(lessonId) != lessons_.end();
}

bool MemoryLessonRepository::update(const core::Lesson& lesson) {
    std::lock_guard<std::mutex> lock(mutex_);
    auto it = lessons_.find(lesson.lessonId);
    if (it != lessons_.end()) { it->second = lesson; return true; }
    return false;
}

bool MemoryLessonRepository::remove(const std::string& lessonId) {
    std::lock_guard<std::mutex> lock(mutex_);
    return lessons_.erase(lessonId) > 0;
}

size_t MemoryLessonRepository::count() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return lessons_.size();
}

size_t MemoryLessonRepository::countByLevel(const std::string& level) const {
    std::lock_guard<std::mutex> lock(mutex_);
    size_t c = 0;
    for (const auto& p : lessons_) if (p.second.level == level) ++c;
    return c;
}

// ============================================================================
// MemoryTestRepository
// ============================================================================

bool MemoryTestRepository::add(const core::Test& test) {
    std::lock_guard<std::mutex> lock(mutex_);
    if (tests_.find(test.testId) != tests_.end()) return false;
    tests_[test.testId] = test;
    return true;
}

std::optional<core::Test> MemoryTestRepository::findById(const std::string& testId) const {
    std::lock_guard<std::mutex> lock(mutex_);
    auto it = tests_.find(testId);
    return (it != tests_.end()) ? std::optional(it->second) : std::nullopt;
}

std::vector<core::Test> MemoryTestRepository::findAll() const {
    std::lock_guard<std::mutex> lock(mutex_);
    std::vector<core::Test> result;
    for (const auto& p : tests_) result.push_back(p.second);
    return result;
}

std::vector<core::Test> MemoryTestRepository::findByLevel(const std::string& level) const {
    std::lock_guard<std::mutex> lock(mutex_);
    std::vector<core::Test> result;
    for (const auto& p : tests_) {
        if (p.second.level == level) result.push_back(p.second);
    }
    return result;
}

std::vector<core::Test> MemoryTestRepository::findByType(const std::string& testType) const {
    std::lock_guard<std::mutex> lock(mutex_);
    std::vector<core::Test> result;
    for (const auto& p : tests_) {
        if (p.second.testType == testType) result.push_back(p.second);
    }
    return result;
}

std::vector<core::Test> MemoryTestRepository::findByLevelAndType(
    const std::string& level, const std::string& testType) const {
    std::lock_guard<std::mutex> lock(mutex_);
    std::vector<core::Test> result;
    for (const auto& p : tests_) {
        if (p.second.level == level && (testType.empty() || p.second.testType == testType)) {
            result.push_back(p.second);
        }
    }
    return result;
}

bool MemoryTestRepository::exists(const std::string& testId) const {
    std::lock_guard<std::mutex> lock(mutex_);
    return tests_.find(testId) != tests_.end();
}

bool MemoryTestRepository::update(const core::Test& test) {
    std::lock_guard<std::mutex> lock(mutex_);
    auto it = tests_.find(test.testId);
    if (it != tests_.end()) { it->second = test; return true; }
    return false;
}

bool MemoryTestRepository::remove(const std::string& testId) {
    std::lock_guard<std::mutex> lock(mutex_);
    return tests_.erase(testId) > 0;
}

size_t MemoryTestRepository::count() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return tests_.size();
}

// ============================================================================
// MemoryChatRepository
// ============================================================================

bool MemoryChatRepository::add(const core::ChatMessage& message) {
    std::lock_guard<std::mutex> lock(mutex_);
    messages_.push_back(message);
    return true;
}

std::optional<core::ChatMessage> MemoryChatRepository::findById(const std::string& messageId) const {
    std::lock_guard<std::mutex> lock(mutex_);
    for (const auto& m : messages_) {
        if (m.messageId == messageId) return m;
    }
    return std::nullopt;
}

std::vector<core::ChatMessage> MemoryChatRepository::findAll() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return messages_;
}

std::vector<core::ChatMessage> MemoryChatRepository::findByUser(const std::string& userId) const {
    std::lock_guard<std::mutex> lock(mutex_);
    std::vector<core::ChatMessage> result;
    for (const auto& m : messages_) {
        if (m.senderId == userId || m.recipientId == userId) {
            result.push_back(m);
        }
    }
    return result;
}

std::vector<core::ChatMessage> MemoryChatRepository::findConversation(
    const std::string& user1, const std::string& user2) const {
    std::lock_guard<std::mutex> lock(mutex_);
    std::vector<core::ChatMessage> result;
    for (const auto& m : messages_) {
        if ((m.senderId == user1 && m.recipientId == user2) ||
            (m.senderId == user2 && m.recipientId == user1)) {
            result.push_back(m);
        }
    }
    return result;
}

std::vector<core::ChatMessage> MemoryChatRepository::findUnreadFor(const std::string& userId) const {
    std::lock_guard<std::mutex> lock(mutex_);
    std::vector<core::ChatMessage> result;
    for (const auto& m : messages_) {
        if (m.recipientId == userId && !m.read) {
            result.push_back(m);
        }
    }
    return result;
}

size_t MemoryChatRepository::countUnreadFor(const std::string& userId) const {
    std::lock_guard<std::mutex> lock(mutex_);
    size_t c = 0;
    for (const auto& m : messages_) {
        if (m.recipientId == userId && !m.read) ++c;
    }
    return c;
}

bool MemoryChatRepository::markAsRead(const std::string& messageId) {
    std::lock_guard<std::mutex> lock(mutex_);
    for (auto& m : messages_) {
        if (m.messageId == messageId) { m.read = true; return true; }
    }
    return false;
}

size_t MemoryChatRepository::markConversationAsRead(
    const std::string& recipientId, const std::string& senderId) {
    std::lock_guard<std::mutex> lock(mutex_);
    size_t c = 0;
    for (auto& m : messages_) {
        if (m.recipientId == recipientId && m.senderId == senderId && !m.read) {
            m.read = true; ++c;
        }
    }
    return c;
}

bool MemoryChatRepository::remove(const std::string& messageId) {
    std::lock_guard<std::mutex> lock(mutex_);
    for (auto it = messages_.begin(); it != messages_.end(); ++it) {
        if (it->messageId == messageId) { messages_.erase(it); return true; }
    }
    return false;
}

size_t MemoryChatRepository::count() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return messages_.size();
}

// ============================================================================
// MemoryExerciseRepository
// ============================================================================

bool MemoryExerciseRepository::addExercise(const core::Exercise& exercise) {
    std::lock_guard<std::mutex> lock(mutex_);
    if (exercises_.find(exercise.exerciseId) != exercises_.end()) return false;
    exercises_[exercise.exerciseId] = exercise;
    return true;
}

std::optional<core::Exercise> MemoryExerciseRepository::findExerciseById(
    const std::string& exerciseId) const {
    std::lock_guard<std::mutex> lock(mutex_);
    auto it = exercises_.find(exerciseId);
    return (it != exercises_.end()) ? std::optional(it->second) : std::nullopt;
}

std::vector<core::Exercise> MemoryExerciseRepository::findAllExercises() const {
    std::lock_guard<std::mutex> lock(mutex_);
    std::vector<core::Exercise> result;
    for (const auto& p : exercises_) result.push_back(p.second);
    return result;
}

std::vector<core::Exercise> MemoryExerciseRepository::findExercisesByLevel(
    const std::string& level) const {
    std::lock_guard<std::mutex> lock(mutex_);
    std::vector<core::Exercise> result;
    for (const auto& p : exercises_) {
        if (p.second.level == level) result.push_back(p.second);
    }
    return result;
}

std::vector<core::Exercise> MemoryExerciseRepository::findExercisesByType(
    const std::string& type) const {
    std::lock_guard<std::mutex> lock(mutex_);
    std::vector<core::Exercise> result;
    for (const auto& p : exercises_) {
        if (p.second.exerciseType == type) result.push_back(p.second);
    }
    return result;
}

bool MemoryExerciseRepository::exerciseExists(const std::string& exerciseId) const {
    std::lock_guard<std::mutex> lock(mutex_);
    return exercises_.find(exerciseId) != exercises_.end();
}

bool MemoryExerciseRepository::updateExercise(const core::Exercise& exercise) {
    std::lock_guard<std::mutex> lock(mutex_);
    auto it = exercises_.find(exercise.exerciseId);
    if (it != exercises_.end()) { it->second = exercise; return true; }
    return false;
}

bool MemoryExerciseRepository::removeExercise(const std::string& exerciseId) {
    std::lock_guard<std::mutex> lock(mutex_);
    return exercises_.erase(exerciseId) > 0;
}

bool MemoryExerciseRepository::addSubmission(const core::ExerciseSubmission& submission) {
    std::lock_guard<std::mutex> lock(mutex_);
    submissions_.push_back(submission);
    return true;
}

std::optional<core::ExerciseSubmission> MemoryExerciseRepository::findSubmissionById(
    const std::string& submissionId) const {
    std::lock_guard<std::mutex> lock(mutex_);
    for (const auto& s : submissions_) {
        if (s.submissionId == submissionId) return s;
    }
    return std::nullopt;
}

std::vector<core::ExerciseSubmission> MemoryExerciseRepository::findAllSubmissions() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return submissions_;
}

std::vector<core::ExerciseSubmission> MemoryExerciseRepository::findSubmissionsByUser(
    const std::string& userId) const {
    std::lock_guard<std::mutex> lock(mutex_);
    std::vector<core::ExerciseSubmission> result;
    for (const auto& s : submissions_) {
        if (s.userId == userId) result.push_back(s);
    }
    return result;
}

std::vector<core::ExerciseSubmission> MemoryExerciseRepository::findSubmissionsByExercise(
    const std::string& exerciseId) const {
    std::lock_guard<std::mutex> lock(mutex_);
    std::vector<core::ExerciseSubmission> result;
    for (const auto& s : submissions_) {
        if (s.exerciseId == exerciseId) result.push_back(s);
    }
    return result;
}

std::vector<core::ExerciseSubmission> MemoryExerciseRepository::findPendingSubmissions() const {
    std::lock_guard<std::mutex> lock(mutex_);
    std::vector<core::ExerciseSubmission> result;
    for (const auto& s : submissions_) {
        if (s.status == "pending") result.push_back(s);
    }
    return result;
}

std::vector<core::ExerciseSubmission> MemoryExerciseRepository::findReviewedSubmissions(
    const std::string& userId) const {
    std::lock_guard<std::mutex> lock(mutex_);
    std::vector<core::ExerciseSubmission> result;
    for (const auto& s : submissions_) {
        if (s.userId == userId && s.status == "reviewed") result.push_back(s);
    }
    return result;
}

bool MemoryExerciseRepository::updateSubmission(const core::ExerciseSubmission& submission) {
    std::lock_guard<std::mutex> lock(mutex_);
    for (auto& s : submissions_) {
        if (s.submissionId == submission.submissionId) { s = submission; return true; }
    }
    return false;
}

bool MemoryExerciseRepository::reviewSubmission(
    const std::string& submissionId, const std::string& teacherId,
    const std::string& feedback, int score, int64_t reviewedAt) {
    std::lock_guard<std::mutex> lock(mutex_);
    for (auto& s : submissions_) {
        if (s.submissionId == submissionId) {
            s.status = "reviewed";
            s.teacherId = teacherId;
            s.teacherFeedback = feedback;
            s.teacherScore = score;
            s.reviewedAt = reviewedAt;
            return true;
        }
    }
    return false;
}

size_t MemoryExerciseRepository::countExercises() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return exercises_.size();
}

size_t MemoryExerciseRepository::countSubmissions() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return submissions_.size();
}

size_t MemoryExerciseRepository::countPendingSubmissions() const {
    std::lock_guard<std::mutex> lock(mutex_);
    size_t c = 0;
    for (const auto& s : submissions_) if (s.status == "pending") ++c;
    return c;
}

// ============================================================================
// MemoryGameRepository
// ============================================================================

bool MemoryGameRepository::addGame(const core::Game& game) {
    std::lock_guard<std::mutex> lock(mutex_);
    if (games_.find(game.gameId) != games_.end()) return false;
    games_[game.gameId] = game;
    return true;
}

std::optional<core::Game> MemoryGameRepository::findGameById(const std::string& gameId) const {
    std::lock_guard<std::mutex> lock(mutex_);
    auto it = games_.find(gameId);
    return (it != games_.end()) ? std::optional(it->second) : std::nullopt;
}

std::vector<core::Game> MemoryGameRepository::findAllGames() const {
    std::lock_guard<std::mutex> lock(mutex_);
    std::vector<core::Game> result;
    for (const auto& p : games_) result.push_back(p.second);
    return result;
}

std::vector<core::Game> MemoryGameRepository::findGamesByLevel(const std::string& level) const {
    std::lock_guard<std::mutex> lock(mutex_);
    std::vector<core::Game> result;
    for (const auto& p : games_) {
        if (p.second.level == level) result.push_back(p.second);
    }
    return result;
}

std::vector<core::Game> MemoryGameRepository::findGamesByType(const std::string& gameType) const {
    std::lock_guard<std::mutex> lock(mutex_);
    std::vector<core::Game> result;
    for (const auto& p : games_) {
        if (p.second.gameType == gameType) result.push_back(p.second);
    }
    return result;
}

std::vector<core::Game> MemoryGameRepository::findGamesByLevelAndType(
    const std::string& level, const std::string& gameType) const {
    std::lock_guard<std::mutex> lock(mutex_);
    std::vector<core::Game> result;
    for (const auto& p : games_) {
        bool matchLevel = level.empty() || p.second.level == level;
        bool matchType = gameType.empty() || gameType == "all" || p.second.gameType == gameType;
        if (matchLevel && matchType) result.push_back(p.second);
    }
    return result;
}

bool MemoryGameRepository::gameExists(const std::string& gameId) const {
    std::lock_guard<std::mutex> lock(mutex_);
    return games_.find(gameId) != games_.end();
}

bool MemoryGameRepository::updateGame(const core::Game& game) {
    std::lock_guard<std::mutex> lock(mutex_);
    auto it = games_.find(game.gameId);
    if (it != games_.end()) { it->second = game; return true; }
    return false;
}

bool MemoryGameRepository::removeGame(const std::string& gameId) {
    std::lock_guard<std::mutex> lock(mutex_);
    return games_.erase(gameId) > 0;
}

bool MemoryGameRepository::addSession(const core::GameSession& session) {
    std::lock_guard<std::mutex> lock(mutex_);
    if (sessions_.find(session.sessionId) != sessions_.end()) return false;
    sessions_[session.sessionId] = session;
    return true;
}

std::optional<core::GameSession> MemoryGameRepository::findSessionById(
    const std::string& sessionId) const {
    std::lock_guard<std::mutex> lock(mutex_);
    auto it = sessions_.find(sessionId);
    return (it != sessions_.end()) ? std::optional(it->second) : std::nullopt;
}

std::vector<core::GameSession> MemoryGameRepository::findSessionsByUser(
    const std::string& userId) const {
    std::lock_guard<std::mutex> lock(mutex_);
    std::vector<core::GameSession> result;
    for (const auto& p : sessions_) {
        if (p.second.userId == userId) result.push_back(p.second);
    }
    return result;
}

std::vector<core::GameSession> MemoryGameRepository::findSessionsByGame(
    const std::string& gameId) const {
    std::lock_guard<std::mutex> lock(mutex_);
    std::vector<core::GameSession> result;
    for (const auto& p : sessions_) {
        if (p.second.gameId == gameId) result.push_back(p.second);
    }
    return result;
}

std::vector<core::GameSession> MemoryGameRepository::findActiveSessionsByUser(
    const std::string& userId) const {
    std::lock_guard<std::mutex> lock(mutex_);
    std::vector<core::GameSession> result;
    for (const auto& p : sessions_) {
        if (p.second.userId == userId && !p.second.completed) {
            result.push_back(p.second);
        }
    }
    return result;
}

bool MemoryGameRepository::updateSession(const core::GameSession& session) {
    std::lock_guard<std::mutex> lock(mutex_);
    auto it = sessions_.find(session.sessionId);
    if (it != sessions_.end()) { it->second = session; return true; }
    return false;
}

bool MemoryGameRepository::completeSession(
    const std::string& sessionId, int score, int64_t endTime) {
    std::lock_guard<std::mutex> lock(mutex_);
    auto it = sessions_.find(sessionId);
    if (it != sessions_.end()) {
        it->second.score = score;
        it->second.endTime = endTime;
        it->second.completed = true;
        return true;
    }
    return false;
}

size_t MemoryGameRepository::countGames() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return games_.size();
}

size_t MemoryGameRepository::countSessions() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return sessions_.size();
}

// ============================================================================
// MemoryVoiceCallRepository
// ============================================================================

bool MemoryVoiceCallRepository::add(const core::VoiceCallSession& call) {
    std::lock_guard<std::mutex> lock(mutex_);
    if (calls_.find(call.callId) != calls_.end()) return false;
    calls_[call.callId] = call;
    return true;
}

std::optional<core::VoiceCallSession> MemoryVoiceCallRepository::findById(
    const std::string& callId) const {
    std::lock_guard<std::mutex> lock(mutex_);
    auto it = calls_.find(callId);
    return (it != calls_.end()) ? std::optional(it->second) : std::nullopt;
}

std::vector<core::VoiceCallSession> MemoryVoiceCallRepository::findAll() const {
    std::lock_guard<std::mutex> lock(mutex_);
    std::vector<core::VoiceCallSession> result;
    for (const auto& p : calls_) result.push_back(p.second);
    return result;
}

std::vector<core::VoiceCallSession> MemoryVoiceCallRepository::findByUser(
    const std::string& userId) const {
    std::lock_guard<std::mutex> lock(mutex_);
    std::vector<core::VoiceCallSession> result;
    for (const auto& p : calls_) {
        if (p.second.involvesUser(userId)) result.push_back(p.second);
    }
    return result;
}

std::vector<core::VoiceCallSession> MemoryVoiceCallRepository::findActiveByUser(
    const std::string& userId) const {
    std::lock_guard<std::mutex> lock(mutex_);
    std::vector<core::VoiceCallSession> result;
    for (const auto& p : calls_) {
        if (p.second.involvesUser(userId) && p.second.isActive()) {
            result.push_back(p.second);
        }
    }
    return result;
}

std::vector<core::VoiceCallSession> MemoryVoiceCallRepository::findPendingForUser(
    const std::string& userId) const {
    std::lock_guard<std::mutex> lock(mutex_);
    std::vector<core::VoiceCallSession> result;
    for (const auto& p : calls_) {
        if (p.second.receiverId == userId && p.second.isPending()) {
            result.push_back(p.second);
        }
    }
    return result;
}

std::optional<core::VoiceCallSession> MemoryVoiceCallRepository::findActiveCall(
    const std::string& userId) const {
    std::lock_guard<std::mutex> lock(mutex_);
    for (const auto& p : calls_) {
        if (p.second.involvesUser(userId) && p.second.isActive()) {
            return p.second;
        }
    }
    return std::nullopt;
}

std::optional<core::VoiceCallSession> MemoryVoiceCallRepository::findPendingCall(
    const std::string& callerId, const std::string& receiverId) const {
    std::lock_guard<std::mutex> lock(mutex_);
    for (const auto& p : calls_) {
        if (p.second.callerId == callerId && p.second.receiverId == receiverId &&
            p.second.isPending()) {
            return p.second;
        }
    }
    return std::nullopt;
}

bool MemoryVoiceCallRepository::update(const core::VoiceCallSession& call) {
    std::lock_guard<std::mutex> lock(mutex_);
    auto it = calls_.find(call.callId);
    if (it != calls_.end()) {
        it->second = call;
        return true;
    }
    return false;
}

bool MemoryVoiceCallRepository::updateStatus(const std::string& callId,
                                              core::VoiceCallStatus status,
                                              core::Timestamp endTime) {
    std::lock_guard<std::mutex> lock(mutex_);
    auto it = calls_.find(callId);
    if (it != calls_.end()) {
        it->second.status = status;
        if (endTime > 0) {
            it->second.endTime = endTime;
        }
        return true;
    }
    return false;
}

bool MemoryVoiceCallRepository::remove(const std::string& callId) {
    std::lock_guard<std::mutex> lock(mutex_);
    return calls_.erase(callId) > 0;
}

size_t MemoryVoiceCallRepository::count() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return calls_.size();
}

size_t MemoryVoiceCallRepository::countActiveForUser(const std::string& userId) const {
    std::lock_guard<std::mutex> lock(mutex_);
    size_t c = 0;
    for (const auto& p : calls_) {
        if (p.second.involvesUser(userId) && p.second.isActive()) ++c;
    }
    return c;
}

} // namespace memory
} // namespace repository
} // namespace english_learning
