#include "Ui.h"

namespace {
constexpr uint16_t kColorBg = 0x0841;         // ほぼ黒
constexpr uint16_t kColorHeader = 0x18E3;     // 濃いグレー
constexpr uint16_t kColorRowOdd = 0x1082;
constexpr uint16_t kColorRowEven = 0x0841;
constexpr uint16_t kColorCurrent = 0x0B5C;    // 青系
constexpr uint16_t kColorText = 0xFFFF;
constexpr uint16_t kColorTextDim = 0x9CF3;
constexpr uint16_t kColorAccent = 0x07FF;     // シアン
constexpr uint16_t kColorBar = 0x2124;
constexpr uint16_t kColorBarPressed = 0x4A69;
constexpr uint16_t kColorIcon = 0xFFFF;
}  // namespace

void Ui::begin(Playlist* playlist, Player* player) {
  _playlist = playlist;
  _player = player;
  auto& d = M5.Display;
  d.setRotation(1);
  d.setFont(&fonts::lgfxJapanGothic_16);
  d.setTextWrap(false);
  d.fillScreen(kColorBg);
  _forceRedraw = true;
}

void Ui::showMessage(const char* line1, const char* line2) {
  auto& d = M5.Display;
  d.startWrite();
  d.fillScreen(kColorBg);
  d.setTextColor(kColorText, kColorBg);
  d.setTextDatum(textdatum_t::middle_center);
  d.drawString(line1, kWidth / 2, kHeight / 2 - (line2 ? 12 : 0));
  if (line2) {
    d.setTextColor(kColorTextDim, kColorBg);
    d.drawString(line2, kWidth / 2, kHeight / 2 + 12);
  }
  d.endWrite();
  _forceRedraw = true;  // 次に update() されたら全体を描き直す
}

// ---------------------------------------------------------------- input

void Ui::update() {
  handleTouch();
  handleHardButtons();

  int index = _player->currentIndex();
  if (index != _drawnIndex && !_dragging) ensureVisible(index);

  uint32_t gen = _player->generation();
  PlayerState state = _player->state();
  uint32_t elapsed = _player->elapsedSec();
  uint8_t progress = _player->progressPercent();
  uint8_t volume = M5.Speaker.getVolume();

  bool stateChanged = _forceRedraw || gen != _drawnGeneration;
  if (stateChanged || elapsed != _drawnElapsed || volume != _drawnVolume) drawHeader();
  if (stateChanged || progress != _drawnProgress) drawProgress();
  if (stateChanged || _scroll != _drawnScroll) drawList();
  if (stateChanged || _touchButton != _drawnTouchButton || state != _drawnState) drawBar();

  _forceRedraw = false;
  _drawnGeneration = gen;
  _drawnIndex = index;
  _drawnScroll = _scroll;
  _drawnElapsed = elapsed;
  _drawnProgress = progress;
  _drawnVolume = volume;
  _drawnTouchButton = _touchButton;
  _drawnState = state;
}

void Ui::handleTouch() {
  const auto& t = M5.Touch.getDetail();
  const bool inList = t.base_y >= kListY && t.base_y < kListY + kRows * kRowH;
  const bool inBar = t.base_y >= kBarY && t.base_y < kBarY + kBarH;

  if (t.wasPressed()) {
    if (inList) {
      _dragging = true;
      _dragStartScroll = _scroll;
    } else if (inBar) {
      int b = t.base_x / kButtonW;
      _touchButton = static_cast<Button>(b < 0 ? 0 : (b >= kButtons ? kButtons - 1 : b));
    }
  }

  if (_dragging && t.isPressed()) {
    _scroll = clampScroll(_dragStartScroll - t.distanceY() / kRowH);
  }

  if (t.wasClicked()) {
    if (inList && _dragging) {
      int row = (t.base_y - kListY) / kRowH;
      size_t idx = static_cast<size_t>(_scroll + row);
      if (idx < _playlist->size()) {
        M5.Speaker.tone(2000, 30);
        _player->play(idx);
      }
    } else if (inBar && _touchButton != BtnNone) {
      pressButton(_touchButton);
    }
  }

  if (t.wasReleased()) {
    _dragging = false;
    _touchButton = BtnNone;
  }
}

void Ui::handleHardButtons() {
  if (M5.BtnA.wasClicked()) pressButton(BtnPrev);
  if (M5.BtnB.wasClicked()) pressButton(BtnPlay);
  if (M5.BtnC.wasClicked()) pressButton(BtnNext);
}

void Ui::pressButton(Button b) {
  switch (b) {
    case BtnVolDown: changeVolume(-kVolumeStep); break;
    case BtnVolUp: changeVolume(+kVolumeStep); break;
    case BtnPrev: _player->prev(); break;
    case BtnPlay: _player->togglePause(); break;
    case BtnNext: _player->next(); break;
    default: break;
  }
}

void Ui::changeVolume(int delta) {
  int v = static_cast<int>(M5.Speaker.getVolume()) + delta;
  if (v < 0) v = 0;
  if (v > 255) v = 255;
  M5.Speaker.setVolume(static_cast<uint8_t>(v));
  M5.Speaker.tone(1000, 30);
}

int Ui::clampScroll(int scroll) const {
  int maxScroll = static_cast<int>(_playlist->size()) - kRows;
  if (maxScroll < 0) maxScroll = 0;
  if (scroll > maxScroll) scroll = maxScroll;
  if (scroll < 0) scroll = 0;
  return scroll;
}

void Ui::ensureVisible(int index) {
  if (index < 0) return;
  if (index < _scroll) {
    _scroll = index;
  } else if (index >= _scroll + kRows) {
    _scroll = index - kRows + 1;
  }
  _scroll = clampScroll(_scroll);
}

// ---------------------------------------------------------------- drawing

String Ui::formatTime(uint32_t sec) {
  char buf[16];
  if (sec >= 3600) {
    snprintf(buf, sizeof(buf), "%u:%02u:%02u", (unsigned)(sec / 3600), (unsigned)(sec / 60 % 60), (unsigned)(sec % 60));
  } else {
    snprintf(buf, sizeof(buf), "%02u:%02u", (unsigned)(sec / 60), (unsigned)(sec % 60));
  }
  return String(buf);
}

void Ui::drawHeader() {
  auto& d = M5.Display;
  int index = _player->currentIndex();
  PlayerState state = _player->state();

  d.startWrite();
  d.fillRect(0, 0, kWidth, kHeaderH, kColorHeader);
  d.setTextDatum(textdatum_t::top_left);

  // 1行目: 曲名
  String title = (index >= 0) ? _playlist->displayName(index) : String("MP3 Player");
  d.setTextColor(kColorText, kColorHeader);
  d.setClipRect(0, 0, kWidth - 4, 24);
  d.drawString(title, 6, 5);
  d.clearClipRect();

  // 2行目: 状態 / 経過時間 / 曲番号
  const char* stateText = (state == PlayerState::Playing) ? "PLAY" : (state == PlayerState::Paused) ? "PAUSE" : "STOP";
  String line2 = String(stateText) + "  " + formatTime(_player->elapsedSec());
  if (index >= 0) line2 += "  " + String(index + 1) + "/" + String(_playlist->size());
  d.setTextColor(kColorTextDim, kColorHeader);
  d.drawString(line2, 6, 27);

  // 右側: 音量バー
  constexpr int vx = 232, vy = 31, vw = 80, vh = 8;
  int fill = M5.Speaker.getVolume() * vw / 255;
  d.drawRect(vx, vy, vw, vh, kColorTextDim);
  d.fillRect(vx + 1, vy + 1, fill > 2 ? fill - 2 : 0, vh - 2, kColorAccent);
  d.setTextDatum(textdatum_t::bottom_right);
  d.drawString("VOL", vx + vw, vy - 2);
  d.setTextDatum(textdatum_t::top_left);
  d.endWrite();
}

void Ui::drawProgress() {
  auto& d = M5.Display;
  int fill = _player->progressPercent() * kWidth / 100;
  d.startWrite();
  d.fillRect(0, kProgressY, fill, kProgressH, kColorAccent);
  d.fillRect(fill, kProgressY, kWidth - fill, kProgressH, kColorBar);
  d.endWrite();
}

void Ui::drawList() {
  auto& d = M5.Display;
  d.startWrite();
  if (_playlist->empty()) {
    d.fillRect(0, kListY, kWidth, kRows * kRowH, kColorBg);
    d.setTextColor(kColorTextDim, kColorBg);
    d.setTextDatum(textdatum_t::middle_center);
    d.drawString("MP3 ファイルがありません", kWidth / 2, kListY + kRows * kRowH / 2);
    d.setTextDatum(textdatum_t::top_left);
  } else {
    for (int row = 0; row < kRows; ++row) drawRow(row);
  }
  d.endWrite();
}

void Ui::drawRow(int row) {
  auto& d = M5.Display;
  int y = kListY + row * kRowH;
  int index = _scroll + row;
  bool valid = static_cast<size_t>(index) < _playlist->size();
  bool current = valid && index == _player->currentIndex();
  uint16_t bg = current ? kColorCurrent : ((index & 1) ? kColorRowOdd : kColorRowEven);

  d.fillRect(0, y, kWidth, kRowH, bg);
  if (!valid) return;

  d.setTextColor(current ? kColorText : kColorTextDim, bg);
  d.setTextDatum(textdatum_t::middle_right);
  d.drawString(String(index + 1), 34, y + kRowH / 2);

  d.setTextColor(kColorText, bg);
  d.setTextDatum(textdatum_t::middle_left);
  d.setClipRect(0, y, kWidth - 4, kRowH);
  d.drawString(_playlist->displayName(index), 44, y + kRowH / 2);
  d.clearClipRect();
  d.setTextDatum(textdatum_t::top_left);
}

void Ui::drawBar() {
  auto& d = M5.Display;
  d.startWrite();
  for (int i = 0; i < kButtons; ++i) drawButton(i);
  d.endWrite();
}

void Ui::drawButton(int i) {
  auto& d = M5.Display;
  int x = i * kButtonW;
  bool pressed = (_touchButton == i);
  uint16_t bg = pressed ? kColorBarPressed : kColorBar;
  d.fillRect(x, kBarY, kButtonW, kBarH, bg);
  d.drawFastVLine(x, kBarY, kBarH, kColorBg);

  int cx = x + kButtonW / 2;
  int cy = kBarY + kBarH / 2;
  constexpr int s = 8;  // アイコンの半径相当
  switch (i) {
    case BtnVolDown:
      d.fillRect(cx - s, cy - 2, 2 * s, 4, kColorIcon);
      break;
    case BtnVolUp:
      d.fillRect(cx - s, cy - 2, 2 * s, 4, kColorIcon);
      d.fillRect(cx - 2, cy - s, 4, 2 * s, kColorIcon);
      break;
    case BtnPrev:
      d.fillRect(cx - s - 2, cy - s, 3, 2 * s, kColorIcon);
      d.fillTriangle(cx + s, cy - s, cx + s, cy + s, cx - s + 2, cy, kColorIcon);
      break;
    case BtnNext:
      d.fillRect(cx + s - 1, cy - s, 3, 2 * s, kColorIcon);
      d.fillTriangle(cx - s, cy - s, cx - s, cy + s, cx + s - 2, cy, kColorIcon);
      break;
    case BtnPlay:
      if (_player->state() == PlayerState::Playing) {
        d.fillRect(cx - s, cy - s, 6, 2 * s, kColorIcon);
        d.fillRect(cx + s - 6, cy - s, 6, 2 * s, kColorIcon);
      } else {
        d.fillTriangle(cx - s + 2, cy - s, cx - s + 2, cy + s, cx + s, cy, kColorIcon);
      }
      break;
  }
}
