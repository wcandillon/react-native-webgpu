#include <gtest/gtest.h>

#include <dirent.h>
#include <sys/stat.h>
#include <unistd.h>
#include <utime.h>

#include <cstdint>
#include <cstdlib>
#include <ctime>
#include <fstream>
#include <string>
#include <thread>
#include <vector>

#include "DawnBlobCache.h"

namespace {

using rnwgpu::DawnBlobCache;
using Bytes = std::vector<uint8_t>;

constexpr off_t kMegabyte = 1024 * 1024;

bool exists(const std::string &path) {
  struct stat info{};
  return stat(path.c_str(), &info) == 0;
}

bool isDirectory(const std::string &path) {
  struct stat info{};
  return stat(path.c_str(), &info) == 0 && S_ISDIR(info.st_mode);
}

off_t fileSize(const std::string &path) {
  struct stat info{};
  return stat(path.c_str(), &info) == 0 ? info.st_size : -1;
}

void writeFile(const std::string &path, const std::string &contents) {
  std::ofstream(path, std::ios::binary) << contents;
}

// A file of `bytes` that takes no disk: a hole, which counts towards the
// cache's limit all the same.
void writeSparseFile(const std::string &path, off_t bytes) {
  writeFile(path, "x");
  ASSERT_EQ(truncate(path.c_str(), bytes), 0);
}

// The names in a directory, leaving out "." and "..".
std::vector<std::string> list(const std::string &dir) {
  std::vector<std::string> names;
  if (DIR *listing = opendir(dir.c_str())) {
    while (dirent *entry = readdir(listing)) {
      const std::string name = entry->d_name;
      if (name != "." && name != "..") {
        names.push_back(name);
      }
    }
    closedir(listing);
  }
  return names;
}

void removeTree(const std::string &path) {
  if (isDirectory(path)) {
    for (const auto &name : list(path)) {
      removeTree(path + "/" + name);
    }
    rmdir(path.c_str());
  } else {
    unlink(path.c_str());
  }
}

// The cache as Dawn sees it: the callbacks chained onto a device descriptor.
struct Callbacks {
  WGPUDawnLoadCacheDataCallback load;
  WGPUDawnStoreCacheDataCallback store;

  // The stored value's size, or 0 for a miss.
  size_t size(const Bytes &key) const {
    return load(key.size(), key.data(), 0, nullptr, nullptr, nullptr);
  }

  // The stored value, or nothing for a miss.
  Bytes read(const Bytes &key) const {
    Bytes value(size(key));
    if (value.empty()) {
      return value;
    }
    const size_t read = load(key.size(), key.data(), value.size(), value.data(),
                             nullptr, nullptr);
    value.resize(read);
    return value;
  }

  // Stores, as Dawn does, without waiting for the write.
  void storeOnly(const Bytes &key, const Bytes &value) const {
    store(key.size(), key.data(), value.size(), value.data(), nullptr, nullptr);
  }

  // Stores, and waits for the write.
  void write(const Bytes &key, const Bytes &value) const {
    storeOnly(key, value);
    DawnBlobCache::waitForWrites();
  }
};

// The cache directory is process-wide: every test gets its own and disables
// the cache when it ends.
class DawnBlobCacheTest : public ::testing::Test {
protected:
  void SetUp() override {
    const char *tmp = getenv("TMPDIR");
    std::string pattern =
        std::string(tmp != nullptr ? tmp : "/tmp") + "/dawn-blob-cache-XXXXXX";
    ASSERT_NE(mkdtemp(pattern.data()), nullptr);
    _root = pattern;
    _dir = _root + "/cache";
    setUpDirectory(_dir);
  }

  void TearDown() override {
    DawnBlobCache::setDirectory("");
    DawnBlobCache::waitForWrites();
    removeTree(_root);
  }

  // Sets the directory and waits for its tidying to be done.
  static void setUpDirectory(const std::string &dir) {
    DawnBlobCache::setDirectory(dir);
    DawnBlobCache::waitForWrites();
  }

  static Callbacks callbacks() {
    wgpu::DeviceDescriptor descriptor;
    WGPUDawnCacheDeviceDescriptor storage =
        WGPU_DAWN_CACHE_DEVICE_DESCRIPTOR_INIT;
    DawnBlobCache::chain(descriptor, storage);
    return {storage.dawnLoadCacheDataCallbackInfo.callback,
            storage.dawnStoreCacheDataCallbackInfo.callback};
  }

  // The cache's entries, leaving out temporary files.
  std::vector<std::string> entries() const {
    std::vector<std::string> paths;
    for (const auto &name : list(_dir)) {
      if (name.size() < 4 || name.compare(name.size() - 4, 4, ".tmp") != 0) {
        paths.push_back(_dir + "/" + name);
      }
    }
    return paths;
  }

  static void age(const std::string &path, time_t seconds) {
    const time_t then = time(nullptr) - seconds;
    utimbuf times{then, then};
    ASSERT_EQ(utime(path.c_str(), &times), 0);
  }

  std::string _root;
  std::string _dir;
};

TEST_F(DawnBlobCacheTest, ChainsItselfInFrontOfTheDescriptorsChain) {
  wgpu::DawnTogglesDescriptor toggles;
  wgpu::DeviceDescriptor descriptor;
  descriptor.nextInChain = &toggles;
  WGPUDawnCacheDeviceDescriptor storage =
      WGPU_DAWN_CACHE_DEVICE_DESCRIPTOR_INIT;

  DawnBlobCache::chain(descriptor, storage);

  EXPECT_EQ(descriptor.nextInChain,
            reinterpret_cast<wgpu::ChainedStruct *>(&storage));
  EXPECT_EQ(storage.chain.sType, WGPUSType_DawnCacheDeviceDescriptor);
  EXPECT_EQ(storage.chain.next,
            reinterpret_cast<WGPUChainedStruct *>(&toggles));
  EXPECT_NE(storage.dawnLoadCacheDataCallbackInfo.callback, nullptr);
  EXPECT_NE(storage.dawnStoreCacheDataCallbackInfo.callback, nullptr);
}

TEST_F(DawnBlobCacheTest, ChainsNothingWithoutADirectory) {
  DawnBlobCache::setDirectory("");
  wgpu::DeviceDescriptor descriptor;
  WGPUDawnCacheDeviceDescriptor storage =
      WGPU_DAWN_CACHE_DEVICE_DESCRIPTOR_INIT;

  DawnBlobCache::chain(descriptor, storage);

  EXPECT_EQ(descriptor.nextInChain, nullptr);
}

TEST_F(DawnBlobCacheTest, SaysWhetherItChained) {
  wgpu::DeviceDescriptor descriptor;
  WGPUDawnCacheDeviceDescriptor storage =
      WGPU_DAWN_CACHE_DEVICE_DESCRIPTOR_INIT;
  EXPECT_TRUE(DawnBlobCache::chain(descriptor, storage));

  DawnBlobCache::setDirectory("");
  wgpu::DeviceDescriptor other;
  WGPUDawnCacheDeviceDescriptor otherStorage =
      WGPU_DAWN_CACHE_DEVICE_DESCRIPTOR_INIT;
  EXPECT_FALSE(DawnBlobCache::chain(other, otherStorage));
}

TEST_F(DawnBlobCacheTest, CreatesItsDirectory) {
  EXPECT_TRUE(isDirectory(_dir));
}

TEST_F(DawnBlobCacheTest, CreatesMissingParentsWhenSetUp) {
  const auto nested = _root + "/a/b/cache";

  setUpDirectory(nested);

  EXPECT_TRUE(isDirectory(nested));
}

TEST_F(DawnBlobCacheTest, StaysOffWhereNoDirectoryCanBeMade) {
  // A directory can't be made under a regular file.
  const auto blocker = _root + "/file";
  writeFile(blocker, "x");

  setUpDirectory(blocker + "/cache");

  wgpu::DeviceDescriptor descriptor;
  WGPUDawnCacheDeviceDescriptor storage =
      WGPU_DAWN_CACHE_DEVICE_DESCRIPTOR_INIT;
  DawnBlobCache::chain(descriptor, storage);
  EXPECT_EQ(descriptor.nextInChain, nullptr);
}

TEST_F(DawnBlobCacheTest, MissesAKeyNeverStored) {
  EXPECT_EQ(callbacks().size({1, 2, 3}), 0u);
}

TEST_F(DawnBlobCacheTest, ReturnsTheExactBytesStored) {
  const auto cache = callbacks();
  const Bytes key{1, 2, 3};
  const Bytes value{0, 255, 7, 0, 42};

  cache.write(key, value);

  EXPECT_EQ(cache.size(key), value.size());
  EXPECT_EQ(cache.read(key), value);
}

// Dawn stores from the thread that compiles, and holds a device lock while it
// stores its pipeline cache: the write must not keep it waiting.
TEST_F(DawnBlobCacheTest, WritesWhatWasStoredOnceWaitedFor) {
  const auto cache = callbacks();
  for (uint8_t i = 0; i < 64; i++) {
    cache.storeOnly({i}, Bytes(1024, i));
  }

  DawnBlobCache::waitForWrites();

  EXPECT_EQ(entries().size(), 64u);
  for (uint8_t i = 0; i < 64; i++) {
    EXPECT_EQ(cache.read({i}), Bytes(1024, i));
  }
}

TEST_F(DawnBlobCacheTest, KeepsEntriesAcrossASecondSetup) {
  callbacks().write({1}, {9, 8, 7});

  setUpDirectory(_dir);

  EXPECT_EQ(callbacks().read({1}), (Bytes{9, 8, 7}));
}

TEST_F(DawnBlobCacheTest, ReplacesAValueStoredAgain) {
  const auto cache = callbacks();
  cache.write({1}, {1, 1, 1, 1});

  cache.write({1}, {2, 2});

  EXPECT_EQ(cache.read({1}), (Bytes{2, 2}));
}

TEST_F(DawnBlobCacheTest, LeavesABufferTooSmallForTheValueUntouched) {
  const auto cache = callbacks();
  const Bytes key{1};
  cache.write(key, {4, 5, 6, 7});
  Bytes buffer{99, 99};

  const size_t read = cache.load(key.size(), key.data(), buffer.size(),
                                 buffer.data(), nullptr, nullptr);

  EXPECT_EQ(read, 0u);
  EXPECT_EQ(buffer, (Bytes{99, 99}));
}

TEST_F(DawnBlobCacheTest, MissesATruncatedEntry) {
  const auto cache = callbacks();
  cache.write({1}, {4, 5, 6, 7});
  const auto entry = entries().at(0);

  ASSERT_EQ(truncate(entry.c_str(), fileSize(entry) - 1), 0);

  EXPECT_EQ(cache.size({1}), 0u);
}

// Two keys whose hashes collide would share a file: the key stored in it
// decides which of them it holds, so a stored key that differs reads as a miss.
TEST_F(DawnBlobCacheTest, MissesAnEntryWhoseStoredKeyDiffers) {
  const auto cache = callbacks();
  cache.write({1, 2}, {4, 5, 6, 7});
  const auto entry = entries().at(0);

  {
    // The key starts after the two 64-bit sizes.
    std::fstream file(entry, std::ios::in | std::ios::out | std::ios::binary);
    file.seekp(2 * sizeof(uint64_t));
    file.put(static_cast<char>(99));
  }

  EXPECT_EQ(cache.size({1, 2}), 0u);
}

TEST_F(DawnBlobCacheTest, NeverTearsAValueWrittenFromManyThreads) {
  const auto cache = callbacks();
  const Bytes sharedKey{88};
  std::vector<std::thread> threads;
  std::vector<int> failures(8, 0);

  for (int thread = 0; thread < 8; thread++) {
    threads.emplace_back([&, thread] {
      const Bytes ownKey{static_cast<uint8_t>(thread), 42};
      const Bytes value(4096, static_cast<uint8_t>(thread));
      for (int round = 0; round < 64; round++) {
        cache.storeOnly(ownKey, value);
        cache.write(sharedKey, value);
        if (cache.read(ownKey) != value) {
          failures[thread]++;
        }
        // Another thread may have replaced it, but never with a mix of two.
        const Bytes shared = cache.read(sharedKey);
        if (shared.size() != value.size() ||
            shared != Bytes(shared.size(), shared[0])) {
          failures[thread]++;
        }
      }
    });
  }
  for (auto &thread : threads) {
    thread.join();
  }

  EXPECT_EQ(failures, std::vector<int>(8, 0));
}

TEST_F(DawnBlobCacheTest, RecreatesItsDirectoryIfItIsCleared) {
  const auto cache = callbacks();
  removeTree(_dir);

  cache.write({1}, {2, 3});

  EXPECT_EQ(cache.read({1}), (Bytes{2, 3}));
}

// On Apple platforms the cache sits inside a folder named after the app, which
// a cache clear can remove too.
TEST_F(DawnBlobCacheTest, RecreatesItsParentsIfTheyAreCleared) {
  const auto parent = _root + "/app";
  setUpDirectory(parent + "/cache");
  const auto cache = callbacks();
  removeTree(parent);

  cache.write({1}, {2, 3});

  EXPECT_EQ(cache.read({1}), (Bytes{2, 3}));
}

TEST_F(DawnBlobCacheTest, RemovesWritesAbandonedByAKilledProcess) {
  const auto abandoned = _dir + "/0123456789abcdef.4242.tmp";
  // The shape of an earlier version's temporary files.
  const auto older = _dir + "/0123456789abcdef.4242.0.tmp";
  writeFile(abandoned, "x");
  writeFile(older, "x");
  age(abandoned, 120);
  age(older, 120);

  setUpDirectory(_dir);

  EXPECT_FALSE(exists(abandoned));
  EXPECT_FALSE(exists(older));
}

TEST_F(DawnBlobCacheTest, LeavesAWriteAnotherProcessMayStillFinish) {
  const auto writing = _dir + "/0123456789abcdef.4242.tmp";
  writeFile(writing, "x");

  setUpDirectory(_dir);

  EXPECT_TRUE(exists(writing));
}

TEST_F(DawnBlobCacheTest, KeepsACacheWithinItsLimit) {
  callbacks().write({1}, {2, 3});

  setUpDirectory(_dir);

  EXPECT_EQ(entries().size(), 1u);
}

// A cache past its limit holds mostly what the app no longer uses, and which
// entries those are cannot be told: it starts over.
TEST_F(DawnBlobCacheTest, ClearsACacheThatOutgrewItsLimitAtTheNextSetup) {
  const auto cache = callbacks();
  cache.write({1}, {2, 3});
  writeSparseFile(_dir + "/00000000000000ff", 65 * kMegabyte);

  setUpDirectory(_dir);

  EXPECT_TRUE(entries().empty());
  EXPECT_EQ(cache.size({1}), 0u);
}

// A long-running session is held to the limit too, until the next setup
// clears the cache.
TEST_F(DawnBlobCacheTest, DropsStoresPastTheLimitUntilTheNextSetup) {
  const auto cache = callbacks();
  const auto large = _dir + "/00000000000000ff";
  writeSparseFile(large, 60 * kMegabyte);
  setUpDirectory(_dir);

  cache.write({1}, Bytes(9 * kMegabyte, 7));

  EXPECT_EQ(cache.size({1}), 0u);
  EXPECT_TRUE(exists(large));

  writeSparseFile(_dir + "/00000000000000fe", 5 * kMegabyte);
  setUpDirectory(_dir);
  cache.write({1}, Bytes(9 * kMegabyte, 7));

  EXPECT_FALSE(exists(large));
  EXPECT_EQ(cache.size({1}), 9u * kMegabyte);
}

// Dawn's monolithic pipeline cache is one entry stored over and over: that is
// not the cache growing.
TEST_F(DawnBlobCacheTest, DoesNotCountAReplacementAsTheCacheGrowing) {
  const auto cache = callbacks();
  writeSparseFile(_dir + "/00000000000000ff", 60 * kMegabyte);
  setUpDirectory(_dir);
  cache.write({1}, Bytes(3 * kMegabyte, 7));

  cache.write({1}, Bytes(3 * kMegabyte, 8));
  cache.write({2}, Bytes(3 * kMegabyte, 9));

  EXPECT_EQ(cache.read({1}), Bytes(3 * kMegabyte, 8));
  EXPECT_EQ(cache.size({2}), 0u);
}

// What the system cleared mid-session is no longer in the cache, so it no
// longer counts against the limit.
TEST_F(DawnBlobCacheTest, StartsCountingAgainWhenItsDirectoryWasCleared) {
  const auto cache = callbacks();
  writeSparseFile(_dir + "/00000000000000ff", 60 * kMegabyte);
  setUpDirectory(_dir);
  removeTree(_dir);

  cache.write({1}, Bytes(9 * kMegabyte, 7));

  EXPECT_EQ(cache.size({1}), 9u * kMegabyte);
}

// Storing it would take most of the cache from all the other entries.
TEST_F(DawnBlobCacheTest, DoesNotStoreAnEntryTooLargeForTheCache) {
  const auto cache = callbacks();
  cache.write({1}, {2, 3});
  cache.write({2}, {4});

  cache.write({1}, Bytes(33 * kMegabyte, 7));

  EXPECT_EQ(cache.read({1}), (Bytes{2, 3}));
  EXPECT_EQ(cache.read({2}), (Bytes{4}));
  EXPECT_EQ(entries().size(), 2u);
}

// A wrong path must never turn clearing the cache into deleting someone's
// files: only names shaped like the cache's own are counted or removed.
TEST_F(DawnBlobCacheTest, LeavesFilesThatAreNotTheCachesAlone) {
  const auto other = _dir + "/notes.txt";
  const auto similar = _dir + "/0123456789abcdefg";
  const auto lookalike = _dir + "/0123456789abcdef.bak";
  const auto large = _dir + "/00000000000000ff";
  writeSparseFile(other, 65 * kMegabyte);
  writeFile(similar, "x");
  writeFile(lookalike, "x");
  age(lookalike, 3600);
  writeSparseFile(large, 65 * kMegabyte);

  setUpDirectory(_dir);

  EXPECT_FALSE(exists(large));
  EXPECT_TRUE(exists(other));
  EXPECT_TRUE(exists(similar));
  EXPECT_TRUE(exists(lookalike));
}

} // namespace
