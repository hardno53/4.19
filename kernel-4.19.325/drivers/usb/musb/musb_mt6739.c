// SPDX-License-Identifier: GPL-2.0
/*
 * MediaTek MT6739 MUSB glue (minimal, peripheral only, PIO)
 * Dibuat buat port A02 (SM-A022F). Register dari vendor 4.14 usb20.
 */
#include <linux/clk.h>
#include <linux/delay.h>
#include <linux/io.h>
#include <linux/module.h>
#include <linux/of.h>
#include <linux/of_address.h>
#include <linux/of_irq.h>
#include <linux/platform_device.h>
#include <linux/regulator/consumer.h>
#include <linux/usb/usb_phy_generic.h>

#include "musb_core.h"

#define MTK_L1INTS	0xa0
#define MTK_L1INTM	0xa4
#define MTK_L1INTP	0xa8
#define L1_TX		BIT(0)
#define L1_RX		BIT(1)
#define L1_USBCOM	BIT(2)
#define L1_DMA		BIT(3)

#define PHY_OFF		0x800

struct mtk_glue {
	struct device *dev;
	struct platform_device *musb;
	void __iomem *phy;	/* base PHY + 0x800 */
	struct clk *clks[3];
	struct regulator *vusb33;
};

static inline u32 prd(struct mtk_glue *g, u32 off)
{
	return readl(g->phy + off);
}

static inline void pclr(struct mtk_glue *g, u32 off, u32 m)
{
	writel(readl(g->phy + off) & ~m, g->phy + off);
}

static inline void pset(struct mtk_glue *g, u32 off, u32 m)
{
	writel(readl(g->phy + off) | m, g->phy + off);
}

static void mtk_phy_tuning(struct mtk_glue *g)
{
	struct device_node *np, *c;

	np = of_find_compatible_node(NULL, NULL, "mediatek,phy_tuning");
	if (!np) {
		dev_info(g->dev, "no phy_tuning node\n");
		return;
	}
	for_each_child_of_node(np, c) {
		u32 off, sh, mask, val, host = 0;

		if (of_property_read_u32(c, "offset", &off) ||
		    of_property_read_u32(c, "shift", &sh) ||
		    of_property_read_u32(c, "mask", &mask) ||
		    of_property_read_u32(c, "value", &val))
			continue;
		of_property_read_u32(c, "host", &host);
		if (host)
			continue;	/* DUGAAN: host=1 -> khusus host mode */
		pclr(g, off, mask << sh);
		pset(g, off, (val & mask) << sh);
		dev_info(g->dev, "tune %s off=0x%x sh=%u m=0x%x v=0x%x\n",
			 c->name, off, sh, mask, val);
	}
	of_node_put(np);
}

static void mtk_phy_device_init(struct mtk_glue *g)
{
	udelay(50);
	pclr(g, 0x1c, BIT(12));
	pclr(g, 0x68, BIT(26));
	pclr(g, 0x6c, BIT(16));
	pclr(g, 0x20, BIT(9) | BIT(8));
	pclr(g, 0x68, BIT(18));
	pclr(g, 0x68, BIT(6) | BIT(7));
	pclr(g, 0x68, 0x3 << 4);
	pclr(g, 0x68, BIT(2));
	pclr(g, 0x68, 0xf << 10);
	pclr(g, 0x68, BIT(20) | BIT(21) | BIT(19) | BIT(17) | BIT(23));
	pclr(g, 0x18, BIT(23));
	pset(g, 0x18, BIT(20));
	pclr(g, 0x18, 0xffu << 24);
	pset(g, 0x18, 0x40u << 24);
	udelay(800);

	/* DEV_ACTIVE: paksa VBUS/session valid */
	pclr(g, 0x6c, 0x10);
	pset(g, 0x6c, 0x2f);
	pset(g, 0x6c, 0x3f << 8);

	pclr(g, 0x18, 0xf0);
	pset(g, 0x18, 0x70);	/* DISCTH 700mV */

	mtk_phy_tuning(g);
}

static struct mtk_glue *to_glue(struct musb *musb)
{
	return dev_get_drvdata(musb->controller->parent);
}

static irqreturn_t mtk_musb_interrupt(int irq, void *__hci)
{
	struct musb *musb = __hci;
	unsigned long flags;
	irqreturn_t ret = IRQ_NONE;
	u32 l1;

	spin_lock_irqsave(&musb->lock, flags);

	l1 = readl(musb->mregs + MTK_L1INTS) & readl(musb->mregs + MTK_L1INTM);
	if (l1 & (L1_TX | L1_RX | L1_USBCOM)) {
		/* INTR* di MTK: write-1-to-clear */
		musb->int_usb = musb_readb(musb->mregs, MUSB_INTRUSB);
		musb_writeb(musb->mregs, MUSB_INTRUSB, musb->int_usb);
		musb->int_tx = musb_readw(musb->mregs, MUSB_INTRTX);
		musb_writew(musb->mregs, MUSB_INTRTX, musb->int_tx);
		musb->int_rx = musb_readw(musb->mregs, MUSB_INTRRX);
		musb_writew(musb->mregs, MUSB_INTRRX, musb->int_rx);
		if (musb->int_usb || musb->int_tx || musb->int_rx)
			ret = musb_interrupt(musb);
		else
			ret = IRQ_HANDLED;
	}

	spin_unlock_irqrestore(&musb->lock, flags);
	return ret;
}

static void mtk_musb_enable(struct musb *musb)
{
	writel(L1_TX | L1_RX | L1_USBCOM, musb->mregs + MTK_L1INTM);
}

static void mtk_musb_disable(struct musb *musb)
{
	writel(0, musb->mregs + MTK_L1INTM);
}

static int mtk_musb_init(struct musb *musb)
{
	struct mtk_glue *g = to_glue(musb);

	usb_phy_generic_register();
	musb->xceiv = usb_get_phy(USB_PHY_TYPE_USB2);
	if (IS_ERR(musb->xceiv)) {
		dev_err(g->dev, "no nop xceiv\n");
		return PTR_ERR(musb->xceiv);
	}
	musb->dyn_fifo = true;
	musb->isr = mtk_musb_interrupt;

	mtk_phy_device_init(g);
	writel(0, musb->mregs + MTK_L1INTM);
	dev_info(g->dev, "phy init done, 0x6c=%08x 0x18=%08x\n",
		 prd(g, 0x6c), prd(g, 0x18));
	return 0;
}

static int mtk_musb_exit(struct musb *musb)
{
	usb_put_phy(musb->xceiv);
	return 0;
}

static struct musb_fifo_cfg mtk_fifo_cfg[] = {
	MUSB_EP_FIFO_DOUBLE(1, FIFO_TX, 512),
	MUSB_EP_FIFO_DOUBLE(1, FIFO_RX, 512),
	MUSB_EP_FIFO_SINGLE(2, FIFO_TX, 512),
	MUSB_EP_FIFO_SINGLE(2, FIFO_RX, 512),
	MUSB_EP_FIFO_SINGLE(3, FIFO_TX, 512),
	MUSB_EP_FIFO_SINGLE(3, FIFO_RX, 512),
	MUSB_EP_FIFO_SINGLE(4, FIFO_TX, 512),
	MUSB_EP_FIFO_SINGLE(4, FIFO_RX, 512),
	MUSB_EP_FIFO_SINGLE(5, FIFO_TX, 64),
	MUSB_EP_FIFO_SINGLE(5, FIFO_RX, 64),
	MUSB_EP_FIFO_SINGLE(6, FIFO_TX, 64),
	MUSB_EP_FIFO_SINGLE(6, FIFO_RX, 64),
};

static const struct musb_hdrc_config mtk_musb_config = {
	.multipoint = true,
	.num_eps = 16,
	.ram_bits = 11,	/* 8KB */
	.fifo_cfg = mtk_fifo_cfg,
	.fifo_cfg_size = ARRAY_SIZE(mtk_fifo_cfg),
};

static struct musb_hdrc_platform_data mtk_musb_pdata;

static const struct musb_platform_ops mtk_musb_ops = {
	.quirks		= 0,	/* flat EP regs, tanpa DMA */
	.init		= mtk_musb_init,
	.exit		= mtk_musb_exit,
	.enable		= mtk_musb_enable,
	.disable	= mtk_musb_disable,
};

static int mtk_probe(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;
	struct mtk_glue *g;
	struct platform_device *musb;
	struct resource *mac, *phy, res[2];
	void __iomem *pbase;
	static const char * const cn[3] = {
		"usb0", "usb0_clk_top_sel", "usb0_clk_univpll3_d4" };
	int irq, i, ret;

	g = devm_kzalloc(dev, sizeof(*g), GFP_KERNEL);
	if (!g)
		return -ENOMEM;
	g->dev = dev;

	mac = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	phy = platform_get_resource(pdev, IORESOURCE_MEM, 1);
	irq = platform_get_irq(pdev, 0);
	if (!mac || !phy || irq <= 0) {
		dev_err(dev, "resource kurang mac=%p phy=%p irq=%d\n",
			mac, phy, irq);
		return -ENODEV;
	}
	pbase = devm_ioremap(dev, phy->start, resource_size(phy));
	if (!pbase)
		return -ENOMEM;
	g->phy = pbase + PHY_OFF;

	for (i = 0; i < 3; i++) {
		g->clks[i] = devm_clk_get(dev, cn[i]);
		if (IS_ERR(g->clks[i])) {
			dev_warn(dev, "clk %s: %ld (lanjut)\n", cn[i],
				 PTR_ERR(g->clks[i]));
			g->clks[i] = NULL;
		}
	}
	if (g->clks[1] && g->clks[2])
		clk_set_parent(g->clks[1], g->clks[2]);
	for (i = 0; i < 3; i++) {
		if (g->clks[i] && clk_prepare_enable(g->clks[i]))
			dev_warn(dev, "enable clk %s gagal\n", cn[i]);
	}

	g->vusb33 = devm_regulator_get_optional(dev, "vusb33");
	if (IS_ERR(g->vusb33)) {
		dev_warn(dev, "vusb33: %ld (lanjut)\n", PTR_ERR(g->vusb33));
		g->vusb33 = NULL;
	} else if (regulator_enable(g->vusb33)) {
		dev_warn(dev, "vusb33 enable gagal\n");
	}

	musb = platform_device_alloc("musb-hdrc", PLATFORM_DEVID_AUTO);
	if (!musb)
		return -ENOMEM;
	musb->dev.parent = dev;
	g->musb = musb;

	memset(res, 0, sizeof(res));
	res[0] = *mac;
	res[0].flags = IORESOURCE_MEM;
	res[0].name = NULL;
	res[1].start = irq;
	res[1].end = irq;
	res[1].flags = IORESOURCE_IRQ |
		       (irq_get_trigger_type(irq) & IORESOURCE_IRQ_SHAREABLE);
	res[1].name = "mc";

	mtk_musb_pdata.mode = MUSB_PERIPHERAL;
	mtk_musb_pdata.config = &mtk_musb_config;
	mtk_musb_pdata.platform_ops = &mtk_musb_ops;

	platform_set_drvdata(pdev, g);

	ret = platform_device_add_resources(musb, res, 2);
	if (!ret)
		ret = platform_device_add_data(musb, &mtk_musb_pdata,
					       sizeof(mtk_musb_pdata));
	if (!ret)
		ret = platform_device_add(musb);
	if (ret) {
		dev_err(dev, "add musb-hdrc gagal %d\n", ret);
		platform_device_put(musb);
		return ret;
	}
	dev_info(dev, "musb-hdrc terdaftar (irq %d)\n", irq);
	return 0;
}

static int mtk_remove(struct platform_device *pdev)
{
	struct mtk_glue *g = platform_get_drvdata(pdev);

	platform_device_unregister(g->musb);
	return 0;
}

static const struct of_device_id mtk_of_match[] = {
	{ .compatible = "mediatek,mt6739-usb20" },
	{ }
};
MODULE_DEVICE_TABLE(of, mtk_of_match);

static struct platform_driver mtk_musb_driver = {
	.probe = mtk_probe,
	.remove = mtk_remove,
	.driver = {
		.name = "musb-mt6739",
		.of_match_table = mtk_of_match,
	},
};
module_platform_driver(mtk_musb_driver);

MODULE_DESCRIPTION("MT6739 MUSB glue (minimal)");
MODULE_LICENSE("GPL v2");
