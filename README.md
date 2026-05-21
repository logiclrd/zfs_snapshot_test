# `zfs_snapshot_test`

This code snippet solves a particular problem that came up during the development of DQD.RealTimeBackup. The service needs to be able to determine whether a path reported by monitoring functions is inside a ZFS snapshot or not. It is not immediately obvious or clearly documented how to do this, but I have come up with the following methodology, following experimentation and incomplete suggestions from Claude AI.

All paths that are contained in a ZFS snapshot will contain the components `/.zfs/snapshot/`. This, however, is not enough to detect with certainty that the path actually _is_ contained in a snapshot. For instance, the user could simply create directories by these names. But, with some additional checks, it is possible to eliminate the edge cases and come up with a confident result.

1. The path might be a relative path. The rest of the processing depends upon it being a full path. The system call `realpath` can expand a relative path to a full absolute path, collapsing `.` and `..` components and duplicate `/` characters along the way.

2. The path might refer to a symbolic link. Unfortunately, `realpath` always follows symbolic links, so if you want to be able to check paths to symlinks themselves (I do), then the `realpath` logic must be reimplemented. This is annoying, but is a SMOP. I haven't done this in this proof of concept.

3. Paths which do not contain the substring `/.zfs/snapshot/` require no further processing. They are not contained in ZFS snapshots.

4. The prefix leading up to this substring must be a ZFS mount point. The `zfs_path_to_zhandle` function, in its implementation, uses a function `getextmntent` which is provided on Linux by libspl. I use the same function and then check the `mnt_fstype` and `mnt_mountp` members, since `getextmntent` will find the mount point also for child paths contained within the mount point.

5. The `stat` function returns a value `st_dev` that identifies the underlying _device_ containing the path. The `st_dev` values for the mount point, `./.zfs` and `./.zfs/snapshot` within the mount point should all be the same. But, the `st_dev` value for `./.zfs/snapshot/SnapshotName` (including the first path component within the tail following the `/.zfs/snapshot/` components) within the mount point should differ.

I don't believe it is possible for a path to satisfy these criteria and not be contained within a ZFS snapshot.

## Requirements

Ubuntu package: `libzfslinux-dev`

I don't know about other distributions. :-)

## Usage

```
$ make
$ zfs list | grep myzfs
mypool/myzfs  23.3G  44.0G  23.3G  /
$ sudo zfs snapshot mypool/myzfs@hello
$ ./probe `realpath probe` /.zfs/snapshot/hello/code/zfs_snapshot_test/probe
/code/zfs_snapshot_test/probe: not in snapshot
/.zfs/snapshot/hello/code/zfs_snapshot_test/probe: in snapshot
$
```
