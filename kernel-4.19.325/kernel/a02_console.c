// SPDX-License-Identifier: GPL-2.0
/* A02 debug: salin log kernel ke zona console ramoops (0x4445f000) */
#include <linux/init.h>
#include <linux/types.h>
#include <linux/io.h>
#include <linux/console.h>

#define A02_CBASE 0x4445F000UL
#define A02_CSIZE 0x40000UL

static void __iomem *a02_cb;
static u32 a02_pos;
static bool a02_wrapped;

static void a02_cwrite(struct console *c, const char *s, unsigned int n)
{
	u32 cap = A02_CSIZE - 12;
	unsigned int i;

	if (!a02_cb)
		return;
	for (i = 0; i < n; i++) {
		__raw_writeb(s[i], a02_cb + 12 + a02_pos);
		if (++a02_pos >= cap) {
			a02_pos = 0;
			a02_wrapped = true;
		}
	}
	__raw_writel(a02_pos, a02_cb + 4);
	__raw_writel(a02_wrapped ? cap : a02_pos, a02_cb + 8);
	wmb();
}

static struct console a02_con = {
	.name = "a02ram",
	.write = a02_cwrite,
	.flags = CON_PRINTBUFFER | CON_ENABLED | CON_ANYTIME,
	.index = -1,
};

void __init a02_con_register(void)
{
	a02_cb = ioremap(A02_CBASE, A02_CSIZE);
	if (!a02_cb)
		return;
	__raw_writel(0x43474244, a02_cb);
	__raw_writel(0, a02_cb + 4);
	__raw_writel(0, a02_cb + 8);
	register_console(&a02_con);
}
