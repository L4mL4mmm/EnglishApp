#ifndef ENGLISH_LEARNING_REPOSITORY_I_VOICE_CALL_REPOSITORY_H
#define ENGLISH_LEARNING_REPOSITORY_I_VOICE_CALL_REPOSITORY_H

#include <string>
#include <vector>
#include <optional>
#include "include/core/voice_call.h"

namespace english_learning {
namespace repository {

/**
 * Interface for voice call session data access operations.
 * Provides CRUD operations for VoiceCallSession entities.
 */
class IVoiceCallRepository {
public:
    virtual ~IVoiceCallRepository() = default;

    // Create
    virtual bool add(const core::VoiceCallSession& call) = 0;

    // Read
    virtual std::optional<core::VoiceCallSession> findById(const std::string& callId) const = 0;
    virtual std::vector<core::VoiceCallSession> findAll() const = 0;
    virtual std::vector<core::VoiceCallSession> findByUser(const std::string& userId) const = 0;
    virtual std::vector<core::VoiceCallSession> findActiveByUser(const std::string& userId) const = 0;
    virtual std::vector<core::VoiceCallSession> findPendingForUser(const std::string& userId) const = 0;
    virtual std::optional<core::VoiceCallSession> findActiveCall(const std::string& userId) const = 0;
    virtual std::optional<core::VoiceCallSession> findPendingCall(
        const std::string& callerId, const std::string& receiverId) const = 0;

    // Update
    virtual bool update(const core::VoiceCallSession& call) = 0;
    virtual bool updateStatus(const std::string& callId, core::VoiceCallStatus status,
                              core::Timestamp endTime = 0) = 0;

    // Delete
    virtual bool remove(const std::string& callId) = 0;

    // Utility
    virtual size_t count() const = 0;
    virtual size_t countActiveForUser(const std::string& userId) const = 0;
};

} // namespace repository
} // namespace english_learning

#endif // ENGLISH_LEARNING_REPOSITORY_I_VOICE_CALL_REPOSITORY_H
