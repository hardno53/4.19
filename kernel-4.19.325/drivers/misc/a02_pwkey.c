// SPDX-License-Identifier: GPL-2.0
#include <linux/init.h>
#include <linux/io.h>
#include <linux/delay.h>
#include <linux/input.h>
#include <linux/workqueue.h>
#include <linux/jiffies.h>
#include <linux/kernel.h>

#define WACS2_CMD	0xC20
#define WACS2_RDATA	0xC24
#define WACS2_VLDCLR	0xC28
#define TOPSTATUS	0x24

static void __iomem *pw;
static struct delayed_work dw;
static struct input_dev *in;
static int last_pwr = 1, last_home = 1;

static int rd(u32 adr, u32 *val)
{
	int t;
	u32 r;

	for (t = 0; t < 10000; t++) {
		if (((readl(pw + WACS2_RDATA) >> 16) & 7) == 0)
			break;
		udelay(1);
	}
	if (t == 10000)
		return -1;
	writel((adr >> 1) << 16, pw + WACS2_CMD);
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

static void poll(struct work_struct *w)
{
	u32 v;

	if (!rd(TOPSTATUS, &v)) {
		int pwr = (v >> 1) & 1, home = (v >> 3) & 1;

		if (pwr != last_pwr) {
			pr_info("a02_pwkey: power %s\n", pwr ? "up" : "down");
			input_report_key(in, KEY_POWER, !pwr);
			input_sync(in);
			last_pwr = pwr;
		}
		if (home != last_home) {
			pr_info("a02_pwkey: voldown %s\n", home ? "up" : "down");
			input_report_key(in, KEY_VOLUMEDOWN, !home);
			input_sync(in);
			last_home = home;
		}
	}
	schedule_delayed_work(&dw, msecs_to_jiffies(30));
}

static int __init a02_pwkey_init(void)
{
	pw = ioremap(0x1000d000UL, 0x1000);
	if (!pw)
		return 0;
	in = input_allocate_device();
	if (!in)
		return 0;
	in->name = "mtk-pmic-keys";
	in->id.bustype = BUS_HOST;
	__set_bit(EV_KEY, in->evbit);
	__set_bit(KEY_POWER, in->keybit);
	__set_bit(KEY_VOLUMEDOWN, in->keybit);
	if (input_register_device(in)) {
		input_free_device(in);
		return 0;
	}
	INIT_DELAYED_WORK(&dw, poll);
	schedule_delayed_work(&dw, 5 * HZ);
	return 0;
}
late_initcall(a02_pwkey_init);
