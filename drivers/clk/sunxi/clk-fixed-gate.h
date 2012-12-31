#ifndef __MACH_SUNXI_CLK_FIXED_GATE_H
#define __MACH_SUNXI_CLK_FIXED_GATE_H

#include <linux/clk-provider.h>
#include <linux/clkdev.h>

struct clk *clk_register_fixed_gate(struct device *dev, const char *name,
		const char *parent_name, unsigned long flags,
		void __iomem *reg, u8 bit_idx, unsigned long fixed_rate,
		spinlock_t *lock);

#endif
