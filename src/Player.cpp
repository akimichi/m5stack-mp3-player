#include "Player.h"

#include <M5Unified.h>

bool Player::begin(Playlist* playlist) {
  _playlist = playlist;
  _out.setup();
  _queue = xQueueCreate(8, sizeof(Command));
  if (_queue == nullptr) return false;
  _file = new AudioFileSourceFS(*playlist->fs());
  // 描画 (loop, 優先度1) より高い優先度で、同じコア1に置く。
  // SD と LCD は同じ SPI バスだが、Arduino SPI のミューテックスで排他される。
  return pdPASS == xTaskCreatePinnedToCore(taskEntry, "mp3decode", 8192, this, 2, nullptr, 1);
}

void Player::play(size_t index) {
  Command c{Command::Play, static_cast<int>(index)};
  xQueueSend(_queue, &c, 0);
}
void Player::togglePause() {
  Command c{Command::TogglePause, 0};
  xQueueSend(_queue, &c, 0);
}
void Player::next() {
  Command c{Command::Next, 0};
  xQueueSend(_queue, &c, 0);
}
void Player::prev() {
  Command c{Command::Prev, 0};
  xQueueSend(_queue, &c, 0);
}
void Player::stop() {
  Command c{Command::Stop, 0};
  xQueueSend(_queue, &c, 0);
}

uint32_t Player::elapsedSec() const {
  uint32_t rate = _out.rate();
  return rate ? _out.frames() / rate : 0;
}

uint8_t Player::progressPercent() const {
  uint32_t size = _size.load();
  if (size == 0) return 0;
  uint64_t pos = _pos.load();
  if (pos > size) pos = size;
  return static_cast<uint8_t>(pos * 100 / size);
}

void Player::taskEntry(void* arg) { static_cast<Player*>(arg)->taskLoop(); }

void Player::taskLoop() {
  Command cmd;
  for (;;) {
    bool running = _mp3.isRunning() && state() == PlayerState::Playing;
    // 再生中は待たずにコマンドを覗き、それ以外は来るまでブロックする
    if (xQueueReceive(_queue, &cmd, running ? 0 : pdMS_TO_TICKS(50)) == pdTRUE) {
      handle(cmd);
      continue;
    }
    if (!running) continue;
    if (!_mp3.loop()) {
      // 曲の終わり (またはデコードエラー) → 次の曲へ
      playRelative(1);
    } else if (_id3 != nullptr) {
      _pos = _id3->getPos();
    }
  }
}

void Player::handle(const Command& cmd) {
  switch (cmd.type) {
    case Command::Play:
      if (cmd.index >= 0 && static_cast<size_t>(cmd.index) < _playlist->size()) {
        if (!startFile(cmd.index)) playRelative(1);
      }
      break;
    case Command::TogglePause:
      if (state() == PlayerState::Playing) {
        setState(PlayerState::Paused);
      } else if (state() == PlayerState::Paused) {
        setState(PlayerState::Playing);
      } else if (_playlist->size() > 0) {
        // 停止中なら、選択中 (前回) の曲か先頭から再生
        int idx = _index.load();
        startFile(idx >= 0 ? idx : 0);
      }
      break;
    case Command::Next:
      playRelative(1);
      break;
    case Command::Prev:
      playRelative(-1);
      break;
    case Command::Stop:
      stopFile();
      setState(PlayerState::Stopped);
      break;
  }
}

void Player::setState(PlayerState s) {
  _state = static_cast<uint8_t>(s);
  ++_generation;
}

bool Player::startFile(size_t index) {
  stopFile();
  const String& path = _playlist->path(index);
  M5_LOGI("play: %s", path.c_str());
  _index = static_cast<int>(index);
  _out.resetFrames();
  _pos = 0;
  _size = 0;
  if (!_file->open(path.c_str())) {
    M5_LOGE("open failed: %s", path.c_str());
    setState(PlayerState::Stopped);
    return false;
  }
  _id3 = new AudioFileSourceID3(_file);
  if (!_mp3.begin(_id3, &_out)) {
    M5_LOGE("mp3 begin failed: %s", path.c_str());
    stopFile();
    setState(PlayerState::Stopped);
    return false;
  }
  _size = _id3->getSize();
  setState(PlayerState::Playing);
  return true;
}

void Player::stopFile() {
  if (_mp3.isRunning()) {
    _mp3.stop();
    _out.stop();
  }
  if (_id3 != nullptr) {
    _id3->close();
    delete _id3;
    _id3 = nullptr;
  }
  if (_file->isOpen()) _file->close();
}

void Player::playRelative(int step) {
  int n = static_cast<int>(_playlist->size());
  if (n == 0) return;
  int base = _index.load();
  if (base < 0) base = (step > 0) ? -1 : 0;
  // 開けないファイルは前方向にスキップし、全曲試してもだめなら停止する
  for (int tries = 0; tries < n; ++tries) {
    base = ((base + step) % n + n) % n;
    if (startFile(base)) return;
    step = 1;
  }
  stopFile();
  setState(PlayerState::Stopped);
}
