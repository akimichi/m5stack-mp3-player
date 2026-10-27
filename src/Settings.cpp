#include "Settings.h"

#include <M5Unified.h>
#include <Preferences.h>

namespace {
constexpr const char* kNamespace = "mp3player";
constexpr const char* kKeySpeed = "speed";
}  // namespace

uint8_t Settings::loadSpeedPercent(uint8_t fallback) {
  Preferences prefs;
  // 一度も保存していないと名前空間がなく、読み取り専用だとエラーログを出して失敗する。
  // 読み書きで開けば名前空間が作られるだけで、キーがなければ fallback が返る。
  if (!prefs.begin(kNamespace, /*readOnly=*/false)) {
    M5_LOGE("nvs open failed");
    return fallback;
  }
  uint8_t v = prefs.getUChar(kKeySpeed, fallback);
  prefs.end();
  return v;
}

void Settings::saveSpeedPercent(uint8_t percent) {
  Preferences prefs;
  if (!prefs.begin(kNamespace, /*readOnly=*/false)) {
    M5_LOGE("nvs open failed");
    return;
  }
  if (prefs.getUChar(kKeySpeed, 0) != percent) {
    prefs.putUChar(kKeySpeed, percent);
  }
  prefs.end();
}
