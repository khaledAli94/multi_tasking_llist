#include "rootfs.h"
#include <vfs.h>

/*
 * rootfs_init
 *
 * Call this FIRST before any other vfs_mount().
 * Mounts rootfs at "/" so VFS has a default owner for all paths.
 *
 * After this you can mount real filesystems:
 *   vfs_mount("/sys", &sysfs_driver, NULL);
 *   vfs_mount("/sd",  &fat_driver,   &sd_fat);
 *
 * Those will shadow "/" for their subtrees via longest-prefix match.
 */
void rootfs_init(void)
{
    vfs_mount("/", &rootfs_driver, NULL);
}