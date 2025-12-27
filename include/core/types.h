#ifndef ENGLISH_LEARNING_CORE_TYPES_H
#define ENGLISH_LEARNING_CORE_TYPES_H

#include <string>
#include <cstdint>

namespace english_learning {
namespace core {

// User roles
enum class UserRole {
    Student,
    Teacher,
    Admin
};

// Learning levels
enum class Level {
    Beginner,
    Intermediate,
    Advanced
};

// Topic categories
enum class Topic {
    Grammar,
    Vocabulary,
    Listening,
    Speaking,
    Reading,
    Writing
};

// Question types for tests
enum class QuestionType {
    MultipleChoice,
    FillBlank,
    SentenceOrder
};

// Exercise types
enum class ExerciseType {
    SentenceRewrite,
    ParagraphWriting,
    TopicSpeaking
};

// Submission status
enum class SubmissionStatus {
    Pending,
    Reviewed
};

// Game types
enum class GameType {
    WordMatch,
    SentenceMatch,
    PictureMatch
};

// Voice call status
enum class VoiceCallStatus {
    Pending,    // Call initiated, waiting for recipient
    Active,     // Call accepted, in progress
    Rejected,   // Call rejected by recipient
    Ended,      // Call ended normally
    Missed,     // Call not answered (timeout)
    Failed      // Call failed (technical error)
};

// Helper functions for string conversion
inline std::string levelToString(Level level) {
    switch (level) {
        case Level::Beginner: return "beginner";
        case Level::Intermediate: return "intermediate";
        case Level::Advanced: return "advanced";
    }
    return "beginner";
}

inline Level stringToLevel(const std::string& str) {
    if (str == "intermediate") return Level::Intermediate;
    if (str == "advanced") return Level::Advanced;
    return Level::Beginner;
}

inline std::string roleToString(UserRole role) {
    switch (role) {
        case UserRole::Student: return "student";
        case UserRole::Teacher: return "teacher";
        case UserRole::Admin: return "admin";
    }
    return "student";
}

inline UserRole stringToRole(const std::string& str) {
    if (str == "teacher") return UserRole::Teacher;
    if (str == "admin") return UserRole::Admin;
    return UserRole::Student;
}

inline std::string topicToString(Topic topic) {
    switch (topic) {
        case Topic::Grammar: return "grammar";
        case Topic::Vocabulary: return "vocabulary";
        case Topic::Listening: return "listening";
        case Topic::Speaking: return "speaking";
        case Topic::Reading: return "reading";
        case Topic::Writing: return "writing";
    }
    return "grammar";
}

inline Topic stringToTopic(const std::string& str) {
    if (str == "vocabulary") return Topic::Vocabulary;
    if (str == "listening") return Topic::Listening;
    if (str == "speaking") return Topic::Speaking;
    if (str == "reading") return Topic::Reading;
    if (str == "writing") return Topic::Writing;
    return Topic::Grammar;
}

inline std::string questionTypeToString(QuestionType type) {
    switch (type) {
        case QuestionType::MultipleChoice: return "multiple_choice";
        case QuestionType::FillBlank: return "fill_blank";
        case QuestionType::SentenceOrder: return "sentence_order";
    }
    return "multiple_choice";
}

inline QuestionType stringToQuestionType(const std::string& str) {
    if (str == "fill_blank") return QuestionType::FillBlank;
    if (str == "sentence_order") return QuestionType::SentenceOrder;
    return QuestionType::MultipleChoice;
}

inline std::string exerciseTypeToString(ExerciseType type) {
    switch (type) {
        case ExerciseType::SentenceRewrite: return "sentence_rewrite";
        case ExerciseType::ParagraphWriting: return "paragraph_writing";
        case ExerciseType::TopicSpeaking: return "topic_speaking";
    }
    return "sentence_rewrite";
}

inline ExerciseType stringToExerciseType(const std::string& str) {
    if (str == "paragraph_writing") return ExerciseType::ParagraphWriting;
    if (str == "topic_speaking") return ExerciseType::TopicSpeaking;
    return ExerciseType::SentenceRewrite;
}

inline std::string gameTypeToString(GameType type) {
    switch (type) {
        case GameType::WordMatch: return "word_match";
        case GameType::SentenceMatch: return "sentence_match";
        case GameType::PictureMatch: return "picture_match";
    }
    return "word_match";
}

inline GameType stringToGameType(const std::string& str) {
    if (str == "sentence_match") return GameType::SentenceMatch;
    if (str == "picture_match") return GameType::PictureMatch;
    return GameType::WordMatch;
}

inline std::string submissionStatusToString(SubmissionStatus status) {
    switch (status) {
        case SubmissionStatus::Pending: return "pending";
        case SubmissionStatus::Reviewed: return "reviewed";
    }
    return "pending";
}

inline SubmissionStatus stringToSubmissionStatus(const std::string& str) {
    if (str == "reviewed") return SubmissionStatus::Reviewed;
    return SubmissionStatus::Pending;
}

inline std::string voiceCallStatusToString(VoiceCallStatus status) {
    switch (status) {
        case VoiceCallStatus::Pending: return "pending";
        case VoiceCallStatus::Active: return "active";
        case VoiceCallStatus::Rejected: return "rejected";
        case VoiceCallStatus::Ended: return "ended";
        case VoiceCallStatus::Missed: return "missed";
        case VoiceCallStatus::Failed: return "failed";
    }
    return "pending";
}

inline VoiceCallStatus stringToVoiceCallStatus(const std::string& str) {
    if (str == "active") return VoiceCallStatus::Active;
    if (str == "rejected") return VoiceCallStatus::Rejected;
    if (str == "ended") return VoiceCallStatus::Ended;
    if (str == "missed") return VoiceCallStatus::Missed;
    if (str == "failed") return VoiceCallStatus::Failed;
    return VoiceCallStatus::Pending;
}

// Timestamp type alias
using Timestamp = int64_t;

} // namespace core
} // namespace english_learning

#endif // ENGLISH_LEARNING_CORE_TYPES_H
