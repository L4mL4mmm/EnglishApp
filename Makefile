# ============================================================================
# ENGLISH LEARNING APP - Makefile
# ============================================================================
# Compile và chạy server/client trên Linux/WSL
#
# Cách dùng:
#   make all      - Compile cả server và client
#   make server   - Compile server
#   make client   - Compile client
#   make clean    - Xóa các file binary
#   make run-server  - Chạy server
#   make run-client  - Chạy client
# ============================================================================

CXX = g++
CXXFLAGS = -std=c++17 -Wall -Wextra -pthread -O2

# Include paths for refactored headers
INCLUDES = -I.

# Flags for GTK (GUI)
GTK_CFLAGS := $(shell pkg-config --cflags gtk+-3.0 2>/dev/null)
GTK_LIBS := $(shell pkg-config --libs gtk+-3.0 2>/dev/null)

# Core header dependencies
CORE_HEADERS = include/core/types.h include/core/user.h include/core/session.h \
               include/core/lesson.h include/core/test.h include/core/chat_message.h \
               include/core/exercise.h include/core/game.h include/core/voice_call.h \
               include/core/all.h

# Protocol header dependencies
PROTOCOL_HEADERS = include/protocol/message_types.h include/protocol/json_parser.h \
                   include/protocol/json_builder.h include/protocol/utils.h \
                   include/protocol/all.h

# Protocol source files
PROTOCOL_SOURCES = src/protocol/json_parser.cpp

# Repository header dependencies
REPOSITORY_HEADERS = include/repository/i_user_repository.h include/repository/i_session_repository.h \
                     include/repository/i_lesson_repository.h include/repository/i_test_repository.h \
                     include/repository/i_chat_repository.h include/repository/i_exercise_repository.h \
                     include/repository/i_game_repository.h include/repository/i_voice_call_repository.h \
                     include/repository/all.h

# Bridge repository headers (wrap global data for service layer)
BRIDGE_HEADERS = src/repository/bridge/bridge_repositories.h src/repository/bridge/bridge_repositories_ext.h

# Repository source files (memory implementations)
REPOSITORY_SOURCES = src/repository/memory/memory_user_repository.cpp \
                     src/repository/memory/memory_session_repository.cpp \
                     src/repository/memory/memory_repositories.cpp

# Service header dependencies (interfaces)
SERVICE_HEADERS = include/service/service_result.h include/service/i_auth_service.h \
                  include/service/i_lesson_service.h include/service/i_test_service.h \
                  include/service/i_chat_service.h include/service/i_exercise_service.h \
                  include/service/i_game_service.h include/service/i_voice_call_service.h \
                  include/service/all.h

# Service source files (implementations)
SERVICE_SOURCES = src/service/auth_service.cpp src/service/lesson_service.cpp \
                  src/service/test_service.cpp src/service/chat_service.cpp \
                  src/service/exercise_service.cpp src/service/game_service.cpp \
                  src/service/voice_call_service.cpp

# All headers
ALL_HEADERS = $(CORE_HEADERS) $(PROTOCOL_HEADERS) $(REPOSITORY_HEADERS) $(BRIDGE_HEADERS) $(SERVICE_HEADERS)

# All library sources
LIB_SOURCES = $(PROTOCOL_SOURCES) $(REPOSITORY_SOURCES) $(SERVICE_SOURCES)

# Targets
all: server client gui

server: server.cpp $(ALL_HEADERS) $(LIB_SOURCES)
	$(CXX) $(CXXFLAGS) $(INCLUDES) -o server server.cpp $(LIB_SOURCES)
	@echo "Server compiled successfully!"

client: client.cpp $(PROTOCOL_HEADERS) $(PROTOCOL_SOURCES)
	$(CXX) $(CXXFLAGS) $(INCLUDES) -o client client.cpp $(PROTOCOL_SOURCES)
	@echo "Client compiled successfully!"

gui: gui_main.cpp client.cpp $(PROTOCOL_HEADERS) $(PROTOCOL_SOURCES)
	$(CXX) $(CXXFLAGS) $(INCLUDES) -DCLIENT_SKIP_MAIN gui_main.cpp client.cpp $(PROTOCOL_SOURCES) -o gui_app $(GTK_CFLAGS) $(GTK_LIBS)
	@echo "GUI App compiled successfully! Run with: ./gui_app"

clean:
	rm -f server client gui_app server.log
	@echo "Cleaned!"

run-server: server
	./server 8888

run-client: client
	./client 127.0.0.1 8888

run-gui: gui
	./gui_app

.PHONY: all clean run-server run-client run-gui
