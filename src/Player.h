// Player.h
//
// MP3 デコードを専用タスクで行う再生エンジン。
// UI (loop) からは play/next/prev/togglePause/stop をコマンドキュー経由で指示し、
// 状態は atomic で読み出す。
#pragma once

#include <Arduino.h>
#include <AudioFileSourceFS.h>
#include <AudioFileSourceID3.h>
#include <AudioGeneratorMP3.h>
#include <freertos/FreeRTOS.h>
#include <freertos/queue.h>

#include <atomic>

#include "AudioOutputM5Speaker.h"
#include "Playlist.h"

enum class PlayerState : uint8_t { Stopped, Playing, Paused };

class Player {
 public:
  /// M5.begin() と M5.Speaker.begin() の後に呼ぶ。デコードタスクを起動する。
  bool begin(Playlist* playlist);

  void play(size_t index);
  void togglePause();
  void next();
  void prev();
  void stop();

  PlayerState state() const { return static_cast<PlayerState>(_state.load()); }
  /// 再生中 (または一時停止中) の曲番号。何もなければ -1。
  int currentIndex() const { return _index.load(); }
  /// 現在の曲の再生経過秒
  uint32_t elapsedSec() const;
  /// ファイル読み出し位置ベースの進捗 (0-100)
  uint8_t progressPercent() const;
  /// 状態 (曲番号 / 再生状態) が変わるたびに増える。UI の再描画判定用。
  uint32_t generation() const { return _generation.load(); }

  /// 再生速度 (%)。100 で等速。デコードタスクが次のバッファから反映するのでキューを通さない。
  void setSpeedPercent(uint8_t percent) { _out.setSpeedPercent(percent); }
  uint8_t speedPercent() const { return _out.speedPercent(); }

 private:
  struct Command {
    enum Type : uint8_t { Play, TogglePause, Next, Prev, Stop } type;
    int index;
  };

  static void taskEntry(void* arg);
  void taskLoop();
  void handle(const Command& cmd);
  bool startFile(size_t index);
  void stopFile();
  void playRelative(int step);
  void setState(PlayerState s);

  Playlist* _playlist = nullptr;
  QueueHandle_t _queue = nullptr;
  AudioFileSourceFS* _file = nullptr;
  AudioFileSourceID3* _id3 = nullptr;
  AudioGeneratorMP3 _mp3;
  AudioOutputM5Speaker _out{&M5.Speaker, 0};

  std::atomic<int> _index{-1};
  std::atomic<uint8_t> _state{static_cast<uint8_t>(PlayerState::Stopped)};
  std::atomic<uint32_t> _generation{0};
  std::atomic<uint32_t> _pos{0};
  std::atomic<uint32_t> _size{0};
};
