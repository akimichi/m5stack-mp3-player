// Settings.h
//
// 電源を切っても残したい設定を ESP32 の NVS (Preferences) に読み書きする。
// 呼び出しは UI (loop タスク) からのみ行い、デコードタスクからは触らない。
#pragma once

#include <Arduino.h>

class Settings {
 public:
  /// 保存済みの再生速度 (%) を返す。未保存なら fallback を返す。
  uint8_t loadSpeedPercent(uint8_t fallback);
  /// 再生速度 (%) を保存する。同じ値なら書き込まない。
  void saveSpeedPercent(uint8_t percent);
};
