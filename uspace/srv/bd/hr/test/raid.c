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

PCUT_INIT;

PCUT_TEST_SUITE(raid);

service_id_t disks[DISKNO];
const char *const disknames[] = {
	"disk1",
	"disk2",
	"disk3",
	"disk4",
};
uint64_t min_blkno;

hr_t *hr_sess;

PCUT_TEST_BEFORE
{
	errno_t rc;
	rc = hr_sess_init(&hr_sess);
	PCUT_ASSERT_ERRNO_VAL(EOK, rc);

	for (size_t i = 0; i < DISKNO; i++) {
		rc = loc_service_get_id(disknames[i], &disks[i], 0);
		if (rc != EOK) {
			printf("run \"batch cfg/create_file_bd_disks.bdsh\"\n");
			return;
		}
		PCUT_ASSERT_ERRNO_VAL(EOK, rc);
	}

	uint64_t blkno;
	uint64_t min = UINT64_MAX;
	for (size_t i = 0; i < DISKNO; i++) {
		rc = block_init(disks[i]);
		PCUT_ASSERT_ERRNO_VAL(EOK, rc);

		rc = block_get_nblocks(disks[i], &blkno);
		PCUT_ASSERT_ERRNO_VAL(EOK, rc);

		if (blkno < min)
			min = blkno;
	}
	min_blkno = min;
}

PCUT_TEST_AFTER
{
	for (size_t i = 0; i < DISKNO; i++)
		block_fini(disks[i]);

	hr_sess_destroy(hr_sess);
}

PCUT_TEST(raid0_blkno)
{
	errno_t rc;
	const char *volname = "_testvol_raid0_blkno";

	uint64_t expected_blkno = min_blkno * DISKNO;
	expected_blkno -= META_BLKNO * DISKNO;

	rc = test_vol_blkno(volname, HR_LVL_0, DISKNO, 0, expected_blkno);
	PCUT_ASSERT_ERRNO_VAL(EOK, rc);
}

PCUT_TEST(raid1_blkno)
{
	errno_t rc;
	const char *volname = "_testvol_raid1_blkno";

	uint64_t expected_blkno = min_blkno - META_BLKNO;

	rc = test_vol_blkno(volname, HR_LVL_1, DISKNO, 0, expected_blkno);
	PCUT_ASSERT_ERRNO_VAL(EOK, rc);
}

PCUT_TEST(raid5_blkno)
{
	errno_t rc;
	const char *volname = "_testvol_raid5_blkno";

	uint64_t expected_blkno = min_blkno * (DISKNO - 1);
	expected_blkno -= META_BLKNO * (DISKNO - 1);

	rc = test_vol_blkno(volname, HR_LVL_5, DISKNO, 0, expected_blkno);
	PCUT_ASSERT_ERRNO_VAL(EOK, rc);
}

PCUT_TEST(raid0_blkno_noop_meta)
{
	errno_t rc;
	const char *volname = "_testvol_raid0_blkno_noop_meta";

	uint64_t expected_blkno = min_blkno * DISKNO;

	rc = test_vol_blkno(volname, HR_LVL_0, DISKNO, HR_VOL_FLAG_NOOP_META,
	    expected_blkno);
	PCUT_ASSERT_ERRNO_VAL(EOK, rc);
}

PCUT_TEST(raid1_blkno_noop_meta)
{
	errno_t rc;
	const char *volname = "_testvol_raid0_blkno_noop_meta";

	uint64_t expected_blkno = min_blkno;

	rc = test_vol_blkno(volname, HR_LVL_1, DISKNO, HR_VOL_FLAG_NOOP_META,
	    expected_blkno);
	PCUT_ASSERT_ERRNO_VAL(EOK, rc);
}

PCUT_TEST(raid5_blkno_noop_meta)
{
	errno_t rc;
	const char *volname = "_testvol_raid5_blkno_noop_meta";

	uint64_t expected_blkno = min_blkno * (DISKNO - 1);

	rc = test_vol_blkno(volname, HR_LVL_5, DISKNO, HR_VOL_FLAG_NOOP_META,
	    expected_blkno);
	PCUT_ASSERT_ERRNO_VAL(EOK, rc);
}

PCUT_TEST(read_only)
{
	errno_t rc;
	service_id_t vol_svc_id;
	const char *volname = "_testvol_read_only";

	hr_config_t *cfg = get_cfg(volname, HR_LVL_1, DISKNO,
	    HR_VOL_FLAG_NOOP_META | HR_VOL_FLAG_READ_ONLY);
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
	bool capa = vol_blkno > 0;
	PCUT_ASSERT_TRUE(capa);

	size_t bsize;
	rc = block_get_bsize(vol_svc_id, &bsize);
	PCUT_ASSERT_ERRNO_VAL(EOK, rc);

	uint8_t *block = calloc(1, bsize);
	PCUT_ASSERT_NOT_NULL(block);

	rc = block_read_direct(vol_svc_id, 0, 1, block);
	PCUT_ASSERT_ERRNO_VAL(EOK, rc);

	rc = block_write_direct(vol_svc_id, 0, 1, block);
	PCUT_ASSERT_ERRNO_VAL(ENOTSUP, rc);

	free(block);

	block_fini(vol_svc_id);
	rc = hr_stop(hr_sess, volname);
	PCUT_ASSERT_ERRNO_VAL(EOK, rc);
}

PCUT_TEST(raid0_state_evaluation)
{
	errno_t rc;
	service_id_t vol_svc_id;
	const char *volname = "_testvol_raid0_state_evaluation";

	hr_config_t *cfg = get_cfg(volname, HR_LVL_0, DISKNO, 0);
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
	PCUT_ASSERT_EQUALS(info.state, HR_VOL_OPTIMAL);

	rc = hr_fail_extent(hr_sess, volname, 0);
	PCUT_ASSERT_ERRNO_VAL(EOK, rc);

	rc = hr_get_vol_info(hr_sess, vol_svc_id, &info);
	PCUT_ASSERT_ERRNO_VAL(EOK, rc);
	PCUT_ASSERT_EQUALS(info.state, HR_VOL_FAULTY);

	size_t bsize;
	rc = block_get_bsize(vol_svc_id, &bsize);
	PCUT_ASSERT_ERRNO_VAL(EOK, rc);

	uint8_t *block = calloc(1, bsize);
	PCUT_ASSERT_NOT_NULL(block);
	/* rc = block_read_direct(vol_svc_id, 0, 1, block); */
	/* XXX: PCUT_ASSERT_ERRNO_VAL(EIO, rc); */
	/* rc = block_write_direct(vol_svc_id, 0, 1, block); */
	/* PCUT_ASSERT_ERRNO_VAL(EIO, rc); */

	free(block);
	block_fini(vol_svc_id);
	rc = hr_stop(hr_sess, volname);
	PCUT_ASSERT_ERRNO_VAL(EOK, rc);
}

PCUT_TEST(raid1_state_evaluation)
{
	errno_t rc;
	service_id_t vol_svc_id;
	const char *volname = "_testvol_raid1_state_evaluation";

	hr_config_t *cfg = get_cfg(volname, HR_LVL_1, DISKNO, 0);
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
	PCUT_ASSERT_EQUALS(info.state, HR_VOL_OPTIMAL);

	for (size_t i = 0; i < DISKNO - 1; i++) {
		rc = hr_fail_extent(hr_sess, volname, i);
		PCUT_ASSERT_ERRNO_VAL(EOK, rc);
	}

	rc = hr_get_vol_info(hr_sess, vol_svc_id, &info);
	PCUT_ASSERT_ERRNO_VAL(EOK, rc);
	PCUT_ASSERT_EQUALS(info.state, HR_VOL_DEGRADED);

	size_t bsize;
	rc = block_get_bsize(vol_svc_id, &bsize);
	PCUT_ASSERT_ERRNO_VAL(EOK, rc);

	uint8_t *block = calloc(1, bsize);
	PCUT_ASSERT_NOT_NULL(block);
	rc = block_read_direct(vol_svc_id, 0, 1, block);
	PCUT_ASSERT_ERRNO_VAL(EOK, rc);

	rc = hr_fail_extent(hr_sess, volname, DISKNO - 1);
	PCUT_ASSERT_ERRNO_VAL(EOK, rc);
	rc = hr_get_vol_info(hr_sess, vol_svc_id, &info);
	PCUT_ASSERT_ERRNO_VAL(EOK, rc);
	PCUT_ASSERT_EQUALS(info.state, HR_VOL_FAULTY);
	/* rc = block_read_direct(vol_svc_id, 0, 1, block); */
	/* XXX: PCUT_ASSERT_ERRNO_VAL(EIO, rc); */
	/* rc = block_write_direct(vol_svc_id, 0, 1, block); */
	/* PCUT_ASSERT_ERRNO_VAL(EIO, rc); */

	free(block);
	block_fini(vol_svc_id);
	rc = hr_stop(hr_sess, volname);
	PCUT_ASSERT_ERRNO_VAL(EOK, rc);
}

PCUT_TEST(raid5_state_evaluation)
{
	errno_t rc;
	service_id_t vol_svc_id;
	const char *volname = "_testvol_raid5_state_evaluation";

	hr_config_t *cfg = get_cfg(volname, HR_LVL_5, DISKNO, 0);
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
	PCUT_ASSERT_EQUALS(info.state, HR_VOL_OPTIMAL);

	rc = hr_fail_extent(hr_sess, volname, 0);
	PCUT_ASSERT_ERRNO_VAL(EOK, rc);

	rc = hr_get_vol_info(hr_sess, vol_svc_id, &info);
	PCUT_ASSERT_ERRNO_VAL(EOK, rc);
	PCUT_ASSERT_EQUALS(info.state, HR_VOL_DEGRADED);

	size_t bsize;
	rc = block_get_bsize(vol_svc_id, &bsize);
	PCUT_ASSERT_ERRNO_VAL(EOK, rc);

	uint8_t *block = calloc(1, bsize);
	PCUT_ASSERT_NOT_NULL(block);
	rc = block_read_direct(vol_svc_id, 0, 1, block);
	PCUT_ASSERT_ERRNO_VAL(EOK, rc);

	rc = hr_fail_extent(hr_sess, volname, 1);
	PCUT_ASSERT_ERRNO_VAL(EOK, rc);
	rc = hr_get_vol_info(hr_sess, vol_svc_id, &info);
	PCUT_ASSERT_ERRNO_VAL(EOK, rc);
	PCUT_ASSERT_EQUALS(info.state, HR_VOL_FAULTY);
	/* rc = block_read_direct(vol_svc_id, 0, 1, block); */
	/* XXX: PCUT_ASSERT_ERRNO_VAL(EIO, rc); */
	/* rc = block_write_direct(vol_svc_id, 0, 1, block); */
	/* PCUT_ASSERT_ERRNO_VAL(EIO, rc); */

	free(block);
	block_fini(vol_svc_id);
	rc = hr_stop(hr_sess, volname);
	PCUT_ASSERT_ERRNO_VAL(EOK, rc);
}

PCUT_TEST(raid0_WO_RO)
{
	errno_t rc;
	const char *volname = "_testvol_raid0_WO_RO";
	uint64_t writeno = 10;

	rc = test_WO_RO(volname, HR_LVL_0, DISKNO, 0, writeno);
	PCUT_ASSERT_ERRNO_VAL(EOK, rc);
}

PCUT_TEST(raid1_WO_RO)
{
	errno_t rc;
	const char *volname = "_testvol_raid1_WO_RO";
	uint64_t writeno = 10;

	rc = test_WO_RO(volname, HR_LVL_1, DISKNO, 0, writeno);
	PCUT_ASSERT_ERRNO_VAL(EOK, rc);
}

PCUT_TEST(raid5_WO_RO)
{
	errno_t rc;
	const char *volname = "_testvol_raid5_WO_RO";
	uint64_t writeno = 10;

	rc = test_WO_RO(volname, HR_LVL_5, DISKNO, 0, writeno);
	PCUT_ASSERT_ERRNO_VAL(EOK, rc);
}

PCUT_TEST(raid1_WO_RD)
{
	errno_t rc;
	const char *volname = "_testvol_raid1_WO_RD";
	uint64_t writeno = 10;

	rc = test_WO_RD(volname, HR_LVL_1, DISKNO, 0, writeno, DISKNO - 1);
	PCUT_ASSERT_ERRNO_VAL(EOK, rc);
}

PCUT_TEST(raid5_WO_RD)
{
	errno_t rc;
	const char *volname = "_testvol_raid5_WO_RD";
	uint64_t writeno = 10;

	rc = test_WO_RD(volname, HR_LVL_5, DISKNO, 0, writeno, 1);
	PCUT_ASSERT_ERRNO_VAL(EOK, rc);
}

PCUT_TEST(raid1_WD_RD)
{
	errno_t rc;
	const char *volname = "_testvol_raid1_WD_RD";
	uint64_t writeno = 10;
	size_t failno = DISKNO - 1;

	rc = test_WD_RD(volname, HR_LVL_1, DISKNO, 0, writeno, failno);
	PCUT_ASSERT_ERRNO_VAL(EOK, rc);
}

PCUT_TEST(raid5_WD_RD)
{
	errno_t rc;
	const char *volname = "_testvol_raid5_WD_RD";
	uint64_t writeno = 10;
	size_t failno = 1;

	rc = test_WD_RD(volname, HR_LVL_5, DISKNO, 0, writeno, failno);
	PCUT_ASSERT_ERRNO_VAL(EOK, rc);
}

PCUT_TEST(raid1_rebuild, PCUT_TEST_SET_TIMEOUT(PCUT_DEFAULT_TEST_TIMEOUT * 5))
{
	errno_t rc;
	const char *volname = "_testvol_raid1_rebuild";
	size_t failno = DISKNO - 1;

	rc = test_rebuild(volname, HR_LVL_1, DISKNO, 0, failno);
	PCUT_ASSERT_ERRNO_VAL(EOK, rc);
}

PCUT_TEST(raid5_rebuild, PCUT_TEST_SET_TIMEOUT(PCUT_DEFAULT_TEST_TIMEOUT * 5))
{
	errno_t rc;
	const char *volname = "_testvol_raid5_rebuild";
	size_t failno = 1;

	rc = test_rebuild(volname, HR_LVL_5, DISKNO, 0, failno);
	PCUT_ASSERT_ERRNO_VAL(EOK, rc);
}

PCUT_TEST(raid1_WD_R_RD, PCUT_TEST_SET_TIMEOUT(PCUT_DEFAULT_TEST_TIMEOUT * 5))
{
	errno_t rc;
	const char *volname = "_testvol_raid1_WD_R_RD";
	uint64_t writeno = 10;

	rc = test_WD_R_RD(volname, HR_LVL_1, 2, 0, writeno);
	PCUT_ASSERT_ERRNO_VAL(EOK, rc);
}

PCUT_TEST(raid5_WD_R_RD, PCUT_TEST_SET_TIMEOUT(PCUT_DEFAULT_TEST_TIMEOUT * 5))
{
	errno_t rc;
	const char *volname = "_testvol_raid5_WD_R_RD";
	uint64_t writeno = 10;

	rc = test_WD_R_RD(volname, HR_LVL_5, 3, 0, writeno);
	PCUT_ASSERT_ERRNO_VAL(EOK, rc);
}

PCUT_TEST(raid0_assembly_basic)
{
	errno_t rc;
	const char *volname = "_testvol_raid0_assembly_basic";

	rc = test_assembly_basic(volname, HR_LVL_0, DISKNO, 0);
	PCUT_ASSERT_ERRNO_VAL(EOK, rc);
}

PCUT_TEST(raid1_assembly_basic)
{
	errno_t rc;
	const char *volname = "_testvol_raid1_assembly_basic";

	rc = test_assembly_basic(volname, HR_LVL_1, DISKNO, 0);
	PCUT_ASSERT_ERRNO_VAL(EOK, rc);
}

PCUT_TEST(raid5_assembly_basic)
{
	errno_t rc;
	const char *volname = "_testvol_raid5_assembly_basic";

	rc = test_assembly_basic(volname, HR_LVL_5, DISKNO, 0);
	PCUT_ASSERT_ERRNO_VAL(EOK, rc);
}

PCUT_TEST(raid1_assembly_part_R)
{
	errno_t rc;
	const char *volname = "_testvol_raid1_assembly_part_R";

	rc = test_assembly_partial_R(volname, HR_LVL_1, DISKNO, 0);
	PCUT_ASSERT_ERRNO_VAL(EOK, rc);
}

PCUT_TEST(raid5_assembly_part_R)
{
	errno_t rc;
	const char *volname = "_testvol_raid5_assembly_part_R";

	rc = test_assembly_partial_R(volname, HR_LVL_5, DISKNO, 0);
	PCUT_ASSERT_ERRNO_VAL(EOK, rc);
}

PCUT_TEST(raid1_assembly_part_W,
    PCUT_TEST_SET_TIMEOUT(PCUT_DEFAULT_TEST_TIMEOUT * 5))
{
	errno_t rc;
	const char *volname = "_testvol_raid1_assembly_part_W";

	rc = test_assembly_partial_W(volname, HR_LVL_1, DISKNO, 0);
	PCUT_ASSERT_ERRNO_VAL(EOK, rc);
}

PCUT_TEST(raid5_assembly_part_W,
    PCUT_TEST_SET_TIMEOUT(PCUT_DEFAULT_TEST_TIMEOUT * 5))
{
	errno_t rc;
	const char *volname = "_testvol_raid5_assembly_part_W";

	rc = test_assembly_partial_W(volname, HR_LVL_5, DISKNO, 0);
	PCUT_ASSERT_ERRNO_VAL(EOK, rc);
}

PCUT_EXPORT(raid);
