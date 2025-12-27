# English Learning Application

A client-server application for learning English, built with modern C++17. The system provides interactive lessons, tests, exercises, vocabulary games, and real-time chat between students and teachers. Designed with clean architecture principles, the codebase is modular, testable, and extensible.

---

## Table of Contents

- [Features](#features)
- [Architecture Overview](#architecture-overview)
- [Project Structure](#project-structure)
- [Technology Stack](#technology-stack)
- [Build and Run Instructions](#build-and-run-instructions)
- [Usage](#usage)
- [Development Notes](#development-notes)
- [Security Notes](#security-notes)
- [Roadmap](#roadmap)
- [Contributing](#contributing)
- [License](#license)

---

## Features

### Server

- Multi-client support with thread-per-connection model
- Session-based authentication with token expiration
- Real-time message push for chat notifications
- Role-based access control (Student, Teacher, Admin)
- In-memory data storage with repository abstraction
- Comprehensive logging to file and console

### Client

- **Console Client**: Full-featured terminal interface with colored output
- **GUI Client**: GTK+ 3.0 graphical interface with modern widgets
- Background thread for receiving push notifications
- Automatic reconnection handling
- Real-time chat with delivery confirmation

### Learning Features

- **Lessons**: Browse and study lessons filtered by level and topic
- **Tests**: Multiple question types (multiple choice, fill-in-blank, sentence ordering)
- **Exercises**: Writing practice with teacher feedback workflow
- **Games**: Word matching, sentence matching, and picture matching games
- **Chat**: Real-time messaging between students and teachers
- **Levels**: Beginner, Intermediate, and Advanced difficulty tiers
- **Topics**: Grammar, Vocabulary, Listening, Speaking, Reading, Writing

### Architecture Quality

- Clean separation of concerns across layers
- Interface-based design enabling dependency injection
- Protocol abstraction for transport-agnostic messaging
- Service layer encapsulating business logic
- Repository pattern for data access abstraction

---

## Architecture Overview

The application follows a layered clean architecture pattern, separating concerns into distinct layers with well-defined boundaries.

```
+-------------------------------------------------------------------+
|                       PRESENTATION LAYER                          |
|  +-------------+  +-------------+  +---------------------------+  |
|  |   Server    |  |   Console   |  |       GUI Client          |  |
|  |  (TCP/IP)   |  |   Client    |  |       (GTK+ 3.0)          |  |
|  +------+------+  +------+------+  +-------------+-------------+  |
+---------|-----------------|-----------------------|----------------+
          |                 |                       |
+---------v-----------------v-----------------------v----------------+
|                        PROTOCOL LAYER                              |
|  +--------------+  +--------------+  +--------------------------+  |
|  | Message Types|  | JSON Parser  |  |     JSON Builder         |  |
|  +--------------+  +--------------+  +--------------------------+  |
+-------------------------------------------------------------------+
          |
+---------v---------------------------------------------------------+
|                        SERVICE LAYER                               |
|  +--------+ +--------+ +--------+ +--------+ +--------+ +------+  |
|  |  Auth  | | Lesson | |  Test  | |Exercise| |  Game  | | Chat |  |
|  |Service | |Service | |Service | |Service | |Service | |Serv. |  |
|  +--------+ +--------+ +--------+ +--------+ +--------+ +------+  |
+-------------------------------------------------------------------+
          |
+---------v---------------------------------------------------------+
|                      REPOSITORY LAYER                              |
|  +-----------------+  +-----------------+  +-------------------+   |
|  | IUserRepository |  |ILessonRepository|  | ITestRepository   |   |
|  +--------+--------+  +--------+--------+  +---------+---------+   |
|           |                    |                     |             |
|  +--------v--------+  +--------v--------+  +---------v---------+   |
|  | MemoryUserRepo  |  |MemoryLessonRepo |  | MemoryTestRepo    |   |
|  +-----------------+  +-----------------+  +-------------------+   |
+-------------------------------------------------------------------+
          |
+---------v---------------------------------------------------------+
|                         CORE LAYER                                 |
|  +------+ +--------+ +------+ +--------+ +------+ +------------+  |
|  | User | | Lesson | | Test | |Exercise| | Game | |  Session   |  |
|  +------+ +--------+ +------+ +--------+ +------+ +------------+  |
+-------------------------------------------------------------------+
```

### Design Rationale

- **Separation of Concerns**: Each layer has a single responsibility, making the codebase easier to understand and maintain.
- **Dependency Inversion**: Upper layers depend on abstractions (interfaces), not concrete implementations.
- **Testability**: Services can be tested in isolation by mocking repository interfaces.
- **Extensibility**: New features can be added by implementing existing interfaces without modifying core logic.
- **Protocol Independence**: The protocol layer can be replaced (e.g., switch from JSON to Protocol Buffers) without affecting business logic.

---

## Project Structure

```
EnglishLearning/
|-- Makefile                    # Build configuration
|-- README.md                   # This file
|-- server.cpp                  # Server entry point and request handlers
|-- client.cpp                  # Console client implementation
|-- gui_main.cpp                # GTK+ GUI client implementation
|-- client_bridge.h             # Shared declarations for GUI/client integration
|
|-- include/                    # Public headers (interfaces and models)
|   |-- core/                   # Domain models
|   |   |-- all.h               # Aggregate include
|   |   |-- types.h             # Enums and type definitions
|   |   |-- user.h              # User entity
|   |   |-- session.h           # Session entity
|   |   |-- lesson.h            # Lesson entity
|   |   |-- test.h              # Test and TestQuestion entities
|   |   |-- exercise.h          # Exercise and Submission entities
|   |   |-- game.h              # Game and GameSession entities
|   |   +-- chat_message.h      # ChatMessage entity
|   |
|   |-- protocol/               # Protocol definitions
|   |   |-- all.h               # Aggregate include
|   |   |-- message_types.h     # Message type constants
|   |   |-- json_parser.h       # JSON parsing utilities
|   |   |-- json_builder.h      # JSON construction utilities
|   |   +-- utils.h             # Timestamp and ID generation
|   |
|   |-- repository/             # Repository interfaces
|   |   |-- all.h               # Aggregate include
|   |   |-- i_user_repository.h
|   |   |-- i_session_repository.h
|   |   |-- i_lesson_repository.h
|   |   |-- i_test_repository.h
|   |   |-- i_exercise_repository.h
|   |   |-- i_game_repository.h
|   |   +-- i_chat_repository.h
|   |
|   +-- service/                # Service interfaces
|       |-- all.h               # Aggregate include
|       |-- service_result.h    # Generic result wrapper
|       |-- i_auth_service.h
|       |-- i_lesson_service.h
|       |-- i_test_service.h
|       |-- i_exercise_service.h
|       |-- i_game_service.h
|       +-- i_chat_service.h
|
|-- src/                        # Implementation files
|   |-- protocol/
|   |   +-- json_parser.cpp     # JSON parsing implementation
|   |
|   |-- repository/
|   |   |-- bridge/             # Adapters for legacy data structures
|   |   |   |-- bridge_repositories.h
|   |   |   +-- bridge_repositories_ext.h
|   |   +-- memory/             # In-memory repository implementations
|   |       |-- all.h
|   |       |-- memory_repositories.h
|   |       |-- memory_repositories.cpp
|   |       |-- memory_user_repository.h
|   |       |-- memory_user_repository.cpp
|   |       |-- memory_session_repository.h
|   |       +-- memory_session_repository.cpp
|   |
|   +-- service/                # Service implementations
|       |-- all.h
|       |-- service_container.h # Dependency injection container
|       |-- auth_service.h / .cpp
|       |-- lesson_service.h / .cpp
|       |-- test_service.h / .cpp
|       |-- exercise_service.h / .cpp
|       |-- game_service.h / .cpp
|       +-- chat_service.h / .cpp
|
+-- doc/                        # Documentation
    |-- ARCHITECTURE.md         # Detailed architecture documentation
    |-- PROTOCOL.md             # Protocol specification
    +-- DEVELOPER_GUIDE.md      # Build and development guide
```

---

## Technology Stack

| Component | Technology |
|-----------|------------|
| Language | C++17 |
| Build System | GNU Make |
| GUI Framework | GTK+ 3.0 |
| Networking | POSIX Sockets (TCP) |
| Threading | POSIX Threads (pthread) |
| Data Format | JSON (custom lightweight parser) |
| Standard Library | C++ STL (containers, algorithms, chrono) |

### Platform Support

| Platform | Status |
|----------|--------|
| Linux | Fully supported |
| WSL/WSL2 | Fully supported |
| macOS | Supported (may require minor adjustments) |
| Windows (native) | Not supported (use WSL) |

---

## Build and Run Instructions

### Prerequisites

```bash
# Ubuntu/Debian
sudo apt-get update
sudo apt-get install build-essential libgtk-3-dev pkg-config

# Fedora
sudo dnf install gcc-c++ gtk3-devel pkgconfig

# Arch Linux
sudo pacman -S base-devel gtk3 pkgconf
```

### Build

```bash
# Build all components
make all

# Build individual components
make server      # Server only
make client      # Console client only
make gui         # GUI client only (requires GTK+)

# Clean build artifacts
make clean
```

### Run

**Start the server:**

```bash
./server [port]
# Example: ./server 8888
```

**Start the console client:**

```bash
./client [server_ip] [port]
# Example: ./client 127.0.0.1 8888
```

**Start the GUI client:**

```bash
./gui_app
```

### Test Accounts

The server initializes with sample data for testing:

| Email | Password | Role |
|-------|----------|------|
| student@example.com | student123 | Student |
| sarah@example.com | teacher123 | Teacher |
| john@example.com | teacher123 | Teacher |

---

## Usage

### Typical User Flow

1. **Start the server** on the host machine
2. **Launch the client** (console or GUI)
3. **Login** with credentials
4. **Set your level** (Beginner, Intermediate, Advanced)
5. **Browse lessons** filtered by topic and level
6. **Take tests** to assess your knowledge
7. **Complete exercises** for teacher feedback
8. **Play games** to reinforce vocabulary
9. **Chat** with teachers for questions

### Console Client Navigation

```
+==========================================+
|     ENGLISH LEARNING APP - MAIN MENU     |
+==========================================+
|  1. Set English Level                    |
|  2. View All Lessons                     |
|  3. Take a Test                          |
|  4. Do Exercises                         |
|  5. Play Games                           |
|  6. Chat with Others                     |
|  7. Logout                               |
|  0. Exit                                 |
+==========================================+
```

### Real-Time Chat

The chat system supports:
- Viewing online/offline status of contacts
- Sending messages with delivery confirmation
- Receiving push notifications for new messages
- Chat history persistence during session

---

## Development Notes

### Coding Principles

- **Single Responsibility**: Each class has one reason to change
- **Open/Closed**: Open for extension, closed for modification
- **Interface Segregation**: Small, focused interfaces
- **Dependency Inversion**: Depend on abstractions, not concretions
- **DRY**: Common utilities extracted to shared headers

### Adding a New Feature

1. **Define the domain model** in `include/core/`
2. **Create the repository interface** in `include/repository/`
3. **Implement the repository** in `src/repository/memory/`
4. **Create the service interface** in `include/service/`
5. **Implement the service** in `src/service/`
6. **Add protocol messages** in `include/protocol/message_types.h`
7. **Add request handlers** in `server.cpp`
8. **Update clients** to use the new feature

### Adding a New Protocol Message

1. Add message type constants to `include/protocol/message_types.h`:

```cpp
constexpr const char* MY_FEATURE_REQUEST = "MY_FEATURE_REQUEST";
constexpr const char* MY_FEATURE_RESPONSE = "MY_FEATURE_RESPONSE";
```

2. Add handler in `server.cpp`:

```cpp
else if (messageType == MessageType::MY_FEATURE_REQUEST) {
    response = handleMyFeatureRequest(payload, sessionToken);
}
```

3. Implement handler function following existing patterns.

4. Update client to send request and handle response.

### Namespace Organization

```cpp
english_learning::core::        // Domain models
english_learning::protocol::    // Protocol utilities
english_learning::repository::  // Data access
english_learning::service::     // Business logic
```

---

## Security Notes

### Password Handling

- Passwords are currently stored in plaintext in memory
- Production deployment should implement password hashing (e.g., bcrypt, Argon2)
- Passwords are transmitted in JSON payloads (use TLS in production)

### Session Management

- 64-character alphanumeric session tokens
- Tokens expire after 1 hour of inactivity
- Tokens are validated on every authenticated request
- Socket-to-session mapping for push notifications

### Known Limitations

- No TLS/SSL encryption (plaintext TCP)
- No rate limiting on authentication attempts
- No input sanitization for SQL injection (N/A with in-memory storage)
- Session tokens stored in memory only

### Recommendations for Production

- Implement TLS encryption for all communications
- Add password hashing before storage
- Implement rate limiting and account lockout
- Add request validation and sanitization
- Use persistent storage with proper access controls

---

## Roadmap

### Planned Improvements

- [ ] **Persistence Layer**: SQLite or PostgreSQL backend
- [ ] **Unit Tests**: GoogleTest framework integration
- [ ] **TLS Support**: Encrypted client-server communication
- [ ] **REST API**: HTTP endpoints for web/mobile clients
- [ ] **Password Hashing**: bcrypt implementation
- [ ] **Logging Framework**: Structured logging with levels
- [ ] **Configuration File**: External configuration support
- [ ] **Docker Support**: Containerized deployment

### Feature Enhancements

- [ ] Audio playback for listening exercises
- [ ] Progress tracking and analytics
- [ ] Leaderboards for games
- [ ] Spaced repetition for vocabulary
- [ ] Teacher dashboard for student management
- [ ] Push notifications for mobile clients

---

## Contributing

Contributions are welcome. Please follow these guidelines:

### Getting Started

1. Fork the repository
2. Create a feature branch: `git checkout -b feature/your-feature`
3. Make your changes
4. Test thoroughly
5. Submit a pull request

### Coding Standards

- Follow existing code style and formatting
- Use meaningful variable and function names
- Add comments for complex logic
- Keep functions focused and small
- Update documentation for new features

### Commit Messages

```
type: short description

Longer description if needed.

- Bullet points for multiple changes
- Reference issues: Fixes #123
```

Types: `feat`, `fix`, `docs`, `refactor`, `test`, `chore`

### Pull Request Process

1. Ensure the build passes: `make clean && make all`
2. Update relevant documentation
3. Describe changes in the PR description
4. Request review from maintainers

---

## License

This project is licensed under the MIT License.

```
MIT License

Copyright (c) 2024 English Learning Project Contributors

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
SOFTWARE.
```

---

## Acknowledgments

- GTK+ development team for the GUI framework
- Contributors to the C++ standard library
- Open source community for inspiration and best practices
