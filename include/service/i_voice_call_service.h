#ifndef ENGLISH_LEARNING_SERVICE_I_VOICE_CALL_SERVICE_H
#define ENGLISH_LEARNING_SERVICE_I_VOICE_CALL_SERVICE_H

#include <string>
#include <vector>
#include "service_result.h"
#include "include/core/voice_call.h"

namespace english_learning {
namespace service {

/**
 * DTO for voice call initiation result.
 */
struct VoiceCallInitiateResult {
    std::string callId;
    std::string callerId;
    std::string receiverId;
    std::string status;
    std::string receiverName;
};

/**
 * DTO for voice call status result.
 */
struct VoiceCallStatusResult {
    std::string callId;
    std::string callerId;
    std::string receiverId;
    std::string status;
    std::string callerName;
    std::string receiverName;
    int64_t startTime;
    int64_t acceptTime;
    int64_t endTime;
    int64_t durationSeconds;
};

/**
 * DTO for pending call notification.
 */
struct PendingCallResult {
    std::string callId;
    std::string callerId;
    std::string callerName;
    std::string audioSource;
    int64_t startTime;
};

/**
 * Interface for voice call services.
 */
class IVoiceCallService {
public:
    virtual ~IVoiceCallService() = default;

    /**
     * Initiate a voice call to another user.
     * @param callerId The caller's user ID
     * @param receiverId The receiver's user ID
     * @param audioSource Audio source type (e.g., "microphone", "file")
     * @return Call session details on success
     */
    virtual ServiceResult<VoiceCallInitiateResult> initiateCall(
        const std::string& callerId,
        const std::string& receiverId,
        const std::string& audioSource = "microphone") = 0;

    /**
     * Accept an incoming voice call.
     * @param callId The call ID to accept
     * @param userId The user accepting the call (must be receiver)
     * @return Updated call status on success
     */
    virtual ServiceResult<VoiceCallStatusResult> acceptCall(
        const std::string& callId,
        const std::string& userId) = 0;

    /**
     * Reject an incoming voice call.
     * @param callId The call ID to reject
     * @param userId The user rejecting the call (must be receiver)
     * @return Updated call status on success
     */
    virtual ServiceResult<VoiceCallStatusResult> rejectCall(
        const std::string& callId,
        const std::string& userId) = 0;

    /**
     * End an active voice call.
     * @param callId The call ID to end
     * @param userId The user ending the call (must be participant)
     * @return Final call status on success
     */
    virtual ServiceResult<VoiceCallStatusResult> endCall(
        const std::string& callId,
        const std::string& userId) = 0;

    /**
     * Get the current status of a call.
     * @param callId The call ID to check
     * @return Call status details
     */
    virtual ServiceResult<VoiceCallStatusResult> getCallStatus(
        const std::string& callId) = 0;

    /**
     * Get any pending incoming calls for a user.
     * @param userId The user's ID
     * @return List of pending calls
     */
    virtual ServiceResult<std::vector<PendingCallResult>> getPendingCalls(
        const std::string& userId) = 0;

    /**
     * Get the active call for a user (if any).
     * @param userId The user's ID
     * @return Active call status or error if no active call
     */
    virtual ServiceResult<VoiceCallStatusResult> getActiveCall(
        const std::string& userId) = 0;

    /**
     * Check if a user is currently in a call.
     * @param userId The user's ID
     * @return True if user is in an active call
     */
    virtual ServiceResult<bool> isUserInCall(const std::string& userId) = 0;

    /**
     * Get call history for a user.
     * @param userId The user's ID
     * @param limit Maximum calls to return (0 for all)
     * @return List of past calls
     */
    virtual ServiceResult<std::vector<VoiceCallStatusResult>> getCallHistory(
        const std::string& userId,
        size_t limit = 20) = 0;
};

} // namespace service
} // namespace english_learning

#endif // ENGLISH_LEARNING_SERVICE_I_VOICE_CALL_SERVICE_H
