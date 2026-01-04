#include "audio_streamer.h"
#include <arpa/inet.h>
#include <cstdio>
#include <cstring>
#include <iostream>
#include <sys/socket.h>
#include <unistd.h>


AudioStreamer::AudioStreamer() : sockfd(-1), localPort(0), running(false) {}

AudioStreamer::~AudioStreamer() { stop(); }

int AudioStreamer::init(int port) {
  // Create UDP socket
  sockfd = socket(AF_INET, SOCK_DGRAM, 0);
  if (sockfd < 0) {
    perror("Socket creation failed");
    return -1;
  }

  // Bind to local port
  struct sockaddr_in addr;
  memset(&addr, 0, sizeof(addr));
  addr.sin_family = AF_INET;
  addr.sin_addr.s_addr = INADDR_ANY;
  addr.sin_port = htons(port);

  if (bind(sockfd, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
    perror("Bind failed");
    close(sockfd);
    sockfd = -1;
    return -1;
  }

  // Get assigned port if 0 was passed
  socklen_t len = sizeof(addr);
  if (getsockname(sockfd, (struct sockaddr *)&addr, &len) == 0) {
    localPort = ntohs(addr.sin_port);
  }

  std::cout << "[Audio] UDP Socket bound to port " << localPort << std::endl;
  return localPort;
}

void AudioStreamer::startStreaming(const std::string &targetIp,
                                   int targetPort) {
  if (running)
    return;

  // Setup target address
  memset(&targetAddr, 0, sizeof(targetAddr));
  targetAddr.sin_family = AF_INET;
  targetAddr.sin_port = htons(targetPort);
  if (inet_pton(AF_INET, targetIp.c_str(), &targetAddr.sin_addr) <= 0) {
    std::cerr << "[Audio] Invalid address: " << targetIp << std::endl;
    return;
  }

  running = true;

  // Start threads
  captureThread = std::thread(&AudioStreamer::captureLoop, this);
  playbackThread = std::thread(&AudioStreamer::playbackLoop, this);

  std::cout << "[Audio] Streaming started to " << targetIp << ":" << targetPort
            << std::endl;
}

void AudioStreamer::stop() {
  if (!running)
    return;

  std::cout << "[Audio] Stopping streamer..." << std::endl;
  running = false;

  // Close socket to unblock recvfrom
  if (sockfd >= 0) {
    shutdown(sockfd, SHUT_RDWR);
    close(sockfd);
    sockfd = -1;
  }

  if (captureThread.joinable())
    captureThread.join();
  if (playbackThread.joinable())
    playbackThread.join();

  std::cout << "[Audio] Streamer stopped" << std::endl;
}

void AudioStreamer::captureLoop() {
  // Open pipe to 'parec' (PulseAudio Record)
  // --format=s16le: Signed 16-bit Little Endian
  // --rate=44100: Standard sampling rate
  // --channels=1: Mono (voice)
  // --latency-msec=20: Low latency buffer
  FILE *pipe = popen("parec --format=s16le --rate=16000 --channels=1 "
                     "--latency-msec=20 2>/dev/null",
                     "r");
  if (!pipe) {
    std::cerr << "[Audio] Failed to open parec pipe" << std::endl;
    return;
  }

  uint8_t buffer[BUFFER_SIZE];
  while (running && sockfd >= 0) {
    size_t n = fread(buffer, 1, BUFFER_SIZE, pipe);
    if (n > 0) {
      sendto(sockfd, buffer, n, 0, (struct sockaddr *)&targetAddr,
             sizeof(targetAddr));
    } else {
      // If read fails (eof or error), just sleep a bit to avoid spin
      usleep(1000);
    }
  }

  pclose(pipe);
}

void AudioStreamer::playbackLoop() {
  // Open pipe to 'pacat' (PulseAudio Playback)
  FILE *pipe = popen("pacat --format=s16le --rate=16000 --channels=1 "
                     "--latency-msec=20 2>/dev/null",
                     "w");
  if (!pipe) {
    std::cerr << "[Audio] Failed to open pacat pipe" << std::endl;
    return;
  }

  uint8_t buffer[BUFFER_SIZE];
  struct sockaddr_in sender;
  socklen_t len = sizeof(sender);

  while (running && sockfd >= 0) {
    ssize_t n = recvfrom(sockfd, buffer, BUFFER_SIZE, 0,
                         (struct sockaddr *)&sender, &len);
    if (n > 0) {
      fwrite(buffer, 1, n, pipe);
    }
  }

  pclose(pipe);
}
