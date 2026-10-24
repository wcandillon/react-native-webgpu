#include <gtest/gtest.h>

#include <sys/stat.h>
#include <utime.h>

#include <cstdint>
#include <cstdlib>
#include <ctime>
#include <filesystem>
#include <fstream>
#include <string>
#include <thread>
#include <vector>

#include "DawnBlobCache.h"

namespace {

namespace fs = std::filesystem;
using rnwgpu::DawnBlobCache;
using Bytes = std::vector<uint8_t>;

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

  void write(const Bytes &key, const Bytes &value) const {
    store(key.size(), key.data(), value.size(), value.data(), nullptr, nullptr);
  }
};

// The cache directory is process-wide: every test gets its own and disables
// the cache when it ends.
class DawnBlobCacheTest : public ::testing::Test {
protected:
  void SetUp() override {
    std::string pattern =
        (fs::temp_directory_path() / "dawn-blob-cache-XXXXXX").string();
    ASSERT_NE(mkdtemp(pattern.data()), nullptr);
    _root = pattern;
    _dir = _root / "cache";
    DawnBlobCache::setDirectory(_dir.string());
  }

  void TearDown() override {
    DawnBlobCache::setDirectory("");
    fs::remove_all(_root);
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
  std::vector<fs::path> entries() const {
    std::vector<fs::path> paths;
    for (const auto &entry : fs::directory_iterator(_dir)) {
      if (entry.path().extension() != ".tmp") {
        paths.push_back(entry.path());
      }
    }
    return paths;
  }

  static void age(const fs::path &path, time_t seconds) {
    const time_t then = time(nullptr) - seconds;
    utimbuf times{then, then};
    ASSERT_EQ(utime(path.c_str(), &times), 0);
  }

  fs::path _root;
  fs::path _dir;
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

TEST_F(DawnBlobCacheTest, CreatesItsDirectory) {
  EXPECT_TRUE(fs::is_directory(_dir));
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

TEST_F(DawnBlobCacheTest, KeepsEntriesAcrossASecondSetup) {
  callbacks().write({1}, {9, 8, 7});

  DawnBlobCache::setDirectory(_dir.string());

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

  fs::resize_file(entry, fs::file_size(entry) - 1);

  EXPECT_EQ(cache.size({1}), 0u);
}

// Two keys whose hashes collide share a file; the key stored in the file
// decides which of them it holds.
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
        cache.write(ownKey, value);
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
  fs::remove_all(_dir);

  cache.write({1}, {2, 3});

  EXPECT_EQ(cache.read({1}), (Bytes{2, 3}));
}

TEST_F(DawnBlobCacheTest, StaysQuietWhenItsDirectoryCannotBeMade) {
  // A directory can't be made under a regular file.
  const auto blocker = _root / "file";
  std::ofstream(blocker).put('x');
  DawnBlobCache::setDirectory((blocker / "cache").string());
  const auto cache = callbacks();

  cache.write({1}, {2, 3});

  EXPECT_EQ(cache.size({1}), 0u);
}

TEST_F(DawnBlobCacheTest, RemovesAWriteAbandonedByAKilledProcess) {
  const auto abandoned = _dir / "0123456789abcdef.4242.0.tmp";
  std::ofstream(abandoned).put('x');
  age(abandoned, 120);

  DawnBlobCache::setDirectory(_dir.string());

  EXPECT_FALSE(fs::exists(abandoned));
}

TEST_F(DawnBlobCacheTest, LeavesAWriteAnotherProcessMayStillFinish) {
  const auto writing = _dir / "0123456789abcdef.4242.0.tmp";
  std::ofstream(writing).put('x');

  DawnBlobCache::setDirectory(_dir.string());

  EXPECT_TRUE(fs::exists(writing));
}

TEST_F(DawnBlobCacheTest, KeepsACacheWithinItsLimit) {
  callbacks().write({1}, {2, 3});

  DawnBlobCache::setDirectory(_dir.string());

  EXPECT_EQ(entries().size(), 1u);
}

// The limit is checked when the cache is set up, not on every store.
TEST_F(DawnBlobCacheTest, ClearsACacheThatOutgrewItsLimitAtTheNextSetup) {
  const auto cache = callbacks();
  cache.write({1}, {2, 3});
  const auto large = _dir / "large";
  std::ofstream(large).put('x');
  fs::resize_file(large, 65 * 1024 * 1024);

  cache.write({2}, {4, 5});
  EXPECT_EQ(entries().size(), 3u);

  DawnBlobCache::setDirectory(_dir.string());
  EXPECT_TRUE(entries().empty());
  EXPECT_EQ(cache.size({1}), 0u);
}

} // namespace
