/*
 * File:   main.c
 * Author: tornem
 *
 * Created on 19 июня 2022 г., 21:26
 */

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

bool switch_namespace(const char * name)
{
	char net_path[PATH_MAX];
	int netns;
	unsigned long mountflags = 0;
	struct statvfs fsstat;

	snprintf(net_path, sizeof(net_path), "%s/%s", NETNS_RUN_DIR, name);
	netns = open(net_path, O_RDONLY | O_CLOEXEC);
	if (netns < 0) {
		fprintf(stderr, "Cannot open network namespace \"%s\": %s\n",
				name, strerror(errno));
		return false;
	}

	if (setns(netns, CLONE_NEWNET) < 0) {
		fprintf(stderr, "setting the network namespace \"%s\" failed: %s\n",
				name, strerror(errno));
		close(netns);
		return false;
	}
	close(netns);

	if (unshare(CLONE_NEWNS) < 0) {
		fprintf(stderr, "unshare failed: %s\n", strerror(errno));
		return false;
	}

	/* Don't let any mounts propagate back to the parent */
	if (mount("", "/", "none", MS_SLAVE | MS_REC, NULL)) {
		fprintf(stderr, "\"mount --make-rslave /\" failed: %s\n",
			strerror(errno));
		return false;
	}

	/* Mount a version of /sys that describes the network namespace */

	if (umount2("/sys", MNT_DETACH) < 0) {
			/* If this fails, perhaps there wasn't a sysfs instance mounted. Good. */
		if (statvfs("/sys", &fsstat) == 0) {
			/* We couldn't umount the sysfs, we'll attempt to overlay it.
			 * A read-only instance can't be shadowed with a read-write one. */
			if (fsstat.f_flag & ST_RDONLY)
					mountflags = MS_RDONLY;
		}
	}
	if (mount(name, "/sys", "sysfs", mountflags, NULL) < 0) {
			fprintf(stderr, "mount of /sys failed: %s\n",strerror(errno));
			return false;
	}

	return true;
}

bool create_ns(const char * name)
{
	char netns_path[PATH_MAX];
	snprintf(netns_path, sizeof(netns_path), NETNS_RUN_DIR"/%s", name);

	/* Create the filesystem state */
	int fd = open(netns_path, O_RDONLY|O_CREAT|O_EXCL, 0);
	if (fd < 0) {
		fprintf(stderr, "Cannot create namespace file \"%s\": %s\n",
			netns_path, strerror(errno));
		return (EXIT_FAILURE);
	}
	close(fd);

	ns_save();
	printf("	default ns is : ");
	show_current_netns_id();

	if (unshare(CLONE_NEWNS) < 0) {
		fprintf(stderr, "Failed to create a new network namespace \"%s\": %s\n",
			name, strerror(errno));
		goto out_delete;
	}

	printf("	after unshare ns is : ");
	show_current_netns_id();

	return true;

out_delete:
	ns_restore();
	on_netns_del(name);

	return false;
}

int main(/*int argc, char** argv*/ void) {

	printf("[STEP] Start...\n");

	printf("[STEP] Create netns dirrectory...\n");
	create_netns_dir();

	printf("[STEP] Create new namespace 'test'...\n");
	create_ns("test");

	printf("[STEP] Drop to default namespace...\n");
	ns_restore();
	on_netns_del("test");

	printf("	after drop to default ns is : ");
	ns_save();
	show_current_netns_id();
	ns_restore();

	return (EXIT_SUCCESS);
}

