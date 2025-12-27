#ifndef ENGLISH_LEARNING_SERVICE_VOICE_CALL_SERVICE_H
#define ENGLISH_LEARNING_SERVICE_VOICE_CALL_SERVICE_H

#include "include/service/i_voice_call_service.h"
#include "include/repository/i_voice_call_repository.h"
#include "include/repository/i_user_repository.h"

namespace english_learning {
namespace service {

/**
 * Implementation of voice call service.
 */
class VoiceCallService : public IVoiceCallService {
public:
    VoiceCallService(
        repository::IVoiceCallRepository& callRepo,
        repository::IUserRepository& userRepo);

    ~VoiceCallService() override = default;

    ServiceResult<VoiceCallInitiateResult> initiateCall(
        const std::string& callerId,
        const std::string& receiverId,
        const std::string& audioSource = "microphone") override;

    ServiceResult<VoiceCallStatusResult> acceptCall(
        const std::string& callId,
        const std::string& userId) override;

    ServiceResult<VoiceCallStatusResult> rejectCall(
        const std::string& callId,
        const std::string& userId) override;

    ServiceResult<VoiceCallStatusResult> endCall(
        const std::string& callId,
        const std::string& userId) override;

    ServiceResult<VoiceCallStatusResult> getCallStatus(
        const std::string& callId) override;

    ServiceResult<std::vector<PendingCallResult>> getPendingCalls(
        const std::string& userId) override;

    ServiceResult<VoiceCallStatusResult> getActiveCall(
        const std::string& userId) override;

    ServiceResult<bool> isUserInCall(const std::string& userId) override;

    ServiceResult<std::vector<VoiceCallStatusResult>> getCallHistory(
        const std::string& userId,
        size_t limit = 20) override;

private:
    repository::IVoiceCallRepository& callRepo_;
    repository::IUserRepository& userRepo_;

    // Helper to convert call session to status result
    VoiceCallStatusResult toStatusResult(const core::VoiceCallSession& call) const;

    // Helper to get user's full name
    std::string getUserName(const std::string& userId) const;
};

} // namespace service
} // namespace english_learning

#endif // ENGLISH_LEARNING_SERVICE_VOICE_CALL_SERVICE_H
