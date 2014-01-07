#ifndef __CLK_SUNXI_H__
#define __CLK_SUNXI_H__

/* Maximum number of parents our clocks have */
#define SUNXI_MAX_PARENTS	5

/* Mux clocks */
struct mux_data;
void __init sunxi_mux_clk_setup(struct device_node *node,
				struct mux_data *data);
extern const struct of_device_id clk_mux_match[] __initconst;

/* Divider clocks */
struct div_data;
void __init sunxi_divider_clk_setup(struct device_node *node,
				    struct div_data *data);
extern const struct of_device_id clk_div_match[] __initconst;
#endif
