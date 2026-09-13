#pragma once
#include "secplus2_identity.h"
#include "defines.h"
#include <FS.h>

class Secplus2IdentityStore {
 public:
  og_identity::ReadResult read(og_identity::Record &record) {
    if (!FILESYS.exists(path())) return og_identity::ReadResult::MISSING;
    File file = FILESYS.open(path(), "r");
    if (!file || file.size() != sizeof(record)) return og_identity::ReadResult::INVALID;
    if (file.read(reinterpret_cast<uint8_t *>(&record), sizeof(record)) != sizeof(record))
      return og_identity::ReadResult::INVALID;
    return og_identity::valid(record) ? og_identity::ReadResult::VALID : og_identity::ReadResult::INVALID;
  }
  bool write(const og_identity::Record &record) {
    File file = FILESYS.open(path(), "w");
    if (!file) return false;
    const bool complete = file.write(reinterpret_cast<const uint8_t *>(&record), sizeof(record)) == sizeof(record);
    file.flush();
    file.close();
    return complete;
  }
 private:
  static const char *path() { return "/sec2id.dat"; }
};
