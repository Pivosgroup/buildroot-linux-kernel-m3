/*
 * Amlogic Ethernet Driver
 *
 * Copyright (C) 2012 Amlogic, Inc.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the named License,
 * or any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307, USA
 *
 * Author: Platform-BJ@amlogic.com
 *
 */

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/string.h>
#include <linux/slab.h>
#include <linux/interrupt.h>
#include <linux/netdevice.h>
#include <linux/etherdevice.h>
#include <linux/io.h>
#include <linux/mii.h>
#include <linux/sched.h>
#include <linux/crc32.h>


#include <asm/delay.h>
#include <mach/pinmux.h>
#include <mach/gpio.h>

#include "am_net8218.h"


#define MODULE_NAME "ethernet"
#define DRIVER_NAME "ethernet"

#define DRV_NAME	DRIVER_NAME
#define DRV_VERSION	"v2.0.0"

MODULE_DESCRIPTION("Amlogic Ethernet Driver");
MODULE_AUTHOR("Platform-BJ@amlogic.com>");
MODULE_LICENSE("Amlogic");
MODULE_VERSION(DRV_VERSION);

// >0 basic init and remove info;
// >1 further setup info;
// >2 rx data dump
// >3 tx data dump
#ifdef CONFIG_AM_ETHERNET_DEBUG_LEVEL
static int g_debug = CONFIG_AM_ETHERNET_DEBUG_LEVEL;
#else
static int g_debug = 1;
#endif
static unsigned int g_tx_cnt = 0;
static unsigned int g_rx_cnt = 0;

module_param_named(amlog_level, g_debug, int, 0664);
MODULE_PARM_DESC(amlog_level, "ethernet debug level\n");

//#define LOOP_BACK_TEST
//#define MAC_LOOPBACK_TEST
//#define PHY_LOOPBACK_TEST

static int running = 0;
static struct net_device *my_ndev = NULL;
static unsigned int g_mac_setup = 0;
static char DEFMAC[] = "\x00\x01\x23\xcd\xee\xaf";

#define PERIPHS_SET_BITS(reg,val)	{	\
    	WRITE_CBUS_REG(reg,READ_CBUS_REG(reg) |(val));}
#define PERIPHS_CLEAR_BITS(reg,val)	{	\
	WRITE_CBUS_REG(reg,READ_CBUS_REG(reg) & ~(val));}

void start_test(struct net_device *dev);
static void write_mac_addr(struct net_device *dev, char *macaddr);
static int ethernet_reset(struct net_device *dev);
/* --------------------------------------------------------------------------*/
/**
 * @brief  mdio_read 
 *
 * @param  dev
 * @param  phyid
 * @param  reg
 *
 * @return 
 */
/* --------------------------------------------------------------------------*/
static int mdio_read(struct net_device *dev, int phyid, int reg)
{
#define WR (1<<1)
#define MDCCLK (0x1) << 2	//our 130 MHz
#define BUSY 0x1

	struct am_net_private *priv = netdev_priv(dev);
	unsigned long busy = 0;
	unsigned long reg4;
	unsigned long val = 0;
	reg4 = phyid << 11 | reg << 6 | MDCCLK | BUSY;
	/*
	do{ //waiting the phy is ready to write ...
	busy=IO_READ32(priv->base_addr+ETH_MAC_4_GMII_Addr);
	}while(busy&0x1);
	*/
	IO_WRITE32(reg4, priv->base_addr + ETH_MAC_4_GMII_Addr);
	do {			//waiting the phy is ready to write ...
		busy = IO_READ32(priv->base_addr + ETH_MAC_4_GMII_Addr);
	} while (busy & 0x1);
	val = IO_READ32(priv->base_addr + ETH_MAC_5_GMII_Data) & 0xffff;
	return val;
}

/* --------------------------------------------------------------------------*/
/**
 * @brief  mdio_write 
 *
 * @param  dev
 * @param  phyid
 * @param  reg
 * @param  val
 */
/* --------------------------------------------------------------------------*/
static void mdio_write(struct net_device *dev, int phyid, int reg, int val)
{

#define WR (1<<1)
#define MDCCLK (0x1) << 2	//our 130 MHz
#define BUSY 0x1

	struct am_net_private *priv = netdev_priv(dev);
	unsigned long busy = 0;
	unsigned long reg4;
	reg4 = phyid << 11 | reg << 6 | MDCCLK | WR | BUSY;
	IO_WRITE32(val, priv->base_addr + ETH_MAC_5_GMII_Data);
	do {			//waiting the phy is ready to write ...
		busy = IO_READ32(priv->base_addr + ETH_MAC_4_GMII_Addr);
	} while (busy & 0x1);
	IO_WRITE32(reg4, priv->base_addr + ETH_MAC_4_GMII_Addr);
	do {			//waiting the phy is ready to write ...
		busy = IO_READ32(priv->base_addr + ETH_MAC_4_GMII_Addr);
	} while (busy & 0x1);
}

/* --------------------------------------------------------------------------*/
/**
 * @brief  data_dump 
 *
 * @param  p
 * @param  len
 */
/* --------------------------------------------------------------------------*/
static void data_dump(unsigned char *p, int len)
{
	int i, j;
	char s[20];
	for (i = 0; i < len; i += 16) {
		printk("%08x:", (unsigned int)p);
		for (j = 0; j < 16 && j < len - 0 * 16; j++) {
			s[j] = (p[j] > 15 && p[j] < 128) ? p[j] : '.';
			printk(" %02x", p[j]);
		}
		s[j] = 0;
		printk(" |%s|\n", s);
		p = p + 16;
	}
}

/* --------------------------------------------------------------------------*/
/**
 * @brief  tx_data_dump 
 *
 * @param  p
 * @param  len
 */
/* --------------------------------------------------------------------------*/
static void tx_data_dump(unsigned char *p, int len)
{
	if ((g_debug == 3) || (g_debug == 5)) {
		printk("---------->\n");
		data_dump(p, len);
	}
	g_tx_cnt++;

	return;
}

/* --------------------------------------------------------------------------*/
/**
 * @brief  rx_data_dump 
 *
 * @param  p
 * @param  len
 */
/* --------------------------------------------------------------------------*/
static void rx_data_dump(unsigned char *p, int len)
{
	if ((g_debug == 4) || (g_debug == 5)) {
		printk("<----------\n");
		data_dump(p, len);
	}
	g_rx_cnt++;

	return;
}

/* --------------------------------------------------------------------------*/
/**
 * @brief  netdev_ioctl 
 *
 * @param  dev
 * @param  rq
 * @param  cmd
 *
 * @return 
 */
/* --------------------------------------------------------------------------*/
static int netdev_ioctl(struct net_device *dev, struct ifreq *rq, int cmd)
{
	struct mii_ioctl_data *data = if_mii(rq);
	struct am_net_private *np = netdev_priv(dev);
	char addr[MAX_ADDR_LEN];
	if (g_debug > 0) {
		printk("Ethernet Driver  ioctl (%x) \n", cmd);
	}
	switch (cmd) {
	case SIOCGMIIPHY:	/* Get address of MII PHY in use. */
		data->phy_id =
		    ((struct am_net_private *)netdev_priv(dev))->phys[0] & 0x1f;
		/* Fall Through */

	case SIOCGMIIREG:	/* Read MII PHY register. */
		spin_lock_irq(&np->lock);
		data->val_out =
		    mdio_read(dev, data->phy_id & 0x1f, data->reg_num & 0x1f);
		spin_unlock_irq(&np->lock);
		return 0;

	case SIOCSMIIREG:	/* Write MII PHY register. */
		if (!capable(CAP_NET_ADMIN)) {
			return -EPERM;
		}
		spin_lock_irq(&np->lock);
		mdio_write(dev, data->phy_id & 0x1f, data->reg_num & 0x1f, data->val_in);
		spin_unlock_irq(&np->lock);
		return 0;
	case SIOCSIFHWADDR:

		if (copy_from_user(&addr,
		                   (void __user *)rq->ifr_hwaddr.sa_data,
		                   MAX_ADDR_LEN)) {
			return -EFAULT;
		}
		if (g_debug > 0)
			printk("set mac addr to %02x:%02x:%02x:%02x:%02x:%02x\n",
			       addr[0], addr[1], addr[2], addr[3], addr[4],
			       addr[5]);
		spin_lock_irq(&np->lock);
		memcpy(dev->dev_addr, &addr, MAX_ADDR_LEN);
		write_mac_addr(dev, addr);
		spin_unlock_irq(&np->lock);
	default:
		if (g_debug > 0) {
			printk("Ethernet Driver unknow ioctl (%x) \n", cmd);
		}
		return -EOPNOTSUPP;
	}
	return 0;
}

/* --------------------------------------------------------------------------*/
/**
 * @brief  init_rxtx_rings 
 *
 * @param  dev
 *
 * @return 
 */
/* --------------------------------------------------------------------------*/
int init_rxtx_rings(struct net_device *dev)
{
	struct am_net_private *np = netdev_priv(dev);
	int i;
#ifndef DMA_USE_SKB_BUF
	unsigned long tx = 0, rx = 0;
#endif
#ifdef DMA_USE_MALLOC_ADDR
	rx = (unsigned long)kmalloc((RX_RING_SIZE) * np->rx_buf_sz, GFP_KERNEL);
	if (rx == 0) {
		printk("error to alloc Rx  ring buf\n");
		return -1;
	}
	tx = (unsigned long)kmalloc((TX_RING_SIZE) * np->rx_buf_sz, GFP_KERNEL);
	if (tx == 0) {
		kfree((void *)rx);
		printk("error to alloc Tx  ring buf\n");
		return -1;
	}
#elif defined(DMA_USE_SKB_BUF)
	//not needed
#else
	tx = TX_BUF_ADDR;
	rx = RX_BUF_ADDR;
#endif

	/* Fill in the Rx buffers.  Handle allocation failure gracefully. */
	for (i = 0; i < RX_RING_SIZE; i++) {
#ifdef DMA_USE_SKB_BUF
		struct sk_buff *skb = dev_alloc_skb(np->rx_buf_sz);
		np->rx_ring[i].skb = skb;
		if (skb == NULL) {
			break;
		}
		skb_reserve(skb, 2);	/* 16 byte alignd for ip */
		skb->dev = dev;	/* Mark as being used by this device. */
		np->rx_ring[i].buf = (unsigned long)skb->data;
#else
		np->rx_ring[i].skb = NULL;
		np->rx_ring[i].buf = (rx + i * np->rx_buf_sz);	//(unsigned long )skb->data;
#endif
		np->rx_ring[i].buf_dma = dma_map_single(&dev->dev, (void *)np->rx_ring[i].buf, np->rx_buf_sz, DMA_FROM_DEVICE);
		np->rx_ring[i].count = (DescChain) | (np->rx_buf_sz & DescSize1Mask);
		np->rx_ring[i].status = (DescOwnByDma);
		np->rx_ring[i].next_dma = &np->rx_ring_dma[i + 1];
		np->rx_ring[i].next = &np->rx_ring[i + 1];

	}

	np->rx_ring[RX_RING_SIZE - 1].next_dma = &np->rx_ring_dma[0];
	np->rx_ring[RX_RING_SIZE - 1].next = &np->rx_ring[0];
	/* Initialize the Tx descriptors */
	for (i = 0; i < TX_RING_SIZE; i++) {
#ifdef DMA_USE_SKB_BUF
		np->tx_ring[i].buf = 0;
#else
		np->tx_ring[i].buf = (tx + i * np->rx_buf_sz);
#endif
		np->tx_ring[i].status = 0;
		np->tx_ring[i].count =
		    (DescChain) | (np->rx_buf_sz & DescSize1Mask);
		np->tx_ring[i].next_dma = &np->tx_ring_dma[i + 1];
		np->tx_ring[i].next = &np->tx_ring[i + 1];
		np->tx_ring[i].skb = NULL;
	}
	np->tx_ring[TX_RING_SIZE - 1].next_dma = &np->tx_ring_dma[0];
	np->tx_ring[TX_RING_SIZE - 1].next = &np->tx_ring[0];
	np->start_tx = &np->tx_ring[0];
	np->last_tx = NULL;
	np->last_rx = &np->rx_ring[RX_RING_SIZE - 1];
	CACHE_WSYNC(np->tx_ring, sizeof(struct _tx_desc)*TX_RING_SIZE);
	CACHE_WSYNC(np->rx_ring, sizeof(struct _rx_desc)*RX_RING_SIZE);


	return 0;
}

EXPORT_SYMBOL(init_rxtx_rings);
/* --------------------------------------------------------------------------*/
/**
 * @brief  alloc_ringdesc 
 *
 * @param  dev
 *
 * @return 
 */
/* --------------------------------------------------------------------------*/
static int alloc_ringdesc(struct net_device *dev)
{
	struct am_net_private *np = netdev_priv(dev);

	np->rx_buf_sz = (dev->mtu <= 1500 ? PKT_BUF_SZ : dev->mtu + 32);
#ifdef USE_COHERENT_MEMORY
	np->rx_ring = dma_alloc_coherent(&dev->dev,
	                                 sizeof(struct _rx_desc) * RX_RING_SIZE,
	                                 (dma_addr_t *)&np->rx_ring_dma, GFP_KERNEL);
#else
	np->rx_ring = kmalloc(sizeof(struct _rx_desc) * RX_RING_SIZE, GFP_KERNEL);
	np->rx_ring_dma = (void*)virt_to_phys(np->rx_ring);
#endif
	if (!np->rx_ring) {
		return -ENOMEM;
	}

	if (!IS_CACHE_ALIGNED(np->rx_ring)) {
		printk("Error the alloc mem is not cache aligned(%p)\n", np->rx_ring);
	}
	printk("NET MDA descpter start addr=%p\n", np->rx_ring);
#ifdef USE_COHERENT_MEMORY
	np->tx_ring = dma_alloc_coherent(&dev->dev,
	                                 sizeof(struct _tx_desc) * TX_RING_SIZE ,
	                                 (dma_addr_t *)&np->tx_ring_dma, GFP_KERNEL);
#else
	np->tx_ring = kmalloc(sizeof(struct _tx_desc) * TX_RING_SIZE, GFP_KERNEL);
	np->tx_ring_dma = (void*)virt_to_phys(np->tx_ring);
#endif
	if (init_rxtx_rings(dev)) {
		printk("init rx tx ring failed!!\n");
		return -1;
	}
	//make sure all the data are write to memory
	return 0;
}

/* --------------------------------------------------------------------------*/
/**
 * @brief  free_ringdesc 
 *
 * @param  dev
 *
 * @return 
 */
/* --------------------------------------------------------------------------*/
static int free_ringdesc(struct net_device *dev)
{
	struct am_net_private *np = netdev_priv(dev);
	int i;
	for (i = 0; i < RX_RING_SIZE; i++) {
		if (np->rx_ring[i].skb) {
			dev_kfree_skb_any(np->rx_ring[i].skb);
			if (np->rx_ring[i].buf_dma != 0) {
				dma_unmap_single(&dev->dev, np->rx_ring[i].buf_dma, np->rx_buf_sz, DMA_FROM_DEVICE);
			}
		}
		np->rx_ring[i].skb = NULL;
	}
	for (i = 0; i < TX_RING_SIZE; i++) {
		if (np->tx_ring[i].skb != NULL) {
			dev_kfree_skb_any(np->tx_ring[i].skb);
			if (np->rx_ring[i].buf_dma != 0) {
				dma_unmap_single(&dev->dev, np->tx_ring[i].buf_dma, np->rx_buf_sz, DMA_TO_DEVICE);
			}
		}
		np->tx_ring[i].skb = NULL;
	}
	if (np->rx_ring) {
#ifdef USE_COHERENT_MEMORY
		dma_free_coherent(&dev->dev,
		                  sizeof(struct _rx_desc) * RX_RING_SIZE ,
		                  np->rx_ring, (dma_addr_t)np->rx_ring_dma);	// for apollo
#else
		kfree(np->rx_ring);
#endif
	}

	np->rx_ring = NULL;
	if (np->tx_ring) {
#ifdef USE_COHERENT_MEMORY
		dma_free_coherent(&dev->dev,
		                  sizeof(struct _tx_desc) * TX_RING_SIZE ,
		                  np->tx_ring, (dma_addr_t)np->tx_ring_dma);	// for apollo
#else
		kfree(np->tx_ring);
#endif
	}
	np->tx_ring = NULL;
	return 0;
}
/* --------------------------------------------------------------------------*/
/**
 * @brief  phy_linked 
 *
 * @param  np
 *
 * @return 
 */
/* --------------------------------------------------------------------------*/
static int phy_linked(struct am_net_private *np)
{
	unsigned int val;
	switch (np->phy_Identifier) {
	case PHY_ATHEROS_8032:
		val = mdio_read(np->dev, np->phys[0], 17);
		val = (val & (1 << 10));
		break;
	case PHY_SMSC_8700:
	case PHY_SMSC_8720:
	default:
		val = mdio_read(np->dev, np->phys[0], 1);
		val = (val & (1 << 2));
	}
	return val;
}
/* --------------------------------------------------------------------------*/
/**
 * @brief  mac_PLL_changed 
 *
 * @param  np
 * @param  clk_mhz
 *
 * @return 
 */
/* --------------------------------------------------------------------------*/
static int mac_PLL_changed(struct am_net_private *np, int clk_mhz)
{
	unsigned long tmp;
	switch (clk_mhz) {
	case 0://disable clock
		PERIPHS_CLEAR_BITS(PREG_ETHERNET_ADDR0, 1);	//disable clk
		PERIPHS_CLEAR_BITS(PREG_ETHERNET_ADDR0, (1 << 0 | 1 << 2 | 1 << 3));
		break;
	case 10:
		if (g_debug > 0) {
			printk("10m\n");
		}
		//(*ETH_MAC_0_Configuration) &= ~(1<<14); // program mac
		tmp =
		    IO_READ32(np->base_addr + ETH_MAC_0_Configuration);
		tmp &= ~(1 << 14);
		IO_WRITE32(tmp, np->base_addr + ETH_MAC_0_Configuration);
		PERIPHS_CLEAR_BITS(PREG_ETHERNET_ADDR0, 1);
		PERIPHS_CLEAR_BITS(PREG_ETHERNET_ADDR0, (1 << 1));
		PERIPHS_SET_BITS(PREG_ETHERNET_ADDR0, 1);
		break;
	case 100:
	default:
		if (g_debug > 0) {
			printk("100m\n");
		}
		//(*ETH_MAC_0_Configuration) |= 1<<14; // program mac
		tmp =
		    IO_READ32(np->base_addr + ETH_MAC_0_Configuration);
		tmp |= 1 << 14;
		IO_WRITE32(tmp, np->base_addr + ETH_MAC_0_Configuration);
		PERIPHS_CLEAR_BITS(PREG_ETHERNET_ADDR0, 1);
		PERIPHS_SET_BITS(PREG_ETHERNET_ADDR0, (1 << 1));
		PERIPHS_SET_BITS(PREG_ETHERNET_ADDR0, 1);
	}
	udelay(10);
	return 0;
}

/* --------------------------------------------------------------------------*/
/**
 * @brief  phy_auto_negotiation_set 
 *
 * @param  np
 */
/* --------------------------------------------------------------------------*/
static void phy_auto_negotiation_set(struct am_net_private *np)
{
	unsigned int rint;
	int s100, full, tmp;
	switch (np->phy_Identifier) {
	case PHY_ATHEROS_8032:
		rint = mdio_read(np->dev, np->phys[0], 0x11);
		s100 = rint & (1 << 14);
		full = ((rint) & (1 << 13));
		break;
	case PHY_SMSC_8700:
	case PHY_SMSC_8720:
	default:
		rint = mdio_read(np->dev, np->phys[0], 31);
		s100 = rint & (1 << 3);
		full = ((rint >> 4) & 1);
		break;
	}
	if (full) {
		if (g_debug > 0) {
			printk("duplex\n");
		}
		//(*ETH_MAC_0_Configuration) |= 1<<11; // program mac
		tmp =
		    IO_READ32(np->base_addr + ETH_MAC_0_Configuration);
		tmp |= 1 << 11;
		IO_WRITE32(tmp, np->base_addr + ETH_MAC_0_Configuration);
	} else {
		if (g_debug > 0) {
			printk("half duplex\n");
		}
		//(*ETH_MAC_0_Configuration) &= ~(1<<11) ; // program mac
		tmp =
		    IO_READ32(np->base_addr + ETH_MAC_0_Configuration);
		tmp &= ~(1 << 11);
		IO_WRITE32(tmp, np->base_addr + ETH_MAC_0_Configuration);
	}
	mac_PLL_changed(np, s100 ? 100 : 10);
	return;
}

/* --------------------------------------------------------------------------*/
/**
 * @brief  netdev_timer 
 *
 * @param  data
 */
/* --------------------------------------------------------------------------*/
static void netdev_timer(unsigned long data)
{
	struct net_device *dev = (struct net_device *)data;
	struct am_net_private *np = netdev_priv(dev);
	unsigned long ioaddr = np->base_addr;
	static int error_num = 0;
	int val;

	if (g_debug > 2)
		printk(KERN_DEBUG "%s: Media selection timer tick, mac status %8.8x \n",
		       dev->name,
		       ioread32(ioaddr + ETH_DMA_5_Status));
	if (!phy_linked(np)) {	//unlink .....
		error_num++;
		if (error_num > 30) {
			error_num = 0;
			spin_lock_irq(&np->lock);
			val = (1 << 14) | (7 << 5) | np->phys[0];
			mdio_write(dev, np->phys[0], 18, val);
			// Auto negotiation restart
			val = mdio_read(dev, np->phys[0], MII_BMCR);

#ifdef PHY_LOOPBACK_TEST
			val = 1 << 14 | 1 << 8 | 1 << 13; //100M,full,seting it as
#else
			val |= BMCR_ANENABLE | BMCR_ANRESTART;
#endif
			mdio_write(dev, np->phys[0], MII_BMCR, val);
			spin_unlock_irq(&np->lock);
		}
		np->timer.expires = jiffies + 1 * HZ;
		netif_stop_queue(dev);
		netif_carrier_off(dev);
		np->phy_set[0] = 0;
	} else {		//linked
		val = mdio_read(dev, np->phys[0], 1);
		if (np->phy_set[0] != val) {
			np->phy_set[0] = val;
			phy_auto_negotiation_set(np);
		}
		error_num = 0;
		netif_carrier_on(dev);
		netif_start_queue(dev);
		np->timer.expires = jiffies + 1 * HZ;
	}
	add_timer(&np->timer);
}

/* --------------------------------------------------------------------------*/
/**
 * @brief  update_status 
 *
 * @param  dev
 * @param  status
 * @param  mask
 *
 * @return 
 */
/* --------------------------------------------------------------------------*/
static inline int update_status(struct net_device *dev, unsigned long status,
                                unsigned long mask)
{
	struct am_net_private *np = netdev_priv(dev);
	int need_reset = 0;
	int need_rx_restart = 0;
	int res = 0;
	if (status & NOR_INTR_EN) {	//Normal Interrupts Process
		if (status & TX_INTR_EN) {	//Transmit Interrupt Process
			IO_WRITE32((1 << 0 | 1 << 16), np->base_addr + ETH_DMA_5_Status);
			res |= 1;
		}
		if (status & RX_INTR_EN) {	//Receive Interrupt Process
			IO_WRITE32((1 << 6 | 1 << 16), np->base_addr + ETH_DMA_5_Status);
			res |= 2;
		}
		if (status & EARLY_RX_INTR_EN) {
			IO_WRITE32((EARLY_RX_INTR_EN | NOR_INTR_EN), np->base_addr + ETH_DMA_5_Status);
		}
		if (status & TX_BUF_UN_EN) {
			IO_WRITE32((1 << 2 | 1 << 16), np->base_addr + ETH_DMA_5_Status);
			res |= 1;
			//this error will cleard in start tx...
			if (g_debug > 1) {
				printk(KERN_WARNING "[" DRV_NAME "]" "Tx bufer unenable\n");
			}
		}
	} else if (status & ANOR_INTR_EN) {	//Abnormal Interrupts Process
		if (status & RX_BUF_UN) {
			IO_WRITE32((RX_BUF_UN | ANOR_INTR_EN), np->base_addr + ETH_DMA_5_Status);
			np->stats.rx_over_errors++;
			need_rx_restart++;
			res |= 2;
			//printk(KERN_WARNING DRV_NAME "Receive Buffer Unavailable\n");
			if (g_debug > 1) {
				printk(KERN_WARNING "[" DRV_NAME "]" "Rx bufer unenable\n");
			}
		}
		if (status & RX_STOP_EN) {
			IO_WRITE32((RX_STOP_EN | ANOR_INTR_EN),
			           np->base_addr + ETH_DMA_5_Status);
			need_rx_restart++;
			res |= 2;
		}
		if (status & RX_WATCH_TIMEOUT) {
			IO_WRITE32((RX_WATCH_TIMEOUT | ANOR_INTR_EN),
			           np->base_addr + ETH_DMA_5_Status);
			need_rx_restart++;
		}
		if (status & FATAL_BUS_ERROR) {
			IO_WRITE32((FATAL_BUS_ERROR | ANOR_INTR_EN),
			           np->base_addr + ETH_DMA_5_Status);
			need_reset++;
			printk(KERN_WARNING "[" DRV_NAME "]" "fatal bus error\n");
		}
		if (status & EARLY_TX_INTR_EN) {
			IO_WRITE32((EARLY_TX_INTR_EN | ANOR_INTR_EN),
			           np->base_addr + ETH_DMA_5_Status);
		}
		if (status & TX_STOP_EN) {
			IO_WRITE32((TX_STOP_EN | ANOR_INTR_EN),
			           np->base_addr + ETH_DMA_5_Status);
			res |= 1;
		}
		if (status & TX_JABBER_TIMEOUT) {
			IO_WRITE32((TX_JABBER_TIMEOUT | ANOR_INTR_EN),
			           np->base_addr + ETH_DMA_5_Status);
			printk(KERN_WARNING "[" DRV_NAME "]" "tx jabber timeout\n");
			np->first_tx = 1;
		}
		if (status & RX_FIFO_OVER) {
			IO_WRITE32((RX_FIFO_OVER | ANOR_INTR_EN),
			           np->base_addr + ETH_DMA_5_Status);
			np->stats.rx_fifo_errors++;
			need_rx_restart++;
			res |= 2;
			printk(KERN_WARNING "[" DRV_NAME "]" "Rx fifo over\n");
		}
		if (status & TX_UNDERFLOW) {
			IO_WRITE32((TX_UNDERFLOW | ANOR_INTR_EN),
			           np->base_addr + ETH_DMA_5_Status);
			printk(KERN_WARNING "[" DRV_NAME "]" "Tx underflow\n");
			np->first_tx = 1;
			res |= 1;
		}
	}

	if (need_reset) {
		printk(KERN_WARNING DRV_NAME "system reset\n");
		free_ringdesc(dev);
		ethernet_reset(dev);
	} else if (need_rx_restart) {
		IO_WRITE32(1, np->base_addr + ETH_DMA_2_Re_Poll_Demand);
	}
	return res;
}

/* --------------------------------------------------------------------------*/
/**
 * @brief  print_rx_error_log 
 *
 * @param  status
 */
/* --------------------------------------------------------------------------*/
static void inline print_rx_error_log(unsigned long status)
{

	if (status & DescRxTruncated) {
		printk(KERN_WARNING "Descriptor Error desc-mask[%d]\n",
		       DescRxTruncated);
	}
	if (status & DescSAFilterFail) {
		printk(KERN_WARNING
		       "Source Address Filter Fail rx desc-mask[%d]\n",
		       DescSAFilterFail);
	}
	if (status & DescRxLengthError) {
		printk(KERN_WARNING "Length Error rx desc-mask[%d]\n",
		       DescRxLengthError);
	}
	if (status & DescRxIPChecksumErr) {
		printk(KERN_WARNING "TCP checksum Error rx desc-mask[%d]\n",
		       DescRxLengthError);
	}
	if (status & DescRxTCPChecksumErr) {
		printk(KERN_WARNING "TCP checksum Error rx desc-mask[%d]\n",
		       DescRxLengthError);
	}
	if (status & DescRxDamaged) {
		printk(KERN_WARNING "Overflow Error rx desc-mask[%d]\n",
		       DescRxDamaged);
	}
	if (status & DescRxMiiError) {
		printk(KERN_WARNING "Receive Error rx desc-mask[%d]\n",
		       DescRxMiiError);
	}
	if (status & DescRxDribbling) {
		printk(KERN_WARNING "Dribble Bit Error rx desc-mask[%d]\n",
		       DescRxDribbling);
	}
	if (status & DescRxCrc) {
		printk(KERN_WARNING "CE: CRC Errorrx desc-mask[%d]\n",
		       DescRxCrc);
	}
}

/**********************
we do the basic rx  tx operation irq;
FIXME:on SMP system..

************************/
/* --------------------------------------------------------------------------*/
/**
 * @brief  net_tasklet 
 *
 * @param  dev_instance
 */
/* --------------------------------------------------------------------------*/
void net_tasklet(unsigned long dev_instance)
{
	struct net_device *dev = (struct net_device *)dev_instance;
	struct am_net_private *np = netdev_priv(dev);
	int len;
	int result;
	unsigned long flags;

#ifndef DMA_USE_SKB_BUF
	struct sk_buff *skb = NULL;
#endif
	spin_lock_irqsave(&np->lock, flags);
	result = np->int_rx_tx;
	np->int_rx_tx = 0;
	spin_unlock_irqrestore(&np->lock, flags);
	if (!running) {
		goto release;
	}
	if (result & 1) {
		struct _tx_desc *c_tx, *tx = NULL;

		c_tx = (void *)IO_READ32(np->base_addr + ETH_DMA_18_Curr_Host_Tr_Descriptor);
		c_tx = np->tx_ring + (c_tx - np->tx_ring_dma);
		tx = np->start_tx;
		CACHE_RSYNC(tx, sizeof(struct _tx_desc));
		while (tx != NULL && tx != c_tx && !(tx->status & DescOwnByDma)) {
#ifdef DMA_USE_SKB_BUF
			spin_lock_irqsave(&np->lock, flags);
			if (tx->skb != NULL) {
				//clear to next send;
				if (np->tx_full) {
					netif_wake_queue(dev);
					np->tx_full = 0;
				}
				if (g_debug > 2) {
					printk("send data ok len=%d\n", tx->skb->len);
				}
				tx_data_dump((unsigned char *)tx->buf, tx->skb->len);
				dev_kfree_skb_any(tx->skb);
				if (tx->buf_dma != 0) {
					dma_unmap_single(&dev->dev, tx->buf_dma, np->rx_buf_sz, DMA_TO_DEVICE);
				}
				tx->skb = NULL;
				tx->buf = 0;
				tx->buf_dma = 0;
				tx->status = 0;
			} else {
				spin_unlock_irqrestore(&np->lock, flags);
				break;
			}
			spin_unlock_irqrestore(&np->lock, flags);
#else
			tx->status = 0;
			CACHE_WSYNC(tx, sizeof(struct _tx_desc));
			if (np->tx_full) {
				netif_wake_queue(dev);
				np->tx_full = 0;
			}
#endif
			tx = tx->next;
			CACHE_RSYNC(tx, sizeof(struct _tx_desc));

		}
		np->start_tx = tx;
		//data tx end... todo
	}
	if (result & 2) {
		//data  rx;
		struct _rx_desc *c_rx, *rx = NULL;
		c_rx = (void *)IO_READ32(np->base_addr + ETH_DMA_19_Curr_Host_Re_Descriptor);
		c_rx = np->rx_ring + (c_rx - np->rx_ring_dma);
		rx = np->last_rx->next;
		while (rx != NULL) {
			//if(rx->status !=IO_READ32(&rx->status))
			//      printk("error of D-chche!\n");
			CACHE_RSYNC(rx, sizeof(struct _rx_desc));
			if (!(rx->status & (DescOwnByDma))) {
				int ip_summed = CHECKSUM_UNNECESSARY;
				len = (rx->status & DescFrameLengthMask) >> DescFrameLengthShift;
				if (unlikely(len < 18 || len > np->rx_buf_sz)) {	//here is fatal error we drop it ;
					np->stats.rx_dropped++;
					np->stats.rx_errors++;
					goto to_next;
				}
				if (unlikely(rx->status & (DescError))) {	//here is not often occur
					print_rx_error_log(rx->status);
					//rx->status=DescOwnByDma;
					if ((rx->status & DescRxIPChecksumErr) || (rx->status & DescRxTCPChecksumErr)) {	//maybe checksum engine's problem;
						//we set the NONE for ip/tcp need check it again
						ip_summed = CHECKSUM_NONE;
					} else {
						np->stats.rx_dropped++;
						np->stats.rx_errors++;
						goto to_next;
					}
				}
				len = len - 4;	//clear the crc
#ifdef DMA_USE_SKB_BUF
				if (rx->skb == NULL) {
					printk("NET skb pointer error!!!\n");
					break;
				}
				if (rx->skb->len > 0) {
					printk("skb have data before,skb=%p,len=%d\n", rx->skb, rx->skb->len);
					rx->skb = NULL;
					goto to_next;
				}
				skb_put(rx->skb, len);
				rx->skb->dev = dev;
				rx->skb->protocol =
				    eth_type_trans(rx->skb, dev);
				/*we have checked in hardware;
				   we not need check again */
				rx->skb->ip_summed = ip_summed;
				if (rx->buf_dma != 0) {
					dma_unmap_single(&dev->dev, rx->buf_dma, np->rx_buf_sz, DMA_FROM_DEVICE);
				}
				rx->buf_dma = 0;
				netif_rx(rx->skb);
				if (g_debug > 3) {
					printk("receive skb=%p\n", rx->skb);
				}
				rx->skb = NULL;
#else
				skb = dev_alloc_skb(len + 4);
				if (skb == NULL) {
					np->stats.rx_dropped++;
					printk("error to alloc skb\n");
					break;
				}
				skb_reserve(skb, 2);
				skb_put(skb, len);
				if (rx->buf_dma != NULL) {
					dma_unmap_single(&dev->dev, (void *)rx->buf_dma, np->rx_buf_sz, DMA_FROM_DEVICE);
				}
				memcpy(skb->data, (void *)rx->buf, len);
				skb->dev = dev;
				skb->protocol = eth_type_trans(skb, dev);
				skb->ip_summed = ip_summed;
				netif_rx(skb);
#endif
				dev->last_rx = jiffies;
				np->stats.rx_packets++;
				np->stats.rx_bytes += len;

				if (g_debug > 3) {
					printk("receive data len=%d\n", len);
				}
				rx_data_dump((unsigned char *)rx->buf,len);
to_next:
#ifdef DMA_USE_SKB_BUF
				if (rx->skb) {
					dev_kfree_skb_any(rx->skb);
				}
				rx->skb = dev_alloc_skb(np->rx_buf_sz + 4);
				if (rx->skb == NULL) {
					printk(KERN_ERR "error to alloc the skb\n");
					rx->buf = 0;
					rx->buf_dma = 0;
					rx->status = 0;
					rx->count = 0;
					np->last_rx = rx;
					CACHE_WSYNC(rx, sizeof(struct _rx_desc));
					break;
				}
				if (g_debug > 3) {
					printk("new malloc skb=%p\n", rx->skb);
				}
				skb_reserve(rx->skb, 2);
				rx->buf = (unsigned long)rx->skb->data;
#endif
				rx->buf_dma = dma_map_single(&dev->dev, (void *)rx->buf, (unsigned long)np->rx_buf_sz, DMA_FROM_DEVICE);	//invalidate for next  dma in;
				rx->count = (DescChain) | (np->rx_buf_sz & DescSize1Mask);
				rx->status = DescOwnByDma;
				CACHE_WSYNC(rx, sizeof(struct _rx_desc));
				np->last_rx = rx;
				rx = rx->next;
			} else {
				break;
			}

		}
	}
release:
	IO_WRITE32(np->irq_mask, (np->base_addr + ETH_DMA_7_Interrupt_Enable));
}

/* --------------------------------------------------------------------------*/
/**
 * @brief  intr_handler 
 *
 * @param  irq
 * @param  dev_instance
 *
 * @return 
 */
/* --------------------------------------------------------------------------*/
static irqreturn_t intr_handler(int irq, void *dev_instance)
{
	struct net_device *dev = (struct net_device *)dev_instance;
	struct am_net_private *np = netdev_priv(dev);
	unsigned long status, mask = 0;
	IO_WRITE32(0, (np->base_addr + ETH_DMA_7_Interrupt_Enable));//disable irq
	tasklet_schedule(&np->rx_tasklet);
	status = IO_READ32(np->base_addr + ETH_DMA_5_Status);
	mask = IO_READ32(np->base_addr + ETH_MAC_Interrupt_Mask);
	np->int_rx_tx |= update_status(dev, status, mask);
	return IRQ_HANDLED;
}

/* --------------------------------------------------------------------------*/
/**
 * @brief  phy_reset 
 *
 * @param  ndev
 *
 * @return 
 */
/* --------------------------------------------------------------------------*/
static int phy_reset(struct net_device *ndev)
{
	struct am_net_private *np = netdev_priv(ndev);
	unsigned long val;
	int k;

	//mac reset ...
	IO_WRITE32(1, np->base_addr + ETH_DMA_0_Bus_Mode);
	//waiting mac reset...
	for (k = 0;
	     (IO_READ32(np->base_addr + ETH_DMA_0_Bus_Mode) & 1) && k < 1000;
	     k++) {
		udelay(1);
	}
	if (k >= 1000) {
		printk("error to reset mac!\n");
		goto error_reset;
	}
	//set for RMII mode;
	val = (1 << 14) | (7 << 5) | np->phys[0];
	mdio_write(ndev, np->phys[0], 18, val);
	val = BMCR_RESET;
	mdio_write(ndev, np->phys[0], MII_BMCR, val);
	//waiting to phy reset ok....
	for (k = 0; (mdio_read(ndev, np->phys[0], MII_BMCR)) & (BMCR_RESET)
	     && k < 1000; k++) {
		udelay(1);
	}
	if (k >= 1000) {
		printk("error to reset phy!\n");
		goto error_reset;
	}
	// mode = 111; turn on auto-neg mode (previously was power-saving)
	val = (1 << 14) | (7 << 5) | np->phys[0];
	mdio_write(ndev, np->phys[0], 18, val);
	// Auto negotiation restart
	val = BMCR_ANENABLE | BMCR_ANRESTART;
	mdio_write(ndev, np->phys[0], MII_BMCR, val);
	if (g_debug > 1) {
		printk("starting to auto negotiation!\n");
	}

	//(*ETH_DMA_0_Bus_Mode) = 0x00100800;
	IO_WRITE32(0x00100800, np->base_addr + ETH_DMA_0_Bus_Mode);

	/*
	   val=*((unsigned short *)&ndev->dev_addr[4]);
	   IO_WRITE32(val,np->base_addr+ETH_MAC_Addr0_High);
	   val=*((unsigned long *)ndev->dev_addr);
	   IO_WRITE32(val,np->base_addr+ETH_MAC_Addr0_Low);
	 */
	write_mac_addr(ndev, ndev->dev_addr);

	val = 0xc80c |		//8<<8 | 8<<17; //tx and rx all 8bit mode;
	      1 << 10;		//checksum offload enabled
#ifdef MAC_LOOPBACK_TEST
	val |= 1 << 12; //mac loop back
#endif
	IO_WRITE32(val, np->base_addr + ETH_MAC_0_Configuration);

	val = 1 << 4;/*receive all muticast*/
	//| 1 << 31;	//receive all the data
	IO_WRITE32(val, np->base_addr + ETH_MAC_1_Frame_Filter);
#ifdef PHY_LOOPBACK_TEST
	/*phy loop back*/
	val = mdio_read(ndev, np->phys[0], MII_BMCR);
	//val=1<<14 | 1<<8 | 1<<13;//100M,full,seting it as ;
	val = 1 << 14 | 1 << 8; /////10M,full,seting it as ;
	mdio_write(ndev, np->phys[0], MII_BMCR, val);

#endif

	IO_WRITE32((unsigned long)&np->rx_ring_dma[0], (np->base_addr + ETH_DMA_3_Re_Descriptor_List_Addr));
	IO_WRITE32((unsigned long)&np->tx_ring_dma[0], (np->base_addr + ETH_DMA_4_Tr_Descriptor_List_Addr));
	IO_WRITE32(np->irq_mask, (np->base_addr + ETH_DMA_7_Interrupt_Enable));
	IO_WRITE32((0), (np->base_addr + ETH_MAC_Interrupt_Mask));
	printk("Current DMA mode=%x\n", IO_READ32(np->base_addr + ETH_DMA_6_Operation_Mode));
	val = (7 << 14 | 1 << 25 | 1 << 8 | 1 << 26 | 1 << 21);/*don't start receive here */
	////1<<21 is Transmit Store and Forward used for tcp/ip checksum insert
	IO_WRITE32(val, (np->base_addr + ETH_DMA_6_Operation_Mode));
	np->phy_set[0] = 0;	//make sure reset the phy speed
	return 0;
error_reset:

	return -1;
}

/* --------------------------------------------------------------------------*/
/**
 * @brief  ethernet_reset 
 *
 * @param  dev
 *
 * @return 
 */
/* --------------------------------------------------------------------------*/
static int ethernet_reset(struct net_device *dev)
{
	struct am_net_private *np = netdev_priv(dev);
	int res;
	unsigned long flags;

	printk(KERN_INFO "Ethernet reset\n");

	spin_lock_irqsave(&np->lock, flags);
	res = alloc_ringdesc(dev);
	spin_unlock_irqrestore(&np->lock, flags);
	if (res != 0) {
		printk(KERN_INFO "can't alloc ring desc!err=%d\n", res);
		goto out_err;
	}
	res = phy_reset(dev);
	if (res != 0) {
		printk(KERN_INFO "can't reset ethernet phy!err=%d\n", res);
		goto out_err;
	}
	np->first_tx = 1;

out_err:
	return res;
}

/* --------------------------------------------------------------------------*/
/**
 * @brief  netdev_open 
 *
 * @param  dev
 *
 * @return 
 */
/* --------------------------------------------------------------------------*/
static int netdev_open(struct net_device *dev)
{
	struct am_net_private *np = netdev_priv(dev);
	unsigned long val;

	int res;
	if (running) {
		return 0;
	}
	printk(KERN_INFO "netdev_open\n");
	res = ethernet_reset(dev);

	if (res != 0) {
		printk(KERN_INFO "ethernet_reset err=%d\n", res);
		goto out_err;
	}
	//netif_device_detach(dev);
	res = request_irq(dev->irq, &intr_handler, IRQF_SHARED, dev->name, dev);
	if (res) {
		printk(KERN_ERR "%s: request_irq error %d.,err=%d\n",
		       dev->name, dev->irq, res);
		goto out_err;
	}

	if (g_debug > 0)
		printk(KERN_DEBUG "%s: opened (irq %d).\n",
		       dev->name, dev->irq);
	//enable_irq(dev->irq);
	/* Set the timer to check for link beat. */
	init_timer(&np->timer);
	np->timer.expires = jiffies + 1;
	np->timer.data = (unsigned long)dev;
	np->timer.function = &netdev_timer;	/* timer handler */
	add_timer(&np->timer);
	val = IO_READ32((np->base_addr + ETH_DMA_6_Operation_Mode));
	val |= (1 << 1); /*start receive*/
	IO_WRITE32(val, (np->base_addr + ETH_DMA_6_Operation_Mode));
	running = 1;
#ifdef LOOP_BACK_TEST
	start_test(np->dev);
#endif
	return 0;
out_err:
	running = 0;
	return -EIO;
}

/* --------------------------------------------------------------------------*/
/**
 * @brief  netdev_close 
 *
 * @param  dev
 *
 * @return 
 */
/* --------------------------------------------------------------------------*/
static int netdev_close(struct net_device *dev)
{
	struct am_net_private *np = netdev_priv(dev);
	unsigned long val;

	if (!running) {
		return 0;
	}
	running = 0;

	IO_WRITE32(0, (np->base_addr + ETH_DMA_6_Operation_Mode));
	IO_WRITE32(0, np->base_addr + ETH_DMA_7_Interrupt_Enable);
	val = IO_READ32((np->base_addr + ETH_DMA_5_Status));
	while ((val & (7 << 17)) || (val & (7 << 20))) { /*DMA not finished?*/
		printk(KERN_ERR "ERROR! MDA is not stoped,val=%lx!\n", val);
		msleep(1);//waiting all dma is finished!!
		val = IO_READ32((np->base_addr + ETH_DMA_5_Status));
	}
	if (g_debug > 0) {
		printk(KERN_INFO "NET MDA is  stoped,val=%lx!\n", val);
	}
	disable_irq(dev->irq);
	netif_carrier_off(dev);
	netif_stop_queue(dev);
	free_ringdesc(dev);
	free_irq(dev->irq, dev);
	del_timer_sync(&np->timer);
	//      free_rxtx_rings(np);
	//      free_ringdesc(np);
	//      PERIPHS_CLEAR_BITS(ETH_PLL_CNTL,1);//disable clk
	if (g_debug > 0) {
		printk(KERN_DEBUG "%s: closed\n", dev->name);
	}

	return 0;
}

/* --------------------------------------------------------------------------*/
/**
 * @brief  start_tx 
 *
 * @param  skb
 * @param  dev
 *
 * @return 
 */
/* --------------------------------------------------------------------------*/
static int start_tx(struct sk_buff *skb, struct net_device *dev)
{
	struct am_net_private *np = netdev_priv(dev);
	int tmp;
	struct _tx_desc *tx;
	unsigned long flags;
	dev->trans_start = jiffies;

	if (!running) {
		return -1;
	}
	if (g_debug > 2) {
		printk(KERN_DEBUG "%s: Transmit frame queued\n", dev->name);
	}
	spin_lock_irqsave(&np->lock, flags);

	if (np->last_tx != NULL) {
		tx = np->last_tx->next;
	} else {
		tx = &np->tx_ring[0];
	}
	CACHE_RSYNC(tx, sizeof(*tx));
	if (tx->status & DescOwnByDma) {
		spin_unlock_irqrestore(&np->lock, flags);
		if (g_debug > 2) {
			printk("tx queue is full \n");
		}
		goto err;
	}
#ifdef DMA_USE_SKB_BUF
	if (tx->skb != NULL) {
		dev_kfree_skb_any(tx->skb);
		if (tx->buf_dma != 0) {
			dma_unmap_single(&dev->dev, tx->buf_dma, np->rx_buf_sz, DMA_FROM_DEVICE);
		}
	}
	tx->skb = skb;
	tx->buf = (unsigned long)skb->data;
#else
	memcpy((void *)tx->buf, skb->data, skb->len);
#endif
	tx->buf_dma = dma_map_single(&dev->dev, (void *)tx->buf, (unsigned long)(skb->len), DMA_TO_DEVICE);
	tx->count = ((skb->len << DescSize1Shift) & DescSize1Mask) | DescTxFirst | DescTxLast | DescTxIntEnable | DescChain;	//|2<<27; (1<<25, ring end)
	if (skb->ip_summed == CHECKSUM_PARTIAL) {
		tx->count |= 0x3 << 27;	//add hw check sum;
	}
	tx->status = DescOwnByDma;
	np->last_tx = tx;
	np->stats.tx_packets++;
	np->stats.tx_bytes += skb->len;
	CACHE_WSYNC(tx, sizeof(*tx));
#ifndef DMA_USE_SKB_BUF
	dev_kfree_skb_any(skb);
#endif
	if (np->first_tx) {
		np->first_tx = 0;
		tmp = IO_READ32(np->base_addr + ETH_DMA_6_Operation_Mode);
		tmp |= (7 << 14) | (1 << 13);
		IO_WRITE32(tmp, np->base_addr + ETH_DMA_6_Operation_Mode);
	} else {
		//ETH_DMA_1_Tr_Poll_Demand
		IO_WRITE32(1, np->base_addr + ETH_DMA_1_Tr_Poll_Demand);
	}
	spin_unlock_irqrestore(&np->lock, flags);
	return 0;
err:
	np->tx_full = 1;
	np->stats.tx_dropped++;
	netif_stop_queue(dev);
	return -1;
}

#ifdef LOOP_BACK_TEST
/* --------------------------------------------------------------------------*/
/**
 * @brief  test_loop_back 
 *
 * @param  dev
 */
/* --------------------------------------------------------------------------*/
void test_loop_back(struct net_device *dev)
{
	//static int start_tx(struct sk_buff *skb, struct net_device *dev)
	//struct am_net_private *np = netdev_priv(dev);
	int i = 0;
	char header[64] = "";
	struct am_net_private *np = netdev_priv(dev);

	printk("start testing!!\n");
	memcpy(header, dev->dev_addr, 6);
	memcpy(header + 8, dev->dev_addr, 6);
	header[12] = 0x80;
	header[13] = 0;
	while (1) {
		struct sk_buff *skb = dev_alloc_skb(1600);
		while (!running) {
			i = 0;
			msleep(10);
		}

		skb_put(skb, 1400);
		memset(skb->data, 0x55, skb->len);
		memcpy(skb->data, header, 16);
		if (start_tx(skb, dev) != 0) {
			/*tx list is full*/
			msleep(1);
			dev_kfree_skb(skb);
		} else {
			i++;
		}
		if (i % 2000 == 0) {
			msleep(1);
			printk("send pkts=%ld,receive pkts=%ld\n", np->stats.tx_packets, np->stats.rx_packets);
		}

	}
}

/* --------------------------------------------------------------------------*/
/**
 * @brief  start_test 
 *
 * @param  dev
 */
/* --------------------------------------------------------------------------*/
void start_test(struct net_device *dev)
{
	static int test_running = 0;
	struct am_net_private *np = netdev_priv(dev);
	if (test_running) {
		return ;
	}
	phy_auto_negotiation_set(np);
	kernel_thread((void *)test_loop_back, (void *)dev, CLONE_FS | CLONE_SIGHAND);
	test_running++;

}
#endif
static struct net_device_stats *get_stats(struct net_device *dev) {
	struct am_net_private *np = netdev_priv(dev);

	return &np->stats;
}

/* --------------------------------------------------------------------------*/
/**
 * @brief  tx_timeout 
 *
 * @param  dev
 */
/* --------------------------------------------------------------------------*/
static void tx_timeout(struct net_device *dev)
{
	struct am_net_private *np = netdev_priv(dev);
	int val;
	//FIXME
	spin_lock_irq(&np->lock);
	val = mdio_read(dev, np->phys[0], MII_BMSR);
	spin_unlock_irq(&np->lock);
	if (!(val & (BMSR_LSTATUS))) {	//unlink .....
		netif_stop_queue(dev);
		netif_carrier_off(dev);
	} else {
		netif_carrier_on(dev);
		netif_wake_queue(dev);
		dev->trans_start = jiffies;
		np->stats.tx_errors++;
	}
	return;
}

/* --------------------------------------------------------------------------*/
/**
 * @brief  write_mac_addr 
 *
 * @param  dev
 * @param  macaddr
 */
/* --------------------------------------------------------------------------*/
static void write_mac_addr(struct net_device *dev, char *macaddr)
{
	struct am_net_private *np = netdev_priv(dev);
	unsigned int val;
	val = *((unsigned short *)&macaddr[4]);
	IO_WRITE32(val, np->base_addr + ETH_MAC_Addr0_High);
	val = *((unsigned long *)macaddr);
	IO_WRITE32(val, np->base_addr + ETH_MAC_Addr0_Low);
	printk("write mac add to:");
	data_dump(macaddr, 6);
}

/* --------------------------------------------------------------------------*/
/**
 * @brief  chartonum 
 *
 * @param  c
 *
 * @return 
 */
/* --------------------------------------------------------------------------*/
static unsigned char inline chartonum(char c)
{
	if (c >= '0' && c <= '9') {
		return c - '0';
	}
	if (c >= 'A' && c <= 'F') {
		return (c - 'A') + 10;
	}
	if (c >= 'a' && c <= 'f') {
		return (c - 'a') + 10;
	}
	return 0;

}

/* --------------------------------------------------------------------------*/
/**
 * @brief  config_mac_addr 
 *
 * @param  dev
 * @param  mac
 */
/* --------------------------------------------------------------------------*/
static void config_mac_addr(struct net_device *dev, void *mac)
{
	if(g_mac_setup == 0) {
		printk("*****WARNING: Haven't setup MAC address! Using random MAC address.\n");
		unsigned long mac_fir = 0;
		unsigned char mac_add[6] = {};

		mac_fir = READ_MPEG_REG(RAND64_ADDR1);

		mac_add[1] = mac_fir&0xFF; mac_add[2] = (mac_fir>>16)&0xFF;
		mac_add[3] = (mac_fir>>8)&0xFF; mac_add[4] = (mac_fir>>24) &0xFF;
		mac_add[5] = (mac_add[1]<<4)|(mac_add[4]>>4);
		memcpy(mac, mac_add, 6);
		printk("mac-addr: %x:%x:%x:%x:%x:%x\n", mac_add[0],mac_add[1],mac_add[2],
							mac_add[3],mac_add[4],mac_add[5]);
	}

	memcpy(dev->dev_addr, mac, 6);
	write_mac_addr(dev, dev->dev_addr);
}

/* --------------------------------------------------------------------------*/
/**
 * @brief  mac_addr_set 
 *
 * @param  line
 *
 * @return 
 */
/* --------------------------------------------------------------------------*/
static int __init mac_addr_set(char *line)
{
	unsigned char mac[6];
	int i = 0;
	for (i = 0; i < 6 && line[0] != '\0' && line[1] != '\0'; i++) {
		mac[i] = chartonum(line[0]) << 4 | chartonum(line[1]);
		line += 3;
	}
	memcpy(DEFMAC, mac, 6);
	printk("******** uboot setup mac-addr: %x:%x:%x:%x:%x:%x\n",
			DEFMAC[0], DEFMAC[1], DEFMAC[2], DEFMAC[3], DEFMAC[4], DEFMAC[5]);
	g_mac_setup++;

	return 1;
}

__setup("mac=", mac_addr_set);


/* --------------------------------------------------------------------------*/
/**
 * @brief  phy_mc_hash 
 *
 * @param  addr
 *
 * @return 
 */
/* --------------------------------------------------------------------------*/
static inline int phy_mc_hash(__u8 *addr)
{
	return (bitrev32(~crc32_le(~0, addr, ETH_ALEN)) >> 26);
}

/* --------------------------------------------------------------------------*/
/**
 * @brief  set_multicast_list 
 *
 * @param  dev
 */
/* --------------------------------------------------------------------------*/
static void set_multicast_list(struct net_device *dev)
{
	struct am_net_private *np = netdev_priv(dev);
	u32  tmp;
	if ((dev->flags & IFF_PROMISC)) {
		tmp = IO_READ32(np->base_addr + ETH_MAC_1_Frame_Filter);
		tmp |= 1;
		IO_WRITE32(tmp, np->base_addr + ETH_MAC_1_Frame_Filter);//promisc module
		printk("ether enter promisc module\n");
	} else {
		tmp = IO_READ32(np->base_addr + ETH_MAC_1_Frame_Filter);
		tmp &= ~1;
		IO_WRITE32(tmp, np->base_addr + ETH_MAC_1_Frame_Filter);//live promisc
		//printk("ether leave promisc module\n");
	}
	if ((dev->flags & IFF_ALLMULTI)) {
		tmp = IO_READ32(np->base_addr + ETH_MAC_1_Frame_Filter);
		tmp |= (1 << 4);
		IO_WRITE32(tmp, np->base_addr + ETH_MAC_1_Frame_Filter);//all muticast
		printk("ether enter all multicast module\n");
	} else {
		tmp = IO_READ32(np->base_addr + ETH_MAC_1_Frame_Filter);
		tmp &= (1 << 4);
		IO_WRITE32(tmp, np->base_addr + ETH_MAC_1_Frame_Filter);//live all muticast
		//printk("ether leave all muticast module\n");
	}

	if (dev->mc_count > 0) {
		int cnt = dev->mc_count;
		u32 hash[2];
		struct dev_mc_list	*addr_list;
		u32 hash_id;
		char * addr;
		hash[0] = 0;
		hash[1] = 0;
		printk("changed the Multicast,mcount=%d\n", dev->mc_count);
		for (addr_list = dev->mc_list; cnt && addr_list != NULL; addr_list = addr_list->next, cnt--) {
			addr = addr_list->dmi_addr;
			hash_id = phy_mc_hash(addr);
			///*
			printk("add mac address:%02x:%02x:%02x:%02x:%02x:%02x,bit=%d\n",
			       addr[0], addr[1], addr[2], addr[3], addr[4], addr[5],
			       hash_id);
			//*/
			//set_bit(hash_id,hash);
			if (hash_id > 31) {
				hash[1] |= 1 << (hash_id - 32);
			} else {
				hash[0] |= 1 << hash_id;
			}
		}
		printk("set hash low=%x,high=%x\n", hash[0], hash[1]);
		IO_WRITE32(hash[1], np->base_addr + ETH_MAC_2_Hash_Table_High);
		IO_WRITE32(hash[0], np->base_addr + ETH_MAC_3_Hash_Table_Low);
		tmp = IO_READ32(np->base_addr + ETH_MAC_1_Frame_Filter);
		tmp = (1 << 2) | 	//hash filter
		      0;
		printk("changed the filter setting to :%x\n", tmp);
		IO_WRITE32(tmp, np->base_addr + ETH_MAC_1_Frame_Filter);//hash muticast
	}
}

static const struct net_device_ops am_netdev_ops = {
	.ndo_open			= netdev_open,
	.ndo_stop			= netdev_close,
	.ndo_start_xmit 		= start_tx,
	.ndo_tx_timeout		= tx_timeout,
	.ndo_set_multicast_list 	= set_multicast_list,
	.ndo_do_ioctl			= netdev_ioctl,
	.ndo_get_stats		= get_stats,
	.ndo_change_mtu		= eth_change_mtu,
	.ndo_set_mac_address 	= eth_mac_addr,
	.ndo_validate_addr		= eth_validate_addr,
};

/* --------------------------------------------------------------------------*/
/**
 * @brief  setup_net_device 
 *
 * @param  dev
 *
 * @return 
 */
/* --------------------------------------------------------------------------*/
static int setup_net_device(struct net_device *dev)
{
	struct am_net_private *np = netdev_priv(dev);
	int res = 0;
	dev->dev.coherent_dma_mask = DMA_BIT_MASK(32);
	dev->features = NETIF_F_GEN_CSUM;
	dev->netdev_ops = &am_netdev_ops;
	dev->ethtool_ops = NULL;	// &netdev_ethtool_ops;
	dev->watchdog_timeo = TX_TIMEOUT;
	np->irq_mask = (1 << 16) |      //NIE: Normal Interrupt Summary Enable
	               (1 << 15) |          //abnormal int summary
	               (1 << 6) |          //Receive Interrupt Enable
	               (1 << 2) |          //Transmit Buffer Unavailable Enable
	               (1 << 3) |          //TJT: Transmit Jabber Timeout
	               (1 << 4) |          //OVF: Receive Overflow
	               (1 << 5) |          //UNF: Transmit Underflow
	               (1 << 7) |          //7 RU: Receive Buffer Unavailable
	               (1 << 8) |          //RPS: Receive Process Stopped
	               (1 << 13) |          //13 FBI: Fatal Bus Error Interrupt
	               (1) | 		//tx interrupt
	               0;
	config_mac_addr(dev, DEFMAC);
	dev_alloc_name(dev, "eth%d");
	memset(&np->stats, 0, sizeof(np->stats));
	return res;
}

/* --------------------------------------------------------------------------*/
/**
 * @brief  probe_init 
 *
 * @param  ndev
 *
 * @return 
 */
/* --------------------------------------------------------------------------*/
static int probe_init(struct net_device *ndev)
{
	int phy = 0;
	int phy_idx = 0;
	int found = 0;
	int res;
	unsigned int val;
	int k, kk;

	struct am_net_private *priv = netdev_priv(ndev);
	priv->dev = ndev;
	ndev->base_addr = (unsigned long)(ETHBASE);
	ndev->irq = ETH_INTERRUPT;
	spin_lock_init(&priv->lock);
	priv->mii_if.dev = ndev;
	priv->mii_if.mdio_read = mdio_read;
	priv->mii_if.mdio_write = mdio_write;
	priv->base_addr = ndev->base_addr;
	if (g_debug > 0) {
		printk("addr is %x\n", (unsigned int)ndev->base_addr);
	}

	for (k = 0; k < 100 && !found; k++) {

		//mac reset ...
		IO_WRITE32(1, priv->base_addr + ETH_DMA_0_Bus_Mode);
		//waiting mac reset...
		for (kk = 0;
		     (IO_READ32(priv->base_addr + ETH_DMA_0_Bus_Mode) & 1) && kk < 1000;
		     kk++) {
			udelay(1);
		}
		if (kk >= 1000) {
			printk("error to reset mac!\n");
			goto error0;
		}

		for (phy = 0; phy < 32 && phy_idx < MII_CNT; phy++) {
			int mii_status = mdio_read(ndev, phy, MII_BMSR);
			if (mii_status != 0xffff && mii_status != 0x0000) {
				priv->phys[phy_idx++] = phy;
				priv->mii_if.advertising =
				    mdio_read(ndev, phy, MII_ADVERTISE);
				priv->mii =
				    (mdio_read(ndev, phy, MII_PHYSID1) << 16) +
				    mdio_read(ndev, phy, MII_PHYSID2);
				if (g_debug > 0)
					printk(KERN_INFO
					       "%s: MII PHY %8.8xh found at address %d, status "
					       "0x%4.4x advertising %4.4x.\n", DRV_NAME,
					       priv->mii, phy, mii_status,
					       priv->mii_if.advertising);
				found++;
			}
		}
	}

	if (!found) {
		printk("can't find any mii phy device !\n");
		res = -EIO;
		goto error0;
	}
	mdio_write(ndev, priv->phys[0], 18, priv->phys[0] | (1 << 14 | 7 << 5));

	val = mdio_read(ndev, priv->phys[0], 2); //phy_rw(0, phyad, 2, &val);
	priv->phy_Identifier = val << 16;
	val = mdio_read(ndev, priv->phys[0], 3); //phy_rw(0, phyad, 3, &val);
	priv->phy_Identifier |= val;
	printk("find phy phy_Identifier=%x\n", priv->phy_Identifier);
	res = setup_net_device(ndev);
	if (res != 0) {
		printk("setup net device error !\n");
		res = -EIO;
		goto error0;
	}

	res = register_netdev(ndev);
	if (res != 0) {
		printk("can't register net  device !\n");
		res = -EBUSY;
		goto error0;
	}
	tasklet_init(&priv->rx_tasklet, net_tasklet, (unsigned long)ndev);
	return 0;
error0:

	return res;
}

/*
 * Ethernet debug
 */
/* --------------------------------------------------------------------------*/
/**
 * @brief  am_net_dump_phyreg 
 */
/* --------------------------------------------------------------------------*/
static void am_net_dump_phyreg(void)
{
	int reg = 0;
	int val = 0;
	struct am_net_private *np = netdev_priv(my_ndev);

	if ((np == NULL) || (np->dev == NULL))
		return;

	printk("========== ETH PHY regs ==========\n");
	for (reg = 0; reg < 32; reg++) {
		val = mdio_read(np->dev, np->phys[0], reg);
		printk("[reg_%d] 0x%x\n", reg, val);
	}
}

/* --------------------------------------------------------------------------*/
/**
 * @brief  am_net_read_phyreg 
 *
 * @param  argc
 * @param  argv
 *
 * @return 
 */
/* --------------------------------------------------------------------------*/
static int am_net_read_phyreg(int argc, char **argv)
{
	int reg = 0;
	int val = 0;
	struct am_net_private *np = netdev_priv(my_ndev);

	if ((np == NULL) || (np->dev == NULL))
		return -1;

	if (argc < 2 || (argv == NULL) || (argv[0] == NULL) || (argv[1] == NULL)) {
		printk("Invalid syntax\n");
		return -1;
	}
	reg = simple_strtol(argv[1], NULL, 16);
	if (reg >= 0 && reg <= 31) {
		val = mdio_read(np->dev, np->phys[0], reg);
		printk("read phy [reg_%d] 0x%x\n", reg, val);
	} else {
		printk("Invalid parameter\n");
	}

	return 0;
}

/* --------------------------------------------------------------------------*/
/**
 * @brief  am_net_write_phyreg 
 *
 * @param  argc
 * @param  argv
 *
 * @return 
 */
/* --------------------------------------------------------------------------*/
static int am_net_write_phyreg(int argc, char **argv)
{
	int reg = 0;
	int val = 0;
	struct am_net_private *np = netdev_priv(my_ndev);

	if ((np == NULL) || (np->dev == NULL))
		return -1;

	if (argc < 3 || (argv == NULL) || (argv[0] == NULL)
			|| (argv[1] == NULL) || (argv[2] == NULL)) {
		printk("Invalid syntax\n");
		return -1;
	}
	reg = simple_strtol(argv[1], NULL, 16);
	val = simple_strtol(argv[2], NULL, 16);
	if (reg >=0 && reg <=31) {
		mdio_write(np->dev, np->phys[0], reg, val);
		printk("write phy [reg_%d] 0x%x, 0x%x\n", reg, val, mdio_read(np->dev, np->phys[0], reg));
	} else {
		printk("Invalid parameter\n");
	}

	return 0;
}

static const char *g_phyreg_help = {
	"Usage:\n"
	"    echo d > phyreg;            //dump ethernet phy reg\n"
	"    echo r reg > phyreg;        //read ethernet phy reg\n"
	"    echo w reg val > phyreg;    //read ethernet phy reg\n"
};

/* --------------------------------------------------------------------------*/
/**
 * @brief  eth_phyreg_help 
 *
 * @param  class
 * @param  attr
 * @param  buf
 *
 * @return 
 */
/* --------------------------------------------------------------------------*/
static ssize_t eth_phyreg_help(struct class *class, struct class_attribute *attr, char *buf)
{
	return sprintf(buf, "%s\n", g_phyreg_help);
}

/* --------------------------------------------------------------------------*/
/**
 * @brief  eth_phyreg_func 
 *
 * @param  class
 * @param  attr
 * @param  buf
 * @param  count
 *
 * @return 
 */
/* --------------------------------------------------------------------------*/
static ssize_t eth_phyreg_func(struct class *class, struct class_attribute *attr,
		const char *buf, size_t count)
{
	int argc;
	char *buff, *p, *para;
	char *argv[4];
	char cmd;

	buff = kstrdup(buf, GFP_KERNEL);
	p = buff;
	for (argc = 0; argc < 4; argc++) {
		para = strsep(&p, " ");
		if (para == NULL)
			break;
		argv[argc] = para;
	}
	if (argc < 1 || argc > 4)
		goto end;

	cmd = argv[0][0];
	switch (cmd) {
	case 'r':
	case 'R':
		am_net_read_phyreg(argc, argv);
		break;
	case 'w':
	case 'W':
		am_net_write_phyreg(argc, argv);
		break;
	case 'd':
	case 'D':
		am_net_dump_phyreg();
		break;
	default:
		goto end;
	}

	return count;

end:
	kfree(buff);
	return 0;
}

/* --------------------------------------------------------------------------*/
/**
 * @brief  am_net_dump_macreg 
 */
/* --------------------------------------------------------------------------*/
static void am_net_dump_macreg(void)
{
	int reg = 0;
	int val = 0;
	struct am_net_private *np = netdev_priv(my_ndev);

	if ((np == NULL) || (np->dev == NULL))
		return;

	printk("========== ETH_MAC regs ==========\n");
	for (reg = 0x0000; reg <= 0x00FC; reg += 0x4) {
		val = IO_READ32(np->base_addr + reg);
		printk("[0x%04x] 0x%x\n", reg, val);
	}
#if 0
	printk("========== ETH_MMC regs ==========\n");
	for (reg = 0x0100; reg <= 0x0284; reg += 0x4) {
		val = IO_READ32(np->base_addr + reg);
		printk("[0x%04x] 0x%x\n", reg, val);
	}
#endif
	printk("========== ETH_DMA regs ==========\n");
	for (reg = 0x1000; reg <= 0x1054; reg += 0x4) {
		val = IO_READ32(np->base_addr + reg);
		printk("[0x%04x] 0x%x\n", reg, val);
	}
}

/* --------------------------------------------------------------------------*/
/**
 * @brief  am_net_read_macreg 
 *
 * @param  argc
 * @param  argv
 *
 * @return 
 */
/* --------------------------------------------------------------------------*/
static int am_net_read_macreg(int argc, char **argv)
{
	int reg = 0;
	int val = 0;
	struct am_net_private *np = netdev_priv(my_ndev);

	if ((np == NULL) || (np->dev == NULL))
		return -1;

	if (argc < 2 || (argv == NULL) || (argv[0] == NULL) || (argv[1] == NULL)) {
		printk("Invalid syntax\n");
		return -1;
	}
	reg = simple_strtol(argv[1], NULL, 16);
	if (reg >=0 && reg <=31) {
		val = IO_READ32(np->base_addr + reg);
		printk("read mac [0x04%x] 0x%x\n", reg, val);
	} else {
		printk("Invalid parameter\n");
	}

	return 0;
}

/* --------------------------------------------------------------------------*/
/**
 * @brief  am_net_write_macreg 
 *
 * @param  argc
 * @param  argv
 *
 * @return 
 */
/* --------------------------------------------------------------------------*/
static int am_net_write_macreg(int argc, char **argv)
{
	int reg = 0;
	int val = 0;
	struct am_net_private *np = netdev_priv(my_ndev);

	if ((np == NULL) || (np->dev == NULL))
		return -1;

	if ((argc < 3) || (argv == NULL) || (argv[0] == NULL)
			|| (argv[1] == NULL) || (argv[2] == NULL)) {
		printk("Invalid syntax\n");
		return -1;
	}
	reg = simple_strtol(argv[1], NULL, 16);
	val = simple_strtol(argv[2], NULL, 16);
	if (reg >=0 && reg <=31) {
		IO_WRITE32(val, np->base_addr + reg);
		printk("write mac [0x%x] 0x%x, 0x%x\n", reg, val, IO_READ32(np->base_addr + reg));
	} else {
		printk("Invalid parameter\n");
	}

	return 0;
}

static const char *g_macreg_help = {
	"Usage:\n"
	"    echo d > macreg;            //dump ethernet mac reg\n"
	"    echo r reg > macreg;        //read ethernet mac reg\n"
	"    echo w reg val > macreg;    //read ethernet mac reg\n"
};

/* --------------------------------------------------------------------------*/
/**
 * @brief  eth_macreg_help 
 *
 * @param  class
 * @param  attr
 * @param  buf
 *
 * @return 
 */
/* --------------------------------------------------------------------------*/
static ssize_t eth_macreg_help(struct class *class, struct class_attribute *attr, char *buf)
{
	return sprintf(buf, "%s\n", g_macreg_help);
}

/* --------------------------------------------------------------------------*/
/**
 * @brief  eth_macreg_func 
 *
 * @param  class
 * @param  attr
 * @param  buf
 * @param  count
 *
 * @return 
 */
/* --------------------------------------------------------------------------*/
static ssize_t eth_macreg_func(struct class *class, struct class_attribute *attr,
		const char *buf, size_t count)
{
	int argc;
	char *buff, *p, *para;
	char *argv[4];
	char cmd;

	buff = kstrdup(buf, GFP_KERNEL);
	p = buff;
	for (argc = 0; argc < 4; argc++) {
		para = strsep(&p, " ");
		if (para == NULL)
			break;
		argv[argc] = para;
	}
	if (argc < 1 || argc > 4)
		goto end;

	cmd = argv[0][0];
	switch (cmd) {
	case 'r':
	case 'R':
		am_net_read_macreg(argc, argv);
		break;
	case 'w':
	case 'W':
		am_net_write_macreg(argc, argv);
		break;
	case 'd':
	case 'D':
		am_net_dump_macreg();
		break;
	default:
		goto end;
	}

	return count;

end:
	kfree(buff);
	return 0;
}

static const char *g_debug_help = {
	"etherent debug:\n"
	"    1. basic module init and remove info.\n"
	"    2. further setup info.\n"
	"    3. tx data dump.\n"
	"    4. rx data dump.\n"
	"    5. tx/rx data dump.\n"
};

/* --------------------------------------------------------------------------*/
/**
 * @brief  eth_debug_show 
 *
 * @param  class
 * @param  attr
 * @param  buf
 *
 * @return 
 */
/* --------------------------------------------------------------------------*/
static ssize_t eth_debug_show(struct class *class, struct class_attribute *attr, char *buf)
{
	printk("%s\n", g_debug_help);
	printk("current ethernet debug: %d\n", g_debug);

	return 0;
}

/* --------------------------------------------------------------------------*/
/**
 * @brief  eth_debug_store 
 *
 * @param  class
 * @param  attr
 * @param  buf
 * @param  count
 *
 * @return 
 */
/* --------------------------------------------------------------------------*/
static ssize_t eth_debug_store(struct class *class, struct class_attribute *attr, const char *buf, size_t count)
{
	unsigned int debug = 0;

	debug = simple_strtoul(buf, NULL, 0);
	if (debug >= 0 && debug < 6) {
		printk("ethernet debug: %d -> %d\n", g_debug, debug);
		g_debug = debug;
	} else {
		printk("set ethernet debug error\n");
	}

	return count;
}

/* --------------------------------------------------------------------------*/
/**
 * @brief  eth_count_show 
 *
 * @param  class
 * @param  attr
 * @param  buf
 *
 * @return 
 */
/* --------------------------------------------------------------------------*/
static ssize_t eth_count_show(struct class *class, struct class_attribute *attr, char *buf)
{
	printk("Ethernet TX count: 0x%08d\n", g_tx_cnt);
	printk("Ethernet RX count: 0x%08d\n", g_rx_cnt);

	return 0;
}

/* --------------------------------------------------------------------------*/
/**
 * @brief  eth_count_store 
 *
 * @param  class
 * @param  attr
 * @param  buf
 * @param  count
 *
 * @return 
 */
/* --------------------------------------------------------------------------*/
static ssize_t eth_count_store(struct class *class, struct class_attribute *attr, const char *buf, size_t count)
{
	unsigned int cnt = 0;

	cnt = simple_strtoul(buf, NULL, 0);
	if (cnt == 0) {
		printk("reset ethernet tx/rx count.\n");
		g_tx_cnt = 0;
		g_rx_cnt = 0;
	} else {
		printk("reset ethernet count error\n");
	}

	return count;
}

static struct class *eth_sys_class;
static CLASS_ATTR(debug, S_IWUSR | S_IRUGO, eth_debug_show, eth_debug_store);
static CLASS_ATTR(count, S_IWUSR | S_IRUGO, eth_count_show, eth_count_store);
static CLASS_ATTR(phyreg, S_IWUSR | S_IRUGO, eth_phyreg_help, eth_phyreg_func);
static CLASS_ATTR(macreg, S_IWUSR | S_IRUGO, eth_macreg_help, eth_macreg_func);

/* --------------------------------------------------------------------------*/
/**
 * @brief  am_eth_class_init 
 *
 * @return 
 */
/* --------------------------------------------------------------------------*/
static int __init am_eth_class_init(void)
{
	int ret = 0;

	eth_sys_class = class_create(THIS_MODULE, DRIVER_NAME);

	ret = class_create_file(eth_sys_class, &class_attr_debug);
	ret = class_create_file(eth_sys_class, &class_attr_count);
	ret = class_create_file(eth_sys_class, &class_attr_phyreg);
	ret = class_create_file(eth_sys_class, &class_attr_macreg);

	return ret;
}

/* --------------------------------------------------------------------------*/
/**
 * @brief  am_net_init 
 *
 * @return 
 */
/* --------------------------------------------------------------------------*/
static int __init am_net_init(void)
{
	int res;
	printk(DRV_NAME "init(dbg[%p]=%d)\n", (&g_debug), g_debug);
	my_ndev = alloc_etherdev(sizeof(struct am_net_private));
	if (my_ndev == NULL) {
		printk(DRV_NAME "ndev alloc failed!!\n");
		return -ENOMEM;
	}
	res = probe_init(my_ndev);
	if (res != 0) {
		free_netdev(my_ndev);
	}

	res = am_eth_class_init();

	return res;
}

/* --------------------------------------------------------------------------*/
/**
 * @brief  am_net_free 
 *
 * @param  ndev
 */
/* --------------------------------------------------------------------------*/
static void am_net_free(struct net_device *ndev)
{
	netdev_close(ndev);
	unregister_netdev(ndev);
}

/* --------------------------------------------------------------------------*/
/**
 * @brief  am_net_exit 
 *
 * @return 
 */
/* --------------------------------------------------------------------------*/
static void __exit am_net_exit(void)
{
	printk(DRV_NAME "exit\n");
	am_net_free(my_ndev);
	free_netdev(my_ndev);
	class_destroy(eth_sys_class);
	return;
}

module_init(am_net_init);
module_exit(am_net_exit);
