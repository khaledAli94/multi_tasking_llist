/* vfs.c */
#include <vfs.h>
#include <string.h>
#include <stdlib.h>

static struct mount_t mounts[MAX_MOUNTS];
static int mount_count = 0;

/* ------------------------------------------------------------------ */
/* find best mount point — longest prefix match                         */
/* ------------------------------------------------------------------ */

static struct mount_t *find_mount(const char *path)
{
    struct mount_t *best = NULL;
    int best_len = -1;
    int i;

    for (i = 0; i < mount_count; i++) {
        int mlen = (int)strlen(mounts[i].path);
        if (strncmp(path, mounts[i].path, mlen) == 0) {
            if (path[mlen] == '\0' || path[mlen] == '/') {
                if (mlen > best_len) {
                    best_len = mlen;
                    best = &mounts[i];
                }
            }
        }
    }

    return best;
}

/* ------------------------------------------------------------------ */
/* strip mount prefix from path                                         */
/* "/sd/foo" with mount "/sd" → "/foo"                                  */
/* "/sd"     with mount "/sd" → "/"                                     */
/* ------------------------------------------------------------------ */

static const char *strip_mount(const char *path, struct mount_t *mp)
{
    int mlen = (int)strlen(mp->path);
    const char *rel = path + mlen;
    if (*rel == '\0')
        return "/";
    return rel;
}

int vfs_mount_count(void)
{
    return mount_count;
}

struct mount_t *vfs_mount_get(int idx)
{
    if (idx < 0 || idx >= mount_count)
        return NULL;
    return &mounts[idx];
}

int vfs_mount(const char *path, struct fs_driver_t *driver, void *prv)
{
    if (mount_count >= MAX_MOUNTS)
        return -1;

    struct mount_t *mp = &mounts[mount_count++];
    strncpy(mp->path, path, VFS_MAX_PATH - 1);
    mp->path[VFS_MAX_PATH - 1] = '\0';
    mp->driver = driver;
    mp->prv    = prv;
    mp->next   = NULL;

    if (driver->mount)
        return driver->mount(mp);

    return 0;
}

int vfs_open(const char *path, struct vnode_t *vn, int flags)
{
    struct mount_t *mp = find_mount(path);
    if (!mp)
        return -1;

    const char *rel = strip_mount(path, mp);

    vn->mount = mp;
    vn->size  = 0;
    vn->type  = VNODE_FILE;
    vn->prv   = NULL;

    strncpy(vn->path, rel, sizeof(vn->path) - 1);
    vn->path[sizeof(vn->path) - 1] = '\0';

    return mp->driver->open(vn, flags);
}

int vfs_close(struct vnode_t *vn)
{
    if (!vn || !vn->mount)
        return -1;
    return vn->mount->driver->close(vn);
}

int vfs_read(struct vnode_t *vn, void *buf, size_t size, size_t off)
{
    if (!vn || !vn->mount)
        return -1;
    return vn->mount->driver->read(vn, buf, size, off);
}

int vfs_write(struct vnode_t *vn, const void *buf, size_t size, size_t off)
{
    if (!vn || !vn->mount)
        return -1;
    return vn->mount->driver->write(vn, buf, size, off);
}

int vfs_mkdir(const char *path)
{
    struct mount_t *mp = find_mount(path);
    if (!mp)
        return -1;

    if (!mp->driver->mkdir)
        return -1;

    const char *rel = strip_mount(path, mp);
    return mp->driver->mkdir(mp, rel);
}

int vfs_readdir(struct vnode_t *vn, int idx, char *name_out)
{
    if (!vn || !vn->mount)
        return -1;
    return vn->mount->driver->readdir(vn, idx, name_out);
}

int vfs_isdir(const char *path)
{
    struct vnode_t vn;
    if (vfs_open(path, &vn, 0) < 0)
        return 0;
    int r = (vn.type == VNODE_DIR);
    vfs_close(&vn);
    return r;
}