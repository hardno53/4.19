// Rekonstruksi sec_rr dari disassembly kernel mmc43
#include <linux/init.h>
#include <linux/io.h>
#include <linux/kernel.h>
#include <linux/notifier.h>
#include <linux/reboot.h>
#include <linux/string.h>

#define SRR_BASE	0x0010dc00
#define SRR_SIZE	0x800
#define SRR_MAGIC	0x43474244
#define OFF_IS_UPLOAD	0x354
#define OFF_UPLOAD_RSN	0x358
#define OFF_IS_PWR_RST	0x35c
#define OFF_PWR_RST_RSN	0x360

static void __iomem *rc;
static u32 off_linux;

static int sec_rr_reboot_notify(struct notifier_block *nb,
				unsigned long action, void *cmd)
{
	u32 rsn = 0x12345670;
	const char *c = cmd;

	if (!rc)
		return 0;

	if (action != SYS_RESTART) {
		wmb();
		writel(0, rc + off_linux + OFF_IS_UPLOAD);
		wmb();
		return 0;
	}

	if (!c)
		c = "(null)";
	else if (!strcmp(c, "recovery-update"))
		rsn += 5;
	else if (!strcmp(c, "fota_bl"))
		rsn += 6;
	else if (!strcmp(c, "recovery"))
		rsn += 4;
	else if (!strcmp(c, "download"))
		rsn += 1;
	else if (!strcmp(c, "bootloader"))
		rsn += 0xd;
	else if (!strcmp(c, "upload"))
		rsn += 2;
	else if (!strcmp(c, "secure"))
		rsn += 7;
	else if (!strcmp(c, "fwup"))
		rsn += 9;

	wmb();
	writel(0x12345678, rc + off_linux + OFF_IS_PWR_RST);
	wmb();
	writel(rsn, rc + off_linux + OFF_PWR_RST_RSN);
	pr_emerg("sec_rr: restart cmd=%s reason=0x%x\n", c, rsn);
	return 0;
}

static struct notifier_block sec_rr_nb = {
	.notifier_call = sec_rr_reboot_notify,
};

static int __init sec_rr_init(void)
{
	u32 off_lk, off_llk, sz_lk, sz;

	rc = ioremap(SRR_BASE, SRR_SIZE);
	if (!rc) {
		pr_emerg("sec_rr: ioremap failed\n");
		return -ENOMEM;
	}

	if (readl(rc) != SRR_MAGIC) {
		pr_emerg("sec_rr: bad sig 0x%x, reinit header\n", readl(rc));
		memset_io(rc, 0, SRR_SIZE);
		wmb();
		writel(SRR_MAGIC, rc);
	}

	off_lk = readl(rc + 0x10);
	off_llk = readl(rc + 0x14);
	sz_lk = readl(rc + 0x18);

	if (!sz_lk) {
		off_linux = 0x200;
		sz = 0x680;
	} else {
		sz = (sz_lk + 0x3f) & 0xffffffc0;
		off_linux = (sz + off_lk == off_llk) ? sz + off_llk : 0x200;
		if (off_linux + 0x480 >= SRR_SIZE) {
			pr_emerg("sec_rr: layout does not fit (off_linux=%u)\n",
				 off_linux);
			iounmap(rc);
			rc = NULL;
			return -EINVAL;
		}
	}

	wmb();
	writel(SRR_SIZE, rc + 0x28);
	wmb();
	writel(off_linux, rc + 0x2c);
	wmb();
	writel(sz, rc + 0x30);
	wmb();
	writel(0x380 - off_linux, rc + 0x3c);
	wmb();
	writel(0, rc + 0x34);
	wmb();
	writel(0, rc + 0x38);
	memset_io(rc + off_linux, 0, SRR_SIZE - off_linux);
	wmb();
	writel(0x66262564, rc + off_linux + OFF_IS_UPLOAD);
	wmb();
	writel(0xcafebabe, rc + off_linux + OFF_UPLOAD_RSN);
	wmb();
	writel(0, rc + off_linux + OFF_IS_PWR_RST);
	wmb();
	writel(0xcafebabe, rc + off_linux + OFF_PWR_RST_RSN);

	register_reboot_notifier(&sec_rr_nb);
	pr_emerg("sec_rr: ready, off_linux=%u off_lk=%u off_llk=%u sz_lk=%u\n",
		 off_linux, off_lk, off_llk, sz_lk);
	return 0;
}
device_initcall(sec_rr_init);
