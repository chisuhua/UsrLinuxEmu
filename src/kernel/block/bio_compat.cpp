#include <kernel/block/bio_compat.h>

#include <linux_compat/types.h>
#include <cerrno>

#include <cstdio>
#include <cstring>
#include <map>
#include <string>
#include <vector>

namespace {

struct block_file {
    std::FILE* fp;
    std::string path;
    unsigned long size;
};

std::map<int, block_file> g_files;
int g_next_fd = 2000;

int alloc_fd(void) { return g_next_fd++; }

}  // namespace

extern "C" {

int us_block_open(const char* path, unsigned long flags) {
  if (!path) return -22;
  std::string mode = "rb+";
  if (flags == 0) mode = "rb";
  else if (flags & 0x40) mode = "wb";  /* O_CREAT */
  else if (flags & 0x200) mode = "ab";  /* O_APPEND */
  std::FILE* fp = std::fopen(path, mode.c_str());
  if (!fp) return -2;  /* -ENOENT */

  std::fseek(fp, 0, SEEK_END);
  long sz = std::ftell(fp);
  std::fseek(fp, 0, SEEK_SET);

  int fd = alloc_fd();
  block_file bf{};
  bf.fp = fp;
  bf.path = path;
  bf.size = (sz < 0) ? 0 : static_cast<unsigned long>(sz);
  g_files[fd] = bf;
  return fd;
}

int us_block_close(int fd) {
  auto it = g_files.find(fd);
  if (it == g_files.end()) return -9;
  if (it->second.fp) std::fclose(it->second.fp);
  g_files.erase(it);
  return 0;
}

long us_block_read(int fd, void* buf, unsigned long count) {
  if (!buf || count == 0) return -22;
  auto it = g_files.find(fd);
  if (it == g_files.end()) return -9;
  return static_cast<long>(std::fread(buf, 1, count, it->second.fp));
}

long us_block_write(int fd, const void* buf, unsigned long count) {
  if (!buf || count == 0) return -22;
  auto it = g_files.find(fd);
  if (it == g_files.end()) return -9;
  return static_cast<long>(std::fwrite(buf, 1, count, it->second.fp));
}

long us_block_seek(int fd, long offset, int whence) {
  auto it = g_files.find(fd);
  if (it == g_files.end()) return -9;
  if (std::fseek(it->second.fp, offset, whence) != 0) return -5;
  return std::ftell(it->second.fp);
}

unsigned long us_block_size(int fd) {
  auto it = g_files.find(fd);
  if (it == g_files.end()) return 0;
  return it->second.size;
}

const char* us_block_path(int fd) {
  auto it = g_files.find(fd);
  if (it == g_files.end()) return nullptr;
  return it->second.path.c_str();
}

}  // extern "C"