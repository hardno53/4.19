// SPDX-License-Identifier: GPL-2.0
#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/input.h>
#include <linux/interrupt.h>
#include <linux/io.h>
#include <linux/of.h>
#include <linux/of_irq.h>
#include <linux/of_address.h>

#define KP_MEM(i)	(0x04 + (i) * 4)
#define KP_DEBOUNCE	0x18
#define KP_EN		0x24
#define NMEM		5

struct a02_kpd {
	void __iomem *base;
	struct input_dev *in;
	u16 old[NMEM];
};

static unsigned int a02_keymap(unsigned int hw)
{
	return hw == 1 ? KEY_VOLUMEUP : 0;
}

static irqreturn_t a02_kpd_irq(int irq, void *d)
{
	struct a02_kpd *k = d;
	int i, j;

	for (i = 0; i < NMEM; i++) {
		u16 new = readw(k->base + KP_MEM(i));
		u16 ch = new ^ k->old[i];

		for (j = 0; j < 16 && ch; j++) {
			unsigned int hw = i * 16 + j, code;
			bool pressed = !(new & (1U << j));

			if (!(ch & (1U << j)))
				continue;
			if (hw >= 72)
				continue;
			pr_info("a02_kpd: hw=%u %s\n", hw, pressed ? "down" : "up");
			code = a02_keymap(hw);
			if (code) {
				input_report_key(k->in, code, pressed);
				input_sync(k->in);
			}
		}
		k->old[i] = new;
	}
	return IRQ_HANDLED;
}

static int a02_kpd_probe(struct platform_device *pdev)
{
	struct a02_kpd *k;
	int irq, ret, i;

	k = devm_kzalloc(&pdev->dev, sizeof(*k), GFP_KERNEL);
	if (!k)
		return -ENOMEM;
	k->base = of_iomap(pdev->dev.of_node, 0);
	if (!k->base)
		return -ENODEV;
	irq = irq_of_parse_and_map(pdev->dev.of_node, 0);
	if (!irq)
		return -ENODEV;
	k->in = devm_input_allocate_device(&pdev->dev);
	if (!k->in)
		return -ENOMEM;
	k->in->name = "mtk-kpd";
	k->in->id.bustype = BUS_HOST;
	__set_bit(EV_KEY, k->in->evbit);
	__set_bit(KEY_VOLUMEUP, k->in->keybit);
	ret = input_register_device(k->in);
	if (ret)
		return ret;
	for (i = 0; i < NMEM; i++)
		k->old[i] = 0xffff;
	k->old[NMEM - 1] = 0x00ff;
	writew(1024 & 0x3fff, k->base + KP_DEBOUNCE);
	writew(1, k->base + KP_EN);
	pr_info("a02_kpd: probe ok irq=%d mem=%04x %04x %04x %04x %04x\n", irq,
		readw(k->base + KP_MEM(0)), readw(k->base + KP_MEM(1)),
		readw(k->base + KP_MEM(2)), readw(k->base + KP_MEM(3)),
		readw(k->base + KP_MEM(4)));
	return devm_request_threaded_irq(&pdev->dev, irq, NULL, a02_kpd_irq,
					 IRQF_ONESHOT, "a02_kpd", k);
}

static const struct of_device_id a02_kpd_of[] = {
	{ .compatible = "mediatek,kp" },
	{ }
};

static struct platform_driver a02_kpd_drv = {
	.probe = a02_kpd_probe,
	.driver = { .name = "a02_kpd", .of_match_table = a02_kpd_of },
};
builtin_platform_driver(a02_kpd_drv);
