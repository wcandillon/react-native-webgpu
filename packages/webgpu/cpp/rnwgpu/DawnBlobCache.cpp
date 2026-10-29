#include "DawnBlobCache.h"

#include <dirent.h>
#include <sys/stat.h>
#include <unistd.h>

#include <algorithm>
#include <atomic>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <ctime>
#include <mutex>
#include <string>
#include <vector>

namespace rnwgpu {

namespace {

// Entries only accumulate as an app's shaders change from release to release,
// so past this size the oldest are removed until the cache fits again. The
// limit is a target, not a guarantee: stores that overlap tidying, or other
// processes using the directory, can leave it a little over until the next
// pass.
constexpr off_t kMaxCacheBytes = 64 * 1024 * 1024;

// An entry this large is not stored, so that one entry cannot take more than
// half the cache from all the others. The entry already on disk under its key,
// if any, is left as it is, and a blob that has outgrown this stops being
// updated.
constexpr uint64_t kMaxEntryBytes = kMaxCacheBytes / 2;

// A session checks the limit again each time the cache has grown by about this
// much through its stores (replacing an entry counts only what it adds), so a
// long-running process cannot take the cache far past it. The count is
// approximate: stores that overlap are measured against the file as it was
// before either replaced it.
constexpr uint64_t kTidyAfterBytes = kMaxCacheBytes / 8;

// tidy() takes a temporary file this old for a write that will never finish
// (its process was killed). A younger one may still be being written by
// another process using the same cache, so it is left. The age is a best
// guess: a writer suspended for longer than this loses its entry when tidy()
// removes its file (its rename then fails), which only costs a cache miss.
constexpr time_t kAbandonedWriteSeconds = 60;

// Never destroyed: a pipeline cache flush on another thread can still be
// storing when the process starts to exit.
std::mutex &directoryMutex() {
  static std::mutex *const mutex = new std::mutex;
  return *mutex;
}

std::string &directorySlot() {
  static std::string *const directory = new std::string;
  return *directory;
}

std::string directory() {
  std::lock_guard<std::mutex> lock(directoryMutex());
  return directorySlot();
}

void tidy(const std::string &dir);

// Creates `dir` and any of its parents that are missing.
void makeDirectories(const std::string &dir) {
  for (size_t slash = dir.find('/', 1); slash != std::string::npos;
       slash = dir.find('/', slash + 1)) {
    mkdir(dir.substr(0, slash).c_str(), 0700);
  }
  mkdir(dir.c_str(), 0700);
}

// One file per entry, named by a 64-bit FNV-1a hash of its key.
std::string entryPath(const std::string &dir, size_t keySize,
                      const uint8_t *key) {
  uint64_t hash = 14695981039346656037ull;
  for (size_t i = 0; i < keySize; ++i) {
    hash ^= key[i];
    hash *= 1099511628211ull;
  }
  char name[18];
  snprintf(name, sizeof(name), "/%016llx",
           static_cast<unsigned long long>(hash));
  return dir + name;
}

// An entry is the key's size, the value's size, the key and the value. The
// stored key is compared on load, so a hash collision reads as a miss, and so
// does an entry shorter than its recorded size. Dawn asks for the size first
// (no buffer), then for the value.
size_t loadEntry(size_t keySize, const uint8_t *key, size_t valueSize,
                 uint8_t *value, void * /* userdata1 */,
                 void * /* userdata2 */) {
  const std::string dir = directory();
  if (dir.empty()) {
    return 0;
  }
  FILE *file = fopen(entryPath(dir, keySize, key).c_str(), "rb");
  if (file == nullptr) {
    return 0;
  }
  size_t result = 0;
  uint64_t sizes[2] = {0, 0};
  if (fread(sizes, sizeof(sizes), 1, file) == 1 && sizes[0] == keySize) {
    std::vector<uint8_t> storedKey(keySize);
    if (fread(storedKey.data(), 1, keySize, file) == keySize &&
        memcmp(storedKey.data(), key, keySize) == 0) {
      const long start = ftell(file);
      fseek(file, 0, SEEK_END);
      const size_t stored = static_cast<size_t>(ftell(file) - start);
      if (stored == sizes[1]) {
        if (value == nullptr || valueSize == 0) {
          result = stored;
        } else if (valueSize >= stored) {
          fseek(file, start, SEEK_SET);
          result = fread(value, 1, stored, file) == stored ? stored : 0;
        }
      }
    }
  }
  fclose(file);
  return result;
}

// Written aside and renamed into place, so a concurrent load never sees half
// an entry. Dawn may store from several threads at once.
void storeEntry(size_t keySize, const uint8_t *key, size_t valueSize,
                const uint8_t *value, void * /* userdata1 */,
                void * /* userdata2 */) {
  const std::string dir = directory();
  if (dir.empty()) {
    return;
  }
  const uint64_t entryBytes = 2 * sizeof(uint64_t) + keySize + valueSize;
  if (entryBytes > kMaxEntryBytes) {
    return;
  }
  static std::atomic<uint64_t> writes{0};
  const std::string path = entryPath(dir, keySize, key);
  const std::string temporary = path + "." + std::to_string(getpid()) + "." +
                                std::to_string(writes.fetch_add(1)) + ".tmp";
  FILE *file = fopen(temporary.c_str(), "wb");
  if (file == nullptr) {
    // The system or the app may have cleared the cache directory, or its
    // parent, during the session.
    makeDirectories(dir);
    file = fopen(temporary.c_str(), "wb");
  }
  if (file == nullptr) {
    return;
  }
  const uint64_t sizes[2] = {keySize, valueSize};
  bool written = fwrite(sizes, sizeof(sizes), 1, file) == 1 &&
                 fwrite(key, 1, keySize, file) == keySize &&
                 fwrite(value, 1, valueSize, file) == valueSize;
  written = fclose(file) == 0 && written;
  struct stat replaced{};
  const uint64_t previousBytes = stat(path.c_str(), &replaced) == 0
                                     ? static_cast<uint64_t>(replaced.st_size)
                                     : 0;
  if (!written || rename(temporary.c_str(), path.c_str()) != 0) {
    remove(temporary.c_str());
    return;
  }
  if (entryBytes <= previousBytes) {
    return;
  }
  static std::atomic<uint64_t> grown{0};
  const uint64_t growth = entryBytes - previousBytes;
  // One thread takes the count when it is over the limit, and resets it only
  // while it still is: growth that lands after another thread has taken the
  // count stays in it for the next pass.
  if (grown.fetch_add(growth) + growth >= kTidyAfterBytes) {
    uint64_t seen = grown.load();
    while (seen >= kTidyAfterBytes) {
      if (grown.compare_exchange_weak(seen, 0)) {
        tidy(dir);
        break;
      }
    }
  }
}

bool endsWith(const std::string &name, const char *suffix) {
  const size_t length = strlen(suffix);
  return name.size() > length &&
         name.compare(name.size() - length, length, suffix) == 0;
}

// Whether a file name starts like one of the cache's own: an entry's 16
// lowercase hex digits, alone or followed by a dot (a temporary file).
bool isOwnFile(const std::string &name) {
  if (name.size() < 16 || (name.size() > 16 && name[16] != '.')) {
    return false;
  }
  for (size_t i = 0; i < 16; i++) {
    const char c = name[i];
    if (!((c >= '0' && c <= '9') || (c >= 'a' && c <= 'f'))) {
      return false;
    }
  }
  return true;
}

// Removes the temporary files of writes a killed process left unfinished, and
// the oldest entries while the cache is over kMaxCacheBytes. Runs when the
// cache is set up and after each kTidyAfterBytes stored. "Oldest" is by write
// time: a load does not refresh an entry.
void tidy(const std::string &dir) {
  // Another thread tidying the same directory is enough.
  static std::mutex *const tidying = new std::mutex;
  std::unique_lock<std::mutex> lock(*tidying, std::try_to_lock);
  if (!lock.owns_lock()) {
    return;
  }
  DIR *listing = opendir(dir.c_str());
  if (listing == nullptr) {
    return;
  }
  struct Entry {
    std::string path;
    time_t modified;
    off_t size;
    ino_t inode;
  };
  std::vector<Entry> entries;
  off_t total = 0;
  const time_t now = time(nullptr);
  while (dirent *entry = readdir(listing)) {
    const std::string name = entry->d_name;
    // Anything else in the directory is not the cache's to delete.
    if (!isOwnFile(name)) {
      continue;
    }
    const std::string path = dir + "/" + name;
    struct stat info{};
    if (stat(path.c_str(), &info) != 0) {
      continue;
    }
    if (endsWith(name, ".tmp")) {
      if (now - info.st_mtime > kAbandonedWriteSeconds) {
        remove(path.c_str());
      }
      continue;
    }
    total += info.st_size;
    entries.push_back({path, info.st_mtime, info.st_size, info.st_ino});
  }
  closedir(listing);
  if (total <= kMaxCacheBytes) {
    return;
  }
  std::sort(entries.begin(), entries.end(), [](const Entry &a, const Entry &b) {
    return a.modified != b.modified ? a.modified < b.modified : a.path < b.path;
  });
  for (const auto &entry : entries) {
    if (total <= kMaxCacheBytes) {
      break;
    }
    // Another store may have replaced the entry since it was listed; the new
    // one is not the old one this pass chose to remove. A replacement is a new
    // file, so its inode tells it apart. Best effort: one landing between this
    // check and the removal still goes, which only costs a cache miss.
    struct stat current{};
    if (stat(entry.path.c_str(), &current) != 0 ||
        current.st_ino != entry.inode || current.st_mtime != entry.modified ||
        current.st_size != entry.size) {
      continue;
    }
    if (remove(entry.path.c_str()) == 0) {
      total -= entry.size;
    }
  }
}

} // namespace

void DawnBlobCache::setDirectory(const std::string &dir) {
  std::string usable = dir;
  if (!usable.empty()) {
    makeDirectories(usable);
    struct stat info{};
    if (stat(usable.c_str(), &info) == 0 && S_ISDIR(info.st_mode)) {
      tidy(usable);
    } else {
      // Nowhere to keep blobs: leave the cache off rather than fail every
      // load and store.
      usable.clear();
    }
  }
  std::lock_guard<std::mutex> lock(directoryMutex());
  directorySlot() = usable;
}

bool DawnBlobCache::chain(wgpu::DeviceDescriptor &descriptor,
                          WGPUDawnCacheDeviceDescriptor &storage) {
  if (directory().empty()) {
    return false;
  }
  storage.dawnLoadCacheDataCallbackInfo.callback = loadEntry;
  storage.dawnStoreCacheDataCallbackInfo.callback = storeEntry;
  storage.chain.next = reinterpret_cast<WGPUChainedStruct *>(
      const_cast<wgpu::ChainedStruct *>(descriptor.nextInChain));
  descriptor.nextInChain = reinterpret_cast<wgpu::ChainedStruct *>(&storage);
  return true;
}

} // namespace rnwgpu
