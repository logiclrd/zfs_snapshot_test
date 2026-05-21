#define _LARGEFILE64_SOURCE

#include <stdint.h>
#include <stdio.h>

#include <share.h>
#include <sys/stat.h>

#include <libzfs.h>

void show_path_type(
	libzfs_handle_t *hdl,
	char *path)
{
	/* This was suggested by Claude AI. It does not work. */

	zfs_handle_t *zh = zfs_path_to_zhandle(hdl, path, 0);

	printf("%s: ", path);

	if ((path[0] != '/') && ((path[0] != '.') && (path[1] != '/')))
		printf("not a valid path\n");
	else if (!zh)
		printf("not recognized (zfs_path_to_zhandle returned NULL)\n");
	else
	{
		zfs_type_t type = zfs_get_type(zh);

		switch (type)
		{
			case ZFS_TYPE_INVALID: printf("invalid\n"); break;
			case ZFS_TYPE_FILESYSTEM: printf("filesystem\n"); break;
			case ZFS_TYPE_SNAPSHOT: printf("snapshot\n"); break;
			case ZFS_TYPE_VOLUME: printf("volume\n"); break;
			case ZFS_TYPE_POOL: printf("pool\n"); break;
			case ZFS_TYPE_BOOKMARK: printf("bookmark\n"); break;
			case ZFS_TYPE_VDEV: printf("virtual device\n"); break;
			default: printf("unknown (%d)\n", type); break;
		}

		zfs_close(zh);
	}
}

int is_in_zfs_snapshot(
	libzfs_handle_t *hdl,
	char *path)
{
	/*
	 * If a path is in a snapshot, then:
	 *
	 * 1. It contains components `/.zfs/snapshot/`.
	 * 2. It has components after `snapshot`.
	 * 3. The prefix leading up to this is a ZFS mount point.
	 * 4. The `st_dev` stat field will match for `mountpoint`, `mountpoint/.zfs` and
	 *    `mountpoint/.zfs/snapshot`.
	 * 5. The `st_dev` stat field will differ for `mountpoint/.zfs/snapshot/snapshotname`.
	 */

#define SNAPSHOT_COMPONENTS "/.zfs/snapshot/"

	char real_path[PATH_MAX];

	if (!realpath(path, real_path))
	{
		if (path[0] != '/')
			return 0;

		strcpy(path, real_path);
	}

	if (real_path[0] != '/')
		return 0;

	char *scan = real_path;

	char *snapshot_components = strstr(scan, SNAPSHOT_COMPONENTS);

	while (snapshot_components != NULL)
	{
		// Criterion 1 satisfied.

		char *tail = snapshot_components + sizeof(SNAPSHOT_COMPONENTS) - 1;

		tail += strspn(tail, "/"); // skip extra separators

		if (tail[0] == '\0')
			continue;

		// Criterion 2 satisfied.
		
		char *mountpoint_candidate;

		if (snapshot_components == path)
			mountpoint_candidate = "/"; // root filesystem
		else
		{
			struct extmnttab mnt_entry;
			struct stat64 stat_buf;

			*snapshot_components = '\0'; // truncate after the mount point name, before `/.zfs/snapshot/...`.

			int result = getextmntent(real_path, &mnt_entry, &stat_buf);

			*snapshot_components = '/';

			if ((result == 0)
			 && (0 == strcmp(mnt_entry.mnt_fstype, "zfs"))
			 && (real_path + strlen(mnt_entry.mnt_mountp) == snapshot_components)
			 && (0 == strncmp(mnt_entry.mnt_mountp, real_path, snapshot_components - real_path)))
			{
				// Criterion 3 satisfied.

				dev_t mnt_dev;
				int head_components_dev_match = 1;
			
				for (int i=0; SNAPSHOT_COMPONENTS[i]; i++)
				{
					if (snapshot_components[i] == '/')
					{
						snapshot_components[i] = '\0';
						result = stat64(real_path, &stat_buf);
						snapshot_components[i] = '/';

						if (result != 0)
							break;

						if (i == 0)
							mnt_dev = stat_buf.st_dev;
						else if (stat_buf.st_dev != mnt_dev)
						{
							head_components_dev_match = 0;
							break;
						}
					}
				}

				if (head_components_dev_match)
				{
					// Criterion 4 satisfied.
					char *next_separator = strchr(tail, '/');

					if (next_separator)
						*next_separator = '\0';

					result = stat64(real_path, &stat_buf);

					if ((result == 0)
					 && (stat_buf.st_dev != mnt_dev))
					{
						// Criteria all satisfied!
						return 1;
					}
				}
			}
		}

		scan = snapshot_components + sizeof(SNAPSHOT_COMPONENTS) - 2;

		snapshot_components = strstr(scan, SNAPSHOT_COMPONENTS);
	}

#undef SNAPSHOT_COMPONENTS

	return 0;
}

int main(int argc, char *argv[])
{
	libzfs_handle_t *hdl = libzfs_init();

	for (int i=1; i < argc; i++)
	{
		char *path = argv[i];

		/* show_path_type(path); // Claude's suggestion, does not work */

		printf("%s: %s\n",
			path,
			is_in_zfs_snapshot(hdl, path) ? "in snapshot" : "not in snapshot");
	}
}

