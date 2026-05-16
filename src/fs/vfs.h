#ifndef __VFS_H__
#define __VFS_H__

#ifdef __cplusplus
extern "C" {
#endif

#include <stddef.h>

#define MAX_MOUNTS 8
#define VFS_MAX_PATH 64


enum vnode_type_t {
    VNODE_FILE,
    VNODE_DIR,
};

struct fs_driver_t;
struct mount_t;

struct vnode_t {
    char path[VFS_MAX_PATH]; // solves problem of passing the relative path from vfs_open to the driver's open callback.
    enum vnode_type_t type;
    struct mount_t *mount;      /* which mount owns this */
    size_t size;       /* file size, 0 for dirs */
    void *prv;        /* fat_file_t* or kobj_t* */
};

/* mount table */
struct mount_t {
    char path[VFS_MAX_PATH];     // "/sd" or "/sys"
    struct fs_driver_t *driver;
    struct mount_t *next;
    void *prv;  // fat_fs_t* or NULL for sysfs
};

struct fs_driver_t {
    const char *name;
    int (*mount)  (struct mount_t *mp);
    int (*open)   (struct vnode_t *vn, int flags);
    int (*close)  (struct vnode_t *vn);
    int (*read)   (struct vnode_t *vn, void *buf, size_t size, size_t off);
    int (*write)  (struct vnode_t *vn, const void *buf, size_t size, size_t off);
    int (*lookup) (struct vnode_t *dir, const char *name, struct vnode_t **out);
    int (*mkdir)  (struct mount_t *mp, const char *path);
    int (*readdir)(struct vnode_t *dir, int idx, char *name_out);
};

/* visible only to rootfs */
int vfs_mount_count(void);
struct mount_t *vfs_mount_get(int idx);

int vfs_mount(const char *path, struct fs_driver_t *driver, void *prv);
int vfs_open(const char *path, struct vnode_t *vn, int flags);
int vfs_close(struct vnode_t *vn);
int vfs_read(struct vnode_t *vn, void *buf, size_t size, size_t off);
int vfs_write(struct vnode_t *vn, const void *buf, size_t size, size_t off);
int vfs_mkdir(const char *path);
int vfs_readdir(struct vnode_t *vn, int idx, char *name_out);
int vfs_isdir(const char *path);


/* put all fs.h here */
#include <sysfs.h>
#include <rootfs.h>
#include <fat.h>

#ifdef __cplusplus
}
#endif

#endif /* __VFS_H__ */