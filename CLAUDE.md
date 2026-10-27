# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## 概要

M5Stack Core2 (V1.1) 用の MP3 プレーヤー。SD カードの `*.mp3` を一覧表示し、タッチで選んだ曲を内蔵スピーカーで再生する。
PlatformIO + Arduino フレームワーク (espressif32 6.x)。コメント・ドキュメント・コミットメッセージはすべて日本語で書く。

## コマンド

```sh
pio run                                        # ビルド (初回はライブラリ取得で数分かかる)
pio run -t upload --upload-port /dev/ttyACM0   # 書き込み。Core2 は USB Single Serial (VID 1A86) として見える
pio device list                                # 接続ポートの確認
pio device monitor -b 115200                   # シリアルログ。対話端末が必要で、Claude Code の Bash からは動かない
```

Claude Code からシリアルログを読むときは `~/.platformio/penv/bin/python` の pyserial で `/dev/ttyACM0` を直接開く
(RTS を一瞬 True にするとリセットがかかり起動ログから拾える)。

テスト・lint の仕組みはない。動作確認は実機で行う。

## 依存関係の固定理由

- `ESP8266Audio@1.9.9` に固定。2.x 系は Arduino core 3.x 向けで、espressif32 6.x (core 2.x) ではビルドできない。
- `M5Unified` に電源 IC (AXP2101) とスピーカーアンプの制御を任せている。Core2 V1.0/V1.1 は自動判別される。
- `board_build.partitions = default_16MB.csv`、PSRAM 有効。

## アーキテクチャ

### 2 タスク構成とタスク間の約束

| タスク | コア / 優先度 | 役割 |
| --- | --- | --- |
| `loop` (Arduino) | core1 / 1 | `M5.update()` → `Ui::update()`。タッチ・ボタン処理と差分描画。NVS の読み書きもここだけ |
| `mp3decode` (`Player::taskLoop`) | core1 / 2 | ESP8266Audio で MP3 をデコードし `AudioOutputM5Speaker` へ流す |

- UI → Player への操作は `Player` のコマンドキュー (`play/next/prev/togglePause/stop`) 経由。再生速度だけは atomic への直接書き込みで、
  デコードタスクが次のバッファから読む (キューを通さない)。
- Player → UI への状態は `std::atomic` で公開する。`generation()` は曲番号や再生状態が変わるたびに増え、UI はこれで再描画要否を判定する。
  新しい状態を足すときは `Ui::update()` の `_drawnXxx` 比較に加えること。
- LCD と SD は同じ SPI バス (VSPI) を共有するが、M5GFX と Arduino `SPI` が同じミューテックスで排他するので、
  別タスクから同時に触っても安全。ただし描画中は SD が読めないので、`loop()` 末尾の `M5.delay(8)` でデコードに時間を譲っている。

### 音声出力 (`AudioOutputM5Speaker.h`)

- ESP8266Audio の `AudioOutput` 実装。PCM バッファ 3 本 (各 2048 ステレオフレーム) をローテーションし、
  `M5.Speaker.playRaw()` に渡す。スピーカータスクからの解放コールバック (`setBufferReleaseCallback`) で `_busy` を落とし、
  解放済みバッファだけ再利用する。`setup()` は最初の再生より前に一度だけ呼ぶ必要がある (コールバック登録はそれ以降できない)。
- `frames()` はスピーカーに渡したフレーム数で、`Player::elapsedSec()` はこれを元のサンプルレートで割る。
- **再生速度** は `playRaw()` に申告するサンプルレートに `_speedPercent` を掛けて実現する (音程も変わる)。
  `frames()` は倍率をかけないので、経過時間は実時間ではなく曲内の位置を指す。

### UI (`Ui.*`)

- 320x240 を固定レイアウトで分割している (ヘッダ 0..47 / 進捗バー 48..53 / 一覧 54..203 / ボタン 204..239)。
  各領域は `Ui.h` の `k*` 定数で定義され、タッチ判定と描画の両方がこれを参照する。
- ヘッダ右側 (`kSideX` = 232 以降) は音量バーと `VOL` ラベル。再生速度はその左隣に右揃えで描き、曲名は速度表示の実測幅の手前で切る。
- ヘッダタップで速度を巡回 (`kSpeedPercents` = 100/95/90/85/80)。巡回表はここが唯一の定義で、`Settings` から読んだ値もこの表で検証する。
- 全画面再描画は `_forceRedraw`、部分再描画は前回描画値との比較で行う。`showMessage()` の後は必ず全体を描き直す。

### 設定の永続化 (`Settings.*`)

- Arduino `Preferences` (NVS) に名前空間 `mp3player` で保存する。読み込みも読み書きモードで開く
  (読み取り専用だと初回起動時に名前空間がなくてエラーログが出る)。
- デコードタスクからは触らない。

## 制限事項 (README より)

対応形式は MP3 のみ。ID3 タグは読み飛ばし、表示はファイル名ベース。曲の総時間は出さず、進捗はファイル位置ベース。
SD の活線挿抜は非対応。ファイル数上限 1000、ディレクトリ走査は 3 階層まで。
