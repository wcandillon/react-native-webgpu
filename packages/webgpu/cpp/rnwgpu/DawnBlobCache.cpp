#include "DawnBlobCache.h"

#include <dirent.h>
#include <sys/stat.h>
#include <unistd.h>

#include <atomic>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <mutex>
#include <string>
#include <vector>

namespace rnwgpu {

namespace {

// Entries only accumulate as an app's shaders change from release to release,
// so past this size the cache is cleared and refills with what the app
// compiles next.
constexpr off_t kMaxCacheBytes = 64 * 1024 * 1024;

// Keeps this library's entries apart from any other user of the same Dawn
// cache in the process.
constexpr const char *kIsolationKey = "react-native-webgpu";

std::mutex &directoryMutex() {
  static std::mutex mutex;
  return mutex;
}

std::string &directorySlot() {
  static std::string directory;
  return directory;
}

std::string directory() {
  std::lock_guard<std::mutex> lock(directoryMutex());
  return directorySlot();
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
  static std::atomic<uint64_t> writes{0};
  const std::string path = entryPath(dir, keySize, key);
  const std::string temporary = path + "." + std::to_string(getpid()) + "." +
                                std::to_string(writes.fetch_add(1)) + ".tmp";
  FILE *file = fopen(temporary.c_str(), "wb");
  if (file == nullptr) {
    return;
  }
  const uint64_t sizes[2] = {keySize, valueSize};
  bool written = fwrite(sizes, sizeof(sizes), 1, file) == 1 &&
                 fwrite(key, 1, keySize, file) == keySize &&
                 fwrite(value, 1, valueSize, file) == valueSize;
  written = fclose(file) == 0 && written;
  if (!written || rename(temporary.c_str(), path.c_str()) != 0) {
    remove(temporary.c_str());
  }
}

bool endsWith(const std::string &name, const char *suffix) {
  const size_t length = strlen(suffix);
  return name.size() > length &&
         name.compare(name.size() - length, length, suffix) == 0;
}

// Removes the temporary files of writes a killed process left unfinished, and
// clears the cache once it has outgrown kMaxCacheBytes.
void tidy(const std::string &dir) {
  DIR *listing = opendir(dir.c_str());
  if (listing == nullptr) {
    return;
  }
  std::vector<std::string> entries;
  off_t total = 0;
  while (dirent *entry = readdir(listing)) {
    const std::string name = entry->d_name;
    if (name == "." || name == "..") {
      continue;
    }
    const std::string path = dir + "/" + name;
    if (endsWith(name, ".tmp")) {
      remove(path.c_str());
      continue;
    }
    struct stat info{};
    if (stat(path.c_str(), &info) == 0) {
      total += info.st_size;
    }
    entries.push_back(path);
  }
  closedir(listing);
  if (total > kMaxCacheBytes) {
    for (const auto &path : entries) {
      remove(path.c_str());
    }
  }
}

} // namespace

void DawnBlobCache::setDirectory(const std::string &dir) {
  if (!dir.empty()) {
    mkdir(dir.c_str(), 0700);
    tidy(dir);
  }
  std::lock_guard<std::mutex> lock(directoryMutex());
  directorySlot() = dir;
}

void DawnBlobCache::chain(wgpu::DeviceDescriptor &descriptor,
                          WGPUDawnCacheDeviceDescriptor &storage) {
  if (directory().empty()) {
    return;
  }
  storage.isolationKey = {kIsolationKey, WGPU_STRLEN};
  storage.dawnLoadCacheDataCallbackInfo.callback = loadEntry;
  storage.dawnStoreCacheDataCallbackInfo.callback = storeEntry;
  storage.chain.next = reinterpret_cast<WGPUChainedStruct *>(
      const_cast<wgpu::ChainedStruct *>(descriptor.nextInChain));
  descriptor.nextInChain = reinterpret_cast<wgpu::ChainedStruct *>(&storage);
}

} // namespace rnwgpu
