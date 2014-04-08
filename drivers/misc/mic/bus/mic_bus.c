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
 * implementation @ drivers/virtio/virtio.c
 */
#include <linux/spinlock.h>
#include <linux/module.h>
#include <linux/idr.h>
#include <linux/mic_bus.h>

/* Unique numbering for mbus devices. */
static DEFINE_IDA(mbus_index_ida);

static ssize_t device_show(struct device *d,
			   struct device_attribute *attr, char *buf)
{
	struct mbus_device *dev = dev_to_mbus(d);
	return sprintf(buf, "0x%04x\n", dev->id.device);
}
static DEVICE_ATTR_RO(device);

static ssize_t vendor_show(struct device *d,
			   struct device_attribute *attr, char *buf)
{
	struct mbus_device *dev = dev_to_mbus(d);
	return sprintf(buf, "0x%04x\n", dev->id.vendor);
}
static DEVICE_ATTR_RO(vendor);

static ssize_t modalias_show(struct device *d,
			     struct device_attribute *attr, char *buf)
{
	struct mbus_device *dev = dev_to_mbus(d);
	return sprintf(buf, "mbus:d%08Xv%08X\n",
		       dev->id.device, dev->id.vendor);
}
static DEVICE_ATTR_RO(modalias);

static struct attribute *mbus_dev_attrs[] = {
	&dev_attr_device.attr,
	&dev_attr_vendor.attr,
	&dev_attr_modalias.attr,
	NULL,
};
ATTRIBUTE_GROUPS(mbus_dev);

static inline int mbus_id_match(const struct mbus_device *dev,
				  const struct mbus_device_id *id)
{
	if (id->device != dev->id.device && id->device != MBUS_DEV_ANY_ID)
		return 0;

	return id->vendor == MBUS_DEV_ANY_ID || id->vendor == dev->id.vendor;
}

/*
 * This looks through all the IDs a driver claims to support.  If any of them
 * match, we return 1 and the kernel will call mbus_dev_probe().
 */
static int mbus_dev_match(struct device *dv, struct device_driver *dr)
{
	unsigned int i;
	struct mbus_device *dev = dev_to_mbus(dv);
	const struct mbus_device_id *ids;

	ids = drv_to_mbus(dr)->id_table;
	for (i = 0; ids[i].device; i++)
		if (mbus_id_match(dev, &ids[i]))
			return 1;
	return 0;
}

static int mbus_uevent(struct device *dv, struct kobj_uevent_env *env)
{
	struct mbus_device *dev = dev_to_mbus(dv);

	return add_uevent_var(env, "MODALIAS=mbus:d%08Xv%08X",
			      dev->id.device, dev->id.vendor);
}

static int mbus_dev_probe(struct device *d)
{
	int err;
	struct mbus_device *dev = dev_to_mbus(d);
	struct mbus_driver *drv = drv_to_mbus(dev->dev.driver);

	err = drv->probe(dev);
	if (!err)
		if (drv->scan)
			drv->scan(dev);
	return err;
}

static int mbus_dev_remove(struct device *d)
{
	struct mbus_device *dev = dev_to_mbus(d);
	struct mbus_driver *drv = drv_to_mbus(dev->dev.driver);

	drv->remove(dev);
	return 0;
}

static struct bus_type mic_bus = {
	.name  = "mic_bus",
	.match = mbus_dev_match,
	.dev_groups = mbus_dev_groups,
	.uevent = mbus_uevent,
	.probe = mbus_dev_probe,
	.remove = mbus_dev_remove,
};

int register_mbus_driver(struct mbus_driver *driver)
{
	driver->driver.bus = &mic_bus;
	return driver_register(&driver->driver);
}
EXPORT_SYMBOL_GPL(register_mbus_driver);

void unregister_mbus_driver(struct mbus_driver *driver)
{
	driver_unregister(&driver->driver);
}
EXPORT_SYMBOL_GPL(unregister_mbus_driver);

int register_mbus_device(struct mbus_device *dev)
{
	int err;

	dev->dev.bus = &mic_bus;

	/* Assign a unique device index and hence name. */
	err = ida_simple_get(&mbus_index_ida, 0, 0, GFP_KERNEL);
	if (err < 0)
		return err;

	dev->index = err;
	dev_set_name(&dev->dev, "mbus-dev%u", dev->index);
	/*
	 * device_register() causes the bus infrastructure to look for a
	 * matching driver.
	 */
	err = device_register(&dev->dev);
	return err;
}
EXPORT_SYMBOL_GPL(register_mbus_device);

void unregister_mbus_device(struct mbus_device *dev)
{
	int index = dev->index; /* save for after device release */

	device_unregister(&dev->dev);
	ida_simple_remove(&mbus_index_ida, index);
}
EXPORT_SYMBOL_GPL(unregister_mbus_device);

static int __init mbus_init(void)
{
	return bus_register(&mic_bus);
}

static void __exit mbus_exit(void)
{
	bus_unregister(&mic_bus);
}

core_initcall(mbus_init);
module_exit(mbus_exit);

MODULE_AUTHOR("Intel Corporation");
MODULE_DESCRIPTION("Intel(R) MIC Bus driver");
MODULE_LICENSE("GPL v2");
