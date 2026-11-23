// SPDX-License-Identifier: GPL-2.0
// Rekonstruksi sec_rr dari disassembly kernel mmc43
#include <linux/init.h>
#include <linux/io.h>
#include <linux/kernel.h>
#include <linux/notifier.h>
#include <linux/reboot.h>
#include <linux/string.h>

#define SEC_RR_BASE   0x10dc00
#define SEC_RR_SIZE   0x800
#define SEC_RR_MAGIC  0x43474244

#define OFF_IS_UPLOAD 0x354
#define OFF_UPLOAD_RSN 0x358
#define OFF_IS_PWR_RST 0x35c
#define OFF_PWR_RST_RSN 0x360

static void __iomem *rc;
static u32 off_linux;

static void rr_w(u32 off, u32 val)
{
	wmb();
	writel(val, rc + off_linux + off);
}

static int sec_rr_reboot_notify(struct notifier_block *nb,
				unsigned long action, void *data)
{
	const char *cmd = data;
	u32 reason = 0x12345670;

	if (!rc)
		return 0;

	if (action == SYS_RESTART) {
		if (cmd) {
			if (!strcmp(cmd, "recovery-update"))
				reason += 5;
			else if (!strcmp(cmd, "fota_bl"))
				reason += 6;
			else if (!strcmp(cmd, "recovery"))
				reason += 4;
			else if (!strcmp(cmd, "download"))
				reason += 1;
			else if (!strcmp(cmd, "bootloader"))
				reason += 0xd;
			else if (!strcmp(cmd, "upload"))
				reason += 2;
			else if (!strcmp(cmd, "secure"))
				reason += 7;
			else if (!strcmp(cmd, "fwup"))
				reason += 9;
		}
		rr_w(OFF_IS_PWR_RST, 0x12345678);
		rr_w(OFF_PWR_RST_RSN, reason);
		pr_info("sec_rr: restart cmd=%s reason=0x%x\n",
			cmd ? cmd : "(null)", reason);
	}
	rr_w(OFF_IS_UPLOAD, 0);
	return 0;
}

static struct notifier_block sec_rr_nb = {
	.notifier_call = sec_rr_reboot_notify,
};

static int __init sec_rr_init(void)
{
	u32 off_lk, off_llk, sz_lk, al, sz_end;

	rc = ioremap(SEC_RR_BASE, SEC_RR_SIZE);
	if (!rc) {
		pr_err("sec_rr: ioremap failed\n");
		return -ENOMEM;
	}
	if (readl(rc) != SEC_RR_MAGIC) {
		pr_info("sec_rr: bad sig 0x%x, reinit header\n", readl(rc));
		memset_io(rc, 0, SEC_RR_SIZE);
		wmb();
		writel(SEC_RR_MAGIC, rc);
	}
	off_lk = readl(rc + 0x10);
	off_llk = readl(rc + 0x14);
	sz_lk = readl(rc + 0x18);

	if (!sz_lk) {
		off_linux = 0x200;
	} else {
		al = (sz_lk + 0x3f) & ~0x3fu;
		off_linux = (al + off_lk == off_llk) ? al + off_llk : 0x200;
	}
	sz_end = off_linux + 0x480;
	if (sz_end >= SEC_RR_SIZE) {
		pr_err("sec_rr: layout does not fit (off_linux=%u)\n", off_linux);
		iounmap(rc);
		rc = NULL;
		return -EINVAL;
	}
	wmb(); writel(SEC_RR_SIZE, rc + 0x28);
	wmb(); writel(off_linux, rc + 0x2c);
	wmb(); writel(sz_end, rc + 0x30);
	wmb(); writel(0x380 - off_linux, rc + 0x3c);
	wmb(); writel(0, rc + 0x34);
	wmb(); writel(0, rc + 0x38);
	memset_io(rc + off_linux, 0, SEC_RR_SIZE - off_linux);
	wmb();
	rr_w(OFF_IS_UPLOAD, 0x66262564);
	rr_w(OFF_UPLOAD_RSN, 0xcafebabe);
	rr_w(OFF_IS_PWR_RST, 0);
	rr_w(OFF_PWR_RST_RSN, 0xcafebabe);
	register_reboot_notifier(&sec_rr_nb);
	pr_info("sec_rr: ready, off_linux=%u off_lk=%u off_llk=%u sz_lk=%u\n",
		off_linux, off_lk, off_llk, sz_lk);
	return 0;
}
device_initcall(sec_rr_init);
