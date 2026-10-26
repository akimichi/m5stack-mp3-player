# M5Stack Core2 V1.1 MP3 Player

SD カードに入れた MP3 ファイルを一覧表示し、タッチで選んだ曲を内蔵スピーカーで再生する
M5Stack Core2 (V1.1) 用のプレーヤーである。PlatformIO + Arduino フレームワークで開発する。

## 必要なもの

- M5Stack Core2 V1.1 (V1.0 でも M5Unified が自動判別するので動くはず)
- microSD カード (FAT32)。ルート以下に `*.mp3` を置く (サブフォルダは 3 階層まで走査)
- PlatformIO Core (CLI) または VSCode の PlatformIO 拡張

## ビルドと書き込み

```sh
pio run                 # ビルドのみ
pio run -t upload       # USB 接続した Core2 に書き込み
pio device monitor      # シリアルログ (115200bps)
```

## 使い方

画面構成 (320x240):

| 領域 | 内容 |
| --- | --- |
| 上部 | 曲名、再生状態 (PLAY / PAUSE / STOP)、経過時間、曲番号、音量バー |
| 進捗バー | ファイル読み出し位置ベースの進捗 |
| 中央 | ファイル一覧 (6 行)。**タップで再生**、**上下ドラッグでスクロール** |
| 下部 | ボタン: `Vol-` / `前の曲` / `再生・一時停止` / `次の曲` / `Vol+` |

画面下の赤丸 (BtnA / BtnB / BtnC) は 前の曲 / 再生・一時停止 / 次の曲 に対応する。
曲が終わると自動的に次の曲へ進み、最後の曲の次は先頭に戻る。

## 構成

| ファイル | 役割 |
| --- | --- |
| `src/main.cpp` | 初期化 (M5Unified, スピーカー, SD) とメインループ |
| `src/Playlist.*` | SD のマウントと `*.mp3` の再帰走査・ソート |
| `src/Player.*` | デコード専用タスク。コマンドキューで UI から操作し、状態は atomic で公開 |
| `src/AudioOutputM5Speaker.h` | ESP8266Audio の出力を `M5.Speaker.playRaw()` に橋渡しする |
| `src/Ui.*` | 画面描画とタッチ / ボタン処理 |

### 設計メモ

- **M5Unified** を使い、Core2 V1.1 の電源 IC (AXP2101) やスピーカーアンプの制御を任せている。
- MP3 デコードは **ESP8266Audio 1.9.x** (Arduino core 2.x 対応版)。
  2.x 系は Arduino core 3.x 向けのため、espressif32 6.x を使う本構成では 1.9.9 に固定している。
- LCD と SD は同じ SPI バス (VSPI) を共有するが、M5GFX が Arduino `SPI` と同じミューテックスで
  排他するため、デコードタスクと描画を別タスクで並行させても安全である
  (M5Unified 公式サンプル `MP3_with_ESP8266Audio` と同じ構成)。
- 描画中にスピーカーが途切れないよう、PCM バッファは 3 本 × 2048 フレーム (約 46ms 分) を確保している。
- 日本語ファイル名は FatFS の LFN / UTF-8 設定が有効なためそのまま扱え、
  表示には M5GFX 同梱の `lgfxJapanGothic_16` を使う。

## 制限事項

- 対応形式は MP3 のみ。ID3 タグは読み飛ばすだけで、表示はファイル名ベース。
- 曲の総時間は表示しない (進捗はファイル位置ベース)。
- SD カードの活線挿抜には対応しない。起動時にカードがなければ挿入待ちになる。
- ファイル数の上限は 1000。
