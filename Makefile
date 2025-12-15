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

# Flags for GTK (GUI)
GTK_CFLAGS := $(shell pkg-config --cflags gtk+-3.0 2>/dev/null)
GTK_LIBS := $(shell pkg-config --libs gtk+-3.0 2>/dev/null)

# Targets
all: server client gui

server: server.cpp
	$(CXX) $(CXXFLAGS) -o server server.cpp
	@echo "Server compiled successfully!"

client: client.cpp
	$(CXX) $(CXXFLAGS) -o client client.cpp
	@echo "Client compiled successfully!"

gui: gui_main.cpp client.cpp
	$(CXX) $(CXXFLAGS) -DCLIENT_SKIP_MAIN gui_main.cpp client.cpp -o gui_app $(GTK_CFLAGS) $(GTK_LIBS)
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
