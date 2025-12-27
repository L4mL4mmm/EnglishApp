#ifndef ENGLISH_LEARNING_SERVICE_CONTAINER_H
#define ENGLISH_LEARNING_SERVICE_CONTAINER_H

#include <memory>

// Repository interfaces
#include "include/repository/i_user_repository.h"
#include "include/repository/i_session_repository.h"
#include "include/repository/i_lesson_repository.h"
#include "include/repository/i_test_repository.h"
#include "include/repository/i_chat_repository.h"
#include "include/repository/i_exercise_repository.h"
#include "include/repository/i_game_repository.h"
#include "include/repository/i_voice_call_repository.h"

// Service interfaces
#include "include/service/i_auth_service.h"
#include "include/service/i_lesson_service.h"
#include "include/service/i_test_service.h"
#include "include/service/i_chat_service.h"
#include "include/service/i_exercise_service.h"
#include "include/service/i_game_service.h"
#include "include/service/i_voice_call_service.h"

// Service implementations
#include "auth_service.h"
#include "lesson_service.h"
#include "test_service.h"
#include "chat_service.h"
#include "exercise_service.h"
#include "game_service.h"
#include "voice_call_service.h"

namespace english_learning {
namespace service {

/**
 * Service container that manages all service instances.
 * Provides dependency injection for repositories into services.
 */
class ServiceContainer {
public:
    /**
     * Constructor with repository dependencies.
     * Creates all service instances with proper dependencies.
     */
    ServiceContainer(
        repository::IUserRepository& userRepo,
        repository::ISessionRepository& sessionRepo,
        repository::ILessonRepository& lessonRepo,
        repository::ITestRepository& testRepo,
        repository::IChatRepository& chatRepo,
        repository::IExerciseRepository& exerciseRepo,
        repository::IGameRepository& gameRepo,
        repository::IVoiceCallRepository& voiceCallRepo)
        : authService_(std::make_unique<AuthService>(userRepo, sessionRepo, chatRepo))
        , lessonService_(std::make_unique<LessonService>(lessonRepo, userRepo))
        , testService_(std::make_unique<TestService>(testRepo, userRepo))
        , chatService_(std::make_unique<ChatService>(chatRepo, userRepo))
        , exerciseService_(std::make_unique<ExerciseService>(exerciseRepo, userRepo))
        , gameService_(std::make_unique<GameService>(gameRepo, userRepo))
        , voiceCallService_(std::make_unique<VoiceCallService>(voiceCallRepo, userRepo))
    {}

    // Service accessors
    IAuthService& auth() { return *authService_; }
    ILessonService& lessons() { return *lessonService_; }
    ITestService& tests() { return *testService_; }
    IChatService& chat() { return *chatService_; }
    IExerciseService& exercises() { return *exerciseService_; }
    IGameService& games() { return *gameService_; }
    IVoiceCallService& voiceCalls() { return *voiceCallService_; }

    // Const accessors
    const IAuthService& auth() const { return *authService_; }
    const ILessonService& lessons() const { return *lessonService_; }
    const ITestService& tests() const { return *testService_; }
    const IChatService& chat() const { return *chatService_; }
    const IExerciseService& exercises() const { return *exerciseService_; }
    const IGameService& games() const { return *gameService_; }
    const IVoiceCallService& voiceCalls() const { return *voiceCallService_; }

private:
    std::unique_ptr<IAuthService> authService_;
    std::unique_ptr<ILessonService> lessonService_;
    std::unique_ptr<ITestService> testService_;
    std::unique_ptr<IChatService> chatService_;
    std::unique_ptr<IExerciseService> exerciseService_;
    std::unique_ptr<IGameService> gameService_;
    std::unique_ptr<IVoiceCallService> voiceCallService_;
};

} // namespace service
} // namespace english_learning

#endif // ENGLISH_LEARNING_SERVICE_CONTAINER_H
