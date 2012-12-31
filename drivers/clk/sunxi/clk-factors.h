#ifndef __MACH_SUNXI_CLK_FACTORS_H
#define __MACH_SUNXI_CLK_FACTORS_H

#include <linux/clk-provider.h>
#include <linux/clkdev.h>

struct clk_factor_table {
	u8 n, k, m, p;
	u32 val;
};

struct clk *clk_register_factors(struct device *dev, const char *name,
				const char *parent_name,
				unsigned long flags, void __iomem *reg,
				u8 m, u8 mlen, u8 k, u8 klen, u8 n,
				u8 nlen, u8 p, u8 plen,
				const struct clk_factor_table *table,
				spinlock_t *lock);
#endif
