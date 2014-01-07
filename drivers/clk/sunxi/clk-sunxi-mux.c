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
#include <linux/of.h>
#include <linux/of_address.h>

#include "clk-sunxi.h"

static DEFINE_SPINLOCK(clk_lock);


/**
 * sunxi_mux_clk_setup() - Setup function for muxes
 */

#define SUNXI_MUX_GATE_WIDTH	2

struct mux_data {
	u8 shift;
};

static const struct mux_data sun4i_cpu_mux_data __initconst = {
	.shift = 16,
};

static const struct mux_data sun6i_a31_ahb1_mux_data __initconst = {
	.shift = 12,
};

static const struct mux_data sun4i_apb1_mux_data __initconst = {
	.shift = 24,
};

void __init sunxi_mux_clk_setup(struct device_node *node,
				struct mux_data *data)
{
	struct clk *clk;
	const char *clk_name = node->name;
	const char *parents[SUNXI_MAX_PARENTS];
	void *reg;
	int i = 0;

	reg = of_iomap(node, 0);

	while (i < SUNXI_MAX_PARENTS &&
	       (parents[i] = of_clk_get_parent_name(node, i)) != NULL)
		i++;

	clk = clk_register_mux(NULL, clk_name, parents, i,
			       CLK_SET_RATE_NO_REPARENT, reg,
			       data->shift, SUNXI_MUX_GATE_WIDTH,
			       0, &clk_lock);

	if (clk)
		of_clk_add_provider(node, of_clk_src_simple_get, clk);
}

/* Matches for mux clocks */
const struct of_device_id clk_mux_match[] __initconst = {
	{.compatible = "allwinner,sun4i-cpu-clk", .data = &sun4i_cpu_mux_data,},
	{.compatible = "allwinner,sun4i-apb1-mux-clk", .data = &sun4i_apb1_mux_data,},
	{.compatible = "allwinner,sun6i-a31-ahb1-mux-clk", .data = &sun6i_a31_ahb1_mux_data,},
	{}
};
