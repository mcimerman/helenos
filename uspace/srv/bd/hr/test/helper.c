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
#include <errno.h>
#include <hr.h>
#include <pcut/pcut.h>
#include <rndgen.h>
#include <stdio.h>
#include <str.h>

#include "helper.h"

extern service_id_t disks[];
extern const char *const disknames[];
extern uint64_t min_blkno;
extern hr_t *hr_sess;

hr_config_t *get_cfg(const char *volname, hr_level_t level, size_t diskno,
    uint8_t vflags)
{
	hr_config_t *cfg = calloc(1, sizeof(*cfg));
	PCUT_ASSERT_NOT_NULL(cfg);

	str_cpy(cfg->devname, sizeof(cfg->devname), volname);
	cfg->level = level;
	cfg->dev_no = diskno;
	for (size_t i = 0; i < diskno; i++)
		cfg->devs[i] = disks[i];
	cfg->vol_flags = vflags;

	return cfg;
}

errno_t test_vol_blkno(const char *volname, hr_level_t level, size_t diskno,
    uint8_t vflags, uint64_t expected)
{
	errno_t rc;
	service_id_t vol_svc_id;

	hr_config_t *cfg = get_cfg(volname, level, diskno, vflags);
	PCUT_ASSERT_NOT_NULL(cfg);

	rc = hr_create(hr_sess, cfg);
	PCUT_ASSERT_ERRNO_VAL(EOK, rc);

	free(cfg);

	rc = loc_service_get_id(volname, &vol_svc_id, 0);
	PCUT_ASSERT_ERRNO_VAL(EOK, rc);

	rc = block_init(vol_svc_id);
	PCUT_ASSERT_ERRNO_VAL(EOK, rc);

	uint64_t vol_blkno;
	rc = block_get_nblocks(vol_svc_id, &vol_blkno);
	PCUT_ASSERT_ERRNO_VAL(EOK, rc);

	PCUT_ASSERT_UINT_EQUALS(expected, vol_blkno);

	block_fini(vol_svc_id);
	rc = hr_stop(hr_sess, volname);
	PCUT_ASSERT_ERRNO_VAL(EOK, rc);

	return EOK;
}

errno_t test_WO_RO(const char *volname, hr_level_t level, size_t diskno,
    uint8_t vflags, uint64_t writeno)
{
	errno_t rc;
	service_id_t vol_svc_id;

	hr_config_t *cfg = get_cfg(volname, level, diskno, vflags);
	PCUT_ASSERT_NOT_NULL(cfg);

	rc = hr_create(hr_sess, cfg);
	PCUT_ASSERT_ERRNO_VAL(EOK, rc);

	free(cfg);

	rc = loc_service_get_id(volname, &vol_svc_id, 0);
	PCUT_ASSERT_ERRNO_VAL(EOK, rc);

	rc = block_init(vol_svc_id);
	PCUT_ASSERT_ERRNO_VAL(EOK, rc);

	hr_vol_info_t info;
	rc = hr_get_vol_info(hr_sess, vol_svc_id, &info);
	PCUT_ASSERT_ERRNO_VAL(EOK, rc);
	PCUT_ASSERT_EQUALS(HR_VOL_OPTIMAL, info.state);

	size_t bsize;
	rc = block_get_bsize(vol_svc_id, &bsize);
	PCUT_ASSERT_ERRNO_VAL(EOK, rc);

	uint64_t vol_blkno;
	rc = block_get_nblocks(vol_svc_id, &vol_blkno);
	PCUT_ASSERT_ERRNO_VAL(EOK, rc);
	bool capa = vol_blkno > 0;
	PCUT_ASSERT_TRUE(capa);

	rndgen_t *rndgen;
	rc = rndgen_create(&rndgen);
	PCUT_ASSERT_ERRNO_VAL(EOK, rc);

	uint32_t *offsets = malloc(sizeof(*offsets) * writeno);
	PCUT_ASSERT_NOT_NULL(offsets);
	for (size_t i = 0; i < writeno; i++) {
		rc = rndgen_uint32(rndgen, &offsets[i]);
		PCUT_ASSERT_ERRNO_VAL(EOK, rc);
		offsets[i] %= vol_blkno;
	}

	uint8_t *block = calloc(1, bsize);
	PCUT_ASSERT_NOT_NULL(block);

	for (size_t i = 0; i < bsize; i++) {
		rc = rndgen_uint8(rndgen, &block[i]);
		PCUT_ASSERT_ERRNO_VAL(EOK, rc);
	}

	for (size_t i = 0; i < writeno; i++) {
		rc = block_write_direct(vol_svc_id, offsets[i], 1, block);
		PCUT_ASSERT_ERRNO_VAL(EOK, rc);
	}

	rc = hr_get_vol_info(hr_sess, vol_svc_id, &info);
	PCUT_ASSERT_ERRNO_VAL(EOK, rc);
	PCUT_ASSERT_EQUALS(HR_VOL_OPTIMAL, info.state);

	uint8_t *read_block = malloc(bsize);
	PCUT_ASSERT_NOT_NULL(block);

	for (size_t i = 0; i < writeno; i++) {
		rc = block_read_direct(vol_svc_id, offsets[i], 1, read_block);
		PCUT_ASSERT_ERRNO_VAL(EOK, rc);
		for (size_t b = 0; b < bsize; b++)
			PCUT_ASSERT_UINT_EQUALS(block[b], read_block[b]);
	}

	rc = hr_get_vol_info(hr_sess, vol_svc_id, &info);
	PCUT_ASSERT_ERRNO_VAL(EOK, rc);
	PCUT_ASSERT_EQUALS(HR_VOL_OPTIMAL, info.state);

	free(offsets);
	rndgen_destroy(rndgen);
	free(block);
	block_fini(vol_svc_id);
	rc = hr_stop(hr_sess, volname);
	PCUT_ASSERT_ERRNO_VAL(EOK, rc);

	return EOK;
}

errno_t test_WO_RD(const char *volname, hr_level_t level, size_t diskno,
    uint8_t vflags, uint64_t writeno, size_t failno)
{
	errno_t rc;
	service_id_t vol_svc_id;

	hr_config_t *cfg = get_cfg(volname, level, diskno, vflags);
	PCUT_ASSERT_NOT_NULL(cfg);

	rc = hr_create(hr_sess, cfg);
	PCUT_ASSERT_ERRNO_VAL(EOK, rc);

	free(cfg);

	rc = loc_service_get_id(volname, &vol_svc_id, 0);
	PCUT_ASSERT_ERRNO_VAL(EOK, rc);

	rc = block_init(vol_svc_id);
	PCUT_ASSERT_ERRNO_VAL(EOK, rc);

	hr_vol_info_t info;
	rc = hr_get_vol_info(hr_sess, vol_svc_id, &info);
	PCUT_ASSERT_ERRNO_VAL(EOK, rc);
	PCUT_ASSERT_EQUALS(HR_VOL_OPTIMAL, info.state);

	size_t bsize;
	rc = block_get_bsize(vol_svc_id, &bsize);
	PCUT_ASSERT_ERRNO_VAL(EOK, rc);

	uint64_t vol_blkno;
	rc = block_get_nblocks(vol_svc_id, &vol_blkno);
	PCUT_ASSERT_ERRNO_VAL(EOK, rc);
	bool capa = vol_blkno > 0;
	PCUT_ASSERT_TRUE(capa);

	rndgen_t *rndgen;
	rc = rndgen_create(&rndgen);
	PCUT_ASSERT_ERRNO_VAL(EOK, rc);

	uint32_t *offsets = malloc(sizeof(*offsets) * writeno);
	PCUT_ASSERT_NOT_NULL(offsets);
	for (size_t i = 0; i < writeno; i++) {
		rc = rndgen_uint32(rndgen, &offsets[i]);
		PCUT_ASSERT_ERRNO_VAL(EOK, rc);
		offsets[i] %= vol_blkno;
	}

	uint8_t *block = calloc(1, bsize);
	PCUT_ASSERT_NOT_NULL(block);
	for (size_t i = 0; i < bsize; i++) {
		rc = rndgen_uint8(rndgen, &block[i]);
		PCUT_ASSERT_ERRNO_VAL(EOK, rc);
	}

	for (size_t i = 0; i < writeno; i++) {
		rc = block_write_direct(vol_svc_id, offsets[i], 1, block);
		PCUT_ASSERT_ERRNO_VAL(EOK, rc);
	}

	rc = hr_get_vol_info(hr_sess, vol_svc_id, &info);
	PCUT_ASSERT_ERRNO_VAL(EOK, rc);
	PCUT_ASSERT_EQUALS(HR_VOL_OPTIMAL, info.state);

	for (size_t i = 0; i < failno; i++) {
		rc = hr_fail_extent(hr_sess, volname, i);
		PCUT_ASSERT_ERRNO_VAL(EOK, rc);
	}

	rc = hr_get_vol_info(hr_sess, vol_svc_id, &info);
	PCUT_ASSERT_ERRNO_VAL(EOK, rc);
	PCUT_ASSERT_EQUALS(HR_VOL_DEGRADED, info.state);

	uint8_t *read_block = malloc(bsize);
	PCUT_ASSERT_NOT_NULL(block);

	for (size_t i = 0; i < writeno; i++) {
		rc = block_read_direct(vol_svc_id, offsets[i], 1, read_block);
		PCUT_ASSERT_ERRNO_VAL(EOK, rc);
		for (size_t b = 0; b < bsize; b++)
			PCUT_ASSERT_UINT_EQUALS(block[b], read_block[b]);
	}

	rc = hr_get_vol_info(hr_sess, vol_svc_id, &info);
	PCUT_ASSERT_ERRNO_VAL(EOK, rc);
	PCUT_ASSERT_EQUALS(HR_VOL_DEGRADED, info.state);

	free(offsets);
	rndgen_destroy(rndgen);
	free(block);
	block_fini(vol_svc_id);
	rc = hr_stop(hr_sess, volname);
	PCUT_ASSERT_ERRNO_VAL(EOK, rc);

	return EOK;
}

errno_t test_WD_RD(const char *volname, hr_level_t level, size_t diskno,
    uint8_t vflags, uint64_t writeno, size_t failno)
{
	errno_t rc;
	service_id_t vol_svc_id;

	hr_config_t *cfg = get_cfg(volname, level, diskno, vflags);
	PCUT_ASSERT_NOT_NULL(cfg);

	rc = hr_create(hr_sess, cfg);
	PCUT_ASSERT_ERRNO_VAL(EOK, rc);

	free(cfg);

	rc = loc_service_get_id(volname, &vol_svc_id, 0);
	PCUT_ASSERT_ERRNO_VAL(EOK, rc);

	rc = block_init(vol_svc_id);
	PCUT_ASSERT_ERRNO_VAL(EOK, rc);

	hr_vol_info_t info;
	rc = hr_get_vol_info(hr_sess, vol_svc_id, &info);
	PCUT_ASSERT_ERRNO_VAL(EOK, rc);
	PCUT_ASSERT_EQUALS(HR_VOL_OPTIMAL, info.state);

	for (size_t i = 0; i < failno; i++) {
		rc = hr_fail_extent(hr_sess, volname, i);
		PCUT_ASSERT_ERRNO_VAL(EOK, rc);
	}

	rc = hr_get_vol_info(hr_sess, vol_svc_id, &info);
	PCUT_ASSERT_ERRNO_VAL(EOK, rc);
	PCUT_ASSERT_EQUALS(HR_VOL_DEGRADED, info.state);

	size_t bsize;
	rc = block_get_bsize(vol_svc_id, &bsize);
	PCUT_ASSERT_ERRNO_VAL(EOK, rc);

	uint64_t vol_blkno;
	rc = block_get_nblocks(vol_svc_id, &vol_blkno);
	PCUT_ASSERT_ERRNO_VAL(EOK, rc);
	bool capa = vol_blkno > 0;
	PCUT_ASSERT_TRUE(capa);

	rndgen_t *rndgen;
	rc = rndgen_create(&rndgen);
	PCUT_ASSERT_ERRNO_VAL(EOK, rc);

	uint32_t *offsets = malloc(sizeof(*offsets) * writeno);
	PCUT_ASSERT_NOT_NULL(offsets);
	for (size_t i = 0; i < writeno; i++) {
		rc = rndgen_uint32(rndgen, &offsets[i]);
		PCUT_ASSERT_ERRNO_VAL(EOK, rc);
		offsets[i] %= vol_blkno;
	}

	uint8_t *block = calloc(1, bsize);
	PCUT_ASSERT_NOT_NULL(block);
	for (size_t i = 0; i < bsize; i++) {
		rc = rndgen_uint8(rndgen, &block[i]);
		PCUT_ASSERT_ERRNO_VAL(EOK, rc);
	}

	for (size_t i = 0; i < writeno; i++) {
		rc = block_write_direct(vol_svc_id, offsets[i], 1, block);
		PCUT_ASSERT_ERRNO_VAL(EOK, rc);
	}
	uint8_t *read_block = malloc(bsize);
	PCUT_ASSERT_NOT_NULL(block);

	for (size_t i = 0; i < writeno; i++) {
		rc = block_read_direct(vol_svc_id, offsets[i], 1, read_block);
		PCUT_ASSERT_ERRNO_VAL(EOK, rc);
		for (size_t b = 0; b < bsize; b++)
			PCUT_ASSERT_UINT_EQUALS(block[b], read_block[b]);
	}

	rc = hr_get_vol_info(hr_sess, vol_svc_id, &info);
	PCUT_ASSERT_ERRNO_VAL(EOK, rc);
	PCUT_ASSERT_EQUALS(HR_VOL_DEGRADED, info.state);

	free(offsets);
	rndgen_destroy(rndgen);
	free(block);
	block_fini(vol_svc_id);
	rc = hr_stop(hr_sess, volname);
	PCUT_ASSERT_ERRNO_VAL(EOK, rc);

	return EOK;
}

errno_t test_rebuild(const char *volname, hr_level_t level, size_t diskno,
    uint8_t vflags, size_t failno)
{
	errno_t rc;
	service_id_t vol_svc_id;

	hr_config_t *cfg = get_cfg(volname, level, diskno, vflags);
	PCUT_ASSERT_NOT_NULL(cfg);

	rc = hr_create(hr_sess, cfg);
	PCUT_ASSERT_ERRNO_VAL(EOK, rc);

	free(cfg);

	rc = loc_service_get_id(volname, &vol_svc_id, 0);
	PCUT_ASSERT_ERRNO_VAL(EOK, rc);

	rc = block_init(vol_svc_id);
	PCUT_ASSERT_ERRNO_VAL(EOK, rc);

	hr_vol_info_t info;
	rc = hr_get_vol_info(hr_sess, vol_svc_id, &info);
	PCUT_ASSERT_ERRNO_VAL(EOK, rc);
	PCUT_ASSERT_EQUALS(HR_VOL_OPTIMAL, info.state);

	for (size_t i = 0; i < failno; i++) {
		rc = hr_fail_extent(hr_sess, volname, i);
		PCUT_ASSERT_ERRNO_VAL(EOK, rc);
	}

	rc = hr_get_vol_info(hr_sess, vol_svc_id, &info);
	PCUT_ASSERT_ERRNO_VAL(EOK, rc);
	PCUT_ASSERT_EQUALS(HR_VOL_DEGRADED, info.state);

	for (size_t i = 0; i < failno; i++) {
		rc = hr_add_hotspare(hr_sess, volname, disknames[i]);
		PCUT_ASSERT_ERRNO_VAL(EOK, rc);
	}

	do {
		rc = hr_get_vol_info(hr_sess, vol_svc_id, &info);
		PCUT_ASSERT_ERRNO_VAL(EOK, rc);
		fibril_sleep(1);
	} while (info.state != HR_VOL_OPTIMAL);

	PCUT_ASSERT_EQUALS(HR_VOL_OPTIMAL, info.state);

	block_fini(vol_svc_id);
	rc = hr_stop(hr_sess, volname);
	PCUT_ASSERT_ERRNO_VAL(EOK, rc);

	return EOK;
}

errno_t test_WD_R_RD(const char *volname, hr_level_t level, size_t diskno,
    uint8_t vflags, uint64_t writeno)
{
	errno_t rc;
	service_id_t vol_svc_id;

	bool enough_disks = DISKNO - diskno > 0;
	PCUT_ASSERT_TRUE(enough_disks);

	hr_config_t *cfg = get_cfg(volname, level, diskno, vflags);
	PCUT_ASSERT_NOT_NULL(cfg);

	rc = hr_create(hr_sess, cfg);
	PCUT_ASSERT_ERRNO_VAL(EOK, rc);

	free(cfg);

	rc = loc_service_get_id(volname, &vol_svc_id, 0);
	PCUT_ASSERT_ERRNO_VAL(EOK, rc);

	rc = block_init(vol_svc_id);
	PCUT_ASSERT_ERRNO_VAL(EOK, rc);

	hr_vol_info_t info;
	rc = hr_get_vol_info(hr_sess, vol_svc_id, &info);
	PCUT_ASSERT_ERRNO_VAL(EOK, rc);
	PCUT_ASSERT_EQUALS(HR_VOL_OPTIMAL, info.state);

	rc = hr_fail_extent(hr_sess, volname, 0);
	PCUT_ASSERT_ERRNO_VAL(EOK, rc);

	rc = hr_get_vol_info(hr_sess, vol_svc_id, &info);
	PCUT_ASSERT_ERRNO_VAL(EOK, rc);
	PCUT_ASSERT_EQUALS(HR_VOL_DEGRADED, info.state);

	size_t bsize;
	rc = block_get_bsize(vol_svc_id, &bsize);
	PCUT_ASSERT_ERRNO_VAL(EOK, rc);

	uint64_t vol_blkno;
	rc = block_get_nblocks(vol_svc_id, &vol_blkno);
	PCUT_ASSERT_ERRNO_VAL(EOK, rc);
	bool capa = vol_blkno > 0;
	PCUT_ASSERT_TRUE(capa);

	rndgen_t *rndgen;
	rc = rndgen_create(&rndgen);
	PCUT_ASSERT_ERRNO_VAL(EOK, rc);

	uint32_t *offsets = malloc(sizeof(*offsets) * writeno);
	PCUT_ASSERT_NOT_NULL(offsets);
	for (size_t i = 0; i < writeno; i++) {
		rc = rndgen_uint32(rndgen, &offsets[i]);
		PCUT_ASSERT_ERRNO_VAL(EOK, rc);
		offsets[i] %= vol_blkno;
	}

	uint8_t *block = calloc(1, bsize);
	PCUT_ASSERT_NOT_NULL(block);
	for (size_t i = 0; i < bsize; i++) {
		rc = rndgen_uint8(rndgen, &block[i]);
		PCUT_ASSERT_ERRNO_VAL(EOK, rc);
	}

	for (size_t i = 0; i < writeno; i++) {
		rc = block_write_direct(vol_svc_id, offsets[i], 1, block);
		PCUT_ASSERT_ERRNO_VAL(EOK, rc);
	}

	rc = hr_add_hotspare(hr_sess, volname, disknames[diskno]);
	PCUT_ASSERT_ERRNO_VAL(EOK, rc);
	do {
		rc = hr_get_vol_info(hr_sess, vol_svc_id, &info);
		PCUT_ASSERT_ERRNO_VAL(EOK, rc);
		fibril_sleep(1);
	} while (info.state != HR_VOL_OPTIMAL);

	rc = hr_get_vol_info(hr_sess, vol_svc_id, &info);
	PCUT_ASSERT_ERRNO_VAL(EOK, rc);
	PCUT_ASSERT_EQUALS(HR_VOL_OPTIMAL, info.state);

	rc = hr_fail_extent(hr_sess, volname, 1);
	PCUT_ASSERT_ERRNO_VAL(EOK, rc);

	rc = hr_get_vol_info(hr_sess, vol_svc_id, &info);
	PCUT_ASSERT_ERRNO_VAL(EOK, rc);
	PCUT_ASSERT_EQUALS(HR_VOL_DEGRADED, info.state);

	uint8_t *read_block = malloc(bsize);
	PCUT_ASSERT_NOT_NULL(block);
	for (size_t i = 0; i < writeno; i++) {
		rc = block_read_direct(vol_svc_id, offsets[i], 1, read_block);
		PCUT_ASSERT_ERRNO_VAL(EOK, rc);
		for (size_t b = 0; b < bsize; b++)
			PCUT_ASSERT_UINT_EQUALS(block[b], read_block[b]);
	}

	rc = hr_get_vol_info(hr_sess, vol_svc_id, &info);
	PCUT_ASSERT_ERRNO_VAL(EOK, rc);
	PCUT_ASSERT_EQUALS(HR_VOL_DEGRADED, info.state);

	free(offsets);
	rndgen_destroy(rndgen);
	free(block);
	block_fini(vol_svc_id);
	rc = hr_stop(hr_sess, volname);
	PCUT_ASSERT_ERRNO_VAL(EOK, rc);

	return EOK;
}

errno_t test_assembly_basic(const char *volname, hr_level_t level,
    size_t diskno, uint8_t vflags)
{
	errno_t rc;
	service_id_t vol_svc_id;

	bool noop_meta = vflags & HR_VOL_FLAG_NOOP_META;
	PCUT_ASSERT_FALSE(noop_meta);

	hr_config_t *cfg = get_cfg(volname, level, diskno, vflags);
	PCUT_ASSERT_NOT_NULL(cfg);

	rc = hr_create(hr_sess, cfg);
	PCUT_ASSERT_ERRNO_VAL(EOK, rc);

	rc = loc_service_get_id(volname, &vol_svc_id, 0);
	PCUT_ASSERT_ERRNO_VAL(EOK, rc);

	hr_vol_info_t info;
	rc = hr_get_vol_info(hr_sess, vol_svc_id, &info);
	PCUT_ASSERT_ERRNO_VAL(EOK, rc);
	PCUT_ASSERT_EQUALS(info.state, HR_VOL_OPTIMAL);

	rc = hr_stop(hr_sess, volname);
	PCUT_ASSERT_ERRNO_VAL(EOK, rc);

	rc = loc_service_get_id(volname, &vol_svc_id, 0);
	PCUT_ASSERT_ERRNO_VAL(ENOENT, rc);

	size_t assembled_cnt;
	rc = hr_assemble(hr_sess, cfg, &assembled_cnt);
	PCUT_ASSERT_ERRNO_VAL(EOK, rc);
	PCUT_ASSERT_UINT_EQUALS(1, assembled_cnt);

	free(cfg);

	rc = loc_service_get_id(volname, &vol_svc_id, 0);
	PCUT_ASSERT_ERRNO_VAL(EOK, rc);

	hr_vol_info_t info_assembly;
	rc = hr_get_vol_info(hr_sess, vol_svc_id, &info_assembly);
	PCUT_ASSERT_ERRNO_VAL(EOK, rc);
	PCUT_ASSERT_TRUE(memcmp(info.devname, info_assembly.devname,
	    sizeof(info.devname)) == 0);
	PCUT_ASSERT_TRUE(info.state == info_assembly.state);
	PCUT_ASSERT_TRUE(info.level == info_assembly.level);
	PCUT_ASSERT_TRUE(info.extent_no == info_assembly.extent_no);
	PCUT_ASSERT_TRUE(info.state == info_assembly.state);
	PCUT_ASSERT_TRUE(info.data_blkno == info_assembly.data_blkno);
	PCUT_ASSERT_TRUE(info.strip_size == info_assembly.strip_size);
	PCUT_ASSERT_TRUE(info.bsize == info_assembly.bsize);
	PCUT_ASSERT_TRUE(info.layout == info_assembly.layout);
	PCUT_ASSERT_TRUE(info.meta_type == info_assembly.meta_type);

	rc = hr_stop(hr_sess, volname);
	PCUT_ASSERT_ERRNO_VAL(EOK, rc);

	return EOK;
}

errno_t test_assembly_partial_R(const char *volname, hr_level_t level,
    size_t diskno, uint8_t vflags)
{
	errno_t rc;
	service_id_t vol_svc_id;

	bool noop_meta = vflags & HR_VOL_FLAG_NOOP_META;
	PCUT_ASSERT_FALSE(noop_meta);

	hr_config_t *cfg = get_cfg(volname, level, diskno, vflags);
	PCUT_ASSERT_NOT_NULL(cfg);

	rc = hr_create(hr_sess, cfg);
	PCUT_ASSERT_ERRNO_VAL(EOK, rc);

	rc = loc_service_get_id(volname, &vol_svc_id, 0);
	PCUT_ASSERT_ERRNO_VAL(EOK, rc);

	hr_vol_info_t info;
	rc = hr_get_vol_info(hr_sess, vol_svc_id, &info);
	PCUT_ASSERT_ERRNO_VAL(EOK, rc);
	PCUT_ASSERT_EQUALS(HR_VOL_OPTIMAL, info.state);

	rc = hr_stop(hr_sess, volname);
	PCUT_ASSERT_ERRNO_VAL(EOK, rc);

	rc = loc_service_get_id(volname, &vol_svc_id, 0);
	PCUT_ASSERT_ERRNO_VAL(ENOENT, rc);

	cfg->devs[diskno - 1] = 0;
	cfg->dev_no = diskno - 1;
	size_t assembled_cnt;
	rc = hr_assemble(hr_sess, cfg, &assembled_cnt);
	PCUT_ASSERT_ERRNO_VAL(EOK, rc);
	PCUT_ASSERT_UINT_EQUALS(1, assembled_cnt);

	rc = loc_service_get_id(volname, &vol_svc_id, 0);
	PCUT_ASSERT_ERRNO_VAL(EOK, rc);

	hr_vol_info_t info_assembly;
	rc = hr_get_vol_info(hr_sess, vol_svc_id, &info_assembly);
	PCUT_ASSERT_ERRNO_VAL(EOK, rc);
	PCUT_ASSERT_EQUALS(HR_VOL_DEGRADED, info_assembly.state);

	size_t bsize = info_assembly.bsize;

	rc = block_init(vol_svc_id);
	PCUT_ASSERT_ERRNO_VAL(EOK, rc);

	uint8_t *block = malloc(bsize);
	PCUT_ASSERT_NOT_NULL(block);
	for (size_t i = 0; i < bsize; i++) {
		rc = block_read_direct(vol_svc_id, 0, 1, block);
		PCUT_ASSERT_ERRNO_VAL(EOK, rc);
	}

	block_fini(vol_svc_id);

	rc = hr_stop(hr_sess, volname);
	PCUT_ASSERT_ERRNO_VAL(EOK, rc);

	rc = loc_service_get_id(volname, &vol_svc_id, 0);
	PCUT_ASSERT_ERRNO_VAL(ENOENT, rc);

	cfg->devs[diskno - 1] = disks[diskno - 1];
	cfg->dev_no = diskno;
	rc = hr_assemble(hr_sess, cfg, &assembled_cnt);
	PCUT_ASSERT_ERRNO_VAL(EOK, rc);
	PCUT_ASSERT_UINT_EQUALS(1, assembled_cnt);

	rc = loc_service_get_id(volname, &vol_svc_id, 0);
	PCUT_ASSERT_ERRNO_VAL(EOK, rc);

	rc = hr_get_vol_info(hr_sess, vol_svc_id, &info_assembly);
	PCUT_ASSERT_ERRNO_VAL(EOK, rc);
	PCUT_ASSERT_EQUALS(HR_VOL_OPTIMAL, info_assembly.state);

	free(block);
	free(cfg);
	rc = hr_stop(hr_sess, volname);
	PCUT_ASSERT_ERRNO_VAL(EOK, rc);

	return EOK;
}

errno_t test_assembly_partial_W(const char *volname, hr_level_t level,
    size_t diskno, uint8_t vflags)
{
	errno_t rc;
	service_id_t vol_svc_id;

	bool noop_meta = vflags & HR_VOL_FLAG_NOOP_META;
	PCUT_ASSERT_FALSE(noop_meta);

	hr_config_t *cfg = get_cfg(volname, level, diskno, vflags);
	PCUT_ASSERT_NOT_NULL(cfg);

	rc = hr_create(hr_sess, cfg);
	PCUT_ASSERT_ERRNO_VAL(EOK, rc);

	rc = loc_service_get_id(volname, &vol_svc_id, 0);
	PCUT_ASSERT_ERRNO_VAL(EOK, rc);

	hr_vol_info_t info;
	rc = hr_get_vol_info(hr_sess, vol_svc_id, &info);
	PCUT_ASSERT_ERRNO_VAL(EOK, rc);
	PCUT_ASSERT_EQUALS(HR_VOL_OPTIMAL, info.state);

	rc = hr_stop(hr_sess, volname);
	PCUT_ASSERT_ERRNO_VAL(EOK, rc);

	rc = loc_service_get_id(volname, &vol_svc_id, 0);
	PCUT_ASSERT_ERRNO_VAL(ENOENT, rc);

	cfg->devs[diskno - 1] = 0;
	cfg->dev_no = diskno - 1;
	size_t assembled_cnt;
	rc = hr_assemble(hr_sess, cfg, &assembled_cnt);
	PCUT_ASSERT_ERRNO_VAL(EOK, rc);
	PCUT_ASSERT_UINT_EQUALS(1, assembled_cnt);

	rc = loc_service_get_id(volname, &vol_svc_id, 0);
	PCUT_ASSERT_ERRNO_VAL(EOK, rc);

	hr_vol_info_t info_assembly;
	rc = hr_get_vol_info(hr_sess, vol_svc_id, &info_assembly);
	PCUT_ASSERT_ERRNO_VAL(EOK, rc);
	PCUT_ASSERT_EQUALS(HR_VOL_DEGRADED, info_assembly.state);

	size_t bsize = info_assembly.bsize;

	rc = block_init(vol_svc_id);
	PCUT_ASSERT_ERRNO_VAL(EOK, rc);

	uint8_t *block = malloc(bsize);
	PCUT_ASSERT_NOT_NULL(block);
	rc = block_write_direct(vol_svc_id, 0, 1, block);
	PCUT_ASSERT_ERRNO_VAL(EOK, rc);

	block_fini(vol_svc_id);

	rc = hr_stop(hr_sess, volname);
	PCUT_ASSERT_ERRNO_VAL(EOK, rc);

	rc = loc_service_get_id(volname, &vol_svc_id, 0);
	PCUT_ASSERT_ERRNO_VAL(ENOENT, rc);

	cfg->devs[diskno - 1] = disks[diskno - 1];
	cfg->dev_no = diskno;
	rc = hr_assemble(hr_sess, cfg, &assembled_cnt);
	PCUT_ASSERT_ERRNO_VAL(EOK, rc);
	PCUT_ASSERT_UINT_EQUALS(1, assembled_cnt);

	rc = loc_service_get_id(volname, &vol_svc_id, 0);
	PCUT_ASSERT_ERRNO_VAL(EOK, rc);

	rc = hr_get_vol_info(hr_sess, vol_svc_id, &info_assembly);
	PCUT_ASSERT_ERRNO_VAL(EOK, rc);
	PCUT_ASSERT_TRUE(info_assembly.state != HR_VOL_OPTIMAL);

	do {
		rc = hr_get_vol_info(hr_sess, vol_svc_id, &info);
		PCUT_ASSERT_ERRNO_VAL(EOK, rc);
		fibril_sleep(1);
	} while (info.state != HR_VOL_OPTIMAL);
	PCUT_ASSERT_EQUALS(HR_VOL_OPTIMAL, info.state);

	free(block);
	free(cfg);
	rc = hr_stop(hr_sess, volname);
	PCUT_ASSERT_ERRNO_VAL(EOK, rc);

	return EOK;
}
