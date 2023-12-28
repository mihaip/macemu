#include "sysdeps.h"

#include <emscripten.h>

#include "disk_js.h"

#define DEBUG 0
#include "debug.h"

typedef int DiskId;

struct disk_js : disk_generic {
  disk_js(DiskId disk_id, bool read_only) : disk_id(disk_id), read_only(read_only) {}

  virtual ~disk_js() {
    EM_ASM_({ workerApi.disks.close($0); }, disk_id);
  }

  virtual bool is_read_only() { return read_only; }

  virtual bool is_media_present() {
    bool is_media_present = EM_ASM_INT({ return workerApi.disks.isMediaPresent($0); }, disk_id) > 0;
    D(bug("disk_js.size: disk_id=%d is_media_present=%s\n", disk_id,
          is_media_present ? "true" : "false"));
    return is_media_present;
  }

  virtual bool is_fixed_disk() {
    bool is_fixed_disk = EM_ASM_INT({ return workerApi.disks.isFixedDisk($0); }, disk_id) > 0;
    D(bug("disk_js.size: disk_id=%d is_fixed_disk=%s\n", disk_id,
          is_fixed_disk ? "true" : "false"));
    return is_fixed_disk;
  }

  virtual void eject() {
    EM_ASM({ workerApi.disks.eject($0); }, disk_id);
    D(bug("disk_js.size: disk_id=%d eject\n", disk_id));
  }

  virtual loff_t size() {
    int size = EM_ASM_INT({ return workerApi.disks.size($0); }, disk_id);
    D(bug("disk_js.size: disk_id=%d size=%d\n", disk_id, size));
    return size;
  }

  virtual size_t read(void* buf, loff_t offset, size_t length) {
    D(bug("disk_js.read: disk_id=%d offset=%lld length=%zu\n", disk_id, offset, length));
    int read_size = EM_ASM_INT({ return workerApi.disks.read($0, $1, $2, $3); }, disk_id, buf,
                               int(offset), int(length));
    return read_size;
  }

  virtual size_t write(void* buf, loff_t offset, size_t length) {
    D(bug("disk_js.write: disk_id=%d offset=%lld length=%zu\n", disk_id, offset, length));
    int write_size = EM_ASM_INT({ return workerApi.disks.write($0, $1, $2, $3); }, disk_id, buf,
                                int(offset), int(length));
    return write_size;
  }

 protected:
  DiskId disk_id;
  bool read_only;
};

disk_generic::status disk_js_factory(const char* path, bool read_only, disk_generic** disk) {
  D(bug("disk_js_factory: path=%s read_only=%s\n", path, read_only ? "true" : "false"));
  DiskId disk_id = EM_ASM_INT({ return workerApi.disks.open(UTF8ToString($0)); }, path);
  D(bug("disk id=%d\n", disk_id));

  if (disk_id == -1) {
    return disk_generic::DISK_UNKNOWN;
  }

  *disk = new disk_js(disk_id, read_only);
  return disk_generic::DISK_VALID;
}