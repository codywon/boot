/*****************************************************************************/
/*! file ap93_pci.c
** /brief PCI support for AP91/93 board
**
**  This provides the support code required for PCI support on the AP91/93
**  board in the U-Boot environment.  This board is a Python based system
**  with a Merlin WLAN interface.  This file also contains the support
**  for initialization of the Merlin radios on the PCi bus, required for
**  pre-configuration for use by Linux.
**
**  Copyright (c) 2008 Atheros Communications Inc.  All rights reserved.
**
*/

#include <common.h>
#include <command.h>
#include <asm/mipsregs.h>
#include <asm/addrspace.h>
#include <config.h>
#include <version.h>
#include <pci.h>
#include "ar7240_soc.h"

/*
** PCI controller "hose" value
*/
static int ar7240_local_write_config(int where, int size, uint32_t value){
	ar7240_reg_wr((AR7240_PCI_CRP + where),value);
	return(0);
}

/*
** We will use the ART configuration information stored in flash to initialize
** these devices as required.
*/

// TODO: calibration data doesn't exist?
/*
void plat_dev_init(void){
	u32 val;
	u32 addr;
	u32 BaseAddr = 0x10000000;
	u32 CalAddr = WLANCAL;
	volatile u16 *calData;
	calData = (u16 *)CalAddr;

	if(calData[0] != 0xa55a && calData[0] != 0x5aa5){
		printf("BOARD IS NOT CALIBRATED!!!\n");
		return;
	}
}
*/

/******************************************************************************/
/*!
**  \brief pci host initialization
**
**  Sets up the PCI controller on the host.  For AR7240 this may not be necessary,
**  but this function is required for board support.
**
** We want a 1:1 mapping between PCI and DDR for inbound and outbound.
** The PCI<---AHB decoding works as follows:
**
** 8 registers in the DDR unit provide software configurable 32 bit offsets
** for each of the eight 16MB PCI windows in the 128MB. The offsets will be
** added to any address in the 16MB segment before being sent to the PCI unit.
**
** Essentially  for any AHB address generated by the CPU,
** 1. the MSB  four bits are stripped off, [31:28],
** 2. Bit 27 is used to decide between the lower 128Mb (PCI) or the rest of
**    the AHB space
** 3. Bits 26:24 are used to access one of the 8 window registers and are
**    masked off.
** 4. If it is a PCI address, then the WINDOW offset in the WINDOW register
**    corresponding to the next 3 bits (bit 26:24) is ADDED to the address,
**    to generate the address to PCI unit.
**
**     eg. CPU address = 0x100000ff
**         window 0 offset = 0x10000000
**         This points to lowermost 16MB window in PCI space.
**         So the resulting address would be 0x000000ff+0x10000000
**         = 0x100000ff
**
**         eg2. CPU address = 0x120000ff
**         WINDOW 2 offset = 0x12000000
**         resulting address would be 0x000000ff+0x12000000
**                         = 0x120000ff
**
** There is no translation for inbound access (PCI device as a master)
**
**  \return N/A
*/

/*
static int ar7240_pcibios_init(void){
	if(is_ar9341()){
		return(0);
	}

	if(((ar7240_reg_rd(AR7240_PCI_LCL_RESET)) & 0x1) == 0x0){
		printf("***** Warning *****: PCIe WLAN H/W not found !!!\n");
		return(0);
	}

	return(1);
}
*/

#define pci_udelay(n)	do { uint32_t i; for(i = 0; i < ((n)/10); i++) udelay(10); } while(0)

int pci_init_board(void){
	uint32_t cmd = 0, reg_val;
	
	reg_val = ar7240_reg_rd(0xb804006c);
	ar7240_reg_wr(0xb804006c, reg_val | 2);

	ar7240_reg_wr(0xb804000c, 1 << 2);

	pci_udelay(100000);

	/*
	if((ar7240_reg_rd(WASP_BOOTSTRAP_REG) & WASP_REF_CLK_25) == 0){
		ar7240_reg_wr_nf(AR934X_PCIE_PLL_DITHER_DIV_MAX,
						 PCIE_PLL_DITHER_DIV_MAX_EN_DITHER_SET(0) |
						 PCIE_PLL_DITHER_DIV_MAX_USE_MAX_SET(1) |
						 PCIE_PLL_DITHER_DIV_MAX_DIV_MAX_INT_SET(0x20) |
						 PCIE_PLL_DITHER_DIV_MAX_DIV_MAX_FRAC_SET(0)
						 );
	} else {
		printf("%s: PCIe PLL not set for 40MHz refclk\n", __func__);
	}
	*/

	ar7240_reg_rmw_set(AR7240_RESET, AR7240_RESET_PCIE);	// core in reset
	pci_udelay(10000);
	
	ar7240_reg_rmw_set(AR7240_RESET, AR7240_RESET_PCIE_PHY);// phy in reset
	pci_udelay(10000);
	
	ar7240_reg_rmw_clear(RST_MISC2_ADDRESS, RST_MISC2_PERSTN_RCPHY_SET(1));	// pci phy analog in reset
	pci_udelay(10000);
	
	ar7240_reg_wr(0x180f0000, 0x1ffc0);			// ltssm is disabled
	pci_udelay(100);

	ar7240_reg_wr_nf(AR7240_PCI_LCL_RESET, 0);	// End point in reset
	pci_udelay(100000);

	//ar7240_reg_rmw_clear(AR7240_RESET, AR7240_RESET_PCIE_PHY);

	/*
	if((ar7240_reg_rd(AR7240_REV_ID) & 0xf) == 0){
		ar7240_reg_wr_nf(AR934X_PCIE_PLL_CONFIG,
						 PCIE_PLL_CONFIG_REFDIV_SET(1) |
						 PCIE_PLL_CONFIG_BYPASS_SET(1) |
						 PCIE_PLL_CONFIG_PLLPWD_SET(1)
						 );
		pci_udelay(10000);
		
		ar7240_reg_wr_nf(AR934X_PCIE_PLL_CONFIG,
						 PCIE_PLL_CONFIG_REFDIV_SET(1) |
						 PCIE_PLL_CONFIG_BYPASS_SET(1) |
						 PCIE_PLL_CONFIG_PLLPWD_SET(0)
						 );
		pci_udelay(1000);
		
		ar7240_reg_wr_nf(AR934X_PCIE_PLL_CONFIG,
						 ar7240_reg_rd(AR934X_PCIE_PLL_CONFIG) &
						 (~PCIE_PLL_CONFIG_BYPASS_SET(1))
						 );
		pci_udelay(1000);
	} else {
	*/
		ar7240_reg_wr_nf(0xb8116c04, (0x0 << 30) | (0x4 << 26) | (0x32 << 19) | (1 << 16) | (3 << 13) | (0x1e << 7));
        pci_udelay(10000);
		
		ar7240_reg_wr_nf(0xb8116c08, (6 << 23));
        pci_udelay(10000);
		
		ar7240_reg_wr_nf(0xb8050010, 0x40010800);
		pci_udelay(10000);
		
		ar7240_reg_wr_nf(0xb8050014, 0xc013fffe);
		pci_udelay(10000);
		
		ar7240_reg_wr_nf(0xb8050018, 0x00138000);
		pci_udelay(10000);
		
		ar7240_reg_wr_nf(0xb8050010, 0x00010800);
		pci_udelay(100000);
		
		ar7240_reg_wr_nf(0xb8050010, 0x00000800);
		pci_udelay(10000);
	/*
	}
	*/
	
	ar7240_reg_rmw_set(RST_MISC2_ADDRESS, RST_MISC2_PERSTN_RCPHY_SET(1)); // pci phy analog out of reset
	pci_udelay(10000);

	ar7240_reg_rmw_clear(AR7240_RESET, AR7240_RESET_PCIE_PHY);	// phy out of reset
	pci_udelay(10000);

	ar7240_reg_rmw_clear(AR7240_RESET, AR7240_RESET_PCIE);	// core out of reset
	pci_udelay(1000);

	cmd =	PCI_COMMAND_MEMORY |
			PCI_COMMAND_MASTER |
			PCI_COMMAND_INVALIDATE |
			PCI_COMMAND_PARITY |
			PCI_COMMAND_SERR |
			PCI_COMMAND_FAST_BACK;

	ar7240_local_write_config(PCI_COMMAND, 4, cmd);		// pci cmd reg init
	ar7240_local_write_config(0x20, 4, 0x1ff01000);		// membase setting
	ar7240_local_write_config(0x24, 4, 0x1ff01000);		// prefetch membase setting

	//if((is_ar7241() || is_ar7242() || is_wasp())){
		ar7240_reg_wr(0x180f0000, 0x1ffc1);		// ltssm enable
	//} else {
	//	ar7240_reg_wr(0x180f0000, 0x1);
	//}
	pci_udelay(100000);

	ar7240_reg_wr_nf(AR7240_PCI_LCL_RESET, 4);		// EP out of reset
	//pci_udelay(100000);

	/*
	 *  Delay increased from 100 to 1000, so as to
	 *  get the correct status from PCI LCL RESET register
	 */
	// pci_udelay(100000);

	/*
	 * Check if the WLAN PCI-E H/W is present, If the
	 * WLAN H/W is not present, skip the PCI platform
	 * initialization code and return
	 */
	//if(((ar7240_reg_rd(AR7240_PCI_LCL_RESET)) & 0x1) == 0x0){
	//	printf("## Error: PCIe WLAN Module not found!\n");
	//}

	//plat_dev_init();
	
	return(0);
}
