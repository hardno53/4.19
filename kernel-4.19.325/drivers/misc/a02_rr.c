// SPDX-License-Identifier: GPL-2.0
#include <linux/init.h>
#include <linux/io.h>
#include <linux/kernel.h>
#include <linux/notifier.h>
#include <linux/reboot.h>
#include <linux/string.h>

#define RC_BASE		0x44400000UL
#define RC_SIZE		0x10000
#define OFF_IS_UPLOAD	0x354
#define OFF_UPLOAD_RSN	0x358
#define OFF_IS_PWR_RST	0x35c
#define OFF_PWR_RST_RSN	0x360
#define SEC_POWER_RESET	0x12345678
#define RR_RECOVERY	0x12345674

static void __iomem *rc;
static u32 off_linux;

static int a02_rr_restart(struct notifier_block *nb, unsigned long mode, void *cmd)
{
	if (!rc || !cmd || strcmp(cmd, "recovery"))
		return NOTIFY_DONE;
	writel(SEC_POWER_RESET, rc + off_linux + OFF_IS_PWR_RST);
	writel(RR_RECOVERY, rc + off_linux + OFF_PWR_RST_RSN);
	writel(0, rc + off_linux + OFF_IS_UPLOAD);
	wmb();
	return NOTIFY_DONE;
}

static struct notifier_block a02_rr_nb = {
	.notifier_call = a02_rr_restart,
	.priority = 255,
};

static int __init a02_rr_init(void)
{
	int i;
	u32 sz;

	rc = ioremap_wc(RC_BASE, RC_SIZE);
	if (!rc) {
		pr_err("a02_rr: ioremap gagal\n");
		return 0;
	}
	for (i = 0; i < 16; i++)
		pr_info("a02_rr: hdr[%02d]=0x%08x\n", i, readl(rc + i * 4));
	off_linux = readl(rc + 44);
	sz = readl(rc + 40);
	if (!sz || sz > RC_SIZE || off_linux >= sz ||
	    off_linux + OFF_PWR_RST_RSN + 4 > RC_SIZE) {
		pr_err("a02_rr: header tidak valid off_linux=0x%x sz=0x%x\n", off_linux, sz);
		iounmap(rc);
		rc = NULL;
		return 0;
	}
	pr_info("a02_rr: off_linux=0x%x is_upload=0x%x rsn=0x%x is_pwr=0x%x pwr_rsn=0x%x\n",
		off_linux,
		readl(rc + off_linux + OFF_IS_UPLOAD),
		readl(rc + off_linux + OFF_UPLOAD_RSN),
		readl(rc + off_linux + OFF_IS_PWR_RST),
		readl(rc + off_linux + OFF_PWR_RST_RSN));
	register_restart_handler(&a02_rr_nb);
	return 0;
}
late_initcall(a02_rr_init);
