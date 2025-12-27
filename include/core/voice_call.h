#ifndef ENGLISH_LEARNING_CORE_VOICE_CALL_H
#define ENGLISH_LEARNING_CORE_VOICE_CALL_H

#include <string>
#include "types.h"

namespace english_learning {
namespace core {

/**
 * VoiceCallSession entity representing a voice call between two users.
 * Tracks caller, receiver, status, timing, and audio source.
 */
struct VoiceCallSession {
    std::string callId;
    std::string callerId;
    std::string receiverId;
    VoiceCallStatus status;
    std::string audioSource;    // e.g., "microphone", "file", "system"
    Timestamp startTime;        // When call was initiated
    Timestamp acceptTime;       // When call was accepted (0 if not accepted)
    Timestamp endTime;          // When call ended (0 if ongoing)

    VoiceCallSession()
        : status(VoiceCallStatus::Pending)
        , startTime(0)
        , acceptTime(0)
        , endTime(0) {}

    VoiceCallSession(const std::string& id, const std::string& caller,
                     const std::string& receiver, Timestamp start)
        : callId(id)
        , callerId(caller)
        , receiverId(receiver)
        , status(VoiceCallStatus::Pending)
        , audioSource("microphone")
        , startTime(start)
        , acceptTime(0)
        , endTime(0) {}

    // Check if call involves a specific user (as caller or receiver)
    bool involvesUser(const std::string& userId) const {
        return callerId == userId || receiverId == userId;
    }

    // Check if call is between two specific users
    bool isBetween(const std::string& user1, const std::string& user2) const {
        return (callerId == user1 && receiverId == user2) ||
               (callerId == user2 && receiverId == user1);
    }

    // Check if call is currently active
    bool isActive() const {
        return status == VoiceCallStatus::Active;
    }

    // Check if call is pending (waiting for response)
    bool isPending() const {
        return status == VoiceCallStatus::Pending;
    }

    // Check if call has ended (any terminal state)
    bool hasEnded() const {
        return status == VoiceCallStatus::Ended ||
               status == VoiceCallStatus::Rejected ||
               status == VoiceCallStatus::Missed ||
               status == VoiceCallStatus::Failed;
    }

    // Accept the call
    void accept(Timestamp time) {
        status = VoiceCallStatus::Active;
        acceptTime = time;
    }

    // Reject the call
    void reject(Timestamp time) {
        status = VoiceCallStatus::Rejected;
        endTime = time;
    }

    // End the call normally
    void end(Timestamp time) {
        status = VoiceCallStatus::Ended;
        endTime = time;
    }

    // Mark call as missed (timeout)
    void miss(Timestamp time) {
        status = VoiceCallStatus::Missed;
        endTime = time;
    }

    // Mark call as failed (technical error)
    void fail(Timestamp time) {
        status = VoiceCallStatus::Failed;
        endTime = time;
    }

    // Get call duration in seconds (0 if not accepted or still ongoing)
    int64_t getDurationSeconds() const {
        if (acceptTime == 0) return 0;
        if (endTime == 0) return 0; // Still ongoing
        return (endTime - acceptTime) / 1000; // Assuming milliseconds
    }
};

} // namespace core
} // namespace english_learning

#endif // ENGLISH_LEARNING_CORE_VOICE_CALL_H
