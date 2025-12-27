#ifndef ENGLISH_LEARNING_REPOSITORY_MEMORY_REPOSITORIES_H
#define ENGLISH_LEARNING_REPOSITORY_MEMORY_REPOSITORIES_H

#include <map>
#include <vector>
#include <mutex>
#include "include/repository/i_lesson_repository.h"
#include "include/repository/i_test_repository.h"
#include "include/repository/i_chat_repository.h"
#include "include/repository/i_exercise_repository.h"
#include "include/repository/i_game_repository.h"
#include "include/repository/i_voice_call_repository.h"

namespace english_learning {
namespace repository {
namespace memory {

/**
 * In-memory implementation of ILessonRepository.
 */
class MemoryLessonRepository : public ILessonRepository {
public:
    bool add(const core::Lesson& lesson) override;
    std::optional<core::Lesson> findById(const std::string& lessonId) const override;
    std::vector<core::Lesson> findAll() const override;
    std::vector<core::Lesson> findByLevel(const std::string& level) const override;
    std::vector<core::Lesson> findByTopic(const std::string& topic) const override;
    std::vector<core::Lesson> findByLevelAndTopic(const std::string& level,
                                                   const std::string& topic) const override;
    bool exists(const std::string& lessonId) const override;
    bool update(const core::Lesson& lesson) override;
    bool remove(const std::string& lessonId) override;
    size_t count() const override;
    size_t countByLevel(const std::string& level) const override;

private:
    mutable std::mutex mutex_;
    std::map<std::string, core::Lesson> lessons_;
};

/**
 * In-memory implementation of ITestRepository.
 */
class MemoryTestRepository : public ITestRepository {
public:
    bool add(const core::Test& test) override;
    std::optional<core::Test> findById(const std::string& testId) const override;
    std::vector<core::Test> findAll() const override;
    std::vector<core::Test> findByLevel(const std::string& level) const override;
    std::vector<core::Test> findByType(const std::string& testType) const override;
    std::vector<core::Test> findByLevelAndType(const std::string& level,
                                                const std::string& testType) const override;
    bool exists(const std::string& testId) const override;
    bool update(const core::Test& test) override;
    bool remove(const std::string& testId) override;
    size_t count() const override;

private:
    mutable std::mutex mutex_;
    std::map<std::string, core::Test> tests_;
};

/**
 * In-memory implementation of IChatRepository.
 */
class MemoryChatRepository : public IChatRepository {
public:
    bool add(const core::ChatMessage& message) override;
    std::optional<core::ChatMessage> findById(const std::string& messageId) const override;
    std::vector<core::ChatMessage> findAll() const override;
    std::vector<core::ChatMessage> findByUser(const std::string& userId) const override;
    std::vector<core::ChatMessage> findConversation(const std::string& user1,
                                                     const std::string& user2) const override;
    std::vector<core::ChatMessage> findUnreadFor(const std::string& userId) const override;
    size_t countUnreadFor(const std::string& userId) const override;
    bool markAsRead(const std::string& messageId) override;
    size_t markConversationAsRead(const std::string& recipientId,
                                  const std::string& senderId) override;
    bool remove(const std::string& messageId) override;
    size_t count() const override;

private:
    mutable std::mutex mutex_;
    std::vector<core::ChatMessage> messages_;
};

/**
 * In-memory implementation of IExerciseRepository.
 */
class MemoryExerciseRepository : public IExerciseRepository {
public:
    // Exercise operations
    bool addExercise(const core::Exercise& exercise) override;
    std::optional<core::Exercise> findExerciseById(const std::string& exerciseId) const override;
    std::vector<core::Exercise> findAllExercises() const override;
    std::vector<core::Exercise> findExercisesByLevel(const std::string& level) const override;
    std::vector<core::Exercise> findExercisesByType(const std::string& type) const override;
    bool exerciseExists(const std::string& exerciseId) const override;
    bool updateExercise(const core::Exercise& exercise) override;
    bool removeExercise(const std::string& exerciseId) override;

    // Submission operations
    bool addSubmission(const core::ExerciseSubmission& submission) override;
    std::optional<core::ExerciseSubmission> findSubmissionById(
        const std::string& submissionId) const override;
    std::vector<core::ExerciseSubmission> findAllSubmissions() const override;
    std::vector<core::ExerciseSubmission> findSubmissionsByUser(
        const std::string& userId) const override;
    std::vector<core::ExerciseSubmission> findSubmissionsByExercise(
        const std::string& exerciseId) const override;
    std::vector<core::ExerciseSubmission> findPendingSubmissions() const override;
    std::vector<core::ExerciseSubmission> findReviewedSubmissions(
        const std::string& userId) const override;
    bool updateSubmission(const core::ExerciseSubmission& submission) override;
    bool reviewSubmission(const std::string& submissionId,
                          const std::string& teacherId,
                          const std::string& feedback,
                          int score,
                          int64_t reviewedAt) override;

    size_t countExercises() const override;
    size_t countSubmissions() const override;
    size_t countPendingSubmissions() const override;

private:
    mutable std::mutex mutex_;
    std::map<std::string, core::Exercise> exercises_;
    std::vector<core::ExerciseSubmission> submissions_;
};

/**
 * In-memory implementation of IGameRepository.
 */
class MemoryGameRepository : public IGameRepository {
public:
    // Game operations
    bool addGame(const core::Game& game) override;
    std::optional<core::Game> findGameById(const std::string& gameId) const override;
    std::vector<core::Game> findAllGames() const override;
    std::vector<core::Game> findGamesByLevel(const std::string& level) const override;
    std::vector<core::Game> findGamesByType(const std::string& gameType) const override;
    std::vector<core::Game> findGamesByLevelAndType(const std::string& level,
                                                     const std::string& gameType) const override;
    bool gameExists(const std::string& gameId) const override;
    bool updateGame(const core::Game& game) override;
    bool removeGame(const std::string& gameId) override;

    // Session operations
    bool addSession(const core::GameSession& session) override;
    std::optional<core::GameSession> findSessionById(const std::string& sessionId) const override;
    std::vector<core::GameSession> findSessionsByUser(const std::string& userId) const override;
    std::vector<core::GameSession> findSessionsByGame(const std::string& gameId) const override;
    std::vector<core::GameSession> findActiveSessionsByUser(const std::string& userId) const override;
    bool updateSession(const core::GameSession& session) override;
    bool completeSession(const std::string& sessionId, int score, int64_t endTime) override;

    size_t countGames() const override;
    size_t countSessions() const override;

private:
    mutable std::mutex mutex_;
    std::map<std::string, core::Game> games_;
    std::map<std::string, core::GameSession> sessions_;
};

/**
 * In-memory implementation of IVoiceCallRepository.
 */
class MemoryVoiceCallRepository : public IVoiceCallRepository {
public:
    bool add(const core::VoiceCallSession& call) override;
    std::optional<core::VoiceCallSession> findById(const std::string& callId) const override;
    std::vector<core::VoiceCallSession> findAll() const override;
    std::vector<core::VoiceCallSession> findByUser(const std::string& userId) const override;
    std::vector<core::VoiceCallSession> findActiveByUser(const std::string& userId) const override;
    std::vector<core::VoiceCallSession> findPendingForUser(const std::string& userId) const override;
    std::optional<core::VoiceCallSession> findActiveCall(const std::string& userId) const override;
    std::optional<core::VoiceCallSession> findPendingCall(
        const std::string& callerId, const std::string& receiverId) const override;
    bool update(const core::VoiceCallSession& call) override;
    bool updateStatus(const std::string& callId, core::VoiceCallStatus status,
                      core::Timestamp endTime = 0) override;
    bool remove(const std::string& callId) override;
    size_t count() const override;
    size_t countActiveForUser(const std::string& userId) const override;

private:
    mutable std::mutex mutex_;
    std::map<std::string, core::VoiceCallSession> calls_;
};

} // namespace memory
} // namespace repository
} // namespace english_learning

#endif // ENGLISH_LEARNING_REPOSITORY_MEMORY_REPOSITORIES_H
