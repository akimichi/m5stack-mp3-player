#include "Playlist.h"

#include <M5Unified.h>
#include <SD.h>
#include <SPI.h>

#include <algorithm>

bool Playlist::mount() {
  int cs = M5.getPin(m5::pin_name_t::sd_spi_cs);
  if (cs < 0) cs = GPIO_NUM_4;  // Core2 の TF_CS
  if (M5.hasSD()) {
    SPI.begin(M5.getPin(m5::pin_name_t::sd_spi_sclk), M5.getPin(m5::pin_name_t::sd_spi_miso),
              M5.getPin(m5::pin_name_t::sd_spi_mosi), cs);
  }
  if (!SD.begin(cs, SPI, 25000000)) {
    _fs = nullptr;
    return false;
  }
  _fs = &SD;
  return true;
}

void Playlist::scan(const char* root) {
  _files.clear();
  if (_fs == nullptr) return;
  scanDir(root, 0);
  std::sort(_files.begin(), _files.end(), [](const String& a, const String& b) {
    return strcasecmp(a.c_str(), b.c_str()) < 0;
  });
  M5_LOGI("%u mp3 files found", (unsigned)_files.size());
}

void Playlist::scanDir(const String& dir, int depth) {
  File d = _fs->open(dir);
  if (!d || !d.isDirectory()) return;
  for (File f = d.openNextFile(); f && _files.size() < kMaxFiles; f = d.openNextFile()) {
    String path = f.path();  // Arduino core 2.x: path() がフルパス、name() がベース名
    String name = f.name();
    bool isDir = f.isDirectory();
    f.close();
    if (isDir) {
      if (depth + 1 < kMaxDepth && !isSkippedDir(name.c_str())) scanDir(path, depth + 1);
    } else if (isMp3(name) && name[0] != '.') {
      _files.push_back(path);
    }
  }
  d.close();
}

bool Playlist::isMp3(const String& name) {
  int dot = name.lastIndexOf('.');
  if (dot < 0) return false;
  String ext = name.substring(dot);
  ext.toLowerCase();
  return ext == ".mp3";
}

bool Playlist::isSkippedDir(const char* name) {
  return name[0] == '.' || strcmp(name, "System Volume Information") == 0;
}

String Playlist::displayName(size_t index) const {
  if (index >= _files.size()) return String();
  const String& p = _files[index];
  int slash = p.lastIndexOf('/');
  String base = p.substring(slash + 1);
  int dot = base.lastIndexOf('.');
  if (dot > 0) base = base.substring(0, dot);
  return base;
}
