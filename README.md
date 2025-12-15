# English Learning App

A comprehensive English learning application with both console and GUI interfaces, featuring lessons, tests, games, and real-time chat functionality.

## Features

- **Multiple Learning Modes**: Lessons, tests, exercises, and interactive games
- **Level-based Content**: Beginner, Intermediate, and Advanced levels
- **Real-time Chat**: Message other users in real-time
- **Two Interfaces**: 
  - Console client for terminal users
  - GTK+ GUI for desktop users with enhanced chat features
- **Multi-user Support**: Server handles multiple concurrent connections

## System Requirements

- **OS**: Linux (Ubuntu/Debian) or WSL (Windows Subsystem for Linux)
- **Compiler**: G++ with C++17 support
- **Libraries**: 
  - GTK+ 3.0 (for GUI version only)
  - pthread (included in standard library)

## Installation

### 1. Install Dependencies

```bash
# Ubuntu/Debian
sudo apt update
sudo apt install g++ make libgtk-3-dev

# Fedora
sudo dnf install g++ make gtk3-devel
```

### 2. Clone/Download the Project

```bash
cd /path/to/EnglishApp
```

### 3. Compile

```bash
# Build everything (server, console client, and GUI)
make all

# Or build individually:
make server    # Server only
make client    # Console client only
make gui       # GUI client only
```

## Running the Application

### Start the Server (Required)

Open a terminal and run:

```bash
./server 8888
```

The server will display:
```
============================================
   ENGLISH LEARNING APP - SERVER
============================================
[INFO] Server started on port 8888
[INFO] Waiting for connections...
```

**Sample Accounts:**
- Email: `student@example.com` | Password: `student123`
- Email: `sarah@example.com` | Password: `teacher123`

### Option 1: Console Client

```bash
./client 127.0.0.1 8888
```

Features full terminal-based interface with all learning features.

### Option 2: GUI Client (Recommended)

```bash
./gui_app
```

**GUI Features:**
- Visual interface with GTK+
- Enhanced chat with conversation history
- **NEW**: Send messages directly from chat popup
- Easy navigation through lessons and tests

## Usage Guide

### First Time Setup

1. **Start the server** in one terminal
2. **Launch a client** (console or GUI) in another terminal
3. **Login** with sample account or **Register** a new account
4. **Set your level**: beginner, intermediate, or advanced

### Main Features

#### 1. Set English Level
Choose your proficiency level to get appropriate content.

#### 2. View Lessons
Browse and study lessons filtered by:
- Grammar
- Vocabulary
- Listening
- Speaking
- Reading
- Writing

#### 3. Take Tests
Complete multiple-choice, fill-in-the-blank, and sentence ordering questions.

#### 4. Chat with Others
- View contact list
- Send messages in real-time
- **GUI**: Open conversation popup to view history and send messages

#### 5. Play Games
Interactive matching games:
- Word matching
- Sentence matching
- Picture matching

## Project Structure

```
EnglishApp/
├── server.cpp              # Server implementation
├── client.cpp              # Console client
├── gui_main.cpp            # GUI client
├── client_bridge.h         # Shared interface
├── Makefile                # Build configuration
├── README.md               # This file
├── DOCUMENTATION.md        # Technical documentation
└── THIẾT KẾ BẢN TIN...md  # Protocol specification (Vietnamese)
```

## Network Protocol

All messages use the format:
```
[4 bytes: message length][JSON message]
```

Supported message types:
- `LOGIN_REQUEST/RESPONSE`
- `REGISTER_REQUEST/RESPONSE`
- `GET_LESSONS_REQUEST/RESPONSE`
- `GET_TEST_REQUEST/RESPONSE`
- `SEND_MESSAGE_REQUEST/RESPONSE`
- `GET_CHAT_HISTORY_REQUEST/RESPONSE`
- And more...

See `DOCUMENTATION.md` for full protocol details.

## Troubleshooting

### "Address already in use"
```bash
# Find process using port 8888
lsof -i :8888

# Kill the process
kill -9 <PID>
```

### "Connection refused"
- Ensure server is running
- Check IP address and port are correct

### GUI won't compile
```bash
# Ensure GTK+ is installed
pkg-config --modversion gtk+-3.0

# If not found, install:
sudo apt install libgtk-3-dev
```

### Can't send messages in chat popup
- Make sure you compiled the latest version: `make clean && make gui`
- The input field and Send button should appear at the bottom of the conversation window

## Development

### Clean Build

```bash
make clean
make all
```

### Running Multiple Clients

To test chat functionality:

1. Terminal 1: `./server 8888`
2. Terminal 2: `./gui_app` (login as student@example.com)
3. Terminal 3: `./gui_app` (login as sarah@example.com)
4. Chat between the two users

## License

Educational project for learning C++ network programming and GUI development.

## Contributors

Developed as a comprehensive English learning platform with client-server architecture.
