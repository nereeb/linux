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

static DEFINE_SPINLOCK(clk_lock);

/**
 * sunxi_divider_clk_setup() helper data
 */

struct div_data {
	u8	shift;
	u8	pow;
	u8	width;
};

static const struct div_data sun4i_axi_data __initconst = {
	.shift	= 0,
	.pow	= 0,
	.width	= 2,
};

static const struct div_data sun4i_ahb_data __initconst = {
	.shift	= 4,
	.pow	= 1,
	.width	= 2,
};

static const struct div_data sun4i_apb0_data __initconst = {
	.shift	= 8,
	.pow	= 1,
	.width	= 2,
};

static const struct div_data sun6i_a31_apb2_div_data __initconst = {
	.shift	= 0,
	.pow	= 0,
	.width	= 4,
};

/**
 * sunxi_divider_clk_setup() - Setup function for simple divider clocks
 */

void __init sunxi_divider_clk_setup(struct device_node *node,
				    struct div_data *data)
{
	struct clk *clk;
	const char *clk_name = node->name;
	const char *clk_parent;
	void *reg;

	reg = of_iomap(node, 0);

	clk_parent = of_clk_get_parent_name(node, 0);

	clk = clk_register_divider(NULL, clk_name, clk_parent, 0,
				   reg, data->shift, data->width,
				   data->pow ? CLK_DIVIDER_POWER_OF_TWO : 0,
				   &clk_lock);
	if (clk)
		of_clk_add_provider(node, of_clk_src_simple_get, clk);
}

/* Matches for divider clocks */
const struct of_device_id clk_div_match[] __initconst = {
	{.compatible = "allwinner,sun4i-axi-clk", .data = &sun4i_axi_data,},
	{.compatible = "allwinner,sun4i-ahb-clk", .data = &sun4i_ahb_data,},
	{.compatible = "allwinner,sun4i-apb0-clk", .data = &sun4i_apb0_data,},
	{.compatible = "allwinner,sun6i-a31-apb2-div-clk", .data = &sun6i_a31_apb2_div_data,},
	{}
};
