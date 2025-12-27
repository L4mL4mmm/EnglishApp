#ifndef ENGLISH_LEARNING_PROTOCOL_MESSAGE_TYPES_H
#define ENGLISH_LEARNING_PROTOCOL_MESSAGE_TYPES_H

#include <string>

namespace english_learning {
namespace protocol {

/**
 * Message type constants for the application protocol.
 * All communication uses JSON messages with a "messageType" field.
 */
namespace MessageType {

// Authentication
constexpr const char* REGISTER_REQUEST = "REGISTER_REQUEST";
constexpr const char* REGISTER_RESPONSE = "REGISTER_RESPONSE";
constexpr const char* LOGIN_REQUEST = "LOGIN_REQUEST";
constexpr const char* LOGIN_RESPONSE = "LOGIN_RESPONSE";
constexpr const char* SET_LEVEL_REQUEST = "SET_LEVEL_REQUEST";
constexpr const char* SET_LEVEL_RESPONSE = "SET_LEVEL_RESPONSE";

// Lessons
constexpr const char* GET_LESSONS_REQUEST = "GET_LESSONS_REQUEST";
constexpr const char* GET_LESSONS_RESPONSE = "GET_LESSONS_RESPONSE";
constexpr const char* GET_LESSON_DETAIL_REQUEST = "GET_LESSON_DETAIL_REQUEST";
constexpr const char* GET_LESSON_DETAIL_RESPONSE = "GET_LESSON_DETAIL_RESPONSE";

// Tests
constexpr const char* GET_TEST_REQUEST = "GET_TEST_REQUEST";
constexpr const char* GET_TEST_RESPONSE = "GET_TEST_RESPONSE";
constexpr const char* SUBMIT_TEST_REQUEST = "SUBMIT_TEST_REQUEST";
constexpr const char* SUBMIT_TEST_RESPONSE = "SUBMIT_TEST_RESPONSE";

// Exercises
constexpr const char* GET_EXERCISE_REQUEST = "GET_EXERCISE_REQUEST";
constexpr const char* GET_EXERCISE_RESPONSE = "GET_EXERCISE_RESPONSE";
constexpr const char* SUBMIT_EXERCISE_REQUEST = "SUBMIT_EXERCISE_REQUEST";
constexpr const char* SUBMIT_EXERCISE_RESPONSE = "SUBMIT_EXERCISE_RESPONSE";
constexpr const char* GET_PENDING_REVIEWS_REQUEST = "GET_PENDING_REVIEWS_REQUEST";
constexpr const char* GET_PENDING_REVIEWS_RESPONSE = "GET_PENDING_REVIEWS_RESPONSE";
constexpr const char* REVIEW_EXERCISE_REQUEST = "REVIEW_EXERCISE_REQUEST";
constexpr const char* REVIEW_EXERCISE_RESPONSE = "REVIEW_EXERCISE_RESPONSE";
constexpr const char* GET_FEEDBACK_REQUEST = "GET_FEEDBACK_REQUEST";
constexpr const char* GET_FEEDBACK_RESPONSE = "GET_FEEDBACK_RESPONSE";
constexpr const char* GET_USER_SUBMISSIONS_REQUEST = "GET_USER_SUBMISSIONS_REQUEST";
constexpr const char* GET_USER_SUBMISSIONS_RESPONSE = "GET_USER_SUBMISSIONS_RESPONSE";

// Games
constexpr const char* GET_GAME_LIST_REQUEST = "GET_GAME_LIST_REQUEST";
constexpr const char* GET_GAME_LIST_RESPONSE = "GET_GAME_LIST_RESPONSE";
constexpr const char* START_GAME_REQUEST = "START_GAME_REQUEST";
constexpr const char* START_GAME_RESPONSE = "START_GAME_RESPONSE";
constexpr const char* SUBMIT_GAME_RESULT_REQUEST = "SUBMIT_GAME_RESULT_REQUEST";
constexpr const char* SUBMIT_GAME_RESULT_RESPONSE = "SUBMIT_GAME_RESULT_RESPONSE";

// Game Admin
constexpr const char* ADD_GAME_REQUEST = "ADD_GAME_REQUEST";
constexpr const char* ADD_GAME_RESPONSE = "ADD_GAME_RESPONSE";
constexpr const char* UPDATE_GAME_REQUEST = "UPDATE_GAME_REQUEST";
constexpr const char* UPDATE_GAME_RESPONSE = "UPDATE_GAME_RESPONSE";
constexpr const char* DELETE_GAME_REQUEST = "DELETE_GAME_REQUEST";
constexpr const char* DELETE_GAME_RESPONSE = "DELETE_GAME_RESPONSE";
constexpr const char* GET_ADMIN_GAMES_REQUEST = "GET_ADMIN_GAMES_REQUEST";
constexpr const char* GET_ADMIN_GAMES_RESPONSE = "GET_ADMIN_GAMES_RESPONSE";

// Chat
constexpr const char* GET_CONTACT_LIST_REQUEST = "GET_CONTACT_LIST_REQUEST";
constexpr const char* GET_CONTACT_LIST_RESPONSE = "GET_CONTACT_LIST_RESPONSE";
constexpr const char* SEND_MESSAGE_REQUEST = "SEND_MESSAGE_REQUEST";
constexpr const char* SEND_MESSAGE_RESPONSE = "SEND_MESSAGE_RESPONSE";
constexpr const char* GET_CHAT_HISTORY_REQUEST = "GET_CHAT_HISTORY_REQUEST";
constexpr const char* GET_CHAT_HISTORY_RESPONSE = "GET_CHAT_HISTORY_RESPONSE";
constexpr const char* MARK_MESSAGES_READ_REQUEST = "MARK_MESSAGES_READ_REQUEST";
constexpr const char* MARK_MESSAGES_READ_RESPONSE = "MARK_MESSAGES_READ_RESPONSE";

// Push Notifications (server -> client)
constexpr const char* RECEIVE_MESSAGE = "RECEIVE_MESSAGE";
constexpr const char* UNREAD_MESSAGES_NOTIFICATION = "UNREAD_MESSAGES_NOTIFICATION";
constexpr const char* EXERCISE_FEEDBACK_NOTIFICATION = "EXERCISE_FEEDBACK_NOTIFICATION";

// Voice Call
constexpr const char* VOICE_CALL_INITIATE_REQUEST = "VOICE_CALL_INITIATE_REQUEST";
constexpr const char* VOICE_CALL_INITIATE_RESPONSE = "VOICE_CALL_INITIATE_RESPONSE";
constexpr const char* VOICE_CALL_ACCEPT_REQUEST = "VOICE_CALL_ACCEPT_REQUEST";
constexpr const char* VOICE_CALL_ACCEPT_RESPONSE = "VOICE_CALL_ACCEPT_RESPONSE";
constexpr const char* VOICE_CALL_REJECT_REQUEST = "VOICE_CALL_REJECT_REQUEST";
constexpr const char* VOICE_CALL_REJECT_RESPONSE = "VOICE_CALL_REJECT_RESPONSE";
constexpr const char* VOICE_CALL_END_REQUEST = "VOICE_CALL_END_REQUEST";
constexpr const char* VOICE_CALL_END_RESPONSE = "VOICE_CALL_END_RESPONSE";
constexpr const char* VOICE_CALL_GET_STATUS_REQUEST = "VOICE_CALL_GET_STATUS_REQUEST";
constexpr const char* VOICE_CALL_GET_STATUS_RESPONSE = "VOICE_CALL_GET_STATUS_RESPONSE";

// Voice Call Push Notifications (server -> client)
constexpr const char* VOICE_CALL_INCOMING = "VOICE_CALL_INCOMING";
constexpr const char* VOICE_CALL_ACCEPTED = "VOICE_CALL_ACCEPTED";
constexpr const char* VOICE_CALL_REJECTED = "VOICE_CALL_REJECTED";
constexpr const char* VOICE_CALL_ENDED = "VOICE_CALL_ENDED";

// Error
constexpr const char* ERROR_RESPONSE = "ERROR_RESPONSE";

} // namespace MessageType

/**
 * Response status constants
 */
namespace ResponseStatus {
    constexpr const char* SUCCESS = "success";
    constexpr const char* ERROR = "error";
}

} // namespace protocol
} // namespace english_learning

#endif // ENGLISH_LEARNING_PROTOCOL_MESSAGE_TYPES_H
