/*
 * Copyright 2013 Emilio López
 *
 * Emilio López <emilio@elopez.com.ar>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#include <linux/clk-provider.h>
#include <linux/clkdev.h>
#include <linux/clk/sunxi.h>
#include <linux/of.h>
#include <linux/of_address.h>

#include "clk-factors.h"
#include "clk-fixed-gate.h"

static DEFINE_SPINLOCK(clk_lock);

/**
 * sunxi_osc_clk_setup() - Setup function for gatable oscillator
 */

#define SUNXI_OSC24M_GATE	0

static void __init sunxi_osc_clk_setup(struct device_node *node)
{
	struct clk *clk;
	const char *clk_name = node->name;
	void *reg;
	u32 rate;

	reg = of_iomap(node, 0);

	if (of_property_read_u32(node, "clock-frequency", &rate))
		return;

	clk = clk_register_fixed_gate(NULL, clk_name, NULL,
				      CLK_IS_ROOT | CLK_IGNORE_UNUSED,
				      reg, SUNXI_OSC24M_GATE, rate, &clk_lock);

	if (clk) {
		of_clk_add_provider(node, of_clk_src_simple_get, clk);
		clk_register_clkdev(clk, clk_name, NULL);
	}
}



/**
 * sunxi_get_pll1_factors() - calculates n, k, m, p factors for PLL1
 * PLL1 rate is calculated as follows
 * rate = (parent_rate * n * (k + 1) >> p) / (m + 1);
 * parent_rate is always 24Mhz
 */

static void sunxi_get_pll1_factors(u32 *freq, u8 *n, u8 *k, u8 *m, u8 *p)
{
	u8 div;

	/* Normalize value to a 6M multiple */
	div = *freq / 6000000;
	*freq = 6000000 * div;

	/* we were called to round the frequency, we can now return */
	if (n == NULL)
		return;

	/* m is always zero for pll1 */
	*m = 0;

	/* k is 1 only on these cases */
	if (*freq >= 768000000 || *freq == 42000000 || *freq == 54000000)
		*k = 1;
	else
		*k = 0;

	/* p will be 3 for divs under 10 */
	if (div < 10)
		*p = 3;

	/* p will be 2 for divs between 10 - 20 and odd divs under 32 */
	else if (div < 20 || (div < 32 && (div & 1)))
		*p = 2;

	/* p will be 1 for even divs under 32, divs under 40 and odd pairs
	 * of divs between 40-62 */
	else if (div < 40 || (div < 64 && (div & 2)))
		*p = 1;

	/* any other entries have p = 0 */
	else
		*p = 0;

	/* calculate a suitable n based on k and p */
	div <<= *p;
	div /= (*k + 1);
	*n = div / 4;
}

/**
 * sunxi_pll1_clk_setup() - Setup function for PLL1 clock
 */

struct clk_factors_config pll1_config = {
	.nshift = 8,
	.nwidth = 5,
	.kshift = 4,
	.kwidth = 2,
	.mshift = 0,
	.mwidth = 2,
	.pshift = 16,
	.pwidth = 2,
};

static void __init sunxi_pll1_clk_setup(struct device_node *node)
{
	struct clk *clk;
	const char *clk_name = node->name;
	const char *parent;
	void *reg;

	reg = of_iomap(node, 0);

	parent = of_clk_get_parent_name(node, 0);

	clk = clk_register_factors(NULL, clk_name, parent, CLK_IGNORE_UNUSED,
				   reg, &pll1_config, sunxi_get_pll1_factors,
				   &clk_lock);

	if (clk) {
		of_clk_add_provider(node, of_clk_src_simple_get, clk);
		clk_register_clkdev(clk, clk_name, NULL);
	}
}



/**
 * sunxi_cpu_clk_setup() - Setup function for CPU mux
 */

#define SUNXI_CPU_GATE		16
#define SUNXI_CPU_GATE_WIDTH	2

static void __init sunxi_cpu_clk_setup(struct device_node *node)
{
	struct clk *clk;
	const char *clk_name = node->name;
	const char **parents = kmalloc(sizeof(char *) * 5, GFP_KERNEL);
	void *reg;
	int i = 0;

	reg = of_iomap(node, 0);

	while (i < 5 && (parents[i] = of_clk_get_parent_name(node, i)) != NULL)
		i++;

	clk = clk_register_mux(NULL, clk_name, parents, i, 0, reg,
			       SUNXI_CPU_GATE, SUNXI_CPU_GATE_WIDTH,
			       0, &clk_lock);

	if (clk) {
		of_clk_add_provider(node, of_clk_src_simple_get, clk);
		clk_register_clkdev(clk, clk_name, NULL);
	}
}



/**
 * sunxi_divider_clk_setup() - Setup function for simple divider clocks
 */

#define SUNXI_DIVISOR_WIDTH	2

struct div_data {
	u8 div;
	u8 pow;
};

static const __initconst struct div_data axi_data = {
	.div = 0,
	.pow = 0,
};

static const __initconst struct div_data ahb_data = {
	.div = 4,
	.pow = 1,
};

static const __initconst struct div_data apb0_data = {
	.div = 8,
	.pow = 1,
};

static void __init sunxi_divider_clk_setup(struct device_node *node, u32 shift,
					   u32 power_of_two)
{
	struct clk *clk;
	const char *clk_name = node->name;
	const char *clk_parent;
	void *reg;

	reg = of_iomap(node, 0);

	clk_parent = of_clk_get_parent_name(node, 0);

	clk = clk_register_divider(NULL, clk_name, clk_parent, 0,
				   reg, shift, SUNXI_DIVISOR_WIDTH,
				   power_of_two ? CLK_DIVIDER_POWER_OF_TWO : 0,
				   &clk_lock);
	if (clk) {
		of_clk_add_provider(node, of_clk_src_simple_get, clk);
		clk_register_clkdev(clk, clk_name, NULL);
	}
}


/* Matches for of_clk_init */
static const __initconst struct of_device_id clk_match[] = {
	{.compatible = "fixed-clock", .data = of_fixed_clk_setup,},
	{.compatible = "allwinner,sunxi-osc-clk", .data = sunxi_osc_clk_setup,},
	{.compatible = "allwinner,sunxi-pll1-clk", .data = sunxi_pll1_clk_setup,},
	{.compatible = "allwinner,sunxi-cpu-clk", .data = sunxi_cpu_clk_setup,},
	{}
};

/* Matches for divider clocks */
static const __initconst struct of_device_id clk_div_match[] = {
	{.compatible = "allwinner,sunxi-axi-clk", .data = &axi_data,},
	{.compatible = "allwinner,sunxi-ahb-clk", .data = &ahb_data,},
	{.compatible = "allwinner,sunxi-apb0-clk", .data = &apb0_data,},
	{}
};

static void __init of_sunxi_divider_clock_setup(void)
{
	struct device_node *np;
	const struct div_data *data;

	for_each_matching_node(np, clk_div_match) {
		const struct of_device_id *match =
		    of_match_node(clk_div_match, np);
		data = match->data;
		sunxi_divider_clk_setup(np, data->div, data->pow);
	}
}

void __init sunxi_init_clocks(void)
{
	/* Register all the simple sunxi clocks on DT */
	of_clk_init(clk_match);

	/* Register divider clocks */
	of_sunxi_divider_clock_setup();
}
