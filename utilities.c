#define _GNU_SOURCE             /* See feature_test_macros(7) */
#include <sched.h>
#include <sys/mount.h>
#include <unistd.h>
#include <sys/stat.h>

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <stdbool.h>
#include <errno.h>

#include <linux/limits.h>
#include <sys/statvfs.h>
#include <fcntl.h>

#include "utilitis.h"

#define NETNS_RUN_DIR "/var/run/netns"
#define PROC_NS_PATH "/proc/self/ns/"

// static int saved_netns = -1;
// static int saved_mntns = -1;

struct {
	int ns_fd;
	int type_of_namespace;
	char path[PATH_MAX];

} ns_points[] = {
	{-1, .type_of_namespace = CLONE_NEWNET, .path = PROC_NS_PATH"net"},
	{-1, .type_of_namespace = CLONE_NEWNS, .path = PROC_NS_PATH"mnt"},
};

#define ARRAY_SIZE(x) (sizeof(x) / sizeof((x)[0]))

void show_current_netns_id(void)
{
	char buff[256]; buff[0] = '\0';
	char * end = buff;

	for (unsigned i = 0; i < ARRAY_SIZE(ns_points); i++)
	{
		if (ns_points[i].ns_fd == -1)
			continue;

		int writed_bytes = readlinkat(ns_points[i].ns_fd, ns_points[i].path, end, sizeof(buff));
		if (writed_bytes == -1)
			abort();

		end += writed_bytes + 1; end[0] = '|'; end++;
	}

	printf("'%s'\n", buff);
}

int create_netns_dir(void)
{
	/* Create the base netns directory if it doesn't exist */
	if (mkdir(NETNS_RUN_DIR, S_IRWXU|S_IRGRP|S_IXGRP|S_IROTH|S_IXOTH)) {
		if (errno != EEXIST) {
			fprintf(stderr, "mkdir %s failed: %s\n",
				NETNS_RUN_DIR, strerror(errno));
			return -1;
		}
	}

	return 0;
}

void ns_save(void)
{
	for (unsigned i = 0; i < ARRAY_SIZE(ns_points); i++)
	{
		if (ns_points[i].ns_fd != -1)
			continue;

		ns_points[i].ns_fd =  open(ns_points[i].path, O_RDONLY | O_CLOEXEC);
		if (ns_points[i].ns_fd == -1)
		{
			perror("Cannot open init namespace");
			abort();
		}
	}
}

void ns_restore(void)
{
	for (unsigned i = 0; i < ARRAY_SIZE(ns_points); i++)
	{
		if (ns_points[i].ns_fd == -1)
			continue;

		if (setns(ns_points[i].ns_fd, ns_points[i].type_of_namespace))
		{
			perror("setns");
			abort();
		}

		close(ns_points[i].ns_fd);
		ns_points[i].ns_fd = -1;
	}
}

int on_netns_del(const char *nsname)
{
	char netns_path[PATH_MAX];

	snprintf(netns_path, sizeof(netns_path), "%s/%s", NETNS_RUN_DIR, nsname);
	umount2(netns_path, MNT_DETACH);
	if (unlink(netns_path) < 0) {
		fprintf(stderr, "Cannot remove namespace file \"%s\": %s\n",
			netns_path, strerror(errno));
		return -1;
	}
	return 0;
}
