// SPDX-License-Identifier: GPL-2.0
#include <linux/init.h>
#include <linux/io.h>
#include <linux/delay.h>
#include <linux/kernel.h>

#define PW_BASE		0x1000d000UL
#define PW_SIZE		0x1000
#define WACS2_CMD	0xC20
#define WACS2_RDATA	0xC24
#define WACS2_VLDCLR	0xC28

static void __iomem *pw;

static int a02_pw_read(u32 adr, u32 *val)
{
	int t;
	u32 r;

	for (t = 0; t < 10000; t++) {
		r = readl(pw + WACS2_RDATA);
		if (((r >> 16) & 7) == 0)
			break;
		udelay(1);
	}
	if (t == 10000)
		return -1;
	writel(((adr >> 1) << 16), pw + WACS2_CMD);
	for (t = 0; t < 10000; t++) {
		r = readl(pw + WACS2_RDATA);
		if (((r >> 16) & 7) == 6)
			break;
		udelay(1);
	}
	if (t == 10000)
		return -2;
	*val = r & 0xffff;
	writel(1, pw + WACS2_VLDCLR);
	return 0;
}

static int __init a02_pw_init(void)
{
	u32 v;
	int ret, i;
	static const u32 adrs[] = { 0x08, 0x0a, 0x0c, 0x0e };

	pw = ioremap(PW_BASE, PW_SIZE);
	if (!pw)
		return 0;
	pr_info("a02_pw: init_done@0xa0=0x%x @0x94=0x%x rdata=0x%x\n",
		readl(pw + 0xA0), readl(pw + 0x94), readl(pw + WACS2_RDATA));
	if (!readl(pw + 0xA0) && !readl(pw + 0x94)) {
		pr_info("a02_pw: pwrap belum init, skip\n");
		return 0;
	}
	for (i = 0; i < ARRAY_SIZE(adrs); i++) {
		ret = a02_pw_read(adrs[i], &v);
		pr_info("a02_pw: reg[0x%04x] ret=%d val=0x%04x\n", adrs[i], ret, ret ? 0 : v);
	}
	return 0;
}
late_initcall(a02_pw_init);
