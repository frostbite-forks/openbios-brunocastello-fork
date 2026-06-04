/*
 *   derived from mol/mol.c,
 *   Copyright (C) 2003, 2004 Samuel Rydh (samuel@ibrium.se)
 *
 *   This program is free software; you can redistribute it and/or
 *   modify it under the terms of the GNU General Public License
 *   version 2
 *
 */

#include "config.h"
#include "arch/common/nvram.h"
#include "packages/nvram.h"
#include "libopenbios/bindings.h"
#include "libc/byteorder.h"
#include "libc/vsprintf.h"

#include "drivers/drivers.h"
#include "macio.h"
#include "cuda.h"
#include "pmu.h"
#include "escc.h"
#include "drivers/pci.h"

#define OW_IO_NVRAM_SIZE   0x00020000
#define OW_IO_NVRAM_OFFSET 0x00060000
#define OW_IO_NVRAM_SHIFT  4

#define NW_IO_NVRAM_SIZE   0x00004000
#define NW_IO_NVRAM_OFFSET 0xfff04000

#define IO_OPENPIC_SIZE    0x00040000
#define IO_OPENPIC_OFFSET  0x00040000

#define DAVBUS_REG_OFFSET  0x14000
#define DAVBUS_REG_SIZE    0x1000
#define DAVBUS_TX_OFFSET   0x8800
#define DAVBUS_TX_SIZE     0x100
#define DAVBUS_RX_OFFSET   0x8900
#define DAVBUS_RX_SIZE     0x100

static char *nvram;

static int macio_nvram_shift(void)
{
	int nvram_flat;

        if (is_oldworld())
                return OW_IO_NVRAM_SHIFT;

	nvram_flat = fw_cfg_read_i32(FW_CFG_PPC_NVRAM_FLAT);
	return nvram_flat ? 0 : 1;
}

int
macio_get_nvram_size(void)
{
	int shift = macio_nvram_shift();
        if (is_oldworld())
                return OW_IO_NVRAM_SIZE >> shift;
        else
                return NW_IO_NVRAM_SIZE >> shift;
}

static unsigned long macio_nvram_offset(void)
{
	unsigned long r;

	/* Hypervisor tells us where NVRAM lies */
	r = fw_cfg_read_i32(FW_CFG_PPC_NVRAM_ADDR);
	if (r)
		return r;

	/* Fall back to hardcoded addresses */
	if (is_oldworld())
		return OW_IO_NVRAM_OFFSET;

	return NW_IO_NVRAM_OFFSET;
}

static unsigned long macio_nvram_size(void)
{
	if (is_oldworld())
		return OW_IO_NVRAM_SIZE;
	else
		return NW_IO_NVRAM_SIZE;
}

void macio_nvram_init(const char *path, phys_addr_t addr)
{
	phandle_t chosen, aliases;
	phandle_t dnode;
	int props[2];
	char buf[64];
        unsigned long nvram_size, nvram_offset;

        nvram_offset = macio_nvram_offset();
        nvram_size = macio_nvram_size();

	nvram = (char*)addr + nvram_offset;
	nvconf_init();
	snprintf(buf, sizeof(buf), "%s", path);
	dnode = nvram_init(buf);
	set_int_property(dnode, "#bytes", arch_nvram_size() );
	props[0] = __cpu_to_be32(nvram_offset);
	props[1] = __cpu_to_be32(nvram_size);
	set_property(dnode, "reg", (char *)&props, sizeof(props));
	set_property(dnode, "device_type", "nvram", 6);
	NEWWORLD(set_property(dnode, "compatible", "nvram,flash", 12));

	chosen = find_dev("/chosen");
	snprintf(buf, sizeof(buf), "%s", get_path_from_ph(dnode));
	push_str(buf);
	fword("open-dev");
	set_int_property(chosen, "nvram", POP());

	aliases = find_dev("/aliases");
	set_property(aliases, "nvram", buf, strlen(buf) + 1);
}

#ifdef DUMP_NVRAM
static void
dump_nvram(void)
{
  int i, j;
  for (i = 0; i < 10; i++)
    {
      for (j = 0; j < 16; j++)
      printk ("%02x ", nvram[(i*16+j)<<4]);
      printk (" ");
      for (j = 0; j < 16; j++)
        if (isprint(nvram[(i*16+j)<<4]))
            printk("%c", nvram[(i*16+j)<<4]);
        else
          printk(".");
      printk ("\n");
      }
}
#endif


void
macio_nvram_put(char *buf)
{
	int i;
        unsigned int it_shift = macio_nvram_shift();

	for (i=0; i < arch_nvram_size(); i++)
		nvram[i << it_shift] = buf[i];
#ifdef DUMP_NVRAM
	printk("new nvram:\n");
	dump_nvram();
#endif
}

void
macio_nvram_get(char *buf)
{
	int i;
        unsigned int it_shift = macio_nvram_shift();

	for (i=0; i< arch_nvram_size(); i++)
                buf[i] = nvram[i << it_shift];

#ifdef DUMP_NVRAM
	printk("current nvram:\n");
	dump_nvram();
#endif
}

static void
openpic_init(const char *path, phys_addr_t addr)
{
        phandle_t dnode;
        int props[2];
        char buf[128];

        fword("new-device");
        push_str("interrupt-controller");
        fword("device-name");

        snprintf(buf, sizeof(buf), "%s/interrupt-controller", path);
        dnode = find_dev(buf);
        set_property(dnode, "device_type", "open-pic", 9);
        set_property(dnode, "compatible", "chrp,open-pic", 14);
        set_property(dnode, "built-in", "", 0);
        props[0] = __cpu_to_be32(IO_OPENPIC_OFFSET);
        props[1] = __cpu_to_be32(IO_OPENPIC_SIZE);
        set_property(dnode, "reg", (char *)&props, sizeof(props));
        set_int_property(dnode, "#interrupt-cells", 2);
        set_int_property(dnode, "#address-cells", 0);
        set_property(dnode, "interrupt-controller", "", 0);
        set_int_property(dnode, "clock-frequency", 4166666);

        fword("finish-device");
}

static void
screamer_init(const char *path)
{
        phandle_t dnode;
        int props[3];
        char buf[256];

        fword("new-device");
        push_str("sound");
        fword("device-name");
        push_str("soundchip");
        fword("device-type");

        snprintf(buf, sizeof(buf), "%s/sound", path);
        dnode = find_dev(buf);
        set_property(dnode, "compatible", "screamer\0awacs\0", 15);
        set_property(dnode, "model", "343S0184", 9);
        set_int_property(dnode, "vendor-id", 0x106b);
        set_int_property(dnode, "device-id", 0x5);
        set_int_property(dnode, "sub-frame", 0x0);
        set_int_property(dnode, "object-model-version", 0x1);
        props[0] = 0x2;
        props[1] = 0x56220000;
        props[2] = 0xac440000;
        set_property(dnode, "sample-rates", (char *)&props, 3 * sizeof(props[0]));

        if (is_oldworld()) {
            set_int_property(dnode, "driver-ptr", 0x3a4060);
            set_int_property(dnode, "driver-ref", 0xffcb);
            set_int_property(dnode, "AAPL,sndhw-plugin-id", 0x3a3350);
            set_int_property(dnode, "AAPL,output-component", 0xc0021);
            set_int_property(dnode, "AAPL,input-component", 0x20023);
            set_int_property(dnode, "AAPL,port-handler-component", 0x20024);
        } else {
            set_int_property(dnode, "#-detects", 3);
            set_int_property(dnode, "#-features", 3);
            set_int_property(dnode, "#-outputs", 2);
            set_int_property(dnode, "#-inputs", 1);
            set_int_property(dnode, "icon-id", 0xffffbf4d);
            set_int_property(dnode, "info-id", 0xffffbf44);
            set_int_property(dnode, "name-id", 0xffffbf4d);
            set_property(dnode, "default-monitor", "none", 5);

            set_property(dnode, "sound-objects",
                                "init operation 2 param 00000001 param-size 4\0"
                                "feature index 0 model Proj7PowerControl\0"
                                "feature index 1 model USBSubwoofer\0"
                                "feature index 2 model NotifySSprockets\0"
                                "detect bit-mask 2 bit-match 2 device 2 index 0 model InSenseBitsDetect\0"
                                "detect bit-mask 4 bit-match 4 device 16 index 1 model InSenseBitsDetect\0"
                                "detect bit-mask 1 bit-match 0 device 32 index 2 model InSenseBitsDetect\0"
                                "input icon-id -16526 index 0 name-id -20520 port-connection 2 "
                                "port-type 0x656D6963 zero-gain 0 model ExternalMic\0"
                                "input icon-id -16526 index 1 name-id -20528 port-connection 2 "
                                "port-type 0x73696E6A zero-gain 0 model InputPort\0"
                                "input icon-id -20184 index 2 name-id -20540 port-connection 3 "
                                "port-type 0x6d6f646d zero-gain 0 model InputPort\0"
                                "input index 3 model NoInput\0"
                                "output device-mask 2 device-match 0 icon-id -16563 index 0 name-id -20525 "
                                "port-connection 2 port-type 0x6973706B model OutputPort\0"
                                "output device-mask 2 device-match 2 icon-id -16563 index 1 name-id -20524 "
                                "port-connection 1 port-type 0x6864706E model OutputPort\0", 0x3e4);
        }

        fword("finish-device");
}

static void
davbus_init(const char *path, phys_addr_t addr)
{
        phandle_t dnode;
        int props[6];
        char buf[128];

        fword("new-device");
        push_str("davbus");
        fword("device-name");
        push_str("soundbus");
        fword("device-type");

        snprintf(buf, sizeof(buf), "%s/davbus", path);
        dnode = find_dev(buf);
        set_property(dnode, "compatible", "davbus", 7);
        set_property(dnode, "built-in", "", 0);

        props[0] = DAVBUS_REG_OFFSET;
        props[1] = DAVBUS_REG_SIZE;
        props[2] = DAVBUS_TX_OFFSET;
        props[3] = DAVBUS_TX_SIZE;
        props[4] = DAVBUS_RX_OFFSET;
        props[5] = DAVBUS_RX_SIZE;
        set_property(dnode, "reg", (char *)&props, 6 * sizeof(props[0]));

        if (is_oldworld()) {
            props[0] = 0x11;
            props[1] = 0x8;
            props[2] = 0x9;
            set_property(dnode, "AAPL,interrupts", (char *)&props, 3 * sizeof(props[0]));

            props[0] = addr + DAVBUS_REG_OFFSET;
            props[1] = addr + DAVBUS_TX_OFFSET;
            props[2] = addr + DAVBUS_RX_OFFSET;
            set_property(dnode, "AAPL,address", (char *)&props, 3 * sizeof(props[0]));

            set_int_property(dnode, "driver-ptr", 0x390510);
            set_int_property(dnode, "driver-ref", 0xffcc);
            set_int_property(dnode, "AAPL,sndio-plugin-id", 0x1ce080);
        } else {
            props[0] = 0x18;
            props[1] = 0x1;
            props[2] = 0x9;
            props[3] = 0x0;
            props[4] = 0xa;
            props[5] = 0x0;
            set_property(dnode, "interrupts", (char *)&props, 6 * sizeof(props[0]));

            props[0] = 0x2;
            props[1] = 0x4;
            props[2] = 0x4;
            set_property(dnode, "AAPL,requested-priorities", (char *)&props, 3 * sizeof(props[0]));

            set_property(dnode, "AAPL,clock-id", "dav au45au49", 13);
        }

        screamer_init(buf);

        fword("finish-device");
}

DECLARE_UNNAMED_NODE(ob_macio, 0, sizeof(int));

/* ( str len -- addr ) */

static void
ob_macio_decode_unit(void *private)
{
	ucell addr;

	const char *arg = pop_fstr_copy();

	addr = strtol(arg, NULL, 16);

	free((char*)arg);

	PUSH(addr);
}

/*  ( addr -- str len ) */

static void
ob_macio_encode_unit(void *private)
{
	char buf[8];

	ucell addr = POP();

	snprintf(buf, sizeof(buf), "%x", addr);

	push_str(buf);
}

static void
ob_macio_dma_alloc(int *idx)
{
    call_parent_method("dma-alloc");
}

static void
ob_macio_dma_free(int *idx)
{
    call_parent_method("dma-free");
}

static void
ob_macio_dma_map_in(int *idx)
{
    call_parent_method("dma-map-in");
}

static void
ob_macio_dma_map_out(int *idx)
{
    call_parent_method("dma-map-out");
}

static void
ob_macio_dma_sync(int *idx)
{
    call_parent_method("dma-sync");
}

NODE_METHODS(ob_macio) = {
        { "decode-unit",	ob_macio_decode_unit	},
        { "encode-unit",	ob_macio_encode_unit	},
        { "dma-alloc",		ob_macio_dma_alloc	},
        { "dma-free",		ob_macio_dma_free		},
        { "dma-map-in",		ob_macio_dma_map_in	},
        { "dma-map-out",	ob_macio_dma_map_out	},
        { "dma-sync",		ob_macio_dma_sync		},
};

void
ob_unin_init(void)
{
        phandle_t dnode;
        int props[2];

        fword("new-device");
        push_str("uni-n");
        fword("device-name");

        dnode = find_dev("/uni-n");
        set_property(dnode, "device_type", "memory-controller", 18);
        set_property(dnode, "compatible", "uni-north", 10);
        if (is_g4da()) {
                set_property(dnode, "model", "AAPL,UniNorth", 14);
                set_int_property(dnode, "device-rev", 0x11);
        } else {
                set_int_property(dnode, "device-rev", 7);
        }
        props[0] = __cpu_to_be32(0xf8000000);
        props[1] = __cpu_to_be32(0x1000000);
        set_property(dnode, "reg", (char *)&props, sizeof(props));

        fword("finish-device");
}

static void
tumbler_init(const char *path)
{
        phandle_t dnode;
        int props[3];
        char buf[256];

        fword("new-device");
        push_str("sound");
        fword("device-name");
        push_str("soundchip");
        fword("device-type");

        snprintf(buf, sizeof(buf), "%s/sound", path);
        dnode = find_dev(buf);
        set_property(dnode, "compatible", "tumbler", 8);
        set_int_property(dnode, "vendor-id", 0x106b);
        set_int_property(dnode, "device-id", 0x0e);
        set_property(dnode, "model", "355S0056", 9);
        set_int_property(dnode, "#-detects", 2);
        set_int_property(dnode, "#-inputs", 0);
        set_int_property(dnode, "#-features", 3);
        set_int_property(dnode, "#-outputs", 3);
        set_property(dnode, "default-monitor", "none", 5);
        set_int_property(dnode, "object-model-version", 1);
        set_int_property(dnode, "sub-frame", 0);
        set_int_property(dnode, "icon-id", 0xffffbf4d);
        set_int_property(dnode, "info-id", 0xffffbf44);
        set_int_property(dnode, "name-id", 0xffffbf4d);
        set_int_property(dnode, "hardware-types", 2);
        set_property(dnode, "equalizer-id", "355S0057", 8);

        props[0] = 0x2;
        props[1] = 0xac440000;
        props[2] = 0xbb800000;
        set_property(dnode, "sample-rates", (char *)&props, 3 * sizeof(props[0]));

        set_int_property(dnode, "i2s-serial-format", 2);
        set_int_property(dnode, "mclk-sample-rate-ratio", 0x100);

        set_property(dnode, "sound-objects",
                        "feature index 0 model Proj14PowerControl\0"
                        "feature index 1 model USBSubwoofer\0"
                        "feature index 2 model Equalizer\0"
                        "detect index 0 bit-mask 2 bit-match 0 device 2 "
                        "registry-name extint-gpio15 model GPIOGenericDetect\0"
                        "detect index 1 bit-mask 2 bit-match 0 device 4 "
                        "registry-name extint-gpio16 model GPIOGenericDetect\0"
                        "output index 0 device-mask 6 device-match 0 icon-id -16563 "
                        "name-id -20525 port-connection 2 port-type 0x6973706B "
                        "model OutputEQPort\0"
                        "output index 1 device-mask 6 device-match 4 icon-id -16563 "
                        "name-id -20523 port-connection 3 port-type 0x6573706B "
                        "model OutputDallasEQPort\0"
                        "output index 2 device-mask 2 device-match 2 icon-id -16563 "
                        "name-id -20524 port-connection 1 port-type 0x6864706E "
                        "model OutputEQPort\0", 0x334);

        fword("finish-device");
}

static void
i2s_init(const char *path, phys_addr_t addr)
{
        phandle_t dnode;
        int props[10];
        char buf[128];

        fword("new-device");
        push_str("i2s");
        fword("device-name");
        push_str("i2s");
        fword("device-type");

        snprintf(buf, sizeof(buf), "%s/i2s", path);
        dnode = find_dev(buf);
        set_property(dnode, "built-in", "", 0);

        props[0] = 0x00010000;
        props[1] = 0x00001000;
        props[2] = 0x00008000;
        props[3] = 0x00000100;
        props[4] = 0x00008100;
        props[5] = 0x00000100;
        props[6] = 0x00008200;
        props[7] = 0x00000100;
        props[8] = 0x00008300;
        props[9] = 0x00000100;
        set_property(dnode, "reg", (char *)&props, 10 * sizeof(props[0]));
        set_int_property(dnode, "#address-cells", 1);
        set_property(dnode, "ranges", "", 0);

        /* i2s-a child */
        fword("new-device");
        push_str("i2s-a");
        fword("device-name");
        push_str("soundbus");
        fword("device-type");

        snprintf(buf, sizeof(buf), "%s/i2s/i2s-a", path);
        dnode = find_dev(buf);
        set_property(dnode, "compatible", "i2sbus", 7);
        set_property(dnode, "built-in", "", 0);

        props[0] = 0x00010000;
        props[1] = 0x00001000;
        props[2] = 0x00008000;
        props[3] = 0x00000100;
        props[4] = 0x00008100;
        props[5] = 0x00000100;
        set_property(dnode, "reg", (char *)&props, 6 * sizeof(props[0]));

        props[0] = 0x1e;
        props[1] = 0x1;
        props[2] = 0x1;
        props[3] = 0x0;
        props[4] = 0x2;
        props[5] = 0x0;
        set_property(dnode, "interrupts", (char *)&props, 6 * sizeof(props[0]));

        props[0] = 0x2;
        props[1] = 0x4;
        props[2] = 0x4;
        set_property(dnode, "AAPL,requested-priorities", (char *)&props, 3 * sizeof(props[0]));

        set_property(dnode, "AAPL,clock-id", "i2s0i045i049i018", 16);

        tumbler_init(buf);

        fword("finish-device");

        fword("finish-device");
}

static void macio_gpio_init(const char *path)
{
    fword("new-device");

    push_str("gpio");
    fword("device-name");

    push_str("gpio");
    fword("device-type");

    PUSH(1);
    fword("encode-int");
    push_str("#address-cells");
    fword("property");

    PUSH(0);
    fword("encode-int");
    push_str("#size-cells");
    fword("property");

    push_str("mac-io-gpio");
    fword("encode-string");
    push_str("compatible");
    fword("property");

    PUSH(0x50);
    fword("encode-int");
    PUSH(0x30);
    fword("encode-int");
    fword("encode+");
    push_str("reg");
    fword("property");

    /* Build the extint-gpio1 for the PMU */
    fword("new-device");
    push_str("extint-gpio1");
    fword("device-name");
    PUSH(0x2f);
    fword("encode-int");
    PUSH(0x1);
    fword("encode-int");
    fword("encode+");
    push_str("interrupts");
    fword("property");
    PUSH(0x9);
    fword("encode-int");
    push_str("reg");
    fword("property");
    push_str("keywest-gpio1");
    fword("encode-string");
    push_str("gpio");
    fword("encode-string");
    fword("encode+");
    push_str("compatible");
    fword("property");
    fword("finish-device");

    /* Build the programmer-switch */
    fword("new-device");
    push_str("programmer-switch");
    fword("device-name");
    push_str("programmer-switch");
    fword("encode-string");
    push_str("device_type");
    fword("property");
    PUSH(0x37);
    fword("encode-int");
    PUSH(0x0);
    fword("encode-int");
    fword("encode+");
    push_str("interrupts");
    fword("property");
    fword("finish-device");

    if (is_g4da()) {
        /* gpio5 - headphone-mute */
        fword("new-device");
        push_str("gpio5");
        fword("device-name");
        push_str("gpio5");
        fword("device-type");
        push_str("keywest-5");
        fword("encode-string");
        push_str("compatible");
        fword("property");
        push_str("headphone-mute");
        fword("encode-string");
        push_str("audio-gpio");
        fword("property");
        PUSH(0);
        fword("encode-int");
        push_str("audio-gpio-active-state");
        fword("property");
        set_property(find_dev("gpio5"), "built-in", "", 0);
        fword("finish-device");

        /* gpio6 - amp-mute */
        fword("new-device");
        push_str("gpio6");
        fword("device-name");
        push_str("gpio6");
        fword("device-type");
        push_str("keywest-6");
        fword("encode-string");
        push_str("compatible");
        fword("property");
        push_str("amp-mute");
        fword("encode-string");
        push_str("audio-gpio");
        fword("property");
        PUSH(0);
        fword("encode-int");
        push_str("audio-gpio-active-state");
        fword("property");
        set_property(find_dev("gpio6"), "built-in", "", 0);
        fword("finish-device");

        /* gpio11 - audio-hw-reset */
        fword("new-device");
        push_str("gpio11");
        fword("device-name");
        push_str("gpio11");
        fword("device-type");
        push_str("keywest-11");
        fword("encode-string");
        push_str("compatible");
        fword("property");
        push_str("audio-hw-reset");
        fword("encode-string");
        push_str("audio-gpio");
        fword("property");
        PUSH(0);
        fword("encode-int");
        push_str("audio-gpio-active-state");
        fword("property");
        set_property(find_dev("gpio11"), "built-in", "", 0);
        fword("finish-device");

        /* extint-gpio15 - headphone detect */
        fword("new-device");
        push_str("extint-gpio15");
        fword("device-name");
        push_str("extint-gpio15");
        fword("device-type");
        push_str("keywest-gpio15");
        fword("encode-string");
        push_str("compatible");
        fword("property");
        set_property(find_dev("extint-gpio15"), "built-in", "", 0);
        PUSH(0x3d);
        fword("encode-int");
        PUSH(0x0);
        fword("encode-int");
        fword("encode+");
        push_str("interrupts");
        fword("property");
        fword("finish-device");

        /* extint-gpio16 - speaker-id (Dallas) */
        fword("new-device");
        push_str("extint-gpio16");
        fword("device-name");
        push_str("extint-gpio16");
        fword("device-type");
        push_str("keywest-gpio16");
        fword("encode-string");
        push_str("compatible");
        fword("property");
        push_str(".DallasDriver");
        fword("encode-string");
        push_str("AAPL,driver-name");
        fword("property");
        push_str("speaker-id");
        fword("encode-string");
        push_str("one-wire-bus");
        fword("property");
        set_property(find_dev("extint-gpio16"), "built-in", "", 0);
        PUSH(0x3e);
        fword("encode-int");
        PUSH(0x0);
        fword("encode-int");
        fword("encode+");
        push_str("interrupts");
        fword("property");
        fword("finish-device");
    }

    fword("finish-device");
}

void
ob_macio_heathrow_init(const char *path, phys_addr_t addr)
{
    phandle_t aliases;

    BIND_NODE_METHODS(get_cur_dev(), ob_macio);

    cuda_init(path, addr);
    macio_nvram_init(path, addr);
    escc_init(path, addr);
    macio_ide_init(path, addr, 2);
    davbus_init(path, addr);

    aliases = find_dev("/aliases");
    set_property(aliases, "mac-io", path, strlen(path) + 1);
}

static void
i2c_init_node(const char *path, phys_addr_t addr)
{
    phandle_t dnode;
    int props[2];
    char buf[128];

    /* Create i2c@18000 node under mac-io */
    push_str(path);
    fword("find-device");

    fword("new-device");
    push_str("i2c");
    fword("device-name");
    push_str("i2c");
    fword("device-type");

    snprintf(buf, sizeof(buf), "%s/i2c", path);
    dnode = find_dev(buf);
    set_property(dnode, "compatible", "keywest-i2c", 12);
    set_property(dnode, "built-in", "", 0);

    props[0] = __cpu_to_be32(0x18000);
    props[1] = __cpu_to_be32(0x1000);
    set_property(dnode, "reg", (char *)&props, sizeof(props));

    set_int_property(dnode, "AAPL,address-step", 0x10);
    set_int_property(dnode, "AAPL,i2c-rate", 100);
    set_property(dnode, "AAPL,driver-name", ".i2c-mac-io", 12);
    set_int_property(dnode, "#address-cells", 1);
    set_int_property(dnode, "#size-cells", 0);

    props[0] = 0x1a;
    props[1] = 0x1;
    set_property(dnode, "interrupts", (char *)&props, 2 * sizeof(props[0]));

    /* deq child (TAS3001 codec) */
    fword("new-device");
    push_str("deq");
    fword("device-name");
    push_str("deq");
    fword("device-type");
    set_int_property(find_dev("/pci/mac-io/i2c/deq"), "i2c-address", 0x68);
    fword("finish-device");

    fword("finish-device");
    device_end();
}

void
ob_macio_keylargo_init(const char *path, phys_addr_t addr)
{
    phandle_t aliases;

    BIND_NODE_METHODS(get_cur_dev(), ob_macio);

    if (has_pmu()) {
        macio_gpio_init(path);
        pmu_init(path, addr);
    } else {
        cuda_init(path, addr);
    }

    escc_init(path, addr);
    macio_ide_init(path, addr, 2);
    openpic_init(path, addr);

    if (is_g4da()) {
        i2s_init(path, addr);
        i2c_init_node(path, addr);
    } else {
        davbus_init(path, addr);
    }

    aliases = find_dev("/aliases");
    set_property(aliases, "mac-io", path, strlen(path) + 1);
}
