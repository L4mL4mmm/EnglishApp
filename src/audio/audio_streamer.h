#ifndef AUDIO_STREAMER_H
#define AUDIO_STREAMER_H

#include <atomic>
#include <netinet/in.h>
#include <string>
#include <thread>


class AudioStreamer {
public:
  AudioStreamer();
  ~AudioStreamer();

  // Initialize UDP socket and bind to a port
  // Returns the bound port number, or -1 on failure
  int init(int port = 0);

  // Start streaming (Capture -> Send) and Playback (Receive -> Play)
  void startStreaming(const std::string &targetIp, int targetPort);

  // Stop all streaming/playback threads
  void stop();

  // Check if currently active
  bool isActive() const { return running; }
  int getLocalPort() const { return localPort; }

private:
  void captureLoop();
  void playbackLoop();
  void cleanup();

  int sockfd;
  int localPort;
  struct sockaddr_in targetAddr;

  std::atomic<bool> running;
  std::thread captureThread;
  std::thread playbackThread;

  static const int BUFFER_SIZE = 1024; // Chunk size for audio packets
};

#endif // AUDIO_STREAMER_H
