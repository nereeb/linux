/*
 * Copyright (C) 2013 Emilio López <emilio@elopez.com.ar>
 *
 * Based on drivers/clk/clk-gate.c,
 *
 * Copyright (C) 2010-2011 Canonical Ltd <jeremy.kerr@canonical.com>
 * Copyright (C) 2011-2012 Mike Turquette, Linaro Ltd <mturquette@linaro.org>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * Fixed rate, gated clock implementation
 */

#include <linux/clk-provider.h>
#include <linux/module.h>
#include <linux/slab.h>
#include <linux/io.h>
#include <linux/err.h>
#include <linux/string.h>

#include "clk-fixed-gate.h"

/**
 * DOC: fixed rate clock which can gate and ungate it's ouput
 *
 * Traits of this clock:
 * prepare - clk_(un)prepare only ensures parent is (un)prepared
 * enable - clk_enable and clk_disable are functional & control gating
 * rate - rate is always a fixed value.  No clk_set_rate support
 * parent - fixed parent.  No clk_set_parent support
 */

struct clk_fixed_gate {
	struct clk_hw hw;
	u8            bit_idx;
	u8            flags;
	unsigned long fixed_rate;
	void __iomem  *reg;
	spinlock_t    *lock;
};

#define to_clk_fixed_gate(_hw) container_of(_hw, struct clk_fixed_gate, hw)

static void clk_fixed_gate_endisable(struct clk_hw *hw, int enable)
{
	struct clk_fixed_gate *gate = to_clk_fixed_gate(hw);
	unsigned long flags = 0;
	u32 reg;

	if (gate->lock)
		spin_lock_irqsave(gate->lock, flags);

	reg = readl(gate->reg);

	if (enable)
		reg |= BIT(gate->bit_idx);
	else
		reg &= ~BIT(gate->bit_idx);

	writel(reg, gate->reg);

	if (gate->lock)
		spin_unlock_irqrestore(gate->lock, flags);
}

static int clk_fixed_gate_enable(struct clk_hw *hw)
{
	clk_fixed_gate_endisable(hw, 1);

	return 0;
}

static void clk_fixed_gate_disable(struct clk_hw *hw)
{
	clk_fixed_gate_endisable(hw, 0);
}

static int clk_fixed_gate_is_enabled(struct clk_hw *hw)
{
	u32 reg;
	struct clk_fixed_gate *gate = to_clk_fixed_gate(hw);

	reg = readl(gate->reg);

	reg &= BIT(gate->bit_idx);

	return reg ? 1 : 0;
}

static unsigned long clk_fixed_gate_recalc_rate(struct clk_hw *hw,
						unsigned long parent_rate)
{
	return to_clk_fixed_gate(hw)->fixed_rate;
}

static const struct clk_ops clk_fixed_gate_ops = {
	.enable = clk_fixed_gate_enable,
	.disable = clk_fixed_gate_disable,
	.is_enabled = clk_fixed_gate_is_enabled,
	.recalc_rate = clk_fixed_gate_recalc_rate,
};

/**
 * clk_register_fixed_gate - register a fixed rate,
 * gate clock with the clock framework
 * @dev: device that is registering this clock
 * @name: name of this clock
 * @parent_name: name of this clock's parent
 * @flags: framework-specific flags for this clock
 * @reg: register address to control gating of this clock
 * @bit_idx: which bit in the register controls gating of this clock
 * @lock: shared register lock for this clock
 */
struct clk *clk_register_fixed_gate(struct device *dev, const char *name,
				    const char *parent_name,
				    unsigned long flags, void __iomem *reg,
				    u8 bit_idx, unsigned long fixed_rate,
				    spinlock_t *lock)
{
	struct clk_fixed_gate *gate;
	struct clk *clk;
	struct clk_init_data init;

	/* allocate the gate */
	gate = kzalloc(sizeof(struct clk_fixed_gate), GFP_KERNEL);
	if (!gate) {
		pr_err("%s: could not allocate fixed gated clk\n", __func__);
		return ERR_PTR(-ENOMEM);
	}

	init.name = name;
	init.ops = &clk_fixed_gate_ops;
	init.flags = flags | CLK_IS_BASIC;
	init.parent_names = (parent_name ? &parent_name : NULL);
	init.num_parents = (parent_name ? 1 : 0);

	/* struct clk_fixed_gate assignments */
	gate->fixed_rate = fixed_rate;
	gate->reg = reg;
	gate->bit_idx = bit_idx;
	gate->lock = lock;
	gate->hw.init = &init;

	clk = clk_register(dev, &gate->hw);

	if (IS_ERR(clk))
		kfree(gate);

	return clk;
}
