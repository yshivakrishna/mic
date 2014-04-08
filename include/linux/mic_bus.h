/*
 * Intel MIC Platform Software Stack (MPSS)
 *
 * Copyright(c) 2014 Intel Corporation.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License, version 2, as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
 * General Public License for more details.
 *
 * The full GNU General Public License is included in this distribution in
 * the file called "COPYING".
 *
 * Intel MIC Bus driver.
 *
 * This implementation is very similar to the the virtio bus driver
 * implementation @ include/linux/virtio.h.
 */
#ifndef _MIC_BUS_H_
#define _MIC_BUS_H_
/*
 * Everything a mbus driver needs to work with any particular mbus
 * implementation.
 */
#include <linux/types.h>
#include <linux/device.h>
#include <linux/mod_devicetable.h>
#include <linux/interrupt.h>
#include <linux/dma-mapping.h>

struct mbus_device_id {
	__u32 device;
	__u32 vendor;
};

#define MBUS_DEV_DMA_HOST 2
#define MBUS_DEV_DMA_MIC 3
#define MBUS_DEV_ANY_ID 0xffffffff

/**
 * mbus_device - representation of a device using mbus
 * @priv: private pointer for the driver's use.
 * @mmio_va: virtual address of mmio space
 * @hw_ops: the hardware ops supported by this device.
 * @id: the device type identification (used to match it with a driver).
 * @dev: underlying device.
 * be used to communicate with.
 * @index: unique position on the mbus bus
 */
struct mbus_device {
	void *priv;
	void __iomem *mmio_va;
	struct mbus_hw_ops *hw_ops;
	struct mbus_device_id id;
	struct device dev;
	int index;
};

/**
 * mbus_driver - operations for a mbus I/O driver
 * @driver: underlying device driver (populate name and owner).
 * @id_table: the ids serviced by this driver.
 * @probe: the function to call when a device is found.  Returns 0 or -errno.
 * @remove: the function to call when a device is removed.
 */
struct mbus_driver {
	struct device_driver driver;
	const struct mbus_device_id *id_table;
	int (*probe)(struct mbus_device *dev);
	void (*scan)(struct mbus_device *dev);
	void (*remove)(struct mbus_device *dev);
};

/**
 * struct mic_irq - opaque pointer used as cookie
 */
struct mic_irq;

/**
 * mbus_hw_ops - Hardware operations for accessing a MIC device on the MIC bus.
 */
struct mbus_hw_ops {
	struct mic_irq* (*request_threaded_irq)(struct mbus_device *mbdev,
			irq_handler_t handler, irq_handler_t thread_fn,
			const char *name, void *data, int intr_src);
	void (*free_irq)(struct mbus_device *mbdev,
			struct mic_irq *cookie, void *data);
	void (*ack_interrupt)(struct mbus_device *mbdev, int num);
};

int register_mbus_device(struct mbus_device *dev);
void unregister_mbus_device(struct mbus_device *dev);

int register_mbus_driver(struct mbus_driver *drv);
void unregister_mbus_driver(struct mbus_driver *drv);

static inline struct mbus_device *dev_to_mbus(struct device *_dev)
{
	return container_of(_dev, struct mbus_device, dev);
}

static inline struct mbus_driver *drv_to_mbus(struct device_driver *drv)
{
	return container_of(drv, struct mbus_driver, driver);
}

static inline void mbus_release_dev(struct device *d)
{
}

static inline int
mbus_add_device(struct mbus_device *mbdev, struct device *pdev, int id,
		struct dma_map_ops *dma_ops, struct mbus_hw_ops *hw_ops,
		void __iomem *mmio_va)
{
	int ret;

	mbdev->mmio_va = mmio_va;
	mbdev->dev.parent = pdev;
	mbdev->id.device = id;
	mbdev->id.vendor = MBUS_DEV_ANY_ID;
	mbdev->dev.archdata.dma_ops = dma_ops;
	mbdev->dev.dma_mask = &mbdev->dev.coherent_dma_mask;
	dma_set_mask(&mbdev->dev, DMA_BIT_MASK(64));
	mbdev->dev.release = mbus_release_dev;
	mbdev->hw_ops = hw_ops;
	dev_set_drvdata(&mbdev->dev, mbdev);

	ret = register_mbus_device(mbdev);
	if (ret) {
		dev_err(mbdev->dev.parent,
			"Failed to register mbus device type %u\n", id);
		return ret;
	}
	return 0;
}

static inline void mbus_remove_device(struct mbus_device *mbdev)
{
	unregister_mbus_device(mbdev);
	memset(mbdev, 0x0, sizeof(*mbdev));
}

#endif /* _MIC_BUS_H */
