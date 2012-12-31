/*
 * Copyright (C) 2013 Emilio López <emilio@elopez.com.ar>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * Adjustable factor-based clock implementation
 */

#include <linux/clk-provider.h>
#include <linux/module.h>
#include <linux/slab.h>
#include <linux/io.h>
#include <linux/err.h>
#include <linux/string.h>

#include <linux/delay.h>

#include "clk-factors.h"

/*
 * DOC: basic adjustable factor-based clock that cannot gate
 *
 * Traits of this clock:
 * prepare - clk_prepare only ensures that parents are prepared
 * enable - clk_enable only ensures that parents are enabled
 * rate - rate is adjustable.
 *        clk->rate = (parent->rate * N * (K + 1) >> P) / (M + 1)
 * parent - fixed parent.  No clk_set_parent support
 */

struct clk_factors {
	struct clk_hw hw;
	void __iomem *reg;
	u8 m;
	u8 mlen;
	u8 k;
	u8 klen;
	u8 n;
	u8 nlen;
	u8 p;
	u8 plen;
	const struct clk_factor_table *table;
	spinlock_t *lock;
};

#define to_clk_factors(_hw) container_of(_hw, struct clk_factors, hw)

#define SETMASK(len, pos)		(((-1U) >> (31-len))  << (pos))
#define CLRMASK(len, pos)		(~(SETMASK(len, pos)))
#define FACTOR_GET(bit, len, reg)	(((reg) & SETMASK(len, bit)) >> (bit))

#define FACTOR_SET(bit, len, reg, val) \
	(((reg) & CLRMASK(len, bit)) | (val << (bit)))

static const struct clk_factor_table *_get_table_factors(const struct
							 clk_factor_table
							 *table,
							 unsigned long int val)
{
	const struct clk_factor_table *clkt;

	for (clkt = table; clkt->val; clkt++)
		if (clkt->val > val)
			return --clkt;

	/* val was too high, return the max we can do */
	return --clkt;
}

static unsigned long clk_factors_recalc_rate(struct clk_hw *hw,
					     unsigned long parent_rate)
{
	u8 n, k, p, m;
	u32 reg;
	unsigned long rate;
	struct clk_factors *factors = to_clk_factors(hw);

	/* Fetch the register value */
	reg = readl(factors->reg);

	/* Get each individual factor */
	n = FACTOR_GET(factors->n, factors->nlen, reg);
	k = FACTOR_GET(factors->k, factors->klen, reg);
	p = FACTOR_GET(factors->p, factors->plen, reg);
	m = FACTOR_GET(factors->m, factors->mlen, reg);

	/* Calculate the rate */
	rate = (parent_rate * n * (k + 1) >> p) / (m + 1);

	return rate;
}

static long clk_factors_round_rate(struct clk_hw *hw, unsigned long rate,
				   unsigned long *parent_rate)
{
	struct clk_factors *factors = to_clk_factors(hw);
	const struct clk_factor_table *value;

	value = _get_table_factors(factors->table, rate);

	return value->val;
}

static int clk_factors_set_rate(struct clk_hw *hw, unsigned long rate,
				unsigned long parent_rate)
{
	u32 reg;
	struct clk_factors *factors = to_clk_factors(hw);
	const struct clk_factor_table *value;
	unsigned long flags = 0;

	value = _get_table_factors(factors->table, rate);

	if (factors->lock)
		spin_lock_irqsave(factors->lock, flags);

	/* Fetch the register value */
	reg = readl(factors->reg);

	/* Set up the new factors */
	reg = FACTOR_SET(factors->m, factors->mlen, reg, value->m);
	reg = FACTOR_SET(factors->k, factors->klen, reg, value->k);
	reg = FACTOR_SET(factors->n, factors->nlen, reg, value->n);
	reg = FACTOR_SET(factors->p, factors->plen, reg, value->p);

	/* Apply them now */
	writel(reg, factors->reg);

	/* delay 500us so pll stabilizes */
	__delay((rate >> 20) * 500 / 2);

	if (factors->lock)
		spin_unlock_irqrestore(factors->lock, flags);

	return 0;
}

static const struct clk_ops clk_factors_ops = {
	.recalc_rate = clk_factors_recalc_rate,
	.round_rate = clk_factors_round_rate,
	.set_rate = clk_factors_set_rate,
};

/**
 * clk_register_factors - register a table based factors clock with
 * the clock framework
 * @dev: device registering this clock
 * @name: name of this clock
 * @parent_name: name of clock's parent
 * @flags: framework-specific flags
 * @reg: register address to adjust factors
 * @m, k, n, p: position of factors n, k, n, p on the bitfield
 * @mlen, klen, nlen, plen: length of the factors m, k, n, p
 * @clk_factors_flags: factors-specific flags for this clock
 * @table: array of factors/value pairs ending with a val set to 0
 * @lock: shared register lock for this clock
 */
struct clk *clk_register_factors(struct device *dev, const char *name,
				 const char *parent_name,
				 unsigned long flags, void __iomem *reg,
				 u8 m, u8 mlen, u8 k, u8 klen, u8 n,
				 u8 nlen, u8 p, u8 plen,
				 const struct clk_factor_table *table,
				 spinlock_t *lock)
{
	struct clk_factors *factors;
	struct clk *clk;
	struct clk_init_data init;

	/* allocate the factors */
	factors = kzalloc(sizeof(struct clk_factors), GFP_KERNEL);
	if (!factors) {
		pr_err("%s: could not allocate factors clk\n", __func__);
		return ERR_PTR(-ENOMEM);
	}

	init.name = name;
	init.ops = &clk_factors_ops;
	init.flags = flags;
	init.parent_names = (parent_name ? &parent_name : NULL);
	init.num_parents = (parent_name ? 1 : 0);

	/* struct clk_factors assignments */
	factors->reg = reg;
	factors->m = m;
	factors->mlen = mlen;
	factors->k = k;
	factors->klen = klen;
	factors->n = n;
	factors->nlen = nlen;
	factors->p = p;
	factors->plen = plen;
	factors->lock = lock;
	factors->hw.init = &init;
	factors->table = table;

	/* register the clock */
	clk = clk_register(dev, &factors->hw);

	if (IS_ERR(clk))
		kfree(factors);

	return clk;
}
