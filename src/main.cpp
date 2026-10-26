// M5Stack Core2 V1.1 MP3 Player
//
// SD カード内の *.mp3 を一覧表示し、タッチで選んだ曲を内蔵スピーカーで再生する。
#include <M5Unified.h>

#include "Player.h"
#include "Playlist.h"
#include "Ui.h"

static Playlist playlist;
static Player player;
static Ui ui;

void setup() {
  auto cfg = M5.config();
  cfg.internal_spk = true;
  M5.begin(cfg);
  M5.Log.setLogLevel(m5::log_target_serial, ESP_LOG_INFO);

  {
    auto spk = M5.Speaker.config();
    // サンプルレートを上げると CPU 負荷は増えるが音質が改善する (M5Unified サンプルと同じ値)
    spk.sample_rate = 96000;
    M5.Speaker.config(spk);
  }
  M5.Speaker.begin();
  M5.Speaker.setVolume(128);

  ui.begin(&playlist, &player);

  ui.showMessage("SD カードを確認中...");
  while (!playlist.mount()) {
    ui.showMessage("SD カードが見つかりません", "カードを挿入してください");
    M5.delay(1000);
  }
  playlist.scan("/");

  if (!player.begin(&playlist)) {
    ui.showMessage("デコードタスクを起動できません");
    for (;;) M5.delay(1000);
  }
  M5.Display.fillScreen(TFT_BLACK);
}

void loop() {
  M5.update();
  ui.update();
  M5.delay(8);  // 描画タスクが SPI を独占しないよう、デコードタスクに時間を譲る
}
