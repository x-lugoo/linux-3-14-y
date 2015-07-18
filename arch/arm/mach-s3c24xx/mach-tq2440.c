/* linux/arch/arm/mach-s3c2440/mach-tq2440.c
 *
 * Copyright (c) 2004-2005 Simtec Electronics
 *	Ben Dooks <ben@simtec.co.uk>
 *
 * http://www.fluff.org/ben/smdk2440/
 *
 * Thanks to Dimity Andric and TomTom for the loan of an tq2440.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
*/

#include <linux/kernel.h>
#include <linux/types.h>
#include <linux/interrupt.h>
#include <linux/list.h>
#include <linux/timer.h>
#include <linux/init.h>
#include <linux/serial_core.h>
#include <linux/platform_device.h>
#include <linux/io.h>
#include <linux/dm9000.h>

#include <asm/mach/arch.h>
#include <asm/mach/map.h>
#include <asm/mach/irq.h>

#include <mach/hardware.h>
#include <asm/irq.h>
#include <asm/mach-types.h>

#include <plat/regs-serial.h>
#include <mach/regs-gpio.h>
#include <mach/regs-lcd.h>

#include <mach/fb.h>
#include <linux/platform_data/i2c-s3c2410.h>
#include <linux/platform_data/at24.h>
#include <linux/i2c.h>

#include <linux/mtd/partitions.h>
#include <linux/platform_data/mtd-nand-s3c2410.h>

#include <plat/clock.h>
#include <plat/devs.h>
#include <plat/cpu.h>
#include <plat/samsung-time.h>
#include <plat/pm.h>

#ifdef CONFIG_TQ2440_EEPROM_USE_GPIO_I2C
#include <linux/i2c-gpio.h>
#include <mach/gpio-samsung.h>
#endif

#include "common.h"
#include "common-smdk.h"

static struct map_desc tq2440_iodesc[] __initdata = {
	/* ISA IO Space map (memory space selected by A24) */

	{
		.virtual	= (u32)S3C24XX_VA_ISA_WORD,
		.pfn		= __phys_to_pfn(S3C2410_CS2),
		.length		= 0x10000,
		.type		= MT_DEVICE,
	}, {
		.virtual	= (u32)S3C24XX_VA_ISA_WORD + 0x10000,
		.pfn		= __phys_to_pfn(S3C2410_CS2 + (1<<24)),
		.length		= SZ_4M,
		.type		= MT_DEVICE,
	}, {
		.virtual	= (u32)S3C24XX_VA_ISA_BYTE,
		.pfn		= __phys_to_pfn(S3C2410_CS2),
		.length		= 0x10000,
		.type		= MT_DEVICE,
	}, {
		.virtual	= (u32)S3C24XX_VA_ISA_BYTE + 0x10000,
		.pfn		= __phys_to_pfn(S3C2410_CS2 + (1<<24)),
		.length		= SZ_4M,
		.type		= MT_DEVICE,
	}
};

#define UCON S3C2410_UCON_DEFAULT | S3C2410_UCON_UCLK
#define ULCON S3C2410_LCON_CS8 | S3C2410_LCON_PNONE | S3C2410_LCON_STOPB
#define UFCON S3C2410_UFCON_RXTRIG8 | S3C2410_UFCON_FIFOMODE

static struct s3c2410_uartcfg tq2440_uartcfgs[] __initdata = {
	[0] = {
		.hwport	     = 0,
		.flags	     = 0,
		.ucon	     = 0x3c5,
		.ulcon	     = 0x03,
		.ufcon	     = 0x51,
	},
	[1] = {
		.hwport	     = 1,
		.flags	     = 0,
		.ucon	     = 0x3c5,
		.ulcon	     = 0x03,
		.ufcon	     = 0x51,
	},
	/* IR port */
	[2] = {
		.hwport	     = 2,
		.flags	     = 0,
		.ucon	     = 0x3c5,
		.ulcon	     = 0x43,
		.ufcon	     = 0x51,
	}
};

/* LCD driver info */

static struct s3c2410fb_display tq2440_lcd_cfg __initdata = {

	.lcdcon5	= S3C2410_LCDCON5_FRM565 |
			  S3C2410_LCDCON5_INVVLINE |
			  S3C2410_LCDCON5_INVVFRAME |
			  S3C2410_LCDCON5_PWREN |
			  S3C2410_LCDCON5_HWSWP,

	.type		= S3C2410_LCDCON1_TFT,

	.width		= 480,
	.height		= 272,

	.pixclock	= 100000,
	.xres		= 480,
	.yres		= 272,
	.bpp		= 16,
	.left_margin	= 2,  /* For HBPD  */
	.right_margin	= 2,  /* For HFPD  */
	.hsync_len	    = 41, /* For HSPW  */
	.upper_margin	= 2,  /* For VBPD  */
	.lower_margin	= 2,  /* For VFPD  */
	.vsync_len	    = 10, /* For VSPW  */
};

#define S3C2410_GPCCON_MASK(x)	(3 << ((x) * 2))
#define S3C2410_GPDCON_MASK(x)	(3 << ((x) * 2))

static struct s3c2410fb_mach_info tq2440_fb_info __initdata = {
	.displays	= &tq2440_lcd_cfg,
	.num_displays	= 1,
	.default_display = 0,

	/* Enable VD[2..7], VD[10..15], VD[18..23] and VCLK, syncs, VDEN
	 * and disable the pull down resistors on pins we are using for LCD
	 * data. */

	.gpcup		= (0xf << 1) | (0x3f << 10),

	.gpccon		= (S3C2410_GPC1_VCLK   | S3C2410_GPC2_VLINE |
			   S3C2410_GPC3_VFRAME | S3C2410_GPC4_VM |
			   S3C2410_GPC8_VD0   | S3C2410_GPC9_VD1 |
			   S3C2410_GPC10_VD2   | S3C2410_GPC11_VD3 |
			   S3C2410_GPC12_VD4   | S3C2410_GPC13_VD5 |
			   S3C2410_GPC14_VD6   | S3C2410_GPC15_VD7),

	.gpccon_mask	= (S3C2410_GPCCON_MASK(1)  | S3C2410_GPCCON_MASK(2)  |
			   S3C2410_GPCCON_MASK(3)  | S3C2410_GPCCON_MASK(4)  |
			   S3C2410_GPCCON_MASK(8) | S3C2410_GPCCON_MASK(9) |
			   S3C2410_GPCCON_MASK(10) | S3C2410_GPCCON_MASK(11) |
			   S3C2410_GPCCON_MASK(12) | S3C2410_GPCCON_MASK(13) |
			   S3C2410_GPCCON_MASK(14) | S3C2410_GPCCON_MASK(15)),

	.gpdup		= (0x3f << 2) | (0x3f << 10),

	.gpdcon		= (S3C2410_GPD2_VD10  | S3C2410_GPD3_VD11 |
			   S3C2410_GPD4_VD12  | S3C2410_GPD5_VD13 |
			   S3C2410_GPD6_VD14  | S3C2410_GPD7_VD15 |
			   S3C2410_GPD10_VD18 | S3C2410_GPD11_VD19 |
			   S3C2410_GPD12_VD20 | S3C2410_GPD13_VD21 |
			   S3C2410_GPD14_VD22 | S3C2410_GPD15_VD23),

	.gpdcon_mask	= (S3C2410_GPDCON_MASK(2)  | S3C2410_GPDCON_MASK(3) |
			   S3C2410_GPDCON_MASK(4)  | S3C2410_GPDCON_MASK(5) |
			   S3C2410_GPDCON_MASK(6)  | S3C2410_GPDCON_MASK(7) |
			   S3C2410_GPDCON_MASK(10) | S3C2410_GPDCON_MASK(11)|
			   S3C2410_GPDCON_MASK(12) | S3C2410_GPDCON_MASK(13)|
			   S3C2410_GPDCON_MASK(14) | S3C2410_GPDCON_MASK(15)),

	.lpcsel		= 0,
};

/* NAND parititon */

static struct mtd_partition tq2440_nand_part[] = {
	[0] = {
		.name	= "Boot",
		.offset = 0,
		.size	= SZ_2M,
	},
	[1] = {
		.name	= "Kernel",
		.offset	= SZ_2M,
		.size	= SZ_1M * 3,
	},
	[2] = {
		.name	= "Rootfs",
		.offset = SZ_1M * 5,
		.size	= MTDPART_SIZ_FULL,
	}
};

static struct s3c2410_nand_set tq2440_nand_sets[] = {
	[0] = {
		.name		= "tq2440-0",
		.nr_chips	= 1,
		.nr_partitions	= ARRAY_SIZE(tq2440_nand_part),
		.partitions	= tq2440_nand_part,
	},
};

/* choose a set of timings which should suit most 512Mbit
 * chips and beyond.
*/

static struct s3c2410_platform_nand tq2440_nand_info = {
	.tacls		= 10,
	.twrph0		= 25,
	.twrph1		= 10,
	.nr_sets	= ARRAY_SIZE(tq2440_nand_sets),
	.sets		= tq2440_nand_sets,
};

/* AT24C02 */
static struct at24_platform_data tq2440_platform_data = {
	.byte_len = SZ_1K*2/8,
	.page_size = 8,
	.flags = AT24_FLAG_TAKE8ADDR,
};

static struct i2c_board_info tq2440_at24c02[] = {
	{
		I2C_BOARD_INFO("24c02", 0x50),
		.platform_data = &tq2440_platform_data,
	},
};

/* DM9000 */
static struct resource s3c_dm9k_resource[] = {
	[0] = {
		.start	= S3C2410_CS4,
		.end	= S3C2410_CS4 + 3,
		.flags	= IORESOURCE_MEM,
	},
	[1] = {
		.start	= S3C2410_CS4 + 4,
		.end	= S3C2410_CS4 + 4 + 3,
		.flags	= IORESOURCE_MEM,
	},
	[2] = {
		.start	= IRQ_EINT7,
		.end	= IRQ_EINT7,
		.flags	= IORESOURCE_IRQ | IRQF_TRIGGER_RISING,
	}

};

static struct dm9000_plat_data s3c_dm9k_platdata = {
	.flags	= DM9000_PLATF_16BITONLY,
};

struct platform_device s3c_device_dm9000 = {
	.name		= "dm9000",
	.id			= 0,
	.num_resources	= ARRAY_SIZE(s3c_dm9k_resource),
	.resource		= s3c_dm9k_resource,
	.dev			= {
		.platform_data = &s3c_dm9k_platdata,
	}
};

#ifdef CONFIG_TQ2440_EEPROM_USE_GPIO_I2C
static struct i2c_gpio_platform_data s3c_gpio_i2c_platdata = {
	.sda_pin = S3C2410_GPE(15),
	.scl_pin = S3C2410_GPE(14),
	//.sda_is_open_drain = 1,
	//.scl_is_open_drain = 1,
	//.scl_is_output_only = 1,
	//.udelay = 100,  // µÍµçÆ½ºÍ¸ßµçÆ½µÄ³ÖÐøÊ±¼ä¶¼ÊÇ100us£¬ÄÇÃ´ÆµÂÊ¾ÍÊÇ5KHz
};

struct platform_device s3c_device_gpio_i2c = {
	.name		= "i2c-gpio",
	.id			= 0,
	.dev			= {
		.platform_data = &s3c_gpio_i2c_platdata,
	}
};
#endif

static struct platform_device *tq2440_devices[] __initdata = {
	&s3c_device_ohci,
	&s3c_device_lcd,
	&s3c_device_wdt,
#ifdef CONFIG_TQ2440_EEPROM_USE_GPIO_I2C
	&s3c_device_gpio_i2c,
#else
	&s3c_device_i2c0,
#endif
	&s3c_device_iis,
	&s3c_device_dm9000,
	&s3c_device_rtc,
	&s3c_device_nand,
};

static void __init tq2440_map_io(void)
{
	s3c24xx_init_io(tq2440_iodesc, ARRAY_SIZE(tq2440_iodesc));
	s3c24xx_init_clocks(12000000);
	s3c24xx_init_uarts(tq2440_uartcfgs, ARRAY_SIZE(tq2440_uartcfgs));
	samsung_set_timer_source(SAMSUNG_PWM3, SAMSUNG_PWM4);
}

static void __init tq2440_machine_init(void)
{
	s3c24xx_fb_set_platdata(&tq2440_fb_info);

	s3c_i2c0_set_platdata(NULL);

	s3c_nand_set_platdata(&tq2440_nand_info);

	platform_add_devices(tq2440_devices, ARRAY_SIZE(tq2440_devices));

	i2c_register_board_info(0, tq2440_at24c02, 1);

	s3c_pm_init();
}

MACHINE_START(TQ2440, "TQ2440")
	/* Maintainer: Ben Dooks <ben-linux@fluff.org> */
	.atag_offset	= 0x100,

	.init_irq	= s3c2440_init_irq,
	.map_io		= tq2440_map_io,
	.init_machine	= tq2440_machine_init,
	.init_time	= samsung_timer_init,
	.restart	= s3c244x_restart,
MACHINE_END
