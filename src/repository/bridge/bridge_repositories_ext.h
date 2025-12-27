#ifndef ENGLISH_LEARNING_BRIDGE_REPOSITORIES_EXT_H
#define ENGLISH_LEARNING_BRIDGE_REPOSITORIES_EXT_H

/**
 * Additional bridge repositories for lesson, test, exercise, and game.
 */

#include "bridge_repositories.h"
#include "include/repository/i_voice_call_repository.h"

namespace english_learning {
namespace repository {
namespace bridge {

/**
 * Bridge lesson repository wrapping global lessons map.
 */
class BridgeLessonRepository : public ILessonRepository {
public:
    BridgeLessonRepository(std::map<std::string, core::Lesson>& lessons)
        : lessons_(lessons) {}

    bool add(const core::Lesson& lesson) override {
        if (lessons_.find(lesson.lessonId) != lessons_.end()) {
            return false;
        }
        lessons_[lesson.lessonId] = lesson;
        return true;
    }

    std::optional<core::Lesson> findById(const std::string& lessonId) const override {
        auto it = lessons_.find(lessonId);
        if (it != lessons_.end()) {
            return it->second;
        }
        return std::nullopt;
    }

    std::vector<core::Lesson> findAll() const override {
        std::vector<core::Lesson> result;
        for (const auto& pair : lessons_) {
            result.push_back(pair.second);
        }
        return result;
    }

    std::vector<core::Lesson> findByLevel(const std::string& level) const override {
        std::vector<core::Lesson> result;
        for (const auto& pair : lessons_) {
            if (pair.second.level == level) {
                result.push_back(pair.second);
            }
        }
        return result;
    }

    std::vector<core::Lesson> findByTopic(const std::string& topic) const override {
        std::vector<core::Lesson> result;
        for (const auto& pair : lessons_) {
            if (pair.second.topic == topic) {
                result.push_back(pair.second);
            }
        }
        return result;
    }

    std::vector<core::Lesson> findByLevelAndTopic(const std::string& level,
                                                   const std::string& topic) const override {
        std::vector<core::Lesson> result;
        for (const auto& pair : lessons_) {
            if (pair.second.level == level && pair.second.topic == topic) {
                result.push_back(pair.second);
            }
        }
        return result;
    }

    bool exists(const std::string& lessonId) const override {
        return lessons_.find(lessonId) != lessons_.end();
    }

    bool update(const core::Lesson& lesson) override {
        auto it = lessons_.find(lesson.lessonId);
        if (it != lessons_.end()) {
            it->second = lesson;
            return true;
        }
        return false;
    }

    bool remove(const std::string& lessonId) override {
        return lessons_.erase(lessonId) > 0;
    }

    size_t count() const override {
        return lessons_.size();
    }

    size_t countByLevel(const std::string& level) const override {
        size_t cnt = 0;
        for (const auto& pair : lessons_) {
            if (pair.second.level == level) {
                cnt++;
            }
        }
        return cnt;
    }

private:
    std::map<std::string, core::Lesson>& lessons_;
};

/**
 * Bridge test repository wrapping global tests map.
 */
class BridgeTestRepository : public ITestRepository {
public:
    BridgeTestRepository(std::map<std::string, core::Test>& tests)
        : tests_(tests) {}

    bool add(const core::Test& test) override {
        if (tests_.find(test.testId) != tests_.end()) {
            return false;
        }
        tests_[test.testId] = test;
        return true;
    }

    std::optional<core::Test> findById(const std::string& testId) const override {
        auto it = tests_.find(testId);
        if (it != tests_.end()) {
            return it->second;
        }
        return std::nullopt;
    }

    std::vector<core::Test> findAll() const override {
        std::vector<core::Test> result;
        for (const auto& pair : tests_) {
            result.push_back(pair.second);
        }
        return result;
    }

    std::vector<core::Test> findByLevel(const std::string& level) const override {
        std::vector<core::Test> result;
        for (const auto& pair : tests_) {
            if (pair.second.level == level) {
                result.push_back(pair.second);
            }
        }
        return result;
    }

    std::vector<core::Test> findByType(const std::string& testType) const override {
        std::vector<core::Test> result;
        for (const auto& pair : tests_) {
            if (pair.second.testType == testType) {
                result.push_back(pair.second);
            }
        }
        return result;
    }

    std::vector<core::Test> findByLevelAndType(const std::string& level,
                                                const std::string& testType) const override {
        std::vector<core::Test> result;
        for (const auto& pair : tests_) {
            if (pair.second.level == level && pair.second.testType == testType) {
                result.push_back(pair.second);
            }
        }
        return result;
    }

    bool exists(const std::string& testId) const override {
        return tests_.find(testId) != tests_.end();
    }

    bool update(const core::Test& test) override {
        auto it = tests_.find(test.testId);
        if (it != tests_.end()) {
            it->second = test;
            return true;
        }
        return false;
    }

    bool remove(const std::string& testId) override {
        return tests_.erase(testId) > 0;
    }

    size_t count() const override {
        return tests_.size();
    }

private:
    std::map<std::string, core::Test>& tests_;
};

/**
 * Bridge exercise repository wrapping global exercises map and submissions.
 */
class BridgeExerciseRepository : public IExerciseRepository {
public:
    BridgeExerciseRepository(
        std::map<std::string, core::Exercise>& exercises,
        std::vector<core::ExerciseSubmission>& submissions,
        std::mutex& mutex)
        : exercises_(exercises), submissions_(submissions), mutex_(mutex) {}

    bool addExercise(const core::Exercise& exercise) override {
        std::lock_guard<std::mutex> lock(mutex_);
        if (exercises_.find(exercise.exerciseId) != exercises_.end()) {
            return false;
        }
        exercises_[exercise.exerciseId] = exercise;
        return true;
    }

    std::optional<core::Exercise> findExerciseById(const std::string& exerciseId) const override {
        std::lock_guard<std::mutex> lock(mutex_);
        auto it = exercises_.find(exerciseId);
        if (it != exercises_.end()) {
            return it->second;
        }
        return std::nullopt;
    }

    std::vector<core::Exercise> findAllExercises() const override {
        std::lock_guard<std::mutex> lock(mutex_);
        std::vector<core::Exercise> result;
        for (const auto& pair : exercises_) {
            result.push_back(pair.second);
        }
        return result;
    }

    std::vector<core::Exercise> findExercisesByLevel(const std::string& level) const override {
        std::lock_guard<std::mutex> lock(mutex_);
        std::vector<core::Exercise> result;
        for (const auto& pair : exercises_) {
            if (pair.second.level == level) {
                result.push_back(pair.second);
            }
        }
        return result;
    }

    std::vector<core::Exercise> findExercisesByType(const std::string& type) const override {
        std::lock_guard<std::mutex> lock(mutex_);
        std::vector<core::Exercise> result;
        for (const auto& pair : exercises_) {
            if (pair.second.exerciseType == type) {
                result.push_back(pair.second);
            }
        }
        return result;
    }

    bool exerciseExists(const std::string& exerciseId) const override {
        std::lock_guard<std::mutex> lock(mutex_);
        return exercises_.find(exerciseId) != exercises_.end();
    }

    bool updateExercise(const core::Exercise& exercise) override {
        std::lock_guard<std::mutex> lock(mutex_);
        auto it = exercises_.find(exercise.exerciseId);
        if (it != exercises_.end()) {
            it->second = exercise;
            return true;
        }
        return false;
    }

    bool removeExercise(const std::string& exerciseId) override {
        std::lock_guard<std::mutex> lock(mutex_);
        return exercises_.erase(exerciseId) > 0;
    }

    bool addSubmission(const core::ExerciseSubmission& submission) override {
        std::lock_guard<std::mutex> lock(mutex_);
        submissions_.push_back(submission);
        return true;
    }

    std::optional<core::ExerciseSubmission> findSubmissionById(
        const std::string& submissionId) const override {
        std::lock_guard<std::mutex> lock(mutex_);
        for (const auto& sub : submissions_) {
            if (sub.submissionId == submissionId) {
                return sub;
            }
        }
        return std::nullopt;
    }

    std::vector<core::ExerciseSubmission> findAllSubmissions() const override {
        std::lock_guard<std::mutex> lock(mutex_);
        return submissions_;
    }

    std::vector<core::ExerciseSubmission> findSubmissionsByUser(
        const std::string& userId) const override {
        std::lock_guard<std::mutex> lock(mutex_);
        std::vector<core::ExerciseSubmission> result;
        for (const auto& sub : submissions_) {
            if (sub.userId == userId) {
                result.push_back(sub);
            }
        }
        return result;
    }

    std::vector<core::ExerciseSubmission> findSubmissionsByExercise(
        const std::string& exerciseId) const override {
        std::lock_guard<std::mutex> lock(mutex_);
        std::vector<core::ExerciseSubmission> result;
        for (const auto& sub : submissions_) {
            if (sub.exerciseId == exerciseId) {
                result.push_back(sub);
            }
        }
        return result;
    }

    std::vector<core::ExerciseSubmission> findPendingSubmissions() const override {
        std::lock_guard<std::mutex> lock(mutex_);
        std::vector<core::ExerciseSubmission> result;
        for (const auto& sub : submissions_) {
            if (sub.isPending()) {
                result.push_back(sub);
            }
        }
        return result;
    }

    std::vector<core::ExerciseSubmission> findReviewedSubmissions(
        const std::string& userId) const override {
        std::lock_guard<std::mutex> lock(mutex_);
        std::vector<core::ExerciseSubmission> result;
        for (const auto& sub : submissions_) {
            if (sub.userId == userId && sub.isReviewed()) {
                result.push_back(sub);
            }
        }
        return result;
    }

    bool updateSubmission(const core::ExerciseSubmission& submission) override {
        std::lock_guard<std::mutex> lock(mutex_);
        for (auto& sub : submissions_) {
            if (sub.submissionId == submission.submissionId) {
                sub = submission;
                return true;
            }
        }
        return false;
    }

    bool reviewSubmission(const std::string& submissionId,
                          const std::string& teacherId,
                          const std::string& feedback,
                          int score,
                          int64_t reviewedAt) override {
        std::lock_guard<std::mutex> lock(mutex_);
        for (auto& sub : submissions_) {
            if (sub.submissionId == submissionId) {
                sub.setReview(teacherId, feedback, score, reviewedAt);
                return true;
            }
        }
        return false;
    }

    size_t countExercises() const override {
        std::lock_guard<std::mutex> lock(mutex_);
        return exercises_.size();
    }

    size_t countSubmissions() const override {
        std::lock_guard<std::mutex> lock(mutex_);
        return submissions_.size();
    }

    size_t countPendingSubmissions() const override {
        std::lock_guard<std::mutex> lock(mutex_);
        size_t count = 0;
        for (const auto& sub : submissions_) {
            if (sub.isPending()) {
                count++;
            }
        }
        return count;
    }

private:
    std::map<std::string, core::Exercise>& exercises_;
    std::vector<core::ExerciseSubmission>& submissions_;
    std::mutex& mutex_;
};

/**
 * Bridge game repository wrapping global games map and sessions.
 */
class BridgeGameRepository : public IGameRepository {
public:
    BridgeGameRepository(
        std::map<std::string, core::Game>& games,
        std::map<std::string, core::GameSession>& gameSessions,
        std::mutex& mutex)
        : games_(games), sessions_(gameSessions), mutex_(mutex) {}

    bool addGame(const core::Game& game) override {
        std::lock_guard<std::mutex> lock(mutex_);
        if (games_.find(game.gameId) != games_.end()) {
            return false;
        }
        games_[game.gameId] = game;
        return true;
    }

    std::optional<core::Game> findGameById(const std::string& gameId) const override {
        std::lock_guard<std::mutex> lock(mutex_);
        auto it = games_.find(gameId);
        if (it != games_.end()) {
            return it->second;
        }
        return std::nullopt;
    }

    std::vector<core::Game> findAllGames() const override {
        std::lock_guard<std::mutex> lock(mutex_);
        std::vector<core::Game> result;
        for (const auto& pair : games_) {
            result.push_back(pair.second);
        }
        return result;
    }

    std::vector<core::Game> findGamesByLevel(const std::string& level) const override {
        std::lock_guard<std::mutex> lock(mutex_);
        std::vector<core::Game> result;
        for (const auto& pair : games_) {
            if (pair.second.level == level) {
                result.push_back(pair.second);
            }
        }
        return result;
    }

    std::vector<core::Game> findGamesByType(const std::string& gameType) const override {
        std::lock_guard<std::mutex> lock(mutex_);
        std::vector<core::Game> result;
        for (const auto& pair : games_) {
            if (pair.second.gameType == gameType) {
                result.push_back(pair.second);
            }
        }
        return result;
    }

    std::vector<core::Game> findGamesByLevelAndType(const std::string& level,
                                                     const std::string& gameType) const override {
        std::lock_guard<std::mutex> lock(mutex_);
        std::vector<core::Game> result;
        for (const auto& pair : games_) {
            if (pair.second.level == level && pair.second.gameType == gameType) {
                result.push_back(pair.second);
            }
        }
        return result;
    }

    bool gameExists(const std::string& gameId) const override {
        std::lock_guard<std::mutex> lock(mutex_);
        return games_.find(gameId) != games_.end();
    }

    bool updateGame(const core::Game& game) override {
        std::lock_guard<std::mutex> lock(mutex_);
        auto it = games_.find(game.gameId);
        if (it != games_.end()) {
            it->second = game;
            return true;
        }
        return false;
    }

    bool removeGame(const std::string& gameId) override {
        std::lock_guard<std::mutex> lock(mutex_);
        return games_.erase(gameId) > 0;
    }

    bool addSession(const core::GameSession& session) override {
        std::lock_guard<std::mutex> lock(mutex_);
        if (sessions_.find(session.sessionId) != sessions_.end()) {
            return false;
        }
        sessions_[session.sessionId] = session;
        return true;
    }

    std::optional<core::GameSession> findSessionById(
        const std::string& sessionId) const override {
        std::lock_guard<std::mutex> lock(mutex_);
        auto it = sessions_.find(sessionId);
        if (it != sessions_.end()) {
            return it->second;
        }
        return std::nullopt;
    }

    std::vector<core::GameSession> findSessionsByUser(
        const std::string& userId) const override {
        std::lock_guard<std::mutex> lock(mutex_);
        std::vector<core::GameSession> result;
        for (const auto& pair : sessions_) {
            if (pair.second.userId == userId) {
                result.push_back(pair.second);
            }
        }
        return result;
    }

    std::vector<core::GameSession> findSessionsByGame(
        const std::string& gameId) const override {
        std::lock_guard<std::mutex> lock(mutex_);
        std::vector<core::GameSession> result;
        for (const auto& pair : sessions_) {
            if (pair.second.gameId == gameId) {
                result.push_back(pair.second);
            }
        }
        return result;
    }

    std::vector<core::GameSession> findActiveSessionsByUser(
        const std::string& userId) const override {
        std::lock_guard<std::mutex> lock(mutex_);
        std::vector<core::GameSession> result;
        for (const auto& pair : sessions_) {
            if (pair.second.userId == userId && pair.second.isActive()) {
                result.push_back(pair.second);
            }
        }
        return result;
    }

    bool updateSession(const core::GameSession& session) override {
        std::lock_guard<std::mutex> lock(mutex_);
        auto it = sessions_.find(session.sessionId);
        if (it != sessions_.end()) {
            it->second = session;
            return true;
        }
        return false;
    }

    bool completeSession(const std::string& sessionId,
                         int score,
                         int64_t endTime) override {
        std::lock_guard<std::mutex> lock(mutex_);
        auto it = sessions_.find(sessionId);
        if (it != sessions_.end()) {
            it->second.complete(score, endTime);
            return true;
        }
        return false;
    }

    size_t countGames() const override {
        std::lock_guard<std::mutex> lock(mutex_);
        return games_.size();
    }

    size_t countSessions() const override {
        std::lock_guard<std::mutex> lock(mutex_);
        return sessions_.size();
    }

private:
    std::map<std::string, core::Game>& games_;
    std::map<std::string, core::GameSession>& sessions_;
    std::mutex& mutex_;
};

/**
 * Bridge voice call repository wrapping global voice calls map.
 */
class BridgeVoiceCallRepository : public IVoiceCallRepository {
public:
    BridgeVoiceCallRepository(
        std::map<std::string, core::VoiceCallSession>& calls,
        std::mutex& mutex)
        : calls_(calls), mutex_(mutex) {}

    bool add(const core::VoiceCallSession& call) override {
        std::lock_guard<std::mutex> lock(mutex_);
        if (calls_.find(call.callId) != calls_.end()) {
            return false;
        }
        calls_[call.callId] = call;
        return true;
    }

    std::optional<core::VoiceCallSession> findById(const std::string& callId) const override {
        std::lock_guard<std::mutex> lock(mutex_);
        auto it = calls_.find(callId);
        if (it != calls_.end()) {
            return it->second;
        }
        return std::nullopt;
    }

    std::vector<core::VoiceCallSession> findAll() const override {
        std::lock_guard<std::mutex> lock(mutex_);
        std::vector<core::VoiceCallSession> result;
        for (const auto& pair : calls_) {
            result.push_back(pair.second);
        }
        return result;
    }

    std::vector<core::VoiceCallSession> findByUser(const std::string& userId) const override {
        std::lock_guard<std::mutex> lock(mutex_);
        std::vector<core::VoiceCallSession> result;
        for (const auto& pair : calls_) {
            if (pair.second.involvesUser(userId)) {
                result.push_back(pair.second);
            }
        }
        return result;
    }

    std::vector<core::VoiceCallSession> findActiveByUser(const std::string& userId) const override {
        std::lock_guard<std::mutex> lock(mutex_);
        std::vector<core::VoiceCallSession> result;
        for (const auto& pair : calls_) {
            if (pair.second.involvesUser(userId) && pair.second.isActive()) {
                result.push_back(pair.second);
            }
        }
        return result;
    }

    std::vector<core::VoiceCallSession> findPendingForUser(const std::string& userId) const override {
        std::lock_guard<std::mutex> lock(mutex_);
        std::vector<core::VoiceCallSession> result;
        for (const auto& pair : calls_) {
            if (pair.second.receiverId == userId && pair.second.isPending()) {
                result.push_back(pair.second);
            }
        }
        return result;
    }

    std::optional<core::VoiceCallSession> findActiveCall(const std::string& userId) const override {
        std::lock_guard<std::mutex> lock(mutex_);
        for (const auto& pair : calls_) {
            if (pair.second.involvesUser(userId) && pair.second.isActive()) {
                return pair.second;
            }
        }
        return std::nullopt;
    }

    std::optional<core::VoiceCallSession> findPendingCall(
        const std::string& callerId, const std::string& receiverId) const override {
        std::lock_guard<std::mutex> lock(mutex_);
        for (const auto& pair : calls_) {
            if (pair.second.callerId == callerId &&
                pair.second.receiverId == receiverId &&
                pair.second.isPending()) {
                return pair.second;
            }
        }
        return std::nullopt;
    }

    bool update(const core::VoiceCallSession& call) override {
        std::lock_guard<std::mutex> lock(mutex_);
        auto it = calls_.find(call.callId);
        if (it != calls_.end()) {
            it->second = call;
            return true;
        }
        return false;
    }

    bool updateStatus(const std::string& callId, core::VoiceCallStatus status,
                      core::Timestamp endTime = 0) override {
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

    bool remove(const std::string& callId) override {
        std::lock_guard<std::mutex> lock(mutex_);
        return calls_.erase(callId) > 0;
    }

    size_t count() const override {
        std::lock_guard<std::mutex> lock(mutex_);
        return calls_.size();
    }

    size_t countActiveForUser(const std::string& userId) const override {
        std::lock_guard<std::mutex> lock(mutex_);
        size_t cnt = 0;
        for (const auto& pair : calls_) {
            if (pair.second.involvesUser(userId) && pair.second.isActive()) {
                cnt++;
            }
        }
        return cnt;
    }

private:
    std::map<std::string, core::VoiceCallSession>& calls_;
    std::mutex& mutex_;
};

} // namespace bridge
} // namespace repository
} // namespace english_learning

#endif // ENGLISH_LEARNING_BRIDGE_REPOSITORIES_EXT_H
