#include "voice_call_service.h"
#include "include/protocol/utils.h"
#include <algorithm>

namespace english_learning {
namespace service {

VoiceCallService::VoiceCallService(
    repository::IVoiceCallRepository& callRepo,
    repository::IUserRepository& userRepo)
    : callRepo_(callRepo)
    , userRepo_(userRepo) {}

std::string VoiceCallService::getUserName(const std::string& userId) const {
    auto userOpt = userRepo_.findById(userId);
    if (userOpt.has_value()) {
        return userOpt.value().fullname;
    }
    return "Unknown";
}

VoiceCallStatusResult VoiceCallService::toStatusResult(
    const core::VoiceCallSession& call) const {
    VoiceCallStatusResult result;
    result.callId = call.callId;
    result.callerId = call.callerId;
    result.receiverId = call.receiverId;
    result.status = core::voiceCallStatusToString(call.status);
    result.callerName = getUserName(call.callerId);
    result.receiverName = getUserName(call.receiverId);
    result.startTime = call.startTime;
    result.acceptTime = call.acceptTime;
    result.endTime = call.endTime;
    result.durationSeconds = call.getDurationSeconds();
    return result;
}

ServiceResult<VoiceCallInitiateResult> VoiceCallService::initiateCall(
    const std::string& callerId,
    const std::string& receiverId,
    const std::string& audioSource) {

    // Validate caller exists
    auto callerOpt = userRepo_.findById(callerId);
    if (!callerOpt.has_value()) {
        return ServiceResult<VoiceCallInitiateResult>::error("Caller not found");
    }

    // Validate receiver exists
    auto receiverOpt = userRepo_.findById(receiverId);
    if (!receiverOpt.has_value()) {
        return ServiceResult<VoiceCallInitiateResult>::error("Receiver not found");
    }

    // Check if caller is already in a call
    auto callerActiveCall = callRepo_.findActiveCall(callerId);
    if (callerActiveCall.has_value()) {
        return ServiceResult<VoiceCallInitiateResult>::error("You are already in a call");
    }

    // Check if receiver is already in a call
    auto receiverActiveCall = callRepo_.findActiveCall(receiverId);
    if (receiverActiveCall.has_value()) {
        return ServiceResult<VoiceCallInitiateResult>::error("Receiver is already in a call");
    }

    // Check if there's already a pending call between these users
    auto pendingCall = callRepo_.findPendingCall(callerId, receiverId);
    if (pendingCall.has_value()) {
        return ServiceResult<VoiceCallInitiateResult>::error("Call already pending");
    }

    // Check if receiver is online
    const core::User& receiver = receiverOpt.value();
    if (!receiver.online) {
        return ServiceResult<VoiceCallInitiateResult>::error("Receiver is offline");
    }

    // Create call session
    core::VoiceCallSession call;
    call.callId = protocol::utils::generateId("call");
    call.callerId = callerId;
    call.receiverId = receiverId;
    call.status = core::VoiceCallStatus::Pending;
    call.audioSource = audioSource;
    call.startTime = protocol::utils::getCurrentTimestamp();
    call.acceptTime = 0;
    call.endTime = 0;

    if (!callRepo_.add(call)) {
        return ServiceResult<VoiceCallInitiateResult>::error("Failed to create call");
    }

    VoiceCallInitiateResult result;
    result.callId = call.callId;
    result.callerId = call.callerId;
    result.receiverId = call.receiverId;
    result.status = core::voiceCallStatusToString(call.status);
    result.receiverName = receiver.fullname;

    return ServiceResult<VoiceCallInitiateResult>::success(result);
}

ServiceResult<VoiceCallStatusResult> VoiceCallService::acceptCall(
    const std::string& callId,
    const std::string& userId) {

    auto callOpt = callRepo_.findById(callId);
    if (!callOpt.has_value()) {
        return ServiceResult<VoiceCallStatusResult>::error("Call not found");
    }

    core::VoiceCallSession call = callOpt.value();

    // Verify user is the receiver
    if (call.receiverId != userId) {
        return ServiceResult<VoiceCallStatusResult>::error("Only the receiver can accept the call");
    }

    // Verify call is pending
    if (!call.isPending()) {
        return ServiceResult<VoiceCallStatusResult>::error("Call is not pending");
    }

    // Check if user is already in another call
    auto activeCall = callRepo_.findActiveCall(userId);
    if (activeCall.has_value() && activeCall.value().callId != callId) {
        return ServiceResult<VoiceCallStatusResult>::error("You are already in another call");
    }

    // Accept the call
    call.accept(protocol::utils::getCurrentTimestamp());
    if (!callRepo_.update(call)) {
        return ServiceResult<VoiceCallStatusResult>::error("Failed to accept call");
    }

    return ServiceResult<VoiceCallStatusResult>::success(toStatusResult(call));
}

ServiceResult<VoiceCallStatusResult> VoiceCallService::rejectCall(
    const std::string& callId,
    const std::string& userId) {

    auto callOpt = callRepo_.findById(callId);
    if (!callOpt.has_value()) {
        return ServiceResult<VoiceCallStatusResult>::error("Call not found");
    }

    core::VoiceCallSession call = callOpt.value();

    // Verify user is the receiver
    if (call.receiverId != userId) {
        return ServiceResult<VoiceCallStatusResult>::error("Only the receiver can reject the call");
    }

    // Verify call is pending
    if (!call.isPending()) {
        return ServiceResult<VoiceCallStatusResult>::error("Call is not pending");
    }

    // Reject the call
    call.reject(protocol::utils::getCurrentTimestamp());
    if (!callRepo_.update(call)) {
        return ServiceResult<VoiceCallStatusResult>::error("Failed to reject call");
    }

    return ServiceResult<VoiceCallStatusResult>::success(toStatusResult(call));
}

ServiceResult<VoiceCallStatusResult> VoiceCallService::endCall(
    const std::string& callId,
    const std::string& userId) {

    auto callOpt = callRepo_.findById(callId);
    if (!callOpt.has_value()) {
        return ServiceResult<VoiceCallStatusResult>::error("Call not found");
    }

    core::VoiceCallSession call = callOpt.value();

    // Verify user is a participant
    if (!call.involvesUser(userId)) {
        return ServiceResult<VoiceCallStatusResult>::error("You are not a participant of this call");
    }

    // Verify call is active or pending
    if (call.hasEnded()) {
        return ServiceResult<VoiceCallStatusResult>::error("Call has already ended");
    }

    // End the call
    call.end(protocol::utils::getCurrentTimestamp());
    if (!callRepo_.update(call)) {
        return ServiceResult<VoiceCallStatusResult>::error("Failed to end call");
    }

    return ServiceResult<VoiceCallStatusResult>::success(toStatusResult(call));
}

ServiceResult<VoiceCallStatusResult> VoiceCallService::getCallStatus(
    const std::string& callId) {

    auto callOpt = callRepo_.findById(callId);
    if (!callOpt.has_value()) {
        return ServiceResult<VoiceCallStatusResult>::error("Call not found");
    }

    return ServiceResult<VoiceCallStatusResult>::success(toStatusResult(callOpt.value()));
}

ServiceResult<std::vector<PendingCallResult>> VoiceCallService::getPendingCalls(
    const std::string& userId) {

    auto pendingCalls = callRepo_.findPendingForUser(userId);
    std::vector<PendingCallResult> results;

    for (const auto& call : pendingCalls) {
        PendingCallResult result;
        result.callId = call.callId;
        result.callerId = call.callerId;
        result.callerName = getUserName(call.callerId);
        result.audioSource = call.audioSource;
        result.startTime = call.startTime;
        results.push_back(result);
    }

    return ServiceResult<std::vector<PendingCallResult>>::success(results);
}

ServiceResult<VoiceCallStatusResult> VoiceCallService::getActiveCall(
    const std::string& userId) {

    auto callOpt = callRepo_.findActiveCall(userId);
    if (!callOpt.has_value()) {
        return ServiceResult<VoiceCallStatusResult>::error("No active call");
    }

    return ServiceResult<VoiceCallStatusResult>::success(toStatusResult(callOpt.value()));
}

ServiceResult<bool> VoiceCallService::isUserInCall(const std::string& userId) {
    auto callOpt = callRepo_.findActiveCall(userId);
    return ServiceResult<bool>::success(callOpt.has_value());
}

ServiceResult<std::vector<VoiceCallStatusResult>> VoiceCallService::getCallHistory(
    const std::string& userId,
    size_t limit) {

    auto calls = callRepo_.findByUser(userId);
    std::vector<VoiceCallStatusResult> results;

    // Filter to ended calls and sort by time (most recent first)
    std::vector<core::VoiceCallSession> endedCalls;
    for (const auto& call : calls) {
        if (call.hasEnded()) {
            endedCalls.push_back(call);
        }
    }

    // Sort by end time descending
    std::sort(endedCalls.begin(), endedCalls.end(),
              [](const core::VoiceCallSession& a, const core::VoiceCallSession& b) {
                  return a.endTime > b.endTime;
              });

    // Apply limit
    size_t count = limit > 0 ? std::min(limit, endedCalls.size()) : endedCalls.size();
    for (size_t i = 0; i < count; ++i) {
        results.push_back(toStatusResult(endedCalls[i]));
    }

    return ServiceResult<std::vector<VoiceCallStatusResult>>::success(results);
}

} // namespace service
} // namespace english_learning
