// AudioOutputM5Speaker.h
//
// ESP8266Audio の AudioOutput 実装。デコード済み PCM を M5.Speaker.playRaw() に流す。
// バッファは3本をローテーションで使い、スピーカータスクが解放を通知
// (setBufferReleaseCallback) したバッファだけを再利用する。
//
// M5Unified の examples/Advanced/MP3_with_ESP8266Audio/AudioOutputM5Speaker.h (MIT) を
// 元に、バッファサイズの拡大と再生フレーム数のリセットを加えたもの。
#pragma once

#include <AudioOutput.h>
#include <M5Unified.h>
#include <atomic>

class AudioOutputM5Speaker : public AudioOutput {
 public:
  AudioOutputM5Speaker(m5::Speaker_Class* speaker, uint8_t virtualChannel = 0)
      : _speaker(speaker), _virtualCh(virtualChannel) {}
  virtual ~AudioOutputM5Speaker() {}

  /// M5.begin() の後、最初に音を出す前に一度だけ呼ぶ。
  /// (解放コールバックは最初の再生要求より前にしか登録できない)
  void setup() { _speaker->setBufferReleaseCallback(this, bufferReleased); }

  bool begin() override { return true; }

  bool ConsumeSample(int16_t sample[2]) override {
    if (_fill < kBufSize) {
      _buffer[_index][_fill] = sample[0];
      _buffer[_index][_fill + 1] = sample[1];
      _fill += 2;
      return true;
    }
    flush();
    return false;
  }

  void flush() override {
    if (_fill == 0) return;
    // playRaw より前に busy を立てる: 解放通知はキュー投入後にしか来ない。
    _busy[_index] = true;
    // 再生速度: スピーカーに申告するサンプルレートを倍率分ずらす (音程も一緒に変わる)
    uint32_t rate = static_cast<uint32_t>(static_cast<uint64_t>(hertz) * _speedPercent.load() / 100);
    bool queued = _speaker->playRaw(_buffer[_index], _fill, rate, true, 1, _virtualCh);
    if (queued) {
      _frames += _fill / 2;
    } else {
      _busy[_index] = false;
    }
    if (++_index >= kBufCount) _index = 0;
    _fill = 0;
    // 次のバッファがスピーカータスクから解放されるまで待つ
    while (_busy[_index]) vTaskDelay(1);
  }

  bool stop() override {
    // キュー済みのバッファは切らずに鳴らし切る。全解放後はどこからも参照されない。
    flush();
    for (size_t i = 0; i < kBufCount; ++i) {
      while (_busy[i]) vTaskDelay(1);
    }
    return true;
  }

  /// スピーカーが受け取ったステレオフレーム数 (= 実際に再生された量)
  uint32_t frames() const { return _frames.load(); }
  /// デコーダが SetRate() で設定したサンプルレート。最初のフレームまでは 0。
  uint32_t rate() const { return hertz; }
  /// 曲の切り替え時に呼び、再生フレーム数を 0 に戻す。
  void resetFrames() { _frames = 0; }

  /// 再生速度 (%)。100 で等速。次に flush() されるバッファから反映される。
  /// frames() はデコーダが出したフレーム数のままなので、経過時間は曲内の位置を指し続ける。
  void setSpeedPercent(uint8_t percent) { _speedPercent = percent; }
  uint8_t speedPercent() const { return _speedPercent.load(); }

 private:
  static constexpr size_t kBufCount = 3;
  // int16 単位 (ステレオ 2048 フレーム ≒ 44.1kHz で 46ms)。
  // UI 側が SPI を占有して描画している間も音が途切れないよう、サンプルより大きめ。
  static constexpr size_t kBufSize = 4096;

  static void bufferReleased(void* arg, const void* data, uint8_t) {
    auto self = static_cast<AudioOutputM5Speaker*>(arg);
    for (size_t i = 0; i < kBufCount; ++i) {
      if (data == self->_buffer[i]) self->_busy[i] = false;
    }
  }

  m5::Speaker_Class* _speaker;
  uint8_t _virtualCh;
  int16_t _buffer[kBufCount][kBufSize];
  // タスク間で共有するフラグは std::atomic にする (plain bool / volatile では不十分)
  std::atomic<bool> _busy[kBufCount] = {{false}, {false}, {false}};
  size_t _fill = 0;
  size_t _index = 0;
  std::atomic<uint32_t> _frames{0};
  std::atomic<uint8_t> _speedPercent{100};
};
