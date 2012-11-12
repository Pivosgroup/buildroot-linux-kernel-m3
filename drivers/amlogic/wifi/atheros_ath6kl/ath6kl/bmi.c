/*
 * Copyright (c) 2004-2011 Atheros Communications Inc.
 *
 * Permission to use, copy, modify, and/or distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
 * OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */

#include "core.h"
#include "hif-ops.h"
#include "target.h"
#include "debug.h"

extern unsigned int chip_pwd_l;
extern unsigned int chip_pwd_l_pin;
extern unsigned int debug_fpga;

static u32 ath6kl_bmi_get_max_data_size(struct ath6kl *ar)
{
	return (ar->hif_type == HIF_TYPE_SDIO) ?
		BMI_SDIO_DATASZ_MAX : BMI_USB_DATASZ_MAX;
}

static u32 ath6kl_bmi_get_max_cmd_size(struct ath6kl *ar)
{
	return ath6kl_bmi_get_max_data_size(ar) + (sizeof(u32) * 3);
}

int ath6kl_bmi_done(struct ath6kl *ar)
{
	int ret;
	u32 cid = BMI_DONE;

	if (ar->bmi.done_sent) {
		ath6kl_dbg(ATH6KL_DBG_BMI, "bmi done skipped\n");
		return 0;
	}

	ar->bmi.done_sent = true;

	ret = ath6kl_hif_bmi_send_buf(ar, (u8 *)&cid, sizeof(cid));
	if (ret) {
		ath6kl_err("Unable to send bmi done: %d\n", ret);
		return ret;
	}

	ath6kl_bmi_cleanup(ar);

	return 0;
}

int ath6kl_assert_chip_pwd_l(struct ath6kl *ar)
{
	int ret = 1;
	u32 reg_addr_1, reg_addr_2, reg_addr_3;
	u32 reg_val_1, reg_val_2, reg_val_3;
	const u32 magic_pattern = 0x4D67506E;

	reg_addr_1 = reg_addr_2 = reg_addr_3 = 0;
	reg_val_1 = reg_val_2 = reg_val_3 = 0;

	reg_addr_3 = 0x47204;

	if (ar->hif_type == HIF_TYPE_USB) {
		reg_addr_1 = AR6004_REV2_BOARD_DATA_ADDRESS-4;
		reg_addr_2 = AR6004_REV2_BOARD_DATA_ADDRESS-8;

		ath6kl_diag_read32(ar, reg_addr_1, &reg_val_1);
		ath6kl_diag_read32(ar, reg_addr_2, &reg_val_2);

		if ((reg_val_1 == magic_pattern) &&
		    (reg_val_2 == ~magic_pattern)) {
			ath6kl_diag_read32(ar, reg_addr_3, &reg_val_3);
            
            ath6kl_err("reg_val1: 0x%08x, reg_val2: 0x%08x, reg_val3: 0x%08x\n",
              reg_val_1, reg_val_2, reg_val_3);
			if ((reg_val_3 & 0xe)==0) {
				ath6kl_err("2nd plug: pattern matched!\n");
				goto DONE;
			} else {
				goto RESET;
			}
		}
		else {
RESET:
			ath6kl_err("1st plug: pattern mismatched!\n");
			reg_val_1 = magic_pattern;  /* MgPn */
			reg_val_2 = ~magic_pattern; /* ~MgPn */
			ret = ath6kl_diag_write32(ar, reg_addr_1, reg_val_1);
			ret = ath6kl_diag_write32(ar, reg_addr_2, reg_val_2);
		}

		/* put gpio4 as general purpose I/O pin */
		reg_addr_1 = 0x14134;
		reg_val_1 = 0;
		ret = ath6kl_diag_write32(ar, reg_addr_1, reg_val_1);

		reg_addr_1 = (0x14050+chip_pwd_l_pin*4);
		reg_val_1 = 0x58;
		ret = ath6kl_diag_write32(ar, reg_addr_1, reg_val_1);

		/* configure as output */
		reg_addr_1 = 0x1401c;
		reg_val_1 = 0;
		ret = ath6kl_diag_read32(ar, reg_addr_1, &reg_val_1);
		reg_val_1 |= (1<<chip_pwd_l_pin);
		ret = ath6kl_diag_write32(ar, reg_addr_1, reg_val_1);

		/* toggle gpio4 to low*/
		reg_addr_1 = 0x14008;
		reg_val_1 = 0;
		ret = ath6kl_diag_read32(ar, reg_addr_1, &reg_val_1);
		reg_val_1 |= (1<<chip_pwd_l_pin);
		ret = ath6kl_diag_write32(ar, reg_addr_1, reg_val_1);

	}
DONE:
	/* restore gpio4 function pin */
	reg_addr_1 = 0x14134;
	reg_val_1 = 1;
	ret = ath6kl_diag_write32(ar, reg_addr_1, reg_val_1);

	return ret;
}

int ath6kl_bmi_get_target_info(struct ath6kl *ar,
			       struct ath6kl_bmi_target_info *targ_info)
{
	int ret;
	u32 cid = BMI_GET_TARGET_INFO;

	if (ar->bmi.done_sent) {
		ath6kl_err("bmi done sent already, cmd %d disallowed\n", cid);
		return -EACCES;
	}

	ret = ath6kl_hif_bmi_send_buf(ar, (u8 *)&cid, sizeof(cid));
	if (ret) {
		ath6kl_err("Unable to send get target info: %d\n", ret);
		return ret;
	}

	if (ar->hif_type == HIF_TYPE_USB) {
		ret = ath6kl_hif_bmi_recv_buf(ar, (u8 *)targ_info,
				sizeof(struct ath6kl_bmi_target_info), true);
	} else {
		ret = ath6kl_hif_bmi_recv_buf(ar, (u8 *)&targ_info->version,
				sizeof(targ_info->version), true);
	}

	if (ret) {
		ath6kl_err("Unable to recv target info: %d\n", ret);
		return ret;
	}

	if (le32_to_cpu(targ_info->version) == TARGET_VERSION_SENTINAL) {
		/* Determine how many bytes are in the Target's targ_info */
		ret = ath6kl_hif_bmi_recv_buf(ar,
				   (u8 *)&targ_info->byte_count,
				   sizeof(targ_info->byte_count),
				   true);
		if (ret) {
			ath6kl_err("unable to read target info byte count: %d\n",
				   ret);
			return ret;
		}

		/*
		 * The target's targ_info doesn't match the host's targ_info.
		 * We need to do some backwards compatibility to make this work.
		 */
		if (le32_to_cpu(targ_info->byte_count) != sizeof(*targ_info)) {
			WARN_ON(1);
			return -EINVAL;
		}

		/* Read the remainder of the targ_info */
		ret = ath6kl_hif_bmi_recv_buf(ar,
				   ((u8 *)targ_info) +
				   sizeof(targ_info->byte_count),
				   sizeof(*targ_info) -
				   sizeof(targ_info->byte_count),
				   true);

		if (ret) {
			ath6kl_err("Unable to read target info (%d bytes): %d\n",
				   targ_info->byte_count, ret);
			return ret;
		}
	}

    /* FIXME
    * ignore chip magic pattern for Kingfisher FPGA */
    if (debug_fpga) {
            chip_pwd_l = 0;
    }

	if( chip_pwd_l )
		ath6kl_assert_chip_pwd_l(ar);

	ath6kl_dbg(ATH6KL_DBG_BMI, "target info (ver: 0x%x type: 0x%x)\n",
		targ_info->version, targ_info->type);

	return 0;
}

int ath6kl_bmi_read(struct ath6kl *ar, u32 addr, u8 *buf, u32 len)
{
	u32 cid = BMI_READ_MEMORY;
	int ret;
	u32 offset;
	u32 len_remain, rx_len;
	u32 max_data_sz = ath6kl_bmi_get_max_data_size(ar);
	u32 max_cmd_sz = ath6kl_bmi_get_max_cmd_size(ar);
	u16 size;

	if (ar->bmi.done_sent) {
		ath6kl_err("bmi done sent already, cmd %d disallowed\n", cid);
		return -EACCES;
	}

	size = max_data_sz + sizeof(cid) + sizeof(addr) + sizeof(len);
	if (size > max_cmd_sz) {
		WARN_ON(1);
		return -EINVAL;
	}
	memset(ar->bmi.cmd_buf, 0, size);

	ath6kl_dbg(ATH6KL_DBG_BMI,
		   "bmi read memory: device: addr: 0x%x, len: %d\n",
		   addr, len);

	len_remain = len;

	while (len_remain) {
		rx_len = (len_remain < max_data_sz) ?
					len_remain : max_data_sz;
		offset = 0;
		memcpy(&(ar->bmi.cmd_buf[offset]), &cid, sizeof(cid));
		offset += sizeof(cid);
		memcpy(&(ar->bmi.cmd_buf[offset]), &addr, sizeof(addr));
		offset += sizeof(addr);
		memcpy(&(ar->bmi.cmd_buf[offset]), &rx_len, sizeof(rx_len));
		offset += sizeof(len);

		ret = ath6kl_hif_bmi_send_buf(ar, ar->bmi.cmd_buf,
					offset);
		if (ret) {
			ath6kl_err("Unable to write to the device: %d\n",
				   ret);
			return ret;
		}
		ret = ath6kl_hif_bmi_recv_buf(ar, ar->bmi.cmd_buf,
					rx_len, true);
		if (ret) {
			ath6kl_err("Unable to read from the device: %d\n",
				   ret);
			return ret;
		}
		memcpy(&buf[len - len_remain], ar->bmi.cmd_buf, rx_len);
		len_remain -= rx_len; addr += rx_len;
	}

	return 0;
}

int ath6kl_bmi_write(struct ath6kl *ar, u32 addr, u8 *buf, u32 len)
{
	u32 cid = BMI_WRITE_MEMORY;
	int ret;
	u32 offset;
	u32 len_remain, tx_len;
	const u32 header = sizeof(cid) + sizeof(addr) + sizeof(len);
	u32 max_data_sz = ath6kl_bmi_get_max_data_size(ar);
	u32 max_cmd_sz = ath6kl_bmi_get_max_cmd_size(ar);
	u8 aligned_buf[max_data_sz];
	u8 *src;

	if (ar->bmi.done_sent) {
		ath6kl_err("bmi done sent already, cmd %d disallowed\n", cid);
		return -EACCES;
	}

	if ((max_data_sz + header) > max_cmd_sz) {
		WARN_ON(1);
		return -EINVAL;
	}

	memset(ar->bmi.cmd_buf, 0, max_data_sz + header);

	ath6kl_dbg(ATH6KL_DBG_BMI,
		  "bmi write memory: addr: 0x%x, len: %d\n", addr, len);

	len_remain = len;
	while (len_remain) {
		src = &buf[len - len_remain];

		if (len_remain < (max_data_sz - header)) {
			if (len_remain & 3) {
				/* align it with 4 bytes */
				len_remain = len_remain +
					     (4 - (len_remain & 3));
				memcpy(aligned_buf, src, len_remain);
				src = aligned_buf;
			}
			tx_len = len_remain;
		} else {
			tx_len = (max_data_sz - header);
		}

		offset = 0;
		memcpy(&(ar->bmi.cmd_buf[offset]), &cid, sizeof(cid));
		offset += sizeof(cid);
		memcpy(&(ar->bmi.cmd_buf[offset]), &addr, sizeof(addr));
		offset += sizeof(addr);
		memcpy(&(ar->bmi.cmd_buf[offset]), &tx_len, sizeof(tx_len));
		offset += sizeof(tx_len);
		memcpy(&(ar->bmi.cmd_buf[offset]), src, tx_len);
		offset += tx_len;

		ret = ath6kl_hif_bmi_send_buf(ar, ar->bmi.cmd_buf, offset);
		if (ret) {
			ath6kl_err("Unable to write to the device: %d\n",
				   ret);
			return ret;
		}
		len_remain -= tx_len; addr += tx_len;
	}

	return 0;
}

int ath6kl_bmi_execute(struct ath6kl *ar, u32 addr, u32 *param)
{
	u32 cid = BMI_EXECUTE;
	int ret;
	u32 offset;
	u32 max_cmd_sz = ath6kl_bmi_get_max_cmd_size(ar);
	u16 size;

	if (ar->bmi.done_sent) {
		ath6kl_err("bmi done sent already, cmd %d disallowed\n", cid);
		return -EACCES;
	}

	size = sizeof(cid) + sizeof(addr) + sizeof(param);
	if (size > max_cmd_sz) {
		WARN_ON(1);
		return -EINVAL;
	}
	memset(ar->bmi.cmd_buf, 0, size);

	ath6kl_dbg(ATH6KL_DBG_BMI, "bmi execute: addr: 0x%x, param: %d)\n",
		   addr, *param);

	offset = 0;
	memcpy(&(ar->bmi.cmd_buf[offset]), &cid, sizeof(cid));
	offset += sizeof(cid);
	memcpy(&(ar->bmi.cmd_buf[offset]), &addr, sizeof(addr));
	offset += sizeof(addr);
	memcpy(&(ar->bmi.cmd_buf[offset]), param, sizeof(*param));
	offset += sizeof(*param);

	ret = ath6kl_hif_bmi_send_buf(ar, ar->bmi.cmd_buf, offset);
	if (ret) {
		ath6kl_err("Unable to write to the device: %d\n", ret);
		return ret;
	}

	ret = ath6kl_hif_bmi_recv_buf(ar, ar->bmi.cmd_buf,
				sizeof(*param), false);
	if (ret) {
		ath6kl_err("Unable to read from the device: %d\n", ret);
		return ret;
	}

	memcpy(param, ar->bmi.cmd_buf, sizeof(*param));

	return 0;
}

int ath6kl_bmi_set_app_start(struct ath6kl *ar, u32 addr)
{
	u32 cid = BMI_SET_APP_START;
	int ret;
	u32 offset;
	u32 max_cmd_sz = ath6kl_bmi_get_max_cmd_size(ar);
	u16 size;

	if (ar->bmi.done_sent) {
		ath6kl_err("bmi done sent already, cmd %d disallowed\n", cid);
		return -EACCES;
	}

	size = sizeof(cid) + sizeof(addr);
	if (size > max_cmd_sz) {
		WARN_ON(1);
		return -EINVAL;
	}
	memset(ar->bmi.cmd_buf, 0, size);

	ath6kl_dbg(ATH6KL_DBG_BMI, "bmi set app start: addr: 0x%x\n", addr);

	offset = 0;
	memcpy(&(ar->bmi.cmd_buf[offset]), &cid, sizeof(cid));
	offset += sizeof(cid);
	memcpy(&(ar->bmi.cmd_buf[offset]), &addr, sizeof(addr));
	offset += sizeof(addr);

	ret = ath6kl_hif_bmi_send_buf(ar, ar->bmi.cmd_buf, offset);
	if (ret) {
		ath6kl_err("Unable to write to the device: %d\n", ret);
		return ret;
	}

	return 0;
}

int ath6kl_bmi_reg_read(struct ath6kl *ar, u32 addr, u32 *param)
{
	u32 cid = BMI_READ_SOC_REGISTER;
	int ret;
	u32 offset;
	u32 max_cmd_sz = ath6kl_bmi_get_max_cmd_size(ar);
	u16 size;

	if (ar->bmi.done_sent) {
		ath6kl_err("bmi done sent already, cmd %d disallowed\n", cid);
		return -EACCES;
	}

	size = sizeof(cid) + sizeof(addr);
	if (size > max_cmd_sz) {
		WARN_ON(1);
		return -EINVAL;
	}
	memset(ar->bmi.cmd_buf, 0, size);

	ath6kl_dbg(ATH6KL_DBG_BMI, "bmi read SOC reg: addr: 0x%x\n", addr);

	offset = 0;
	memcpy(&(ar->bmi.cmd_buf[offset]), &cid, sizeof(cid));
	offset += sizeof(cid);
	memcpy(&(ar->bmi.cmd_buf[offset]), &addr, sizeof(addr));
	offset += sizeof(addr);

	ret = ath6kl_hif_bmi_send_buf(ar, ar->bmi.cmd_buf, offset);
	if (ret) {
		ath6kl_err("Unable to write to the device: %d\n", ret);
		return ret;
	}

	ret = ath6kl_hif_bmi_recv_buf(ar, ar->bmi.cmd_buf,
				sizeof(*param), true);
	if (ret) {
		ath6kl_err("Unable to read from the device: %d\n", ret);
		return ret;
	}
	memcpy(param, ar->bmi.cmd_buf, sizeof(*param));

	return 0;
}

int ath6kl_bmi_reg_write(struct ath6kl *ar, u32 addr, u32 param)
{
	u32 cid = BMI_WRITE_SOC_REGISTER;
	int ret;
	u32 offset;
	u32 max_cmd_sz = ath6kl_bmi_get_max_cmd_size(ar);
	u16 size;

	if (ar->bmi.done_sent) {
		ath6kl_err("bmi done sent already, cmd %d disallowed\n", cid);
		return -EACCES;
	}

	size = sizeof(cid) + sizeof(addr) + sizeof(param);
	if (size > max_cmd_sz) {
		WARN_ON(1);
		return -EINVAL;
	}
	memset(ar->bmi.cmd_buf, 0, size);

	ath6kl_dbg(ATH6KL_DBG_BMI,
		   "bmi write SOC reg: addr: 0x%x, param: %d\n",
		    addr, param);

	offset = 0;
	memcpy(&(ar->bmi.cmd_buf[offset]), &cid, sizeof(cid));
	offset += sizeof(cid);
	memcpy(&(ar->bmi.cmd_buf[offset]), &addr, sizeof(addr));
	offset += sizeof(addr);
	memcpy(&(ar->bmi.cmd_buf[offset]), &param, sizeof(param));
	offset += sizeof(param);

	ret = ath6kl_hif_bmi_send_buf(ar, ar->bmi.cmd_buf, offset);
	if (ret) {
		ath6kl_err("Unable to write to the device: %d\n", ret);
		return ret;
	}

	return 0;
}

int ath6kl_bmi_lz_data(struct ath6kl *ar, u8 *buf, u32 len)
{
	u32 cid = BMI_LZ_DATA;
	int ret;
	u32 offset;
	u32 len_remain, tx_len;
	const u32 header = sizeof(cid) + sizeof(len);
	u32 max_data_sz = ath6kl_bmi_get_max_data_size(ar);
	u32 max_cmd_sz = ath6kl_bmi_get_max_cmd_size(ar);
	u16 size;

	if (ar->bmi.done_sent) {
		ath6kl_err("bmi done sent already, cmd %d disallowed\n", cid);
		return -EACCES;
	}

	size = max_data_sz + header;
	if (size > max_cmd_sz) {
		WARN_ON(1);
		return -EINVAL;
	}
	memset(ar->bmi.cmd_buf, 0, size);

	ath6kl_dbg(ATH6KL_DBG_BMI, "bmi send LZ data: len: %d)\n",
		   len);

	len_remain = len;
	while (len_remain) {
		tx_len = (len_remain < (max_data_sz - header)) ?
			  len_remain : (max_data_sz - header);

		offset = 0;
		memcpy(&(ar->bmi.cmd_buf[offset]), &cid, sizeof(cid));
		offset += sizeof(cid);
		memcpy(&(ar->bmi.cmd_buf[offset]), &tx_len, sizeof(tx_len));
		offset += sizeof(tx_len);
		memcpy(&(ar->bmi.cmd_buf[offset]), &buf[len - len_remain],
			tx_len);
		offset += tx_len;

		ret = ath6kl_hif_bmi_send_buf(ar, ar->bmi.cmd_buf, offset);
		if (ret) {
			ath6kl_err("Unable to write to the device: %d\n",
				   ret);
			return ret;
		}

		len_remain -= tx_len;
	}

	return 0;
}

int ath6kl_bmi_lz_stream_start(struct ath6kl *ar, u32 addr)
{
	u32 cid = BMI_LZ_STREAM_START;
	int ret;
	u32 offset;
	u32 max_cmd_sz = ath6kl_bmi_get_max_cmd_size(ar);
	u16 size;

	if (ar->bmi.done_sent) {
		ath6kl_err("bmi done sent already, cmd %d disallowed\n", cid);
		return -EACCES;
	}

	size = sizeof(cid) + sizeof(addr);
	if (size > max_cmd_sz) {
		WARN_ON(1);
		return -EINVAL;
	}
	memset(ar->bmi.cmd_buf, 0, size);

	ath6kl_dbg(ATH6KL_DBG_BMI,
		   "bmi LZ stream start: addr: 0x%x)\n",
		    addr);

	offset = 0;
	memcpy(&(ar->bmi.cmd_buf[offset]), &cid, sizeof(cid));
	offset += sizeof(cid);
	memcpy(&(ar->bmi.cmd_buf[offset]), &addr, sizeof(addr));
	offset += sizeof(addr);

	ret = ath6kl_hif_bmi_send_buf(ar, ar->bmi.cmd_buf, offset);
	if (ret) {
		ath6kl_err("Unable to start LZ stream to the device: %d\n",
			   ret);
		return ret;
	}

	return 0;
}

int ath6kl_bmi_fast_download(struct ath6kl *ar, u32 addr, u8 *buf, u32 len)
{
	int ret;
	u32 last_word = 0;
	u32 last_word_offset = len & ~0x3;
	u32 unaligned_bytes = len & 0x3;

	ret = ath6kl_bmi_lz_stream_start(ar, addr);
	if (ret)
		return ret;

	if (unaligned_bytes) {
		/* copy the last word into a zero padded buffer */
		memcpy(&last_word, &buf[last_word_offset], unaligned_bytes);
	}

	ret = ath6kl_bmi_lz_data(ar, buf, last_word_offset);
	if (ret)
		return ret;

	if (unaligned_bytes)
		ret = ath6kl_bmi_lz_data(ar, (u8 *)&last_word, 4);

	if (!ret) {
		/* Close compressed stream and open a new (fake) one.
		 * This serves mainly to flush Target caches. */
		ret = ath6kl_bmi_lz_stream_start(ar, 0x00);
	}
	return ret;
}

int ath6kl_bmi_init(struct ath6kl *ar)
{
	u32 max_cmd_sz = ath6kl_bmi_get_max_cmd_size(ar);

	ar->bmi.cmd_buf = kzalloc(max_cmd_sz, GFP_ATOMIC);

	if (!ar->bmi.cmd_buf)
		return -ENOMEM;

	return 0;
}

void ath6kl_bmi_cleanup(struct ath6kl *ar)
{
	kfree(ar->bmi.cmd_buf);
	ar->bmi.cmd_buf = NULL;
}
