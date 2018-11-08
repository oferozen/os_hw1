/*********************************************************************
 *                
 * Filename:      ali-ircc.h
 * Version:       0.5
 * Description:   Driver for the ALI M1535D and M1543C FIR Controller
 * Status:        Experimental.
 * Author:        Benjamin Kong <benjamin_kong@ali.com.tw>
 * Created at:    2000/10/16 03:46PM
 * Modified at:   2001/1/3 02:55PM
 * Modified by:   Benjamin Kong <benjamin_kong@ali.com.tw>
 * 
 *     Copyright (c) 2000 Benjamin Kong <benjamin_kong@ali.com.tw>
 *     All Rights Reserved
 *      
 *     This program is free software; you can redistribute it and/or 
 *     modify it under the terms of the GNU General Public License as 
 *     published by the Free Software Foundation; either version 2 of 
 *     the License, or (at your option) any later version.
 *  
 ********************************************************************/

#include <linux/module.h>

#include <linux/kernel.h>
#include <linux/types.h>
#include <linux/skbuff.h>
#include <linux/netdevice.h>
#include <linux/ioport.h>
#include <linux/delay.h>
#include <linux/slab.h>
#include <linux/init.h>
#include <linux/rtnetlink.h>
#include <linux/serial_reg.h>

#include <asm/io.h>
#include <asm/dma.h>
#include <asm/byteorder.h>

#include <linux/pm.h>

#include <net/irda/wrapper.h>
#include <net/irda/irda.h>
#include <net/irda/irmod.h>
#include <net/irda/irlap_frame.h>
#include <net/irda/irda_device.h>

#include <net/irda/ali-ircc.h>

#define CHIP_IO_EXTENT 8
#define BROKEN_DONGLE_ID

static char *driver_name = "ali-ircc";

/* Module parameters */
static int qos_mtt_bits = 0x07;  /* 1 ms or more */

/* Use BIOS settions by default, but user may supply module parameters */
static unsigned int io[]  = { ~0, ~0, ~0, ~0 };
static unsigned int irq[] = { 0, 0, 0, 0 };
static unsigned int dma[] = { 0, 0, 0, 0 };

static int  ali_ircc_probe_43(ali_chip_t *chip, chipio_t *info);
static int  ali_ircc_probe_53(ali_chip_t *chip, chipio_t *info);
static int  ali_ircc_init_43(ali_chip_t *chip, chipio_t *info);
static int  ali_ircc_init_53(ali_chip_t *chip, chipio_t *info);

/* These are the currently known ALi sourth-bridge chipsets, the only one difference
 * is that M1543C doesn't support HP HDSL-3600
 */
static ali_chip_t chips[] =
{
	{ "M1543", { 0x3f0, 0x370 }, 0x51, 0x23, 0x20, 0x43, ali_ircc_probe_53, ali_ircc_init_43 },
	{ "M1535", { 0x3f0, 0x370 }, 0x51, 0x23, 0x20, 0x53, ali_ircc_probe_53, ali_ircc_init_53 },
	{ NULL }
};

/* Max 4 instances for now */
static struct ali_ircc_cb *dev_self[] = { NULL, NULL, NULL, NULL };

/* Dongle Types */
static char *dongle_types[] = {
	"TFDS6000",
	"HP HSDL-3600",
	"HP HSDL-1100",	
	"No dongle connected",
};

/* Some prototypes */
static int  ali_ircc_open(int i, chipio_t *info);

#ifdef MODULE
static int  ali_ircc_close(struct ali_ircc_cb *self);
#endif /* MODULE */

static int  ali_ircc_setup(chipio_t *info);
static int  ali_ircc_is_receiving(struct ali_ircc_cb *self);
static int  ali_ircc_net_init(struct net_device *dev);
static int  ali_ircc_net_open(struct net_device *dev);
static int  ali_ircc_net_close(struct net_device *dev);
static int  ali_ircc_net_ioctl(struct net_device *dev, struct ifreq *rq, int cmd);
static int  ali_ircc_pmproc(struct pm_dev *dev, pm_request_t rqst, void *data);
static void ali_ircc_change_speed(struct ali_ircc_cb *self, __u32 baud);
static void ali_ircc_interrupt(int irq, void *dev_id, struct pt_regs *regs);
static void ali_ircc_suspend(struct ali_ircc_cb *self);
static void ali_ircc_wakeup(struct ali_ircc_cb *self);
static struct net_device_stats *ali_ircc_net_get_stats(struct net_device *dev);

/* SIR function */
static int  ali_ircc_sir_hard_xmit(struct sk_buff *skb, struct net_device *dev);
static void ali_ircc_sir_interrupt(int irq, struct ali_ircc_cb *self, struct pt_regs *regs);
static void ali_ircc_sir_receive(struct ali_ircc_cb *self);
static void ali_ircc_sir_write_wakeup(struct ali_ircc_cb *self);
static int  ali_ircc_sir_write(int iobase, int fifo_size, __u8 *buf, int len);
static void ali_ircc_sir_change_speed(struct ali_ircc_cb *priv, __u32 speed);

/* FIR function */
static int  ali_ircc_fir_hard_xmit(struct sk_buff *skb, struct net_device *dev);
static void ali_ircc_fir_change_speed(struct ali_ircc_cb *priv, __u32 speed);
static void ali_ircc_fir_interrupt(int irq, struct ali_ircc_cb *self, struct pt_regs *regs);
static int  ali_ircc_dma_receive(struct ali_ircc_cb *self); 
static int  ali_ircc_dma_receive_complete(struct ali_ircc_cb *self);
static int  ali_ircc_dma_xmit_complete(struct ali_ircc_cb *self);
static void ali_ircc_dma_xmit(struct ali_ircc_cb *self);

/* My Function */
static int  ali_ircc_read_dongle_id (int i, chipio_t *info);
static void ali_ircc_change_dongle_speed(struct ali_ircc_cb *priv, int speed);

/* ALi chip function */
static void SIR2FIR(int iobase);
static void FIR2SIR(int iobase);
static void SetCOMInterrupts(struct ali_ircc_cb *self , unsigned char enable);

/*
 * Function ali_ircc_init ()
 *
 *    Initialize chip. Find out whay kinds of chips we are dealing with
 *    and their configuation registers address
 */
int __init ali_ircc_init(void)
{
	ali_chip_t *chip;
	chipio_t info;
	int ret = -ENODEV;
	int cfg, cfg_base;
	int reg, revision;
	int i = 0;
	
	IRDA_DEBUG(2, __FUNCTION__ "(), ---------------- Start ----------------\n");
	
	/* Probe for all the ALi chipsets we know about */
	for (chip= chips; chip->name; chip++, i++) 
	{
		IRDA_DEBUG(2, __FUNCTION__"(), Probing for %s ...\n", chip->name);
				
		/* Try all config registers for this chip */
		for (cfg=0; cfg<2; cfg++)
		{
			cfg_base = chip->cfg[cfg];
			if (!cfg_base)
				continue;
				
			memset(&info, 0, sizeof(chipio_t));
			info.cfg_base = cfg_base;
			info.fir_base = io[i];
			info.dma = dma[i];
			info.irq = irq[i];
			
			
			/* Enter Configuration */
			outb(chip->entr1, cfg_base);
			outb(chip->entr2, cfg_base);
			
			/* Select Logical Device 5 Registers (UART2) */
			outb(0x07, cfg_base);
			outb(0x05, cfg_base+1);
			
			/* Read Chip Identification Register */
			outb(chip->cid_index, cfg_base);	
			reg = inb(cfg_base+1);	
				
			if (reg == chip->cid_value)
			{
				IRDA_DEBUG(2, __FUNCTION__
					"(), Chip found at 0x%03x\n", cfg_base);
					   
				outb(0x1F, cfg_base);
				revision = inb(cfg_base+1);
				IRDA_DEBUG(2, __FUNCTION__ 
					   "(), Found %s chip, revision=%d\n",
					   chip->name, revision);					
				
				/* 
				 * If the user supplies the base address, then
				 * we init the chip, if not we probe the values
				 * set by the BIOS
				 */				
				if (io[i] < 2000)
				{
					chip->init(chip, &info);
				}
				else
				{
					chip->probe(chip, &info);	
				}
				
				if (ali_ircc_open(i, &info) == 0)
					ret = 0;
				i++;				
			}
			else
			{
				IRDA_DEBUG(2, __FUNCTION__ 
					   "(), No %s chip at 0x%03x\n", chip->name, cfg_base);
			}
			/* Exit configuration */
			outb(0xbb, cfg_base);
		}
	}		
		
	IRDA_DEBUG(2, __FUNCTION__ "(), ----------------- End -----------------\n");					   		
	return ret;
}

/*
 * Function ali_ircc_cleanup ()
 *
 *    Close all configured chips
 *
 */
#ifdef MODULE
static void ali_ircc_cleanup(void)
{
	int i;

	IRDA_DEBUG(2, __FUNCTION__ "(), ---------------- Start ----------------\n");	
	
	pm_unregister_all(ali_ircc_pmproc);

	for (i=0; i < 4; i++) {
		if (dev_self[i])
			ali_ircc_close(dev_self[i]);
	}
	
	IRDA_DEBUG(2, __FUNCTION__ "(), ----------------- End -----------------\n");
}
#endif /* MODULE */

/*
 * Function ali_ircc_open (int i, chipio_t *inf)
 *
 *    Open driver instance
 *
 */
static int ali_ircc_open(int i, chipio_t *info)
{
	struct net_device *dev;
	struct ali_ircc_cb *self;
	struct pm_dev *pmdev;
	int dongle_id;
	int ret;
	int err;
			
	IRDA_DEBUG(2, __FUNCTION__ "(), ---------------- Start ----------------\n");	
	
	/* Set FIR FIFO and DMA Threshold */
	if ((ali_ircc_setup(info)) == -1)
		return -1;
		
	/* Allocate new instance of the driver */
	self = kmalloc(sizeof(struct ali_ircc_cb), GFP_KERNEL);
	if (self == NULL) 
	{
		ERROR(__FUNCTION__ "(), can't allocate memory for control block!\n");
		return -ENOMEM;
	}
	memset(self, 0, sizeof(struct ali_ircc_cb));
	spin_lock_init(&self->lock);
   
	/* Need to store self somewhere */
	dev_self[i] = self;
	self->index = i;

	/* Initialize IO */
	self->io.cfg_base  = info->cfg_base;	/* In ali_ircc_probe_53 assign 		*/
	self->io.fir_base  = info->fir_base;	/* info->sir_base = info->fir_base 	*/
	self->io.sir_base  = info->sir_base; 	/* ALi SIR and FIR use the same address */
        self->io.irq       = info->irq;
        self->io.fir_ext   = CHIP_IO_EXTENT;
        self->io.dma       = info->dma;
        self->io.fifo_size = 16;		/* SIR: 16, FIR: 32 Benjamin 2000/11/1 */
	
	/* Reserve the ioports that we need */
	if (!request_region(self->io.fir_base, self->io.fir_ext, driver_name)) {
		WARNING(__FUNCTION__ "(), can't get iobase of 0x%03x\n",
			self->io.fir_base);
		dev_self[i] = NULL;
		kfree(self);
		return -ENODEV;
	}

	/* Initialize QoS for this device */
	irda_init_max_qos_capabilies(&self->qos);
	
	/* The only value we must override it the baudrate */
	self->qos.baud_rate.bits = IR_9600|IR_19200|IR_38400|IR_57600|
		IR_115200|IR_576000|IR_1152000|(IR_4000000 << 8); // benjamin 2000/11/8 05:27PM
			
	self->qos.min_turn_time.bits = qos_mtt_bits;
			
	irda_qos_bits_to_value(&self->qos);
	
	self->flags = IFF_FIR|IFF_MIR|IFF_SIR|IFF_DMA|IFF_PIO; 	// benjamin 2000/11/8 05:27PM	

	/* Max DMA buffer size needed = (data_size + 6) * (window_size) + 6; */
	self->rx_buff.truesize = 14384; 
	self->tx_buff.truesize = 14384;

	/* Allocate memory if needed */
	self->rx_buff.head = (__u8 *) kmalloc(self->rx_buff.truesize,
					      GFP_KERNEL |GFP_DMA); 
	if (self->rx_buff.head == NULL) 
	{
		kfree(self);
		return -ENOMEM;
	}
	memset(self->rx_buff.head, 0, self->rx_buff.truesize);
	
	self->tx_buff.head = (__u8 *) kmalloc(self->tx_buff.truesize, 
					      GFP_KERNEL|GFP_DMA); 
	if (self->tx_buff.head == NULL) {
		kfree(self->rx_buff.head);
		kfree(self);
		return -ENOMEM;
	}
	memset(self->tx_buff.head, 0, self->tx_buff.truesize);

	self->rx_buff.in_frame = FALSE;
	self->rx_buff.state = OUTSIDE_FRAME;
	self->tx_buff.data = self->tx_buff.head;
	self->rx_buff.data = self->rx_buff.head;
	
	/* Reset Tx queue info */
	self->tx_fifo.len = self->tx_fifo.ptr = self->tx_fifo.free = 0;
	self->tx_fifo.tail = self->tx_buff.head;

	if (!(dev = dev_alloc("irda%d", &err))) {
		ERROR(__FUNCTION__ "(), dev_alloc() failed!\n");
		return -ENOMEM;
	}

	dev->priv = (void *) self;
	self->netdev = dev;
	
	/* Override the network functions we need to use */
	dev->init            = ali_ircc_net_init;
	dev->hard_start_xmit = ali_ircc_sir_hard_xmit;
	dev->open            = ali_ircc_net_open;
	dev->stop            = ali_ircc_net_close;
	dev->do_ioctl        = ali_ircc_net_ioctl;
	dev->get_stats	     = ali_ircc_net_get_stats;

	rtnl_lock();
	err = register_netdevice(dev);
	rtnl_unlock();
	if (err) {
		ERROR(__FUNCTION__ "(), register_netdev() failed!\n");
		return -1;
	}
	MESSAGE("IrDA: Registered device %s\n", dev->name);

	/* Check dongle id */
	dongle_id = ali_ircc_read_dongle_id(i, info);
	MESSAGE(__FUNCTION__ "(), %s, Found dongle: %s\n", driver_name, dongle_types[dongle_id]);
		
	self->io.dongle_id = dongle_id;
	
        pmdev = pm_register(PM_SYS_DEV, PM_SYS_IRDA, ali_ircc_pmproc);
        if (pmdev)
                pmdev->data = self;

	IRDA_DEBUG(2, __FUNCTION__ "(), ----------------- End -----------------\n");
	
	return 0;
}


#ifdef MODULE
/*
 * Function ali_ircc_close (self)
 *
 *    Close driver instance
 *
 */
static int ali_ircc_close(struct ali_ircc_cb *self)
{
	int iobase;

	IRDA_DEBUG(4, __FUNCTION__ "(), ---------------- Start ----------------\n");

	ASSERT(self != NULL, return -1;);

        iobase = self->io.fir_base;

	/* Remove netdevice */
	if (self->netdev) {
		rtnl_lock();
		unregister_netdevice(self->netdev);
		rtnl_unlock();
	}

	/* Release the PORT that this driver is using */
	IRDA_DEBUG(4, __FUNCTION__ "(), Releasing Region %03x\n", self->io.fir_base);
	release_region(self->io.fir_base, self->io.fir_ext);

	if (self->tx_buff.head)
		kfree(self->tx_buff.head);
	
	if (self->rx_buff.head)
		kfree(self->rx_buff.head);

	dev_self[self->index] = NULL;
	kfree(self);
	
	IRDA_DEBUG(2, __FUNCTION__ "(), ----------------- End -----------------\n");
	
	return 0;
}
#endif /* MODULE */

/*
 * Function ali_ircc_init_43 (chip, info)
 *
 *    Initialize the ALi M1543 chip. 
 */
static int ali_ircc_init_43(ali_chip_t *chip, chipio_t *info) 
{
	/* All controller information like I/O address, DMA channel, IRQ
	 * are set by BIOS
	 */
	
	return 0;
}

/*
 * Function ali_ircc_init_53 (chip, info)
 *
 *    Initialize the ALi M1535 chip. 
 */
static int ali_ircc_init_53(ali_chip_t *chip, chipio_t *info) 
{
	/* All controller information like I/O address, DMA channel, IRQ
	 * are set by BIOS
	 */
	
	return 0;
}

/*
 * Function ali_ircc_probe_43 (chip, info)
 *    	
 *	Probes for the ALi M1543
 */
static int ali_ircc_probe_43(ali_chip_t *chip, chipio_t *info)
{
	return 0;	
}

/*
 * Function ali_ircc_probe_53 (chip, info)
 *    	
 *	Probes for the ALi M1535D or M1535
 */
static int ali_ircc_probe_53(ali_chip_t *chip, chipio_t *info)
{
	int cfg_base = info->cfg_base;
	int hi, low, reg;
	
	IRDA_DEBUG(2, __FUNCTION__ "(), ---------------- Start ----------------\n");
	
	/* Enter Configuration */
	outb(chip->entr1, cfg_base);
	outb(chip->entr2, cfg_base);
	
	/* Select Logical Device 5 Registers (UART2) */
	outb(0x07, cfg_base);
	outb(0x05, cfg_base+1);
	
	/* Read address control register */
	outb(0x60, cfg_base);
	hi = inb(cfg_base+1);	
	outb(0x61, cfg_base);
	low = inb(cfg_base+1);
	info->fir_base = (hi<<8) + low;
	
	info->sir_base = info->fir_base;
	
	IRDA_DEBUG(2, __FUNCTION__ "(), probing fir_base=0x%03x\n", info->fir_base);
		
	/* Read IRQ control register */
	outb(0x70, cfg_base);
	reg = inb(cfg_base+1);
	info->irq = reg & 0x0f;
	IRDA_DEBUG(2, __FUNCTION__ "(), probing irq=%d\n", info->irq);
	
	/* Read DMA channel */
	outb(0x74, cfg_base);
	reg = inb(cfg_base+1);
	info->dma = reg & 0x07;
	
	if(info->dma == 0x04)
		WARNING(__FUNCTION__ "(), No DMA channel assigned !\n");
	else
		IRDA_DEBUG(2, __FUNCTION__ "(), probing dma=%d\n", info->dma);
	
	/* Read Enabled Status */
	outb(0x30, cfg_base);
	reg = inb(cfg_base+1);
	info->enabled = (reg & 0x80) && (reg & 0x01);
	IRDA_DEBUG(2, __FUNCTION__ "(), probing enabled=%d\n", info->enabled);
	
	/* Read Power Status */
	outb(0x22, cfg_base);
	reg = inb(cfg_base+1);
	info->suspended = (reg & 0x20);
	IRDA_DEBUG(2, __FUNCTION__ "(), probing suspended=%d\n", info->suspended);
	
	/* Exit configuration */
	outb(0xbb, cfg_base);
		
	IRDA_DEBUG(2, __FUNCTION__ "(), ----------------- End -----------------\n");	
	
	return 0;	
}

/*
 * Function ali_ircc_setup (info)
 *
 *    	Set FIR FIFO and DMA Threshold
 *	Returns non-negative on success.
 *
 */
static int ali_ircc_setup(chipio_t *info)
{
	unsigned char tmp;
	int version;
	int iobase = info->fir_base;
	
	IRDA_DEBUG(2, __FUNCTION__ "(), ---------------- Start ----------------\n");
	
	/* Switch to FIR space */
	SIR2FIR(iobase);
	
	/* Master Reset */
	outb(0x40, iobase+FIR_MCR); // benjamin 2000/11/30 11:45AM
	
	/* Read FIR ID Version Register */
	switch_bank(iobase, BANK3);
	version = inb(iobase+FIR_ID_VR);
	
	/* Should be 0x00 in the M1535/M1535D */
	if(version != 0x00)
	{
		ERROR("%s, Wrong chip version %02x\n", driver_name, version);
		return -1;
	}
	
	// MESSAGE("%s, Found chip at base=0x%03x\n", driver_name, info->cfg_base);
	
	/* Set FIR FIFO Threshold Register */
	switch_bank(iobase, BANK1);
	outb(RX_FIFO_Threshold, iobase+FIR_FIFO_TR);
	
	/* Set FIR DMA Threshold Register */
	outb(RX_DMA_Threshold, iobase+FIR_DMA_TR);
	
	/* CRC enable */
	switch_bank(iobase, BANK2);
	outb(inb(iobase+FIR_IRDA_CR) | IRDA_CR_CRC, iobase+FIR_IRDA_CR);
	
	/* NDIS driver set TX Length here BANK2 Alias 3, Alias4*/
	
	/* Switch to Bank 0 */
	switch_bank(iobase, BANK0);
	
	tmp = inb(iobase+FIR_LCR_B);
	tmp &=~0x20; // disable SIP
	tmp |= 0x80; // these two steps make RX mode
	tmp &= 0xbf;	
	outb(tmp, iobase+FIR_LCR_B);
		
	/* Disable Interrupt */
	outb(0x00, iobase+FIR_IER);
	
	
	/* Switch to SIR space */
	FIR2SIR(iobase);
	
	MESSAGE("%s, driver loaded (Benjamin Kong)\n", driver_name);
	
	/* Enable receive interrupts */ 
	// outb(UART_IER_RDI, iobase+UART_IER); //benjamin 2000/11/23 01:25PM
	// Turn on the interrupts in ali_ircc_net_open
	
	IRDA_DEBUG(2, __FUNCTION__ "(), ----------------- End ------------------\n");	
	
	return 0;
}

/*
 * Function ali_ircc_read_dongle_id (int index, info)
 *
 * Try to read dongle indentification. This procedure needs to be executed
 * once after power-on/reset. It also needs to be used whenever you suspect
 * that the user may have plugged/unplugged the IrDA Dongle.
 */
static int ali_ircc_read_dongle_id (int i, chipio_t *info)
{
	int dongle_id, reg;
	int cfg_base = info->cfg_base;
	
	IRDA_DEBUG(2, __FUNCTION__ "(), ---------------- Start ----------------\n");
		
	/* Enter Configuration */
	outb(chips[i].entr1, cfg_base);
	outb(chips[i].entr2, cfg_base);
	
	/* Select Logical Device 5 Registers (UART2) */
	outb(0x07, cfg_base);
	outb(0x05, cfg_base+1);
	
	/* Read Dongle ID */
	outb(0xf0, cfg_base);
	reg = inb(cfg_base+1);	
	dongle_id = ((reg>>6)&0x02) | ((reg>>5)&0x01);
	IRDA_DEBUG(2, __FUNCTION__ "(), probing dongle_id=%d, dongle_types=%s\n", 
		dongle_id, dongle_types[dongle_id]);
	
	/* Exit configuration */
	outb(0xbb, cfg_base);
			
	IRDA_DEBUG(2, __FUNCTION__ "(), ----------------- End ------------------\n");	
	
	return dongle_id;
}

/*
 * Function ali_ircc_interrupt (irq, dev_id, regs)
 *
 *    An interrupt from the chip has arrived. Time to do some work
 *
 */
static void ali_ircc_interrupt(int irq, void *dev_id, struct pt_regs *regs)
{
	struct net_device *dev = (struct net_device *) dev_id;
	struct ali_ircc_cb *self;
		
	IRDA_DEBUG(2, __FUNCTION__ "(), ---------------- Start ----------------\n");
		
 	if (!dev) {
		WARNING("%s: irq %d for unknown device.\n", driver_name, irq);
		return;
	}	
	
	self = (struct ali_ircc_cb *) dev->priv;
	
	spin_lock(&self->lock);
	
	/* Dispatch interrupt handler for the current speed */
	if (self->io.speed > 115200)
		ali_ircc_fir_interrupt(irq, self, regs);
	else
		ali_ircc_sir_interrupt(irq, self, regs);	
		
	spin_unlock(&self->lock);
	
	IRDA_DEBUG(2, __FUNCTION__ "(), ----------------- End ------------------\n");		
}
/*
 * Function ali_ircc_fir_interrupt(irq, struct ali_ircc_cb *self, regs)
 *
 *    Handle MIR/FIR interrupt
 *
 */
static void ali_ircc_fir_interrupt(int irq, struct ali_ircc_cb *self, struct pt_regs *regs)
{
	__u8 eir, OldMessageCount;
	int iobase, tmp;
	
	IRDA_DEBUG(1, __FUNCTION__ "(), ---------------- Start ----------------\n");
	
	iobase = self->io.fir_base;
	
	switch_bank(iobase, BANK0);	
	self->InterruptID = inb(iobase+FIR_IIR);		
	self->BusStatus = inb(iobase+FIR_BSR);	
	
	OldMessageCount = (self->LineStatus + 1) & 0x07;
	self->LineStatus = inb(iobase+FIR_LSR);	
	//self->ier = inb(iobase+FIR_IER); 		2000/12/1 04:32PM
	eir = self->InterruptID & self->ier; /* Mask out the interesting ones */ 
	
	IRDA_DEBUG(1, __FUNCTION__ "(), self->InterruptID = %x\n",self->InterruptID);
	IRDA_DEBUG(1, __FUNCTION__ "(), self->LineStatus = %x\n",self->LineStatus);
	IRDA_DEBUG(1, __FUNCTION__ "(), self->ier = %x\n",self->ier);
	IRDA_DEBUG(1, __FUNCTION__ "(), eir = %x\n",eir);
	
	/* Disable interrupts */
	 SetCOMInterrupts(self, FALSE);
	
	/* Tx or Rx Interrupt */
	
	if (eir & IIR_EOM) 
	{		
		if (self->io.direction == IO_XMIT) /* TX */
		{
			IRDA_DEBUG(1, __FUNCTION__ "(), ******* IIR_EOM (Tx) *******\n");
			
			if(ali_ircc_dma_xmit_complete(self))
			{
				if (irda_device_txqueue_empty(self->netdev)) 
				{
					/* Prepare for receive */
					ali_ircc_dma_receive(self);					
					self->ier = IER_EOM;									
				}
			}
			else
			{
				self->ier = IER_EOM; 					
			}
									
		}	
		else /* RX */
		{
			IRDA_DEBUG(1, __FUNCTION__ "(), ******* IIR_EOM (Rx) *******\n");
			
			if(OldMessageCount > ((self->LineStatus+1) & 0x07))
			{
				self->rcvFramesOverflow = TRUE;	
				IRDA_DEBUG(1, __FUNCTION__ "(), ******* self->rcvFramesOverflow = TRUE ******** \n");
			}
						
			if (ali_ircc_dma_receive_complete(self))
			{
				IRDA_DEBUG(1, __FUNCTION__ "(), ******* receive complete ******** \n");
				
				self->ier = IER_EOM;				
			}
			else
			{
				IRDA_DEBUG(1, __FUNCTION__ "(), ******* Not receive complete ******** \n");
				
				self->ier = IER_EOM | IER_TIMER;								
			}	
		
		}		
	}
	/* Timer Interrupt */
	else if (eir & IIR_TIMER)
	{	
		if(OldMessageCount > ((self->LineStatus+1) & 0x07))
		{
			self->rcvFramesOverflow = TRUE;	
			IRDA_DEBUG(1, __FUNCTION__ "(), ******* self->rcvFramesOverflow = TRUE ******* \n");
		}
		/* Disable Timer */
		switch_bank(iobase, BANK1);
		tmp = inb(iobase+FIR_CR);
		outb( tmp& ~CR_TIMER_EN, iobase+FIR_CR);
		
		/* Check if this is a Tx timer interrupt */
		if (self->io.direction == IO_XMIT)
		{
			ali_ircc_dma_xmit(self);
			
			/* Interrupt on EOM */
			self->ier = IER_EOM;
									
		}
		else /* Rx */
		{
			if(ali_ircc_dma_receive_complete(self)) 
			{
				self->ier = IER_EOM;
			}
			else
			{
				self->ier = IER_EOM | IER_TIMER;
			}	
		}		
	}
	
	/* Restore Interrupt */	
	SetCOMInterrupts(self, TRUE);	
		
	IRDA_DEBUG(1, __FUNCTION__ "(), ----------------- End ---------------\n");
}

/*
 * Function ali_ircc_sir_interrupt (irq, self, eir)
 *
 *    Handle SIR interrupt
 *
 */
static void ali_ircc_sir_interrupt(int irq, struct ali_ircc_cb *self, struct pt_regs *regs)
{
	int iobase;
	int iir, lsr;
	
	IRDA_DEBUG(2, __FUNCTION__ "(), ---------------- Start ----------------\n");
	
	iobase = self->io.sir_base;

	iir = inb(iobase+UART_IIR) & UART_IIR_ID;
	if (iir) {	
		/* Clear interrupt */
		lsr = inb(iobase+UART_LSR);

		IRDA_DEBUG(4, __FUNCTION__ 
			   "(), iir=%02x, lsr=%02x, iobase=%#x\n", 
			   iir, lsr, iobase);

		switch (iir) 
		{
			case UART_IIR_RLSI:
				IRDA_DEBUG(2, __FUNCTION__ "(), RLSI\n");
				break;
			case UART_IIR_RDI:
				/* Receive interrupt */
				ali_ircc_sir_receive(self);
				break;
			case UART_IIR_THRI:
				if (lsr & UART_LSR_THRE)
				{
					/* Transmitter ready for data */
					ali_ircc_sir_write_wakeup(self);				
				}				
				break;
			default:
				IRDA_DEBUG(0, __FUNCTION__ "(), unhandled IIR=%#x\n", iir);
				break;
		} 
		
	}
	
	
	IRDA_DEBUG(2, __FUNCTION__ "(), ----------------- End ------------------\n");	
}


/*
 * Function ali_ircc_sir_receive (self)
 *
 *    Receive one frame from the infrared port
 *
 */
static void ali_ircc_sir_receive(struct ali_ircc_cb *self) 
{
	int boguscount = 0;
	int iobase;
	
	IRDA_DEBUG(2, __FUNCTION__ "(), ---------------- Start ----------------\n");
	ASSERT(self != NULL, return;);

	iobase = self->io.sir_base;

	/*  
	 * Receive all characters in Rx FIFO, unwrap and unstuff them. 
         * async_unwrap_char will deliver all found frames  
	 */
	do {
		async_unwrap_char(self->netdev, &self->stats, &self->rx_buff, 
				  inb(iobase+UART_RX));

		/* Make sure we don't stay here to long */
		if (boguscount++ > 32) {
			IRDA_DEBUG(2,__FUNCTION__ "(), breaking!\n");
			break;
		}
	} while (inb(iobase+UART_LSR) & UART_LSR_DR);	
	
	IRDA_DEBUG(2, __FUNCTION__ "(), ----------------- End ------------------\n");	
}

/*
 * Function ali_ircc_sir_write_wakeup (tty)
 *
 *    Called by the driver when there's room for more data.  If we have
 *    more packets to send, we send them here.
 *
 */
static void ali_ircc_sir_write_wakeup(struct ali_ircc_cb *self)
{
	int actual = 0;
	int iobase;	

	ASSERT(self != NULL, return;);

	IRDA_DEBUG(2, __FUNCTION__ "(), ---------------- Start ----------------\n");
	
	iobase = self->io.sir_base;

	/* Finished with frame?  */
	if (self->tx_buff.len > 0)  
	{
		/* Write data left in transmit buffer */
		actual = ali_ircc_sir_write(iobase, self->io.fifo_size, 
				      self->tx_buff.data, self->tx_buff.len);
		self->tx_buff.data += actual;
		self->tx_buff.len  -= actual;
	} 
	else 
	{
		if (self->new_speed) 
		{
			/* We must wait until all data are gone */
			while(!(inb(iobase+UART_LSR) & UART_LSR_TEMT))
				IRDA_DEBUG(1, __FUNCTION__ "(), UART_LSR_THRE\n");
			
			IRDA_DEBUG(1, __FUNCTION__ "(), Changing speed! self->new_speed = %d\n", self->new_speed);
			ali_ircc_change_speed(self, self->new_speed);
			self->new_speed = 0;			
			
			// benjamin 2000/11/10 06:32PM
			if (self->io.speed > 115200)
			{
				IRDA_DEBUG(2, __FUNCTION__ "(), ali_ircc_change_speed from UART_LSR_TEMT \n");				
					
				self->ier = IER_EOM;
				// SetCOMInterrupts(self, TRUE);							
				return;							
			}
		}
		else
		{
			netif_wake_queue(self->netdev);	
		}
			
		self->stats.tx_packets++;
		
		/* Turn on receive interrupts */
		outb(UART_IER_RDI, iobase+UART_IER);
	}
		
	IRDA_DEBUG(2, __FUNCTION__ "(), ----------------- End ------------------\n");	
}

static void ali_ircc_change_speed(struct ali_ircc_cb *self, __u32 baud)
{
	struct net_device *dev = self->netdev;
	int iobase;
	
	IRDA_DEBUG(1, __FUNCTION__ "(), ---------------- Start ----------------\n");
	
	IRDA_DEBUG(2, __FUNCTION__ "(), setting speed = %d \n", baud);
	
	iobase = self->io.fir_base;
	
	SetCOMInterrupts(self, FALSE); // 2000/11/24 11:43AM
	
	/* Go to MIR, FIR Speed */
	if (baud > 115200)
	{
		
					
		ali_ircc_fir_change_speed(self, baud);			
		
		/* Install FIR xmit handler*/
		dev->hard_start_xmit = ali_ircc_fir_hard_xmit;		
				
		/* Enable Interuupt */
		self->ier = IER_EOM; // benjamin 2000/11/20 07:24PM					
				
		/* Be ready for incomming frames */
		ali_ircc_dma_receive(self);	// benajmin 2000/11/8 07:46PM not complete
	}	
	/* Go to SIR Speed */
	else
	{
		ali_ircc_sir_change_speed(self, baud);
				
		/* Install SIR xmit handler*/
		dev->hard_start_xmit = ali_ircc_sir_hard_xmit;
	}
	
		
	SetCOMInterrupts(self, TRUE);	// 2000/11/24 11:43AM
		
	netif_wake_queue(self->netdev);	
	
	IRDA_DEBUG(2, __FUNCTION__ "(), ----------------- End ------------------\n");	
}

static void ali_ircc_fir_change_speed(struct ali_ircc_cb *priv, __u32 baud)
{
		
	int iobase; 
	struct ali_ircc_cb *self = (struct ali_ircc_cb *) priv;
	struct net_device *dev;

	IRDA_DEBUG(1, __FUNCTION__ "(), ---------------- Start ----------------\n");
		
	ASSERT(self != NULL, return;);

	dev = self->netdev;
	iobase = self->io.fir_base;
	
	IRDA_DEBUG(1, __FUNCTION__ "(), self->io.speed = %d, change to speed = %d\n",self->io.speed,baud);
	
	/* Come from SIR speed */
	if(self->io.speed <=115200)
	{
		SIR2FIR(iobase);
	}
		
	/* Update accounting for new speed */
	self->io.speed = baud;
		
	// Set Dongle Speed mode
	ali_ircc_change_dongle_speed(self, baud);
		
	IRDA_DEBUG(1, __FUNCTION__ "(), ----------------- End ------------------\n");	
}

/*
 * Function ali_sir_change_speed (self, speed)
 *
 *    Set speed of IrDA port to specified baudrate
 *
 */
static void ali_ircc_sir_change_speed(struct ali_ircc_cb *priv, __u32 speed)
{
	struct ali_ircc_cb *self = (struct ali_ircc_cb *) priv;
	unsigned long flags;
	int iobase; 
	int fcr;    /* FIFO control reg */
	int lcr;    /* Line control reg */
	int divisor;

	IRDA_DEBUG(1, __FUNCTION__ "(), ---------------- Start ----------------\n");
	
	IRDA_DEBUG(1, __FUNCTION__ "(), Setting speed to: %d\n", speed);

	ASSERT(self != NULL, return;);

	iobase = self->io.sir_base;
	
	/* Come from MIR or FIR speed */
	if(self->io.speed >115200)
	{	
		// Set Dongle Speed mode first
		ali_ircc_change_dongle_speed(self, speed);
			
		FIR2SIR(iobase);
	}
		
	// Clear Line and Auxiluary status registers 2000/11/24 11:47AM
		
	inb(iobase+UART_LSR);
	inb(iobase+UART_SCR);
		
	/* Update accounting for new speed */
	self->io.speed = speed;

	spin_lock_irqsave(&self->lock, flags);

	divisor = 115200/speed;
	
	fcr = UART_FCR_ENABLE_FIFO;

	/* 
	 * Use trigger level 1 to avoid 3 ms. timeout delay at 9600 bps, and
	 * almost 1,7 ms at 19200 bps. At speeds above that we can just forget
	 * about this timeout since it will always be fast enough. 
	 */
	if (self->io.speed < 38400)
		fcr |= UART_FCR_TRIGGER_1;
	else 
		fcr |= UART_FCR_TRIGGER_14;
        
	/* IrDA ports use 8N1 */
	lcr = UART_LCR_WLEN8;
	
	outb(UART_LCR_DLAB | lcr, iobase+UART_LCR); /* Set DLAB */
	outb(divisor & 0xff,      iobase+UART_DLL); /* Set speed */
	outb(divisor >> 8,	  iobase+UART_DLM);
	outb(lcr,		  iobase+UART_LCR); /* Set 8N1	*/
	outb(fcr,		  iobase+UART_FCR); /* Enable FIFO's */

	/* without this, the conection will be broken after come back from FIR speed,
	   but with this, the SIR connection is harder to established */
	outb((UART_MCR_DTR | UART_MCR_RTS | UART_MCR_OUT2), iobase+UART_MCR);
	
	spin_unlock_irqrestore(&self->lock, flags);
	
	IRDA_DEBUG(1, __FUNCTION__ "(), ----------------- End ------------------\n");	
}

static void ali_ircc_change_dongle_speed(struct ali_ircc_cb *priv, int speed)
{
	
	struct ali_ircc_cb *self = (struct ali_ircc_cb *) priv;
	int iobase,dongle_id;
	unsigned long flags;
	int tmp = 0;
			
	IRDA_DEBUG(1, __FUNCTION__ "(), ---------------- Start ----------------\n");	
	
	iobase = self->io.fir_base; 	/* or iobase = self->io.sir_base; */
	dongle_id = self->io.dongle_id;
	
	save_flags(flags);
	cli();
		
	IRDA_DEBUG(1, __FUNCTION__ "(), Set Speed for %s , Speed = %d\n", dongle_types[dongle_id], speed);		
	
	switch_bank(iobase, BANK2);
	tmp = inb(iobase+FIR_IRDA_CR);
		
	/* IBM type dongle */
	if(dongle_id == 0)
	{				
		if(speed == 4000000)
		{
			//	      __ __	
			// SD/MODE __|     |__ __
			//               __ __ 
			// IRTX    __ __|     |__
			//         T1 T2 T3 T4 T5
			
			tmp &=  ~IRDA_CR_HDLC;		// HDLC=0
			tmp |= IRDA_CR_CRC;	   	// CRC=1
			
			switch_bank(iobase, BANK2);
			outb(tmp, iobase+FIR_IRDA_CR);
			
      			// T1 -> SD/MODE:0 IRTX:0
      			tmp &= ~0x09;
      			tmp |= 0x02;
      			outb(tmp, iobase+FIR_IRDA_CR);
      			udelay(2);
      			
      			// T2 -> SD/MODE:1 IRTX:0
      			tmp &= ~0x01;
      			tmp |= 0x0a;
      			outb(tmp, iobase+FIR_IRDA_CR);
      			udelay(2);
      			
      			// T3 -> SD/MODE:1 IRTX:1
      			tmp |= 0x0b;
      			outb(tmp, iobase+FIR_IRDA_CR);
      			udelay(2);
      			
      			// T4 -> SD/MODE:0 IRTX:1
      			tmp &= ~0x08;
      			tmp |= 0x03;
      			outb(tmp, iobase+FIR_IRDA_CR);
      			udelay(2);
      			
      			// T5 -> SD/MODE:0 IRTX:0
      			tmp &= ~0x09;
      			tmp |= 0x02;
      			outb(tmp, iobase+FIR_IRDA_CR);
      			udelay(2);
      			
      			// reset -> Normal TX output Signal
      			outb(tmp & ~0x02, iobase+FIR_IRDA_CR);      			
		}
		else /* speed <=1152000 */
		{	
			//	      __	
			// SD/MODE __|  |__
			//
			// IRTX    ________
			//         T1 T2 T3  
			
			/* MIR 115200, 57600 */
			if (speed==1152000)
			{
				tmp |= 0xA0;	   //HDLC=1, 1.152Mbps=1
      			}
      			else
      			{
				tmp &=~0x80;	   //HDLC 0.576Mbps
				tmp |= 0x20;	   //HDLC=1,
      			}			
      			
      			tmp |= IRDA_CR_CRC;	   	// CRC=1
      			
      			switch_bank(iobase, BANK2);
      			outb(tmp, iobase+FIR_IRDA_CR);
						
			/* MIR 115200, 57600 */	
						
			//switch_bank(iobase, BANK2);			
			// T1 -> SD/MODE:0 IRTX:0
      			tmp &= ~0x09;
      			tmp |= 0x02;
      			outb(tmp, iobase+FIR_IRDA_CR);
      			udelay(2);
      			
      			// T2 -> SD/MODE:1 IRTX:0
      			tmp &= ~0x01;     
      			tmp |= 0x0a;      
      			outb(tmp, iobase+FIR_IRDA_CR);
      			
      			// T3 -> SD/MODE:0 IRTX:0
      			tmp &= ~0x09;
      			tmp |= 0x02;
      			outb(tmp, iobase+FIR_IRDA_CR);
      			udelay(2);
      			
      			// reset -> Normal TX output Signal
      			outb(tmp & ~0x02, iobase+FIR_IRDA_CR);      						
		}		
	}
	else if (dongle_id == 1) /* HP HDSL-3600 */
	{
		switch(speed)
		{
		case 4000000:
			tmp &=  ~IRDA_CR_HDLC;	// HDLC=0
			break;	
			
		case 1152000:
			tmp |= 0xA0;	   	// HDLC=1, 1.152Mbps=1
      			break;
      			
      		case 576000:
      			tmp &=~0x80;	   	// HDLC 0.576Mbps
			tmp |= 0x20;	   	// HDLC=1,
			break;
      		}			
			
		tmp |= IRDA_CR_CRC;	   	// CRC=1
			
		switch_bank(iobase, BANK2);
      		outb(tmp, iobase+FIR_IRDA_CR);		
	}
	else /* HP HDSL-1100 */
	{
		if(speed <= 115200) /* SIR */
		{
			
			tmp &= ~IRDA_CR_FIR_SIN;	// HP sin select = 0
			
			switch_bank(iobase, BANK2);
      			outb(tmp, iobase+FIR_IRDA_CR);			
		}
		else /* MIR FIR */
		{	
			
			switch(speed)
			{
			case 4000000:
				tmp &=  ~IRDA_CR_HDLC;	// HDLC=0
				break;	
			
			case 1152000:
				tmp |= 0xA0;	   	// HDLC=1, 1.152Mbps=1
      				break;
      			
      			case 576000:
      				tmp &=~0x80;	   	// HDLC 0.576Mbps
				tmp |= 0x20;	   	// HDLC=1,
				break;
      			}			
			
			tmp |= IRDA_CR_CRC;	   	// CRC=1
			tmp |= IRDA_CR_FIR_SIN;		// HP sin select = 1
			
			switch_bank(iobase, BANK2);
      			outb(tmp, iobase+FIR_IRDA_CR);			
		}
	}
			
	switch_bank(iobase, BANK0);
	
	restore_flags(flags);
		
	IRDA_DEBUG(1, __FUNCTION__ "(), ----------------- End ------------------\n");		
}

/*
 * Function ali_ircc_sir_write (driver)
 *
 *    Fill Tx FIFO with transmit data
 *
 */
static int ali_ircc_sir_write(int iobase, int fifo_size, __u8 *buf, int len)
{
	int actual = 0;
	
	IRDA_DEBUG(2, __FUNCTION__ "(), ---------------- Start ----------------\n");
		
	/* Tx FIFO should be empty! */
	if (!(inb(iobase+UART_LSR) & UART_LSR_THRE)) {
		IRDA_DEBUG(0, __FUNCTION__ "(), failed, fifo not empty!\n");
		return 0;
	}
        
	/* Fill FIFO with current frame */
	while ((fifo_size-- > 0) && (actual < len)) {
		/* Transmit next byte */
		outb(buf[actual], iobase+UART_TX);

		actual++;
	}
	
        IRDA_DEBUG(2, __FUNCTION__ "(), ----------------- End ------------------\n");	
	return actual;
}

/*
 * Function ali_ircc_net_init (dev)
 *
 *    Initialize network device
 *
 */
static int ali_ircc_net_init(struct net_device *dev)
{
	IRDA_DEBUG(2, __FUNCTION__ "(), ---------------- Start ----------------\n");
	
	/* Setup to be a normal IrDA network device driver */
	irda_device_setup(dev);

	/* Insert overrides below this line! */

	IRDA_DEBUG(2, __FUNCTION__ "(), ----------------- End ------------------\n");	
	
	return 0;
}

/*
 * Function ali_ircc_net_open (dev)
 *
 *    Start the device
 *
 */
static int ali_ircc_net_open(struct net_device *dev)
{
	struct ali_ircc_cb *self;
	int iobase;
	char hwname[32];
		
	IRDA_DEBUG(2, __FUNCTION__ "(), ---------------- Start ----------------\n");
	
	ASSERT(dev != NULL, return -1;);
	
	self = (struct ali_ircc_cb *) dev->priv;
	
	ASSERT(self != NULL, return 0;);
	
	iobase = self->io.fir_base;
	
	/* Request IRQ and install Interrupt Handler */
	if (request_irq(self->io.irq, ali_ircc_interrupt, 0, dev->name, dev)) 
	{
		WARNING("%s, unable to allocate irq=%d\n", driver_name, 
			self->io.irq);
		return -EAGAIN;
	}
	
	/*
	 * Always allocate the DMA channel after the IRQ, and clean up on 
	 * failure.
	 */
	if (request_dma(self->io.dma, dev->name)) {
		WARNING("%s, unable to allocate dma=%d\n", driver_name, 
			self->io.dma);
		free_irq(self->io.irq, self);
		return -EAGAIN;
	}
	
	/* Turn on interrups */
	outb(UART_IER_RLSI | UART_IER_RDI |UART_IER_THRI, iobase+UART_IER);

	/* Ready to play! */
	netif_start_queue(dev); //benjamin by irport
	
	/* Give self a hardware name */
	sprintf(hwname, "ALI-FIR @ 0x%03x", self->io.fir_base);

	/* 
	 * Open new IrLAP layer instance, now that everything should be
	 * initialized properly 
	 */
	self->irlap = irlap_open(dev, &self->qos, hwname);
		
	MOD_INC_USE_COUNT;

	IRDA_DEBUG(2, __FUNCTION__ "(), ----------------- End ------------------\n");	
	
	return 0;
}

/*
 * Function ali_ircc_net_close (dev)
 *
 *    Stop the device
 *
 */
static int ali_ircc_net_close(struct net_device *dev)
{	

	struct ali_ircc_cb *self;
	//int iobase;
			
	IRDA_DEBUG(4, __FUNCTION__ "(), ---------------- Start ----------------\n");
		
	ASSERT(dev != NULL, return -1;);

	self = (struct ali_ircc_cb *) dev->priv;
	ASSERT(self != NULL, return 0;);

	/* Stop device */
	netif_stop_queue(dev);
	
	/* Stop and remove instance of IrLAP */
	if (self->irlap)
		irlap_close(self->irlap);
	self->irlap = NULL;
		
	disable_dma(self->io.dma);

	/* Disable interrupts */
	SetCOMInterrupts(self, FALSE);
	       
	free_irq(self->io.irq, dev);
	free_dma(self->io.dma);

	MOD_DEC_USE_COUNT;

	IRDA_DEBUG(2, __FUNCTION__ "(), ----------------- End ------------------\n");	
	
	return 0;
}

/*
 * Function ali_ircc_fir_hard_xmit (skb, dev)
 *
 *    Transmit the frame
 *
 */
static int ali_ircc_fir_hard_xmit(struct sk_buff *skb, struct net_device *dev)
{
	struct ali_ircc_cb *self;
	unsigned long flags;
	int iobase;
	__u32 speed;
	int mtt, diff;
	
	IRDA_DEBUG(1, __FUNCTION__ "(), ---------------- Start -----------------\n");	
	
	self = (struct ali_ircc_cb *) dev->priv;
	iobase = self->io.fir_base;

	netif_stop_queue(dev);
	
	/* Check if we need to change the speed */
	speed = irda_get_next_speed(skb);
	if ((speed != self->io.speed) && (speed != -1)) {
		/* Check for empty frame */
		if (!skb->len) {
			ali_ircc_change_speed(self, speed); 
			dev_kfree_skb(skb);
			return 0;
		} else
			self->new_speed = speed;
	}

	spin_lock_irqsave(&self->lock, flags);
	
	/* Register and copy this frame to DMA memory */
	self->tx_fifo.queue[self->tx_fifo.free].start = self->tx_fifo.tail;
	self->tx_fifo.queue[self->tx_fifo.free].len = skb->len;
	self->tx_fifo.tail += skb->len;

	self->stats.tx_bytes += skb->len;

	memcpy(self->tx_fifo.queue[self->tx_fifo.free].start, skb->data, 
	       skb->len);
	
	self->tx_fifo.len++;
	self->tx_fifo.free++;

	/* Start transmit only if there is currently no transmit going on */
	if (self->tx_fifo.len == 1) 
	{
		/* Check if we must wait the min turn time or not */
		mtt = irda_get_mtt(skb);
				
		if (mtt) 
		{
			/* Check how much time we have used already */
			do_gettimeofday(&self->now);
			
			diff = self->now.tv_usec - self->stamp.tv_usec;
			/* self->stamp is set from ali_ircc_dma_receive_complete() */
							
			IRDA_DEBUG(1, __FUNCTION__ "(), ******* diff = %d ******* \n", diff);	
			
			if (diff < 0) 
				diff += 1000000;
			
			/* Check if the mtt is larger than the time we have
			 * already used by all the protocol processing
			 */
			if (mtt > diff)
			{				
				mtt -= diff;
								
				/* 
				 * Use timer if delay larger than 1000 us, and
				 * use udelay for smaller values which should
				 * be acceptable
				 */
				if (mtt > 500) 
				{
					/* Adjust for timer resolution */
					mtt = (mtt+250) / 500; 	/* 4 discard, 5 get advanced, Let's round off */
					
					IRDA_DEBUG(1, __FUNCTION__ "(), ************** mtt = %d ***********\n", mtt);	
					
					/* Setup timer */
					if (mtt == 1) /* 500 us */
					{
						switch_bank(iobase, BANK1);
						outb(TIMER_IIR_500, iobase+FIR_TIMER_IIR);
					}	
					else if (mtt == 2) /* 1 ms */
					{
						switch_bank(iobase, BANK1);
						outb(TIMER_IIR_1ms, iobase+FIR_TIMER_IIR);
					}					
					else /* > 2ms -> 4ms */
					{
						switch_bank(iobase, BANK1);
						outb(TIMER_IIR_2ms, iobase+FIR_TIMER_IIR);
					}
					
					
					/* Start timer */
					outb(inb(iobase+FIR_CR) | CR_TIMER_EN, iobase+FIR_CR);
					self->io.direction = IO_XMIT;
					
					/* Enable timer interrupt */
					self->ier = IER_TIMER;
					SetCOMInterrupts(self, TRUE);					
					
					/* Timer will take care of the rest */
					goto out; 
				} 
				else
					udelay(mtt);
			} // if (if (mtt > diff)
		}// if (mtt) 
				
		/* Enable EOM interrupt */
		self->ier = IER_EOM;
		SetCOMInterrupts(self, TRUE);
		
		/* Transmit frame */
		ali_ircc_dma_xmit(self);
	} // if (self->tx_fifo.len == 1) 
	
 out:
 	
	/* Not busy transmitting anymore if window is not full */
	if (self->tx_fifo.free < MAX_TX_WINDOW)
		netif_wake_queue(self->netdev);
	
	/* Restore bank register */
	switch_bank(iobase, BANK0);

	spin_unlock_irqrestore(&self->lock, flags);
	dev_kfree_skb(skb);

	IRDA_DEBUG(1, __FUNCTION__ "(), ----------------- End ------------------\n");	
	return 0;	
}


static void ali_ircc_dma_xmit(struct ali_ircc_cb *self)
{
	int iobase, tmp;
	unsigned char FIFO_OPTI, Hi, Lo;
	
	
	IRDA_DEBUG(1, __FUNCTION__ "(), ---------------- Start -----------------\n");	
	
	iobase = self->io.fir_base;
	
	/* FIFO threshold , this method comes from NDIS5 code */
	
	if(self->tx_fifo.queue[self->tx_fifo.ptr].len < TX_FIFO_Threshold)
		FIFO_OPTI = self->tx_fifo.queue[self->tx_fifo.ptr].len-1;
	else
		FIFO_OPTI = TX_FIFO_Threshold;
	
	/* Disable DMA */
	switch_bank(iobase, BANK1);
	outb(inb(iobase+FIR_CR) & ~CR_DMA_EN, iobase+FIR_CR);
	
	self->io.direction = IO_XMIT;
	
	setup_dma(self->io.dma, 
		  self->tx_fifo.queue[self->tx_fifo.ptr].start, 
		  self->tx_fifo.queue[self->tx_fifo.ptr].len, 
		  DMA_TX_MODE);
		
	/* Reset Tx FIFO */
	switch_bank(iobase, BANK0);
	outb(LCR_A_FIFO_RESET, iobase+FIR_LCR_A);
	
	/* Set Tx FIFO threshold */
	if (self->fifo_opti_buf!=FIFO_OPTI) 
	{
		switch_bank(iobase, BANK1);
	    	outb(FIFO_OPTI, iobase+FIR_FIFO_TR) ;
	    	self->fifo_opti_buf=FIFO_OPTI;
	}
	
	/* Set Tx DMA threshold */
	switch_bank(iobase, BANK1);
	outb(TX_DMA_Threshold, iobase+FIR_DMA_TR);
	
	/* Set max Tx frame size */
	Hi = (self->tx_fifo.queue[self->tx_fifo.ptr].len >> 8) & 0x0f;
	Lo = self->tx_fifo.queue[self->tx_fifo.ptr].len & 0xff;
	switch_bank(iobase, BANK2);
	outb(Hi, iobase+FIR_TX_DSR_HI);
	outb(Lo, iobase+FIR_TX_DSR_LO);
	
	/* Disable SIP , Disable Brick Wall (we don't support in TX mode), Change to TX mode */
	switch_bank(iobase, BANK0);	
	tmp = inb(iobase+FIR_LCR_B);
	tmp &= ~0x20; // Disable SIP
	outb(((unsigned char)(tmp & 0x3f) | LCR_B_TX_MODE) & ~LCR_B_BW, iobase+FIR_LCR_B);
	IRDA_DEBUG(1, __FUNCTION__ "(), ******* Change to TX mode: FIR_LCR_B = 0x%x ******* \n", inb(iobase+FIR_LCR_B));
	
	outb(0, iobase+FIR_LSR);
			
	/* Enable DMA and Burst Mode */
	switch_bank(iobase, BANK1);
	outb(inb(iobase+FIR_CR) | CR_DMA_EN | CR_DMA_BURST, iobase+FIR_CR);
	
	switch_bank(iobase, BANK0); 
	
	IRDA_DEBUG(1, __FUNCTION__ "(), ----------------- End ------------------\n");
}

static int  ali_ircc_dma_xmit_complete(struct ali_ircc_cb *self)
{
	int iobase;
	int ret = TRUE;
	
	IRDA_DEBUG(1, __FUNCTION__ "(), ---------------- Start -----------------\n");	
	
	iobase = self->io.fir_base;
	
	/* Disable DMA */
	switch_bank(iobase, BANK1);
	outb(inb(iobase+FIR_CR) & ~CR_DMA_EN, iobase+FIR_CR);
	
	/* Check for underrun! */
	switch_bank(iobase, BANK0);
	if((inb(iobase+FIR_LSR) & LSR_FRAME_ABORT) == LSR_FRAME_ABORT)
	
	{
		ERROR(__FUNCTION__ "(), ********* LSR_FRAME_ABORT *********\n");	
		self->stats.tx_errors++;
		self->stats.tx_fifo_errors++;		
	}
	else 
	{
		self->stats.tx_packets++;
	}

	/* Check if we need to change the speed */
	if (self->new_speed) 
	{
		ali_ircc_change_speed(self, self->new_speed);
		self->new_speed = 0;
	}

	/* Finished with this frame, so prepare for next */
	self->tx_fifo.ptr++;
	self->tx_fifo.len--;

	/* Any frames to be sent back-to-back? */
	if (self->tx_fifo.len) 
	{
		ali_ircc_dma_xmit(self);
		
		/* Not finished yet! */
		ret = FALSE;
	} 
	else 
	{	/* Reset Tx FIFO info */
		self->tx_fifo.len = self->tx_fifo.ptr = self->tx_fifo.free = 0;
		self->tx_fifo.tail = self->tx_buff.head;
	}

	/* Make sure we have room for more frames */
	if (self->tx_fifo.free < MAX_TX_WINDOW) {
		/* Not busy transmitting anymore */
		/* Tell the network layer, that we can accept more frames */
		netif_wake_queue(self->netdev);
	}
		
	switch_bank(iobase, BANK0); 
	
	IRDA_DEBUG(1, __FUNCTION__ "(), ----------------- End ------------------\n");	
	return ret;
}

/*
 * Function ali_ircc_dma_receive (self)
 *
 *    Get ready for receiving a frame. The device will initiate a DMA
 *    if it starts to receive a frame.
 *
 */
static int ali_ircc_dma_receive(struct ali_ircc_cb *self) 
{
	int iobase, tmp;
	
	IRDA_DEBUG(1, __FUNCTION__ "(), ---------------- Start -----------------\n");	
	
	iobase = self->io.fir_base;
	
	/* Reset Tx FIFO info */
	self->tx_fifo.len = self->tx_fifo.ptr = self->tx_fifo.free = 0;
	self->tx_fifo.tail = self->tx_buff.head;
		
	/* Disable DMA */
	switch_bank(iobase, BANK1);
	outb(inb(iobase+FIR_CR) & ~CR_DMA_EN, iobase+FIR_CR);
	
	/* Reset Message Count */
	switch_bank(iobase, BANK0);
	outb(0x07, iobase+FIR_LSR);
		
	self->rcvFramesOverflow = FALSE;	
	
	self->LineStatus = inb(iobase+FIR_LSR) ;
	
	/* Reset Rx FIFO info */
	self->io.direction = IO_RECV;
	self->rx_buff.data = self->rx_buff.head;
		
	/* Reset Rx FIFO */
	// switch_bank(iobase, BANK0);
	outb(LCR_A_FIFO_RESET, iobase+FIR_LCR_A); 
	
	self->st_fifo.len = self->st_fifo.pending_bytes = 0;
	self->st_fifo.tail = self->st_fifo.head = 0;
		
	setup_dma(self->io.dma, self->rx_buff.data, self->rx_buff.truesize, 
		  DMA_RX_MODE);	
	 
	/* Set Receive Mode,Brick Wall */
	//switch_bank(iobase, BANK0);
	tmp = inb(iobase+FIR_LCR_B);
	outb((unsigned char)(tmp &0x3f) | LCR_B_RX_MODE | LCR_B_BW , iobase + FIR_LCR_B); // 2000/12/1 05:16PM
	IRDA_DEBUG(1, __FUNCTION__ "(), *** Change To RX mode: FIR_LCR_B = 0x%x *** \n", inb(iobase+FIR_LCR_B));
			
	/* Set Rx Threshold */
	switch_bank(iobase, BANK1);
	outb(RX_FIFO_Threshold, iobase+FIR_FIFO_TR);
	outb(RX_DMA_Threshold, iobase+FIR_DMA_TR);
		
	/* Enable DMA and Burst Mode */
	// switch_bank(iobase, BANK1);
	outb(CR_DMA_EN | CR_DMA_BURST, iobase+FIR_CR);
				
	switch_bank(iobase, BANK0); 
	IRDA_DEBUG(1, __FUNCTION__ "(), ----------------- End ------------------\n");	
	return 0;
}

static int  ali_ircc_dma_receive_complete(struct ali_ircc_cb *self)
{
	struct st_fifo *st_fifo;
	struct sk_buff *skb;
	__u8 status, MessageCount;
	int len, i, iobase, val;	

	IRDA_DEBUG(1, __FUNCTION__ "(), ---------------- Start -----------------\n");	

	st_fifo = &self->st_fifo;		
	iobase = self->io.fir_base;	
		
	switch_bank(iobase, BANK0);
	MessageCount = inb(iobase+ FIR_LSR)&0x07;
	
	if (MessageCount > 0)	
		IRDA_DEBUG(0, __FUNCTION__ "(), Messsage count = %d,\n", MessageCount);	
		
	for (i=0; i<=MessageCount; i++)
	{
		/* Bank 0 */
		switch_bank(iobase, BANK0);
		status = inb(iobase+FIR_LSR);
		
		switch_bank(iobase, BANK2);
		len = inb(iobase+FIR_RX_DSR_HI) & 0x0f;
		len = len << 8; 
		len |= inb(iobase+FIR_RX_DSR_LO);
		
		IRDA_DEBUG(1, __FUNCTION__ "(), RX Length = 0x%.2x,\n", len);	
		IRDA_DEBUG(1, __FUNCTION__ "(), RX Status = 0x%.2x,\n", status);
		
		if (st_fifo->tail >= MAX_RX_WINDOW) {
			IRDA_DEBUG(0, __FUNCTION__ "(), window is full!\n");
			continue;
		}
			
		st_fifo->entries[st_fifo->tail].status = status;
		st_fifo->entries[st_fifo->tail].len = len;
		st_fifo->pending_bytes += len;
		st_fifo->tail++;
		st_fifo->len++;
	}
			
	for (i=0; i<=MessageCount; i++)
	{	
		/* Get first entry */
		status = st_fifo->entries[st_fifo->head].status;
		len    = st_fifo->entries[st_fifo->head].len;
		st_fifo->pending_bytes -= len;
		st_fifo->head++;
		st_fifo->len--;			
		
		/* Check for errors */
		if ((status & 0xd8) || self->rcvFramesOverflow || (len==0)) 		
		{
			IRDA_DEBUG(0,__FUNCTION__ "(), ************* RX Errors ************ \n");	
			
			/* Skip frame */
			self->stats.rx_errors++;
			
			self->rx_buff.data += len;
			
			if (status & LSR_FIFO_UR) 
			{
				self->stats.rx_frame_errors++;
				IRDA_DEBUG(0,__FUNCTION__ "(), ************* FIFO Errors ************ \n");
			}	
			if (status & LSR_FRAME_ERROR)
			{
				self->stats.rx_frame_errors++;
				IRDA_DEBUG(0,__FUNCTION__ "(), ************* FRAME Errors ************ \n");
			}
							
			if (status & LSR_CRC_ERROR) 
			{
				self->stats.rx_crc_errors++;
				IRDA_DEBUG(0,__FUNCTION__ "(), ************* CRC Errors ************ \n");
			}
			
			if(self->rcvFramesOverflow)
			{
				self->stats.rx_frame_errors++;
				IRDA_DEBUG(0,__FUNCTION__ "(), ************* Overran DMA buffer ************ \n");								
			}
			if(len == 0)
			{
				self->stats.rx_frame_errors++;
				IRDA_DEBUG(0,__FUNCTION__ "(), ********** Receive Frame Size = 0 ********* \n");
			}
		}	 
		else 
		{
			
			if (st_fifo->pending_bytes < 32) 
			{
				switch_bank(iobase, BANK0);
				val = inb(iobase+FIR_BSR);	
				if ((val& BSR_FIFO_NOT_EMPTY)== 0x80) 
				{
					IRDA_DEBUG(0, __FUNCTION__ "(), ************* BSR_FIFO_NOT_EMPTY ************ \n");
					
					/* Put this entry back in fifo */
					st_fifo->head--;
					st_fifo->len++;
					st_fifo->pending_bytes += len;
					st_fifo->entries[st_fifo->head].status = status;
					st_fifo->entries[st_fifo->head].len = len;
						
					/*  
		 			* DMA not finished yet, so try again 
		 			* later, set timer value, resolution 
		 			* 500 us 
		 			*/
					 
					switch_bank(iobase, BANK1);
					outb(TIMER_IIR_500, iobase+FIR_TIMER_IIR); // 2001/1/2 05:07PM
					
					/* Enable Timer */
					outb(inb(iobase+FIR_CR) | CR_TIMER_EN, iobase+FIR_CR);
						
					return FALSE; /* I'll be back! */
				}
			}		
			
			/* 
			 * Remember the time we received this frame, so we can
			 * reduce the min turn time a bit since we will know
			 * how much time we have used for protocol processing
			 */
			do_gettimeofday(&self->stamp);

			skb = dev_alloc_skb(len+1);
			if (skb == NULL)  
			{
				WARNING(__FUNCTION__ "(), memory squeeze, "
					"dropping frame.\n");
				self->stats.rx_dropped++;

				return FALSE;
			}
			
			/* Make sure IP header gets aligned */
			skb_reserve(skb, 1); 
			
			/* Copy frame without CRC, CRC is removed by hardware*/
			skb_put(skb, len);
			memcpy(skb->data, self->rx_buff.data, len);

			/* Move to next frame */
			self->rx_buff.data += len;
			self->stats.rx_bytes += len;
			self->stats.rx_packets++;

			skb->dev = self->netdev;
			skb->mac.raw  = skb->data;
			skb->protocol = htons(ETH_P_IRDA);
			netif_rx(skb);
		}
	}
	
	switch_bank(iobase, BANK0);	
		
	IRDA_DEBUG(1, __FUNCTION__ "(), ----------------- End ------------------\n");	
	return TRUE;
}



/*
 * Function ali_ircc_sir_hard_xmit (skb, dev)
 *
 *    Transmit the frame!
 *
 */
static int ali_ircc_sir_hard_xmit(struct sk_buff *skb, struct net_device *dev)
{
	struct ali_ircc_cb *self;
	unsigned long flags;
	int iobase;
	__u32 speed;
	
	IRDA_DEBUG(2, __FUNCTION__ "(), ---------------- Start ----------------\n");
	
	ASSERT(dev != NULL, return 0;);
	
	self = (struct ali_ircc_cb *) dev->priv;
	ASSERT(self != NULL, return 0;);

	iobase = self->io.sir_base;

	netif_stop_queue(dev);
	
	/* Check if we need to change the speed */
	speed = irda_get_next_speed(skb);
	if ((speed != self->io.speed) && (speed != -1)) {
		/* Check for empty frame */
		if (!skb->len) {
			ali_ircc_change_speed(self, speed); 
			dev_kfree_skb(skb);
			return 0;
		} else
			self->new_speed = speed;
	}

	spin_lock_irqsave(&self->lock, flags);

	/* Init tx buffer */
	self->tx_buff.data = self->tx_buff.head;

        /* Copy skb to tx_buff while wrapping, stuffing and making CRC */
	self->tx_buff.len = async_wrap_skb(skb, self->tx_buff.data, 
					   self->tx_buff.truesize);
	
	self->stats.tx_bytes += self->tx_buff.len;

	/* Turn on transmit finished interrupt. Will fire immediately!  */
	outb(UART_IER_THRI, iobase+UART_IER); 

	spin_unlock_irqrestore(&self->lock, flags);

	dev_kfree_skb(skb);
	
	IRDA_DEBUG(2, __FUNCTION__ "(), ----------------- End ------------------\n");	
	
	return 0;	
}


/*
 * Function ali_ircc_net_ioctl (dev, rq, cmd)
 *
 *    Process IOCTL commands for this device
 *
 */
static int ali_ircc_net_ioctl(struct net_device *dev, struct ifreq *rq, int cmd)
{
	struct if_irda_req *irq = (struct if_irda_req *) rq;
	struct ali_ircc_cb *self;
	unsigned long flags;
	int ret = 0;
	
	IRDA_DEBUG(2, __FUNCTION__ "(), ---------------- Start ----------------\n");
	
	ASSERT(dev != NULL, return -1;);

	self = dev->priv;

	ASSERT(self != NULL, return -1;);

	IRDA_DEBUG(2, __FUNCTION__ "(), %s, (cmd=0x%X)\n", dev->name, cmd);
	
	/* Disable interrupts & save flags */
	save_flags(flags);
	cli();	
	
	switch (cmd) {
	case SIOCSBANDWIDTH: /* Set bandwidth */
		IRDA_DEBUG(1, __FUNCTION__ "(), SIOCSBANDWIDTH\n");
		/*
		 * This function will also be used by IrLAP to change the
		 * speed, so we still must allow for speed change within
		 * interrupt context.
		 */
		if (!in_interrupt() && !capable(CAP_NET_ADMIN))
			return -EPERM;
		
		ali_ircc_change_speed(self, irq->ifr_baudrate);		
		break;
	case SIOCSMEDIABUSY: /* Set media busy */
		IRDA_DEBUG(1, __FUNCTION__ "(), SIOCSMEDIABUSY\n");
		if (!capable(CAP_NET_ADMIN))
			return -EPERM;
		irda_device_set_media_busy(self->netdev, TRUE);
		break;
	case SIOCGRECEIVING: /* Check if we are receiving right now */
		IRDA_DEBUG(2, __FUNCTION__ "(), SIOCGRECEIVING\n");
		irq->ifr_receiving = ali_ircc_is_receiving(self);
		break;
	default:
		ret = -EOPNOTSUPP;
	}
	
	restore_flags(flags);
	
	IRDA_DEBUG(2, __FUNCTION__ "(), ----------------- End ------------------\n");	
	
	return ret;
}

/*
 * Function ali_ircc_is_receiving (self)
 *
 *    Return TRUE is we are currently receiving a frame
 *
 */
static int ali_ircc_is_receiving(struct ali_ircc_cb *self)
{
	unsigned long flags;
	int status = FALSE;
	int iobase;		
	
	IRDA_DEBUG(2, __FUNCTION__ "(), ---------------- Start -----------------\n");
	
	ASSERT(self != NULL, return FALSE;);

	spin_lock_irqsave(&self->lock, flags);

	if (self->io.speed > 115200) 
	{
		iobase = self->io.fir_base;
		
		switch_bank(iobase, BANK1);
		if((inb(iobase+FIR_FIFO_FR) & 0x3f) != 0) 		
		{
			/* We are receiving something */
			IRDA_DEBUG(1, __FUNCTION__ "(), We are receiving something\n");
			status = TRUE;
		}
		switch_bank(iobase, BANK0);		
	} 
	else
	{ 
		status = (self->rx_buff.state != OUTSIDE_FRAME);
	}
	
	spin_unlock_irqrestore(&self->lock, flags);
	
	IRDA_DEBUG(2, __FUNCTION__ "(), ----------------- End ------------------\n");
	
	return status;
}

static struct net_device_stats *ali_ircc_net_get_stats(struct net_device *dev)
{
	struct ali_ircc_cb *self = (struct ali_ircc_cb *) dev->priv;
	
	IRDA_DEBUG(2, __FUNCTION__ "(), ---------------- Start ----------------\n");
		
	IRDA_DEBUG(2, __FUNCTION__ "(), ----------------- End ------------------\n");	
	
	return &self->stats;
}

static void ali_ircc_suspend(struct ali_ircc_cb *self)
{
	IRDA_DEBUG(2, __FUNCTION__ "(), ---------------- Start ----------------\n");
	
	MESSAGE("%s, Suspending\n", driver_name);

	if (self->io.suspended)
		return;

	ali_ircc_net_close(self->netdev);

	self->io.suspended = 1;
	
	IRDA_DEBUG(2, __FUNCTION__ "(), ----------------- End ------------------\n");	
}

static void ali_ircc_wakeup(struct ali_ircc_cb *self)
{
	IRDA_DEBUG(2, __FUNCTION__ "(), ---------------- Start ----------------\n");
	
	if (!self->io.suspended)
		return;
	
	ali_ircc_net_open(self->netdev);
	
	MESSAGE("%s, Waking up\n", driver_name);

	self->io.suspended = 0;
	
	IRDA_DEBUG(2, __FUNCTION__ "(), ----------------- End ------------------\n");	
}

static int ali_ircc_pmproc(struct pm_dev *dev, pm_request_t rqst, void *data)
{
        struct ali_ircc_cb *self = (struct ali_ircc_cb*) dev->data;
        
        IRDA_DEBUG(2, __FUNCTION__ "(), ---------------- Start ----------------\n");
	
        if (self) {
                switch (rqst) {
                case PM_SUSPEND:
                        ali_ircc_suspend(self);
                        break;
                case PM_RESUME:
                        ali_ircc_wakeup(self);
                        break;
                }
        }
        
        IRDA_DEBUG(2, __FUNCTION__ "(), ----------------- End ------------------\n");	
        
	return 0;
}


/* ALi Chip Function */

static void SetCOMInterrupts(struct ali_ircc_cb *self , unsigned char enable)
{
	
	unsigned char newMask;
	
	int iobase = self->io.fir_base; /* or sir_base */

	IRDA_DEBUG(2, __FUNCTION__ "(), -------- Start -------- ( Enable = %d )\n", enable);	
	
	/* Enable the interrupt which we wish to */
	if (enable){
		if (self->io.direction == IO_XMIT)
		{
			if (self->io.speed > 115200) /* FIR, MIR */
			{
				newMask = self->ier;
			}
			else /* SIR */
			{
				newMask = UART_IER_THRI | UART_IER_RDI;
			}
		}
		else {
			if (self->io.speed > 115200) /* FIR, MIR */
			{
				newMask = self->ier;
			}
			else /* SIR */
			{
				newMask = UART_IER_RDI;
			}
		}
	}
	else /* Disable all the interrupts */
	{
		newMask = 0x00;

	}

	//SIR and FIR has different registers
	if (self->io.speed > 115200)
	{	
		switch_bank(iobase, BANK0);
		outb(newMask, iobase+FIR_IER);
	}
	else
		outb(newMask, iobase+UART_IER);
		
	IRDA_DEBUG(2, __FUNCTION__ "(), ----------------- End ------------------\n");	
}

static void SIR2FIR(int iobase)
{
	//unsigned char tmp;
	unsigned long flags;
		
	IRDA_DEBUG(1, __FUNCTION__ "(), ---------------- Start ----------------\n");
	
	save_flags(flags);
	cli();
	
	outb(0x28, iobase+UART_MCR);
	outb(0x68, iobase+UART_MCR);
	outb(0x88, iobase+UART_MCR);		
	
	restore_flags(flags);
		
	outb(0x60, iobase+FIR_MCR); 	/*  Master Reset */
	outb(0x20, iobase+FIR_MCR); 	/*  Master Interrupt Enable */
	
	//tmp = inb(iobase+FIR_LCR_B);	/* SIP enable */
	//tmp |= 0x20;
	//outb(tmp, iobase+FIR_LCR_B);	
	
	IRDA_DEBUG(1, __FUNCTION__ "(), ----------------- End ------------------\n");	
}

static void FIR2SIR(int iobase)
{
	unsigned char val;
	unsigned long flags;
	
	IRDA_DEBUG(1, __FUNCTION__ "(), ---------------- Start ----------------\n");
	
	save_flags(flags);
	cli();
	
	outb(0x20, iobase+FIR_MCR); 	/* IRQ to low */
	outb(0x00, iobase+UART_IER); 	
		
	outb(0xA0, iobase+FIR_MCR); 	/* Don't set master reset */
	outb(0x00, iobase+UART_FCR);
	outb(0x07, iobase+UART_FCR);		
	
	val = inb(iobase+UART_RX);
	val = inb(iobase+UART_LSR);
	val = inb(iobase+UART_MSR);
	
	restore_flags(flags);
	
	IRDA_DEBUG(1, __FUNCTION__ "(), ----------------- End ------------------\n");
}

#ifdef MODULE
MODULE_AUTHOR("Benjamin Kong <benjamin_kong@ali.com.tw>");
MODULE_DESCRIPTION("ALi FIR Controller Driver");
MODULE_LICENSE("GPL");


MODULE_PARM(io,  "1-4i");
MODULE_PARM_DESC(io, "Base I/O addresses");
MODULE_PARM(irq, "1-4i");
MODULE_PARM_DESC(irq, "IRQ lines");
MODULE_PARM(dma, "1-4i");
MODULE_PARM_DESC(dma, "DMA channels");

int init_module(void)
{
	return ali_ircc_init();	
}

void cleanup_module(void)
{
	ali_ircc_cleanup();
}
#endif /* MODULE */