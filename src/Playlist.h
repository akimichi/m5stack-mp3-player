// Playlist.h
//
// SD カードのマウントと *.mp3 ファイルの一覧化。
#pragma once

#include <Arduino.h>
#include <FS.h>
#include <vector>

class Playlist {
 public:
  static constexpr size_t kMaxFiles = 1000;  // これ以上は無視する
  static constexpr int kMaxDepth = 3;        // ルートからのディレクトリ階層の上限

  /// M5Unified が知っているピンで SD を SPI マウントする。失敗なら false。
  bool mount();
  /// root 以下を再帰的に走査し、*.mp3 をパス順に並べる。
  void scan(const char* root = "/");

  bool mounted() const { return _fs != nullptr; }
  fs::FS* fs() const { return _fs; }
  size_t size() const { return _files.size(); }
  bool empty() const { return _files.empty(); }
  /// フルパス
  const String& path(size_t index) const { return _files[index]; }
  /// 表示用: 拡張子を除いたファイル名
  String displayName(size_t index) const;

 private:
  void scanDir(const String& dir, int depth);
  static bool isMp3(const String& name);
  static bool isSkippedDir(const char* name);

  fs::FS* _fs = nullptr;
  std::vector<String> _files;
};
