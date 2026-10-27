// Ui.h
//
// 320x240 の LCD にファイル一覧と再生状態を描き、タッチと BtnA/B/C を処理する。
//
//  y   0..47   ヘッダ (曲名 / 状態・経過時間 / 音量 / 再生速度。タップで速度を巡回)
//  y  48..53   進捗バー
//  y  54..203  ファイル一覧 (25px x 6 行、タップで再生、上下ドラッグでスクロール)
//  y 204..239  操作ボタン [Vol-] [|<] [>/||] [>|] [Vol+]
//  画面下の BtnA / BtnB / BtnC は 前の曲 / 再生・一時停止 / 次の曲
#pragma once

#include <M5Unified.h>

#include "Player.h"
#include "Playlist.h"

class Ui {
 public:
  void begin(Playlist* playlist, Player* player);
  /// M5.update() の後に毎フレーム呼ぶ。入力処理と必要な部分の再描画を行う。
  void update();
  /// 一覧の代わりに全画面でメッセージを表示する (SD なし など)。
  void showMessage(const char* line1, const char* line2 = nullptr);

 private:
  static constexpr int kWidth = 320;
  static constexpr int kHeight = 240;
  static constexpr int kHeaderH = 48;
  static constexpr int kProgressY = 48;
  static constexpr int kProgressH = 6;
  static constexpr int kListY = 54;
  static constexpr int kRowH = 25;
  static constexpr int kRows = 6;
  static constexpr int kBarY = 204;
  static constexpr int kBarH = 36;
  static constexpr int kButtons = 5;
  static constexpr int kButtonW = kWidth / kButtons;
  static constexpr int kVolumeStep = 16;
  // ヘッダ右側 (x >= kSideX) に音量バーと速度表示を置き、曲名はその手前で切る
  static constexpr int kSideX = 232;

  enum Button : int8_t { BtnNone = -1, BtnVolDown = 0, BtnPrev, BtnPlay, BtnNext, BtnVolUp };

  void handleTouch();
  void handleHardButtons();
  void pressButton(Button b);
  void changeVolume(int delta);
  void cycleSpeed();
  int clampScroll(int scroll) const;
  void ensureVisible(int index);

  void drawHeader();
  void drawProgress();
  void drawList();
  void drawRow(int row);
  void drawBar();
  void drawButton(int i);
  static String formatTime(uint32_t sec);

  Playlist* _playlist = nullptr;
  Player* _player = nullptr;

  int _scroll = 0;             // 一覧の先頭行の曲番号
  bool _dragging = false;      // 一覧内でタッチが始まった
  bool _headerTouch = false;   // ヘッダ内でタッチが始まった
  int _dragStartScroll = 0;
  Button _touchButton = BtnNone;  // 押下中の画面下ボタン

  // 前回描画した内容。差分があるところだけ描き直す。
  bool _forceRedraw = true;
  uint32_t _drawnGeneration = 0;
  int _drawnIndex = -2;
  int _drawnScroll = -1;
  uint32_t _drawnElapsed = 0;
  uint8_t _drawnProgress = 0;
  uint8_t _drawnVolume = 0;
  uint8_t _drawnSpeed = 0;
  Button _drawnTouchButton = BtnNone;
  PlayerState _drawnState = PlayerState::Stopped;
};
