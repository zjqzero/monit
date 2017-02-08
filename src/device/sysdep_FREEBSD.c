/*
 * Copyright (C) Tildeslash Ltd. All rights reserved.
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU Affero General Public License version 3.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU Affero General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 *
 * In addition, as a special exception, the copyright holders give
 * permission to link the code of portions of this program with the
 * OpenSSL library under certain conditions as described in each
 * individual source file, and distribute linked combinations
 * including the two.
 *
 * You must obey the GNU Affero General Public License in all respects
 * for all of the code used other than OpenSSL.
 */

/**
 *  System dependent filesystem methods.
 *
 *  @file
 */

#include "config.h"

#ifdef HAVE_STDIO_H
#include <stdio.h>
#endif

#ifdef HAVE_ERRNO_H
#include <errno.h>
#endif

#ifdef HAVE_STRING_H
#include <string.h>
#endif

#ifdef HAVE_SYS_PARAM_H
#include <sys/param.h>
#endif

#if defined HAVE_SYS_UCRED_H
#include <sys/ucred.h>
#endif

#ifdef HAVE_SYS_MOUNT_H
#include <sys/mount.h>
#endif

#ifdef HAVE_FCNTL_H
#include <fcntl.h>
#endif

#ifdef HAVE_CTYPE_H
#include <ctype.h>
#endif

#ifdef HAVE_KVM_H
#include <kvm.h>
#endif

#ifdef HAVE_PATHS_H
#include <paths.h>
#endif

#ifdef HAVE_DEVSTAT_H
#include <devstat.h>
#endif

#include "monit.h"

// libmonit
#include "system/Time.h"
#include "io/File.h"


/* ------------------------------------------------------------- Definitions */


static struct {
        uint64_t timestamp;
        struct statinfo disk;
} _statistics = {};


/* --------------------------------------- Static constructor and destructor */


static void __attribute__ ((constructor)) _constructor() {
        _statistics.disk.dinfo = CALLOC(1, sizeof(struct devinfo));
}


static void __attribute__ ((destructor)) _destructor() {
        FREE(_statistics.disk.dinfo);
}


/* ----------------------------------------------------------------- Private */


static uint64_t _bintimeToMilli(struct bintime *time) {
        return time->sec * 1000 + (((uint64_t)1000 * (uint32_t)(time->frac >> 32)) >> 32);
}


// Parse the device path like /dev/da0p2 into name:instance -> da:0
static boolean_t _parseDevice(const char *path, Device_T device) {
        const char *base = File_basename(path);
        for (int i = 0; base[i]; i++) {
                if (isdigit(*(base + i))) {
                        strncpy(device->key, base, i < sizeof(device->key) ? i : sizeof(device->key) - 1);
                        device->instance = Str_parseInt(base + i);
                        return true;
                }
        }
        LogError("filesystem statistics error -- cannot parse device '%s'\n", path);
        return false;
}


static boolean_t _getStatistics(uint64_t now) {
        // Refresh only if the statistics are older then 1 second (handle also backward time jumps)
        if (now > _statistics.timestamp + 1000 || now < _statistics.timestamp - 1000) {
                if (devstat_getdevs(NULL, &(_statistics.disk)) == -1) {
                        LogError("filesystem statistics error -- devstat_getdevs: %s\n", devstat_errbuf);
                        return false;
                }
                _statistics.timestamp = now;
        }
        return true;
}


static boolean_t _getDummyDiskActivity(void *_inf) {
        return true;
}


static boolean_t _getBlockDiskActivity(void *_inf) {
        Info_T inf = _inf;
        uint64_t now = Time_milli();
        boolean_t rv = _getStatistics(now);
        if (rv) {
                for (int i = 0; i < _statistics.disk.dinfo->numdevs; i++) {
                        if (_statistics.disk.dinfo->devices[i].unit_number == inf->priv.filesystem.object.instance && IS(_statistics.disk.dinfo->devices[i].device_name, inf->priv.filesystem.object.key)) {
                                uint64_t now = _statistics.disk.snap_time * 1000;
                                Statistics_update(&(inf->priv.filesystem.read.time), now, _bintimeToMilli(&(_statistics.disk.dinfo->devices[i].duration[DEVSTAT_READ])));
                                Statistics_update(&(inf->priv.filesystem.read.bytes), now, _statistics.disk.dinfo->devices[i].bytes[DEVSTAT_READ]);
                                Statistics_update(&(inf->priv.filesystem.read.operations),  now, _statistics.disk.dinfo->devices[i].operations[DEVSTAT_READ]);
                                Statistics_update(&(inf->priv.filesystem.write.time), now, _bintimeToMilli(&(_statistics.disk.dinfo->devices[i].duration[DEVSTAT_WRITE])));
                                Statistics_update(&(inf->priv.filesystem.write.bytes), now, _statistics.disk.dinfo->devices[i].bytes[DEVSTAT_WRITE]);
                                Statistics_update(&(inf->priv.filesystem.write.operations), now, _statistics.disk.dinfo->devices[i].operations[DEVSTAT_WRITE]);
                                break;
                        }
                }
        }
        return rv;
}


static boolean_t _getDiskUsage(void *_inf) {
        Info_T inf = _inf;
        struct statfs usage;
        if (statfs(inf->priv.filesystem.object.mountpoint, &usage) != 0) {
                LogError("Error getting usage statistics for filesystem '%s' -- %s\n", inf->priv.filesystem.object.mountpoint, STRERROR);
                return false;
        }
        inf->priv.filesystem.f_bsize = usage.f_bsize;
        inf->priv.filesystem.f_blocks = usage.f_blocks;
        inf->priv.filesystem.f_blocksfree = usage.f_bavail;
        inf->priv.filesystem.f_blocksfreetotal = usage.f_bfree;
        inf->priv.filesystem.f_files = usage.f_files;
        inf->priv.filesystem.f_filesfree = usage.f_ffree;
        inf->priv.filesystem._flags = inf->priv.filesystem.flags;
        inf->priv.filesystem.flags = usage.f_flags;
        return true;
}


static boolean_t _compareMountpoint(const char *mountpoint, struct statfs *mnt) {
        return IS(mountpoint, mnt->f_mntonname);
}


static boolean_t _compareDevice(const char *device, struct statfs *mnt) {
        return IS(device, mnt->f_mntfromname);
}


static boolean_t _setDevice(Info_T inf, const char *path, boolean_t (*compare)(const char *path, struct statfs *mnt)) {
        int countfs = getfsstat(NULL, 0, MNT_NOWAIT);
        if (countfs != -1) {
                struct statfs *mnt = CALLOC(countfs, sizeof(struct statfs));
                if ((countfs = getfsstat(mnt, countfs * sizeof(struct statfs), MNT_NOWAIT)) != -1) {
                        for (int i = 0; i < countfs; i++) {
                                struct statfs *mntItem = mnt + i;
                                if (compare(path, mntItem)) {
                                        if (IS(mntItem->f_fstypename, "ufs")) {
                                                inf->priv.filesystem.object.getDiskActivity = _getBlockDiskActivity;
                                                if (! _parseDevice(mntItem->f_mntfromname, &(inf->priv.filesystem.object))) {
                                                        goto error;
                                                }
                                        } else {
                                                //FIXME: can add ZFS support (see sysdep_SOLARIS.c), but libzfs headers are not installed on FreeBSD by default (part of "cddl" set)
                                                inf->priv.filesystem.object.getDiskActivity = _getDummyDiskActivity;
                                        }
                                        strncpy(inf->priv.filesystem.object.device, mntItem->f_mntfromname, sizeof(inf->priv.filesystem.object.device) - 1);
                                        strncpy(inf->priv.filesystem.object.mountpoint, mntItem->f_mntonname, sizeof(inf->priv.filesystem.object.mountpoint) - 1);
                                        strncpy(inf->priv.filesystem.object.type, mntItem->f_fstypename, sizeof(inf->priv.filesystem.object.type) - 1);
                                        inf->priv.filesystem.object.getDiskUsage = _getDiskUsage;
                                        inf->priv.filesystem.object.mounted = true;
                                        FREE(mnt);
                                        return true;
                                }
                        }
                }
                FREE(mnt);
        }
        LogError("Lookup for '%s' filesystem failed\n", path);
error:
        inf->priv.filesystem.object.mounted = false;
        return false;
}


static boolean_t _getDevice(Info_T inf, const char *path, boolean_t (*compare)(const char *path, struct statfs *mnt)) {
        if (_setDevice(inf, path, compare)) {
                return (inf->priv.filesystem.object.getDiskUsage(inf) && inf->priv.filesystem.object.getDiskActivity(inf));
        }
        return false;
}


/* ------------------------------------------------------------------ Public */


boolean_t Filesystem_getByMountpoint(Info_T inf, const char *path) {
        ASSERT(inf);
        ASSERT(path);
        return _getDevice(inf, path, _compareMountpoint);
}


boolean_t Filesystem_getByDevice(Info_T inf, const char *path) {
        ASSERT(inf);
        ASSERT(path);
        return _getDevice(inf, path, _compareDevice);
}

