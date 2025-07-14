/*
 * Copyright (c) 2025 Miroslav Cimerman
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * - Redistributions of source code must retain the above copyright
 *   notice, this list of conditions and the following disclaimer.
 * - Redistributions in binary form must reproduce the above copyright
 *   notice, this list of conditions and the following disclaimer in the
 *   documentation and/or other materials provided with the distribution.
 * - The name of the author may not be used to endorse or promote products
 *   derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include <block.h>
#include <byteorder.h>
#include <errno.h>
#include <getopt.h>
#include <mem.h>
#include <stdlib.h>
#include <stdio.h>
#include <stdatomic.h>
#include <str.h>
#include <abi/ipc/ipc.h>
#include <perf.h>
#include <vfs/vfs.h>
#include <str_error.h>
#include <stdint.h>

#define NAME "iotester"

static void usage(void);

static const char usage_str[] =
    "iotester: I/O performance testing utilitily.\n"
    "Usage: " NAME " -t type -f file -d dev -T seconds\n"
    "\n"
    "All options are mandatory:\n"
    "  -t    I/O type: r, w\n"
    "  -f    Input file\n"
    "  -d    Target device\n"
    "  -T    Seconds after which to interrupt workers (0 means do not int.)\n";

static struct option const long_options[] = {
	{ 0, 0, 0, 0 }
};

static void usage(void)
{
	printf("%s", usage_str);
}

enum {
	M_IOPS,
	M_LAT,
	M_TPUT,
};

typedef struct {
	uint64_t ba;
	uint64_t len;
} io_range_t;

typedef struct {
	void *buf;
	io_range_t *ranges;
	size_t range_cnt;
	size_t bsize;

	service_id_t svc_id;
	struct {
		uint64_t bytes_processed;
		uint64_t io_ops_done;
		double avg_lat;
	} result;
	atomic_bool stop;
	bool write;
} worker_data_t;

bool can_start = false;
atomic_bool stop = false;
long long timeout = -1;
size_t done = 0;
fibril_mutex_t mtx;
fibril_condvar_t cv;

static errno_t timeout_worker(void *arg)
{
	(void)arg;

	fibril_sleep(timeout);
	atomic_store_explicit(&stop, true, memory_order_relaxed);

	return EOK;
}

static errno_t bench_worker(void *arg)
{
	errno_t rc;
	worker_data_t *wd = arg;

	fibril_mutex_lock(&mtx);
	while (!can_start)
		fibril_condvar_wait(&cv, &mtx);
	fibril_mutex_unlock(&mtx);

	nsec_t lats = 0;
	uint64_t processed = 0;
	uint64_t io = 0;
	for (size_t i = 0; i < wd->range_cnt; i++) {
		stopwatch_t stopwatch;
		stopwatch_init(&stopwatch);
		stopwatch_start(&stopwatch);

		if (stop)
			break;
		if (wd->write) {
			rc = block_write_direct(wd->svc_id, wd->ranges[i].ba,
			    wd->ranges[i].len, wd->buf);
		} else {
			rc = block_read_direct(wd->svc_id, wd->ranges[i].ba,
			    wd->ranges[i].len, wd->buf);
		}
		assert(rc == EOK);

		stopwatch_stop(&stopwatch);

		lats += stopwatch_get_nanos(&stopwatch);
		processed += wd->ranges[i].len * wd->bsize;
		io++;
	}

	/* average latency in us */
	wd->result.avg_lat = NSEC2USEC((double)lats) / io;
	wd->result.bytes_processed = processed;
	wd->result.io_ops_done = io;

	fibril_mutex_lock(&mtx);
	done++;
	fibril_mutex_unlock(&mtx);

	fibril_condvar_signal(&cv);

	return EOK;
}

int main(int argc, char **argv)
{
	errno_t rc;
	int c;
	int type = -1;
	char *file = NULL;
	char *devname = NULL;
	service_id_t svc_id;
	size_t bsize;
	int fd;
	size_t nread;
#ifdef __SAVE__OUT__
	int wfd;
	size_t nwrite;
#endif
	uint64_t pos = 0;
	worker_data_t *worker_data = NULL;
	io_range_t *io_ranges = NULL;
	char *msg_buf = NULL;

	if (argc != 9) {
		usage();
		return 1;
	}

	atomic_init(&stop, false);
	fibril_mutex_initialize(&mtx);
	fibril_condvar_initialize(&cv);

	c = 0;
	optreset = 1;
	optind = 0;

	while (c != -1) {
		c = getopt_long(argc, argv, "t:f:d:T:", long_options, NULL);
		switch (c) {
		case 't':
			if (str_cmp(optarg, "r") == 0)
				type = 0;
			else if (str_cmp(optarg, "w") == 0)
				type = 1;
			break;
		case 'f':
			file = optarg;
			break;
		case 'd':
			devname = optarg;
			break;
		case 'T':
			timeout = strtol(optarg, NULL, 10);
			break;
		case '?':
			usage();
			return 1;
		}
	}

	if (type == -1 || !file || !devname || timeout < 0) {
		usage();
		return 1;
	}

	rc = loc_service_get_id(devname, &svc_id, 0);
	if (rc != EOK) {
		printf("bdwrite: error resolving device \"%s\"\n", devname);
		return 1;
	}

	rc = block_init(svc_id);
	if (rc != EOK) {
		printf("bdwrite: error initializing block device \"%s\"\n", devname);
		return 1;
	}

	rc = block_get_bsize(svc_id, &bsize);
	if (rc != EOK) {
		printf("bdwrite: error getting block size of \"%s\"\n", devname);
		block_fini(svc_id);
		return 1;
	}

	rc = vfs_lookup_open(file, WALK_REGULAR, MODE_READ, &fd);
	if (rc != EOK) {
		printf("open(): '%s': %s\n", file, str_error(rc));
		return 1;
	}

	uint64_t concurrency;
	rc = vfs_read(fd, &pos, &concurrency, sizeof(uint64_t), &nread);
	if (rc != EOK || nread != sizeof(uint64_t))
		goto error;
	concurrency = uint64_t_le2host(concurrency);

	uint64_t io_per_fibril;
	rc = vfs_read(fd, &pos, &io_per_fibril, sizeof(uint64_t), &nread);
	if (rc != EOK || nread != sizeof(uint64_t))
		goto error;
	io_per_fibril = uint64_t_le2host(io_per_fibril);

	uint64_t max_xfer;
	rc = vfs_read(fd, &pos, &max_xfer, sizeof(uint64_t), &nread);
	if (rc != EOK || nread != sizeof(uint64_t))
		goto error;
	max_xfer = uint64_t_le2host(max_xfer);

	if (max_xfer != DATA_XFER_LIMIT) {
		printf("maximum transfer size specified = %" PRIu64 ",\n",
		    max_xfer);
		printf("!= DATA_XFER_SIZE (%ju),\n",
		    (uintmax_t)DATA_XFER_LIMIT);
		rc = EINVAL;
		goto error;
	}

	io_ranges = malloc(concurrency * io_per_fibril * sizeof(*io_ranges));
	if (io_ranges == NULL) {
		rc = ENOMEM;
		goto error;
	}

	for (size_t i = 0; i < concurrency * io_per_fibril; i++) {
		uint64_t val;
		rc = vfs_read(fd, &pos, &val, sizeof(uint64_t), &nread);
		if (rc != EOK || nread != sizeof(uint64_t))
			goto error;
		val = uint64_t_le2host(val);

		io_ranges[i].ba = val;

		rc = vfs_read(fd, &pos, &val, sizeof(uint64_t), &nread);
		if (rc != EOK || nread != sizeof(uint64_t))
			goto error;
		val = uint64_t_le2host(val);

		io_ranges[i].len = val;
	}

	worker_data = calloc(1, concurrency * sizeof(*worker_data));
	if (worker_data == NULL) {
		rc = ENOMEM;
		goto error;
	}

	for (size_t c = 0; c < concurrency; c++) {
		worker_data[c].range_cnt = io_per_fibril;
		worker_data[c].ranges = &io_ranges[c * io_per_fibril];
		worker_data[c].bsize = bsize;
		worker_data[c].write = type;
		worker_data[c].svc_id = svc_id;
		worker_data[c].buf = calloc(1, DATA_XFER_LIMIT);
		if (worker_data[c].buf == NULL) {
			rc = ENOMEM;
			goto error;
		}
	}

	printf("spawning workers\n");
	for (size_t c = 0; c < concurrency; c++) {
		fid_t f = 0;
		f = fibril_create(&bench_worker, &worker_data[c]);
		assert(f);
		fibril_start(f);
		fibril_detach(f);
	}

	fibril_sleep(1);

	stopwatch_t stopwatch;
	stopwatch_init(&stopwatch);
	stopwatch_start(&stopwatch);

	fibril_mutex_lock(&mtx);
	can_start = true;
	fibril_mutex_unlock(&mtx);
	fibril_condvar_broadcast(&cv);

	if (timeout > 0) {
		fid_t f = fibril_create(&timeout_worker, NULL);
		assert(f);
		fibril_start(f);
		fibril_detach(f);
	}

	fibril_mutex_lock(&mtx);
	while (done < concurrency)
		fibril_condvar_wait(&cv, &mtx);
	fibril_mutex_unlock(&mtx);

	stopwatch_stop(&stopwatch);
	nsec_t t = stopwatch_get_nanos(&stopwatch);

	int msg_buf_sz = 1024 * 16;
	int off = 0;
	int p_w = 0;
	msg_buf = calloc(1, msg_buf_sz);
	if (msg_buf == NULL) {
		rc = ENOMEM;
		goto error;
	}

	p_w = snprintf(msg_buf + off, msg_buf_sz - off,
	    "Results of %s for %s\n", file, devname);
	if (p_w < 0 || p_w >= (int)(msg_buf_sz - off)) {
		printf("error writing final msg\n");
		rc = ENOMEM;
		goto error;
	}
	off += p_w;

	uint64_t p = 0;
	uint64_t ios = 0;
	double avg = 0;
	for (size_t c = 0; c < concurrency; c++) {
		p += worker_data[c].result.bytes_processed;
		ios += worker_data[c].result.io_ops_done;
		avg += worker_data[c].result.avg_lat;
	}

	p_w = snprintf(msg_buf + off, msg_buf_sz - off,
	    "tput: %lf KB/s\n", ((double)p * 1000000) / t);
	if (p_w < 0 || p_w >= (int)(msg_buf_sz - off)) {
		printf("error writing final msg\n");
		rc = ENOMEM;
		goto error;
	}
	off += p_w;

	p_w = snprintf(msg_buf + off, msg_buf_sz - off,
	    "IOPS: %lf IOPS\n", (double)(ios * 1000000000) / t);
	if (p_w < 0 || p_w >= (int)(msg_buf_sz - off)) {
		printf("error writing final msg\n");
		rc = ENOMEM;
		goto error;
	}
	off += p_w;

	avg /= concurrency;
	p_w = snprintf(msg_buf + off, msg_buf_sz - off,
	    "latency: %lf ms (%lf s)\n", USEC2MSEC(avg), (USEC2SEC(avg)));
	if (p_w < 0 || p_w >= (int)(msg_buf_sz - off)) {
		printf("error writing final msg\n");
		rc = ENOMEM;
		goto error;
	}
	off += p_w;

	printf("%s", msg_buf);

	/*
	 * Nasty parsing for saving test results of specific volume name
	 * formats to files. Was used just to export results to the host.
	 */
#ifdef __SAVE__OUT__
	char *testname = file;
	while (*testname != '_')
		testname++;

	char *devname_used = devname;

	char devname_single[20] = { 0 };
	if (devname_used[0] != 'h') {
		while (*devname_used != '0')
			devname_used++;
		for (size_t i = 0; i < 7; i++)
			devname_single[i] = devname_used[i];
		devname_used = devname_single;
	}

	char tt = 'r';
	if (type == 1)
		tt = 'w';
	char wfile[256] = { 0 };
	snprintf(wfile, sizeof(wfile), "_TESTRESULT_%c_%s_%s", tt, testname,
	    devname_used);
	/* printf("wfile: %s\n", wfile); */

	rc = vfs_lookup_open(wfile,
	    WALK_REGULAR | WALK_MUST_CREATE, MODE_WRITE, &wfd);
	if (rc != EOK) {
		printf("failed vfs lookup for wfile\n");
		goto error;
	}

	size_t s = str_size(msg_buf);

	pos = 0;
	rc = vfs_write(wfd, &pos, msg_buf, s + 1, &nwrite);
	if (rc != EOK) {
		printf("failed vfs write\n");
		goto error;
	}
#endif

error:
	if (worker_data != NULL) {
		for (size_t c = 0; c < concurrency; c++) {
			if (worker_data[c].buf)
				free(worker_data[c].buf);
		}
	}
#ifdef __SAVE__OUT__
	vfs_put(wfd);
#endif
	free(msg_buf);
	free(io_ranges);
	free(worker_data);
	vfs_put(fd);
	block_fini(svc_id);
	return rc;
}

/** @}
 */
