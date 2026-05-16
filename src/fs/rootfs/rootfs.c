#include "rootfs.h"
#include <string.h>


/*  
rootfs solve one problem ,who owns '/' ?
*/

/* ------------------------------------------------------------------ */
/* helpers                                                              */
/* ------------------------------------------------------------------ */

/*
 * Is this mount a direct child of root?
 *
 *   "/sys"      → yes  (one slash, no inner slash)
 *   "/sd"       → yes
 *   "/sd/boot"  → no   (nested mount, not direct child)
 *   "/"         → no   (root itself)
 */
static int is_direct_child(const char *mpath)
{
    /* must start with '/' */
    if (mpath[0] != '/')
        return 0;

    /* skip root itself */
    if (mpath[1] == '\0')
        return 0;

    /* must have no second slash */
    const char *p = mpath + 1;
    while (*p) {
        if (*p == '/')
            return 0;
        p++;
    }

    return 1;
}

/* ------------------------------------------------------------------ */
/* driver callbacks                                                     */
/* ------------------------------------------------------------------ */

static int rootfs_mount(struct mount_t *mp)
{
    /* nothing to initialize */
    (void)mp;
    return 0;
}

static int rootfs_open(struct vnode_t *vn, int flags)
{
    (void)flags;

    /*
     * rootfs only owns "/".
     * Any sub-path under "/" that matches another mount point
     * will have been caught by find_mount before we are called.
     * So if we are called, path is "/" — always a directory.
     */
    vn->type = VNODE_DIR;
    vn->size = 0;
    vn->prv  = NULL;
    return 0;
}

static int rootfs_close(struct vnode_t *vn)
{
    /* nothing was allocated */
    vn->prv = NULL;
    return 0;
}

static int rootfs_read(struct vnode_t *vn, void *buf, size_t size, size_t off)
{
    /* rootfs  cannot mkdir (virtual) */
    (void)vn; (void)buf; (void)size; (void)off;
    return -1;
}

static int rootfs_write(struct vnode_t *vn, const void *buf, size_t size, size_t off)
{
    /* rootfs is read-only */
    (void)vn; (void)buf; (void)size; (void)off;
    return -1;
}

static int rootfs_mkdir(struct mount_t *mp, const char *path)
{
    (void)mp; (void)path;
    return -1;   /* rootfs is virtual, no real dirs */
}

/*
 * rootfs_readdir
 *
 * Returns the name of the nth direct child mount point.
 *
 * Example: mounts = { "/", "/sys", "/sd" }
 *   idx=0 → "sys"
 *   idx=1 → "sd"
 *   idx=2 → -1 (end)
 */
static int rootfs_readdir(struct vnode_t *vn, int idx, char *name_out)
{
    (void)vn;

    int visible = 0;
    int count   = vfs_mount_count();
    int i;


     for (i = 0; i < count; i++) {
        struct mount_t *mp = vfs_mount_get(i);
        if (!mp)
            break;

        if (!is_direct_child(mp->path))
            continue;

        if (visible == idx) {
            /* strip leading '/' — return just "sys" not "/sys" */
            strncpy(name_out, mp->path + 1, 255);
            name_out[255] = '\0';
            return 0;
        }

        visible++;
    }

    return -1;   /* idx out of range — end of directory */
}

static int rootfs_lookup(struct vnode_t *dir, const char *name, struct vnode_t **out)
{
    /*
     * VFS routes by path prefix already.
     * lookup is not needed for rootfs — return not found
     * and let the caller use vfs_open with the full path.
     */
    (void)dir; (void)name; (void)out;
    return -1;
}

struct fs_driver_t rootfs_driver = {
    .name    = "rootfs",
    .mount   = rootfs_mount,
    .open    = rootfs_open,
    .close   = rootfs_close,
    .read    = rootfs_read,
    .write   = rootfs_write,
    .lookup  = rootfs_lookup,
    .readdir = rootfs_readdir,
};