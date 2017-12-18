/*
* Most of this source has been derived from the Linux USB
* project:
* (C) Copyright Linus Torvalds 1999
* (C) Copyright Johannes Erdfelt 1999-2001
* (C) Copyright Andreas Gal 1999
* (C) Copyright Gregory P. Smith 1999
* (C) Copyright Deti Fliegl 1999 (new USB architecture)
* (C) Copyright Randy Dunlap 2000
* (C) Copyright David Brownell 2000 (kernel hotplug, usb_device_id)
* (C) Copyright Yggdrasil Computing, Inc. 2000
*     (usb_device_id matching changes by Adam J. Richter)
*
* Adapted for U-Boot:
* (C) Copyright 2001 Denis Peter, MPL AG Switzerland
*
* SPDX-License-Identifier:	GPL-2.0+
*/

/*
* How it works:
*
* Since this is a bootloader, the devices will not be automatic
* (re)configured on hotplug, but after a restart of the USB the
* device should work.
*
* For each transfer (except "Interrupt") we wait for completion.
*/

#include <StdAfx.h>
#include <stdlib.h>
#include <stdio.h>
#include <stdint.h>
#include <align.h>

#include "errno.h"
#include "usb_defs.h"
#include "usbdescriptors.h"
#include "ch9.h"
#include "usb.h"

#ifdef CONFIG_4xx
#include <asm/4xx_pci.h>
#endif

#ifndef USB_BUFSIZ
#define USB_BUFSIZ	512
#endif

static int asynch_allowed;
char usb_started; /* flag for the started/stopped USB status */

#ifndef CONFIG_DM_USB
struct usb_device usb_dev[USB_MAX_DEVICE] = { 0 };

#ifndef CONFIG_USB_MAX_CONTROLLER_COUNT
#define CONFIG_USB_MAX_CONTROLLER_COUNT 1
#endif

//Wrapers of several low level routines to access USB controller.
static int submit_bulk_msg(struct usb_device *dev, unsigned long pipe,
	void *buffer, int transfer_len)
{
	__COMMON_USB_CONTROLLER* pUsbCtrl = NULL;

	if (NULL == dev)
	{
		return -1;
	}
	pUsbCtrl = (__COMMON_USB_CONTROLLER*)dev->controller;
	return pUsbCtrl->ctrlOps.submit_bulk_msg(dev, pipe, buffer, transfer_len);
}

static int submit_control_msg(struct usb_device *dev, unsigned long pipe, void *buffer,
	int transfer_len, struct devrequest *setup)
{
	__COMMON_USB_CONTROLLER* pUsbCtrl = NULL;
	
	if (NULL == dev)
	{
		return -1;
	}
	pUsbCtrl = (__COMMON_USB_CONTROLLER*)dev->controller;
	return pUsbCtrl->ctrlOps.submit_control_msg(dev, pipe, buffer, transfer_len, setup);
}

static int submit_int_msg(struct usb_device *dev, unsigned long pipe, void *buffer,
	int transfer_len, int interval)
{
	__COMMON_USB_CONTROLLER* pUsbCtrl = NULL;

	if (NULL == dev)
	{
		return -1;
	}
	pUsbCtrl = (__COMMON_USB_CONTROLLER*)dev->controller;
	return pUsbCtrl->ctrlOps.submit_int_msg(dev, pipe, buffer, transfer_len, interval);
}

static struct int_queue *create_int_queue(struct usb_device *dev, unsigned long pipe,
	int queuesize, int elementsize, void *buffer, int interval)
{
	__COMMON_USB_CONTROLLER* pUsbCtrl = NULL;

	if (NULL == dev)
	{
		return NULL;
	}
	pUsbCtrl = (__COMMON_USB_CONTROLLER*)dev->controller;
	return pUsbCtrl->ctrlOps.create_int_queue(dev, pipe, queuesize, elementsize, buffer, interval);
}

static int destroy_int_queue(struct usb_device *dev, struct int_queue *queue)
{
	__COMMON_USB_CONTROLLER* pUsbCtrl = NULL;

	if (NULL == dev)
	{
		return -1;
	}
	pUsbCtrl = (__COMMON_USB_CONTROLLER*)dev->controller;
	return pUsbCtrl->ctrlOps.destroy_int_queue(dev, queue);
}

static void *poll_int_queue(struct usb_device *dev, struct int_queue *queue)
{
	__COMMON_USB_CONTROLLER* pUsbCtrl = NULL;

	if (NULL == dev)
	{
		return NULL;
	}
	pUsbCtrl = (__COMMON_USB_CONTROLLER*)dev->controller;
	return pUsbCtrl->ctrlOps.poll_int_queue(dev, queue);
}

/***************************************************************************
* Init USB Device
*/
int usb_init(void)
{
	void *ctrl;
	struct usb_device *dev;
	int i, start_index = 0, index = 0;
	int controllers_initialized = 0;
	int ret;

	//dev_index = 0;
	USBManager.dev_index = 0;

	asynch_allowed = 1;
	usb_hub_reset();

	/* first make all devices unknown */
	for (i = 0; i < USB_MAX_DEVICE; i++) {
		memset(&usb_dev[i], 0, sizeof(struct usb_device));
		usb_dev[i].devnum = -1;
	}

	while (UsbDriverEntry[index].usb_lowlevel_init)
	{
		/* init low_level USB */
		for (i = 0; i < CONFIG_USB_MAX_CONTROLLER_COUNT; i++) {
			/* init low_level USB */
			_hx_printf("%s[%d]:   ", UsbDriverEntry[index].ctrlDesc,i);
			ret = UsbDriverEntry[index].usb_lowlevel_init(i, USB_INIT_HOST, &ctrl);
			if (ret == -ENODEV) {	/* No such device. */
				puts("Port not available.\r\n");
				controllers_initialized++;
				continue;
			}
			if (ret) {		/* Other error. */
				debug("%s lowlevel init failed,ret = %d.\r\n",
					UsbDriverEntry[index].ctrlDesc,
					ret);
				continue;
			}

			/*
			* lowlevel init is OK, now scan the bus for devices
			* i.e. search HUBs and configure them
			*/
			controllers_initialized++;
			start_index = USBManager.dev_index;
			//start_index = dev_index;
			_hx_printf("Scanning bus %d for devices... \r\n", i);
			ret = usb_alloc_new_device(ctrl, &dev);
			if (ret)
				break;

			/*
			* device 0 is always present
			* (root hub, so let it analyze)
			*/
			ret = usb_new_device(dev);
			if (ret){
				printf("usb_init: Create new device failed.\r\n");
				usb_free_device(dev->controller);
			}

			if (start_index == USBManager.dev_index) {
				puts("No USB Device found\r\n");
				continue;
			}
			else {
				printf("%d USB Device(s) found\r\n",
					USBManager.dev_index - start_index);
			}

			usb_started = 1;
		}
		index++;
		mdelay(100);  //Pause for debugging.
	}

	debug("scan end\r\n");
	/* if we were not able to find at least one working bus, bail out */
	if (controllers_initialized == 0)
		puts("USB error: all controllers failed lowlevel init\r\n");

	return usb_started ? 0 : -ENODEV;
}

/******************************************************************************
* Stop USB this stops the LowLevel Part and deregisters USB devices.
*/
int usb_stop(void)
{
	int i, index = 0;

	if (usb_started) {
		asynch_allowed = 1;
		usb_started = 0;
		usb_hub_reset();

		while (UsbDriverEntry[index].usb_lowlevel_stop)
		{
			for (i = 0; i < CONFIG_USB_MAX_CONTROLLER_COUNT; i++) {
				if (UsbDriverEntry[index].usb_lowlevel_stop(i))
					printf("Failed to stop USB controller %d\r\n", i);
			}
			index++;
		}
	}

	return 0;
}

/******************************************************************************
* Detect if a USB device has been plugged or unplugged.
*/
int usb_detect_change(void)
{
	int i, j;
	int change = 0;

	for (j = 0; j < USB_MAX_DEVICE; j++) {
		for (i = 0; i < usb_dev[j].maxchild; i++) {
			struct usb_port_status status;

			if (usb_get_port_status(&usb_dev[j], i + 1,
				&status) < 0)
				/* USB request failed */
				continue;

			if (le16_to_cpu(status.wPortChange) &
				USB_PORT_STAT_C_CONNECTION)
				change++;
		}
	}

	return change;
}

/*
* disables the asynch behaviour of the control message. This is used for data
* transfers that uses the exclusiv access to the control and bulk messages.
* Returns the old value so it can be restored later.
*/
int usb_disable_asynch(int disable)
{
	int old_value = asynch_allowed;

	asynch_allowed = !disable;
	return old_value;
}
#endif /* !CONFIG_DM_USB */


/*-------------------------------------------------------------------
* Message wrappers.
*
*/

/*
* submits an Interrupt Message
*/
int usb_submit_int_msg(struct usb_device *dev, unsigned long pipe,
	void *buffer, int transfer_len, int interval)
{
	return submit_int_msg(dev, pipe, buffer, transfer_len, interval);
}

/*
* submits a control message and waits for comletion (at least timeout * 1ms)
* If timeout is 0, we don't wait for completion (used as example to set and
* clear keyboards LEDs). For data transfers, (storage transfers) we don't
* allow control messages with 0 timeout, by previousely resetting the flag
* asynch_allowed (usb_disable_asynch(1)).
* returns the transfered length if OK or -1 if error. The transfered length
* and the current status are stored in the dev->act_len and dev->status.
*/
int usb_control_msg(struct usb_device *dev, unsigned int pipe,
	unsigned char request, unsigned char requesttype,
	unsigned short value, unsigned short index,
	void *data, unsigned short size, int timeout)
{
	ALLOC_CACHE_ALIGN_BUFFER(struct devrequest, setup_packet, 1);
	int err;

	if ((timeout == 0) && (!asynch_allowed)) {
		/* request for a asynch control pipe is not allowed */
		return -EINVAL;
	}

	/* set setup command */
	setup_packet->requesttype = requesttype;
	setup_packet->request = request;
	setup_packet->value = cpu_to_le16(value);
	setup_packet->index = cpu_to_le16(index);
	setup_packet->length = cpu_to_le16(size);
	//debug("usb_control_msg: request: 0x%X, requesttype: 0x%X, " \
	//	"value 0x%X index 0x%X length 0x%X timeout %d.\r\n",
	//	request, requesttype, value, index, size,timeout);
	dev->status = USB_ST_NOT_PROC; /*not yet processed */

	err = submit_control_msg(dev, pipe, data, size, setup_packet);
	if (err < 0)
	{
		//debug("Submit control msg failed with err = %d.\r\n", err);
		return err;
	}
	if (timeout == 0)
		return (int)size;

	/*
	* Wait for status to update until timeout expires, USB driver
	* interrupt handler may set the status when the USB operation has
	* been completed.
	*/
	while (timeout--) {
		if (!(dev->status & USB_ST_NOT_PROC))
			break;
		mdelay(1);
	}
	if (dev->status)
	{
		debug("Submit control msg return -1.\r\n");
		return -1;
	}

	//debug("Submit control msg return with value %d.\r\n", dev->act_len);
	return dev->act_len;
}

/*-------------------------------------------------------------------
* submits bulk message, and waits for completion. returns 0 if Ok or
* negative if Error.
* synchronous behavior
*/
int usb_bulk_msg(struct usb_device *dev, unsigned int pipe,
	void *data, int len, int *actual_length, int timeout)
{
	int ret = -1;

	if (len < 0)
	{
		return -EINVAL;
	}

	dev->status = USB_ST_NOT_PROC; /*not yet processed */
	ret = submit_bulk_msg(dev, pipe, data, len);
	*actual_length = dev->act_len;
	return ret;

	/*
	 * The following code is obsoluted.
	 */
#if 0
	if (ret < 0)
	{
		return ret;
	}

	while (timeout--) {
		if (!(dev->status & USB_ST_NOT_PROC))
			break;
		mdelay(1);
	}
	*actual_length = dev->act_len;
	if (dev->status == 0)
		return 0;
	else
		return -EIO;
#endif
}

/*-------------------------------------------------------------------
* Max Packet stuff
*/

/*
* returns the max packet size, depending on the pipe direction and
* the configurations values
*/
int usb_maxpacket(struct usb_device *dev, unsigned long pipe)
{
	/* direction is out -> use emaxpacket out */
	if ((pipe & USB_DIR_IN) == 0)
		return dev->epmaxpacketout[((pipe >> 15) & 0xf)];
	else
		return dev->epmaxpacketin[((pipe >> 15) & 0xf)];
}

/*
* The routine usb_set_maxpacket_ep() is extracted from the loop of routine
* usb_set_maxpacket(), because the optimizer of GCC 4.x chokes on this routine
* when it is inlined in 1 single routine. What happens is that the register r3
* is used as loop-count 'i', but gets overwritten later on.
* This is clearly a compiler bug, but it is easier to workaround it here than
* to update the compiler (Occurs with at least several GCC 4.{1,2},x
* CodeSourcery compilers like e.g. 2007q3, 2008q1, 2008q3 lite editions on ARM)
*
* NOTE: Similar behaviour was observed with GCC4.6 on ARMv5.
*/
#ifdef __MS_VC__
static void
#else
static void noinline
#endif
usb_set_maxpacket_ep(struct usb_device *dev, int if_idx, int ep_idx)
{
	int b;
	struct usb_endpoint_descriptor *ep;
	u16 ep_wMaxPacketSize;

	ep = &dev->config.if_desc[if_idx].ep_desc[ep_idx];

	b = ep->bEndpointAddress & USB_ENDPOINT_NUMBER_MASK;
	ep_wMaxPacketSize = get_unaligned(&ep->wMaxPacketSize);

	if ((ep->bmAttributes & USB_ENDPOINT_XFERTYPE_MASK) ==
		USB_ENDPOINT_XFER_CONTROL) {
		/* Control => bidirectional */
		dev->epmaxpacketout[b] = ep_wMaxPacketSize;
		dev->epmaxpacketin[b] = ep_wMaxPacketSize;
		debug("##Control EP epmaxpacketout/in[%d] = %d\r\n",
			b, dev->epmaxpacketin[b]);
	}
	else {
		if ((ep->bEndpointAddress & 0x80) == 0) {
			/* OUT Endpoint */
			if (ep_wMaxPacketSize > dev->epmaxpacketout[b]) {
				dev->epmaxpacketout[b] = ep_wMaxPacketSize;
				debug("##EP epmaxpacketout[%d] = %d\r\n",
					b, dev->epmaxpacketout[b]);
			}
		}
		else {
			/* IN Endpoint */
			if (ep_wMaxPacketSize > dev->epmaxpacketin[b]) {
				dev->epmaxpacketin[b] = ep_wMaxPacketSize;
				debug("##EP epmaxpacketin[%d] = %d\r\n",
					b, dev->epmaxpacketin[b]);
			}
		} /* if out */
	} /* if control */
}

/*
* set the max packed value of all endpoints in the given configuration
*/
static int usb_set_maxpacket(struct usb_device *dev)
{
	int i, ii;

	for (i = 0; i < dev->config.desc.bNumInterfaces; i++)
		for (ii = 0; ii < dev->config.if_desc[i].desc.bNumEndpoints; ii++)
			usb_set_maxpacket_ep(dev, i, ii);

	return 0;
}

/*******************************************************************************
* Parse the config, located in buffer, and fills the dev->config structure.
* Note that all little/big endian swapping are done automatically.
* (wTotalLength has already been swapped and sanitized when it was read.)
*/
static int usb_parse_config(struct usb_device *dev,
	unsigned char *buffer, int cfgno)
{
	struct usb_descriptor_header *head;
	struct usb_interface_descriptor* usb_int = NULL;
	int index, ifno, epno, curr_if_num;
	int cs_int_len = 0;
	int int_assoc_index = 0;
	u16 ep_wMaxPacketSize;
	struct usb_interface* if_desc = NULL;
	struct usb_interface* pri_if = NULL;  //First interface desc,in case of multiple alternate settings.

	ifno = -1;
	epno = -1;
	curr_if_num = -1;

	dev->configno = cfgno;
	head = (struct usb_descriptor_header *) &buffer[0];
	if (head->bDescriptorType != USB_DT_CONFIG) {
		_hx_printf(" ERROR: NOT USB_CONFIG_DESC %x\r\n",
			head->bDescriptorType);
		return -EINVAL;
	}
	if (head->bLength != USB_DT_CONFIG_SIZE) {
		_hx_printf("ERROR: Invalid USB CFG length (%d)\r\n", head->bLength);
		return -EINVAL;
	}
	memcpy(&dev->config, head, USB_DT_CONFIG_SIZE);
	dev->config.no_of_if = 0;
	dev->config.no_of_if_assoc = 0;

	index = dev->config.desc.bLength;
	/* Ok the first entry must be a configuration entry,
	* now process the others */
	head = (struct usb_descriptor_header *) &buffer[index];
	while (index + 1 < dev->config.desc.wTotalLength && head->bLength) {
		switch (head->bDescriptorType) {
		case USB_DT_INTERFACE:
			if (head->bLength != USB_DT_INTERFACE_SIZE) {
				_hx_printf("ERROR: Invalid USB IF length (%d)\r\n",
					head->bLength);
				break;
			}
			if (index + USB_DT_INTERFACE_SIZE >
				dev->config.desc.wTotalLength) {
				_hx_printf("USB IF descriptor overflowed buffer!\r\n");
				break;
			}
			usb_int = (struct usb_interface_descriptor*)head;
			if (usb_int->bInterfaceNumber != curr_if_num) {
				/* this is a new interface, copy new desc */
				ifno = dev->config.no_of_if;
				if (ifno >= USB_MAXINTERFACES) {
					_hx_printf("%s:too many USB interfaces!\r\n",__func__);
					/* try to go on with what we have */
					return -EINVAL;
				}
				if_desc = &dev->config.if_desc[ifno];
				dev->config.no_of_if++;
				memcpy(if_desc, head,USB_DT_INTERFACE_SIZE);
				if_desc->no_of_ep = 0;
				if_desc->num_altsetting = 1;
				curr_if_num = if_desc->desc.bInterfaceNumber;
				pri_if = if_desc;  //Save as primary interface.
			}
			else {
				/* found alternate setting for the interface */
				/*if (ifno >= 0) {
					if_desc = &dev->config.if_desc[ifno];
					if_desc->num_altsetting++;
				}*/
				/* Also save to interface desc slot. */
				ifno = dev->config.no_of_if;
				if (ifno >= USB_MAXINTERFACES) {
					_hx_printf("%s:too many USB interfaces!\r\n", __func__);
					return -EINVAL;
				}
				if_desc = &dev->config.if_desc[ifno];
				dev->config.no_of_if++;
				memcpy(if_desc, head, USB_DT_INTERFACE_SIZE);
				if_desc->no_of_ep = 0;
				if_desc->num_altsetting = 0;  //Mark it as alternate setting interface.
				curr_if_num = if_desc->desc.bInterfaceNumber;
				pri_if->num_altsetting++;
				//Debugging.
				debug("%s:find alternate setting[if_num = %d,ifno = %d,alt_set = %d,no_of_p = %d].\r\n",
					__func__,
					curr_if_num,
					ifno,
					usb_int->bAlternateSetting,
					usb_int->bNumEndpoints);
			}
			break;
		case USB_DT_INTERFACE_ASSOCIATION:
			if (USB_MAXINTERFACEASSOC == int_assoc_index)
			{
				_hx_printf("%s:too many interface associations.\r\n", __func__);
				break;
			}
			//Save the interface association to device config space.
			memcpy(&dev->config.int_assoc[int_assoc_index], head, head->bLength);
			_hx_printf("%s:add interface association[index = %d,len = %d] to device.\r\n", 
				__func__,
				int_assoc_index,
				head->bLength);
			int_assoc_index++;
			dev->config.no_of_if_assoc++;
			break;
		case USB_DT_CS_INTERFACE:
		case USB_DT_CS_ENDPOINT:
			if (head->bLength > (USB_MAX_CSINTERFACE_LEN - cs_int_len))
			{
				_hx_printf("%s:no enough CS interface space[rest %d bytes,reqd %d bytes].\r\n",
					__func__, (USB_MAX_CSINTERFACE_LEN - cs_int_len), head->bLength);
				return -EINVAL;
			}
			//Save CS interface to USB device space.
			memcpy(&dev->config.pClassSpecificInterfaces[cs_int_len], head, head->bLength);
			cs_int_len += head->bLength;
			debug("%s:parse CS interface with len = %d.\r\n", __func__, head->bLength);
			break;
		case USB_DT_ENDPOINT:
			if ((head->bLength != USB_DT_ENDPOINT_SIZE) && (head->bLength != USB_DT_ENDPOINT_AUDIO_SIZE)) {
				_hx_printf("ERROR: Invalid USB EP length (%d)\r\n",
					head->bLength);
				break;
			}
			if (index + USB_DT_ENDPOINT_SIZE >
				dev->config.desc.wTotalLength) {
				_hx_printf("USB EP descriptor overflowed buffer!\r\n");
				break;
			}
			if (ifno < 0) {
				_hx_printf("Endpoint descriptor out of order!\r\n");
				break;
			}
			epno = dev->config.if_desc[ifno].no_of_ep;
			if_desc = &dev->config.if_desc[ifno];
			if (epno > USB_MAXENDPOINTS) {
				_hx_printf("Interface %d has too many endpoints!\r\n",
					if_desc->desc.bInterfaceNumber);
				return -EINVAL;
			}
			/* found an endpoint */
			if_desc->no_of_ep++;
			memcpy(&if_desc->ep_desc[epno], head,
				head->bLength);
			ep_wMaxPacketSize = get_unaligned(&dev->config.\
				if_desc[ifno].\
				ep_desc[epno].\
				wMaxPacketSize);
			put_unaligned(le16_to_cpu(ep_wMaxPacketSize),
				&dev->config.\
				if_desc[ifno].\
				ep_desc[epno].\
				wMaxPacketSize);
			debug("if %d, ep %d\n", ifno, epno);
			break;
		case USB_DT_SS_ENDPOINT_COMP:
			if (head->bLength != USB_DT_SS_EP_COMP_SIZE) {
				_hx_printf("ERROR: Invalid USB EPC length (%d)\r\n",
					head->bLength);
				break;
			}
			if (index + USB_DT_SS_EP_COMP_SIZE >
				dev->config.desc.wTotalLength) {
				puts("USB EPC descriptor overflowed buffer!\r\n");
				break;
			}
			if (ifno < 0 || epno < 0) {
				puts("EPC descriptor out of order!\r\n");
				break;
			}
			if_desc = &dev->config.if_desc[ifno];
			memcpy(&if_desc->ss_ep_comp_desc[epno], head,
				USB_DT_SS_EP_COMP_SIZE);
			break;
		default:
			if (head->bLength == 0)
				return -EINVAL;
			_hx_printf("unknown Description Type : %d\r\n",
				head->bDescriptorType);
#ifdef DEBUG
			{
				unsigned char *ch = (unsigned char *)head;
				int i;

				for (i = 0; i < head->bLength; i++)
					debug("%02X ", *ch++);
				debug("\r\n\r\n\r\n");
			}
#endif
			break;
		}
		index += head->bLength;
		head = (struct usb_descriptor_header *)&buffer[index];
	}
	return 0;
}

/***********************************************************************
* Clears an endpoint
* endp: endpoint number in bits 0-3;
* direction flag in bit 7 (1 = IN, 0 = OUT)
*/
int usb_clear_halt(struct usb_device *dev, int pipe)
{
	int result;
	int endp = usb_pipeendpoint(pipe) | (usb_pipein(pipe) << 7);

	result = usb_control_msg(dev, usb_sndctrlpipe(dev, 0),
		USB_REQ_CLEAR_FEATURE, USB_RECIP_ENDPOINT, 0,
		endp, NULL, 0, USB_CNTL_TIMEOUT * 3);

	/* don't clear if failed */
	if (result < 0)
		return result;

	/*
	* NOTE: we do not get status and verify reset was successful
	* as some devices are reported to lock up upon this check..
	*/

	usb_endpoint_running(dev, usb_pipeendpoint(pipe), usb_pipeout(pipe));

	/* toggle is reset on clear */
	usb_settoggle(dev, usb_pipeendpoint(pipe), usb_pipeout(pipe), 0);
	return 0;
}


/**********************************************************************
* get_descriptor type
*/
static int usb_get_descriptor(struct usb_device *dev, unsigned char type,
	unsigned char index, void *buf, int size)
{
	int res;
	res = usb_control_msg(dev, usb_rcvctrlpipe(dev, 0),
		USB_REQ_GET_DESCRIPTOR, USB_DIR_IN,
		(type << 8) + index, 0,
		buf, size, USB_CNTL_TIMEOUT);
	return res;
}

/**********************************************************************
* gets configuration cfgno and store it in the buffer
*/
unsigned char* usb_get_configuration_no(struct usb_device *dev, int cfgno)
{
	int result;
	unsigned int length;
	struct usb_config_descriptor *config = NULL;
	ALLOC_CACHE_ALIGN_BUFFER(unsigned char, buffer, 16);

	config = (struct usb_config_descriptor *)&buffer[0];
	result = usb_get_descriptor(dev, USB_DT_CONFIG, cfgno, buffer, 9);
	if (result < 9) {
		if (result < 0)
			_hx_printf("unable to get descriptor, error %lX\r\n",
			dev->status);
		else
			_hx_printf("config descriptor too short " \
			"(expected %i, got %i)\r\n", 9, result);
		return NULL;
	}
	length = le16_to_cpu(config->wTotalLength);

	if (length > 2048) {
		printf("%s: failed to get descriptor - too long: %d\r\n",
			"usb_get_configuariton_no", length);
		return NULL;
	}

	//Allocate buffer to contain configure descriptor,the caller's responsibility to release it.
	config = (struct usb_config_descriptor*)aligned_malloc(length, ARCH_DMA_MINALIGN);
	if (NULL == config)
	{
		_hx_printf("%s:failed to allocate mem[length = %d].\r\n", __func__, length);
		return NULL;
	}
	result = usb_get_descriptor(dev, USB_DT_CONFIG, cfgno, config, length);
	if (result < 0)
	{
		_hx_printf("%s:failed to get config descriptor.\r\n", __func__);
		aligned_free(config);
		return NULL;
	}
	debug("get_conf_no %d Result %d, wLength %d\r\n", cfgno, result, length);
	config->wTotalLength = length; /* validated, with CPU byte order */

	return (unsigned char*)config;
}

/********************************************************************
* set address of a device to the value in dev->devnum.
* This can only be done by addressing the device via the default address (0)
*/
static int usb_set_address(struct usb_device *dev)
{
	int res;

	debug("set address %d\r\n", dev->devnum);
	res = usb_control_msg(dev, usb_snddefctrl(dev),
		USB_REQ_SET_ADDRESS, 0,
		(dev->devnum), 0,
		NULL, 0, USB_CNTL_TIMEOUT);
	return res;
}

/********************************************************************
* set interface number to interface
*/
int usb_set_interface(struct usb_device *dev, int interface, int alternate)
{
	struct usb_interface *if_face = NULL;
	int ret, i;

	for (i = 0; i < dev->config.desc.bNumInterfaces; i++) {
		if (dev->config.if_desc[i].desc.bInterfaceNumber == interface) {
			if_face = &dev->config.if_desc[i];
			break;
		}
	}
	if (!if_face) {
		printf("selecting invalid interface %d", interface);
		return -EINVAL;
	}
	/*
	* We should return now for devices with only one alternate setting.
	* According to 9.4.10 of the Universal Serial Bus Specification
	* Revision 2.0 such devices can return with a STALL. This results in
	* some USB sticks timeouting during initialization and then being
	* unusable in U-Boot.
	*/
	if (if_face->num_altsetting == 1)
		return 0;

	ret = usb_control_msg(dev, usb_sndctrlpipe(dev, 0),
		USB_REQ_SET_INTERFACE, USB_RECIP_INTERFACE,
		alternate, interface, NULL, 0,
		USB_CNTL_TIMEOUT * 5);
	if (ret < 0)
		return ret;

	return 0;
}

/********************************************************************
* set configuration number to configuration
*/
static int usb_set_configuration(struct usb_device *dev, int configuration)
{
	int res;
	debug("set configuration %d\r\n", configuration);
	/* set setup command */
	res = usb_control_msg(dev, usb_sndctrlpipe(dev, 0),
		USB_REQ_SET_CONFIGURATION, 0,
		configuration, 0,
		NULL, 0, USB_CNTL_TIMEOUT);
	if (res == 0) {
		dev->toggle[0] = 0;
		dev->toggle[1] = 0;
		return 0;
	}
	else
		return -EIO;
}

/********************************************************************
* set protocol to protocol
*/
int usb_set_protocol(struct usb_device *dev, int ifnum, int protocol)
{
	return usb_control_msg(dev, usb_sndctrlpipe(dev, 0),
		USB_REQ_SET_PROTOCOL, USB_TYPE_CLASS | USB_RECIP_INTERFACE,
		protocol, ifnum, NULL, 0, USB_CNTL_TIMEOUT);
}

/********************************************************************
* set idle
*/
int usb_set_idle(struct usb_device *dev, int ifnum, int duration, int report_id)
{
	return usb_control_msg(dev, usb_sndctrlpipe(dev, 0),
		USB_REQ_SET_IDLE, USB_TYPE_CLASS | USB_RECIP_INTERFACE,
		(duration << 8) | report_id, ifnum, NULL, 0, USB_CNTL_TIMEOUT);
}

/********************************************************************
* get report
*/
int usb_get_report(struct usb_device *dev, int ifnum, unsigned char type,
	unsigned char id, void *buf, int size)
{
	return usb_control_msg(dev, usb_rcvctrlpipe(dev, 0),
		USB_REQ_GET_REPORT,
		USB_DIR_IN | USB_TYPE_CLASS | USB_RECIP_INTERFACE,
		(type << 8) + id, ifnum, buf, size, USB_CNTL_TIMEOUT);
}

/********************************************************************
* get class descriptor
*/
int usb_get_class_descriptor(struct usb_device *dev, int ifnum,
	unsigned char type, unsigned char id, void *buf, int size)
{
	return usb_control_msg(dev, usb_rcvctrlpipe(dev, 0),
		USB_REQ_GET_DESCRIPTOR, USB_RECIP_INTERFACE | USB_DIR_IN,
		(type << 8) + id, ifnum, buf, size, USB_CNTL_TIMEOUT);
}

/********************************************************************
* get string index in buffer
*/
static int usb_get_string(struct usb_device *dev, unsigned short langid,
	unsigned char index, void *buf, int size)
{
	int i;
	int result;

	for (i = 0; i < 3; ++i) {
		/* some devices are flaky */
		result = usb_control_msg(dev, usb_rcvctrlpipe(dev, 0),
			USB_REQ_GET_DESCRIPTOR, USB_DIR_IN,
			(USB_DT_STRING << 8) + index, langid, buf, size,
			USB_CNTL_TIMEOUT);

		if (result > 0)
			break;
	}

	return result;
}


static void usb_try_string_workarounds(unsigned char *buf, int *length)
{
	int newlength, oldlength = *length;

	for (newlength = 2; newlength + 1 < oldlength; newlength += 2)
		if (!isprint(buf[newlength]) || buf[newlength + 1])
			break;

	if (newlength > 2) {
		buf[0] = newlength;
		*length = newlength;
	}
}


static int usb_string_sub(struct usb_device *dev, unsigned int langid,
	unsigned int index, unsigned char *buf)
{
	int rc;

	/* Try to read the string descriptor by asking for the maximum
	* possible number of bytes */
	rc = usb_get_string(dev, langid, index, buf, 255);

	/* If that failed try to read the descriptor length, then
	* ask for just that many bytes */
	if (rc < 2) {
		rc = usb_get_string(dev, langid, index, buf, 2);
		if (rc == 2)
			rc = usb_get_string(dev, langid, index, buf, buf[0]);
	}

	if (rc >= 2) {
		if (!buf[0] && !buf[1])
			usb_try_string_workarounds(buf, &rc);

		/* There might be extra junk at the end of the descriptor */
		if (buf[0] < rc)
			rc = buf[0];

		rc = rc - (rc & 1); /* force a multiple of two */
	}

	if (rc < 2)
		rc = -EINVAL;

	return rc;
}


/********************************************************************
* usb_string:
* Get string index and translate it to ascii.
* returns string length (> 0) or error (< 0)
*/
int usb_string(struct usb_device *dev, int index, char *buf, size_t size)
{
	ALLOC_CACHE_ALIGN_BUFFER(unsigned char, mybuf, USB_BUFSIZ);
	unsigned char *tbuf;
	int err;
	unsigned int u, idx;

	if (size <= 0 || !buf || !index)
		return -EINVAL;
	buf[0] = 0;
	tbuf = &mybuf[0];

	/* get langid for strings if it's not yet known */
	if (!dev->have_langid) {
		err = usb_string_sub(dev, 0, 0, tbuf);
		if (err < 0) {
			debug("error getting string descriptor 0 " \
				"(error=%lx)\n", dev->status);
			return -EIO;
		}
		else if (tbuf[0] < 4) {
			debug("string descriptor 0 too short\r\n");
			return -EIO;
		}
		else {
			dev->have_langid = -1;
			dev->string_langid = tbuf[2] | (tbuf[3] << 8);
			/* always use the first langid listed */
			debug("USB device number %d default " \
				"language ID 0x%x\r\n",
				dev->devnum, dev->string_langid);
		}
	}

	err = usb_string_sub(dev, dev->string_langid, index, tbuf);
	if (err < 0)
		return err;

	size--;		/* leave room for trailing NULL char in output buffer */
	for (idx = 0, u = 2; u < (unsigned int)err; u += 2) {
		if (idx >= size)
			break;
		if (tbuf[u + 1])			/* high byte */
			buf[idx++] = '?';  /* non-ASCII character */
		else
			buf[idx++] = tbuf[u];
	}
	buf[idx] = 0;
	err = idx;
	return err;
}


/********************************************************************
* USB device handling:
* the USB device are static allocated [USB_MAX_DEVICE].
*/

#ifndef CONFIG_DM_USB

/* returns a pointer to the device with the index [index].
* if the device is not assigned (dev->devnum==-1) returns NULL
*/
struct usb_device *usb_get_dev_index(int index)
{
	if (usb_dev[index].devnum == -1)
		return NULL;
	else
		return &usb_dev[index];
}

int usb_alloc_new_device(struct udevice *controller, struct usb_device **devp)
{
	int i;
	unsigned char* pCSInterface = NULL;

	debug("New Device %d\n", USBManager.dev_index);
	if (USBManager.dev_index == USB_MAX_DEVICE) {
		_hx_printf("ERROR, too many USB Devices, max=%d\r\n", USB_MAX_DEVICE);
		return -ENOSPC;
	}
	//Allocate space to contain Class Specific Interfaces for USB device.
	pCSInterface = _hx_malloc(USB_MAX_CSINTERFACE_LEN);
	if (NULL == pCSInterface)
	{
		_hx_printf("%s:failed to allocate CS_Interface space.\r\n", __func__);
		return -ENOMEM;
	}
	memset(pCSInterface, 0, USB_MAX_CSINTERFACE_LEN);

	/* default Address is 0, real addresses start with 1 */
	usb_dev[USBManager.dev_index].devnum = USBManager.dev_index + 1;
	usb_dev[USBManager.dev_index].maxchild = 0;
	for (i = 0; i < USB_MAXCHILDREN; i++)
		usb_dev[USBManager.dev_index].children[i] = NULL;
	usb_dev[USBManager.dev_index].parent = NULL;
	usb_dev[USBManager.dev_index].controller = controller;
	//dev_index++;
	USBManager.dev_index++;
	*devp = &usb_dev[USBManager.dev_index - 1];
	//Assign Class Specific Interfaces space.
	(*devp)->config.pClassSpecificInterfaces = pCSInterface;

	return 0;
}

/*
* Free the newly created device node.
* Called in error cases where configuring a newly attached
* device fails for some reason.
*/
void usb_free_device(struct udevice *controller)
{
	//dev_index--;
	USBManager.dev_index--;
	debug("Freeing device node: %d\r\n", USBManager.dev_index);
	memset(&usb_dev[USBManager.dev_index], 0, sizeof(struct usb_device));
	usb_dev[USBManager.dev_index].devnum = -1;
}

/*
* XHCI issues Enable Slot command and thereafter
* allocates device contexts. Provide a weak alias
* function for the purpose, so that XHCI overrides it
* and EHCI/OHCI just work out of the box.
*/
#ifdef __MS_VC__
#if !defined(CONFIG_USB_XHCI)  //xHCI will allocate device by it self.
int usb_alloc_device(struct usb_device *udev)
{
	return 0;
}
#endif  //CONFIG_USB_XHCI
#else
__weak int usb_alloc_device(struct usb_device *udev)
{
	return 0;
}
#endif  //__MS_VC__

#endif /* !CONFIG_DM_USB */

static int usb_hub_port_reset(struct usb_device *dev, struct usb_device *hub)
{
	if (hub) {
		unsigned short portstatus;
		int err;

		/* reset the port for the second time */
		err = legacy_hub_port_reset(hub, dev->portnr - 1, &portstatus);
		if (err < 0) {
			printf("\r\n     Couldn't reset port %i\r\n", dev->portnr);
			return err;
		}
	}
	else {
		usb_reset_root_port(dev);
	}

	return 0;
}

static int get_descriptor_len(struct usb_device *dev, int len, int expect_len)
{
#ifdef __MS_VC__
	struct usb_device_descriptor* desc;
#else
	__maybe_unused struct usb_device_descriptor *desc;
#endif
	ALLOC_CACHE_ALIGN_BUFFER(unsigned char, tmpbuf, USB_BUFSIZ);
	int err;

	desc = (struct usb_device_descriptor *)tmpbuf;

	err = usb_get_descriptor(dev, USB_DT_DEVICE, 0, desc, len);
	if (err < expect_len) {
		if (err < 0) {
			_hx_printf("unable to get device descriptor (error=%d)\r\n",
				err);
			return err;
		}
		else {
			_hx_printf("USB device descriptor short read (expected %i, got %i)\r\n",
				expect_len, err);
			return -EIO;
		}
	}
	debug("usb_get_descriptor return ok with value = %d.\r\n", err);
	memcpy(&dev->descriptor, tmpbuf, sizeof(dev->descriptor));

	return 0;
}

static int usb_setup_descriptor(struct usb_device *dev, bool do_read)
{
	/*
	* This is a Windows scheme of initialization sequence, with double
	* reset of the device (Linux uses the same sequence)
	* Some equipment is said to work only with such init sequence; this
	* patch is based on the work by Alan Stern:
	* http://sourceforge.net/mailarchive/forum.php?
	* thread_id=5729457&forum_id=5398
	*/

	/*
	* send 64-byte GET-DEVICE-DESCRIPTOR request.  Since the descriptor is
	* only 18 bytes long, this will terminate with a short packet.  But if
	* the maxpacket size is 8 or 16 the device may be waiting to transmit
	* some more, or keeps on retransmitting the 8 byte header.
	*/

	if (dev->speed == USB_SPEED_LOW) {
		dev->descriptor.bMaxPacketSize0 = 8;
		dev->maxpacketsize = PACKET_SIZE_8;
	}
	else {
		dev->descriptor.bMaxPacketSize0 = 64;
		dev->maxpacketsize = PACKET_SIZE_64;
	}
	dev->epmaxpacketin[0] = dev->descriptor.bMaxPacketSize0;
	dev->epmaxpacketout[0] = dev->descriptor.bMaxPacketSize0;

	if (do_read) {
		int err;

		/*
		* Validate we've received only at least 8 bytes, not that we've
		* received the entire descriptor. The reasoning is:
		* - The code only uses fields in the first 8 bytes, so that's all we
		*   need to have fetched at this stage.
		* - The smallest maxpacket size is 8 bytes. Before we know the actual
		*   maxpacket the device uses, the USB controller may only accept a
		*   single packet. Consequently we are only guaranteed to receive 1
		*   packet (at least 8 bytes) even in a non-error case.
		*
		* At least the DWC2 controller needs to be programmed with the number
		* of packets in addition to the number of bytes. A request for 64
		* bytes of data with the maxpacket guessed as 64 (above) yields a
		* request for 1 packet.
		*/
		err = get_descriptor_len(dev, 64, 8);
		debug("get_descriptor_len returns %d.\r\n", err);
		if (err)
		{
			debug("usb_setup_descriptor return with value = %d,returned by get_descriptor_len.\r\n", err);
			return err;
		}
	}

	dev->epmaxpacketin[0] = dev->descriptor.bMaxPacketSize0;
	dev->epmaxpacketout[0] = dev->descriptor.bMaxPacketSize0;
	switch (dev->descriptor.bMaxPacketSize0) {
	case 8:
		dev->maxpacketsize = PACKET_SIZE_8;
		break;
	case 16:
		dev->maxpacketsize = PACKET_SIZE_16;
		break;
	case 32:
		dev->maxpacketsize = PACKET_SIZE_32;
		break;
	case 64:
		dev->maxpacketsize = PACKET_SIZE_64;
		break;
	default:
		printf("usb_new_device: invalid max packet size\r\n");
		return -EIO;
	}
	debug("dev_addr = %d,maxpktsize = %d,epmaxpacketin = %d,epmaxpacketout = %d.\r\n",
		dev->devnum,
		dev->maxpacketsize,
		dev->epmaxpacketin[0],
		dev->epmaxpacketout[0]);
	return 0;
}

static int usb_prepare_device(struct usb_device *dev, int addr, bool do_read,
struct usb_device *parent)
{
	int err;

	/*
	* Allocate usb 3.0 device context.
	* USB 3.0 (xHCI) protocol tries to allocate device slot
	* and related data structures first. This call does that.
	* Refer to sec 4.3.2 in xHCI spec rev1.0
	*/
	err = usb_alloc_device(dev);
	if (err) {
		printf("Cannot allocate device context to get SLOT_ID\r\n");
		return err;
	}
	err = usb_setup_descriptor(dev, do_read);
	if (err)
	{
		debug("usb_prepare_device return with value = %d,returned by usb_setup_descriptor.\r\n", err);
		return err;
	}
	err = usb_hub_port_reset(dev, parent);
	if (err)
	{
		debug("usb_prepare_device return with value = %d,returned by usb_hub_port_reset.\r\n", err);
		return err;
	}

	dev->devnum = addr;
	err = usb_set_address(dev); /* set address */

	if (err < 0) {
		printf("USB device not accepting new address " \
			"(error=%lX)\r\n", dev->status);
		return err;
	}

	mdelay(10);	/* Let the SET_ADDRESS settle */

	return 0;
}

int usb_select_config(struct usb_device *dev)
{
	//ALLOC_CACHE_ALIGN_BUFFER(unsigned char, tmpbuf, USB_BUFSIZ);
	int err;
	unsigned char* pConfig = NULL;

	err = get_descriptor_len(dev, USB_DT_DEVICE_SIZE, USB_DT_DEVICE_SIZE);
	if (err)
	{
		return err;
	}

	/* correct le values */
	__le16_to_cpus(&dev->descriptor.bcdUSB);
	__le16_to_cpus(&dev->descriptor.idVendor);
	__le16_to_cpus(&dev->descriptor.idProduct);
	__le16_to_cpus(&dev->descriptor.bcdDevice);

	/* only support for one config for now */
	pConfig = usb_get_configuration_no(dev, 0);
	if (NULL == pConfig) {
		_hx_printf("usb_new_device: Cannot read configuration, " \
			"skipping device %04x:%04x\r\n",
			dev->descriptor.idVendor, dev->descriptor.idProduct);
		return -EIO;
	}
	usb_parse_config(dev, pConfig, 0);
	//Should release the space of config descriptor returned by usb_get_configuration_no.
	aligned_free(pConfig);

	usb_set_maxpacket(dev);
	/*
	* we set the default configuration here
	* This seems premature. If the driver wants a different configuration
	* it will need to select itself.
	*/
	err = usb_set_configuration(dev, dev->config.desc.bConfigurationValue);
	if (err < 0) {
		printf("failed to set default configuration " \
			"len %d, status %lX\r\n", dev->act_len, dev->status);
		return err;
	}
	debug("new device strings: Mfr=%d, Product=%d, SerialNumber=%d\r\n",
		dev->descriptor.iManufacturer, dev->descriptor.iProduct,
		dev->descriptor.iSerialNumber);
	memset(dev->mf, 0, sizeof(dev->mf));
	memset(dev->prod, 0, sizeof(dev->prod));
	memset(dev->serial, 0, sizeof(dev->serial));
	if (dev->descriptor.iManufacturer)
		usb_string(dev, dev->descriptor.iManufacturer,
		dev->mf, sizeof(dev->mf));
	if (dev->descriptor.iProduct)
		usb_string(dev, dev->descriptor.iProduct,
		dev->prod, sizeof(dev->prod));
	if (dev->descriptor.iSerialNumber)
		usb_string(dev, dev->descriptor.iSerialNumber,
		dev->serial, sizeof(dev->serial));
	debug("Manufacturer %s\r\n", dev->mf);
	debug("Product      %s\r\n", dev->prod);
	debug("SerialNumber %s\r\n", dev->serial);

	return 0;
}

int usb_setup_device(struct usb_device *dev, bool do_read,
struct usb_device *parent)
{
	int addr;
	int ret;

	/* We still haven't set the Address yet */
	addr = dev->devnum;
	dev->devnum = 0;

	ret = usb_prepare_device(dev, addr, do_read, parent);
	if (ret)
	{
		debug("usb_setup_device return with value = %d.\r\n", ret);
		return ret;
	}
	ret = usb_select_config(dev);
	return ret;
}

#ifndef CONFIG_DM_USB
/*
* By the time we get here, the device has gotten a new device ID
* and is in the default state. We need to identify the thing and
* get the ball rolling..
*
* Returns 0 for success, != 0 for error.
*/
int usb_new_device(struct usb_device *dev)
{
	bool do_read = true;
	int err;

	/*
	* XHCI needs to issue a Address device command to setup
	* proper device context structures, before it can interact
	* with the device. So a get_descriptor will fail before any
	* of that is done for XHCI unlike EHCI.
	*/
#ifdef CONFIG_USB_XHCI
	do_read = false;
#endif
	err = usb_setup_device(dev, do_read, dev->parent);
	if (err)
	{
		debug("usb_new_device return with value = %d.\r\n", err);
		return err;
	}
	//Add a corresponding physical device into HelloX's USB management framework.
	if (!USBManager.AddUsbDevice(dev))
	{
		debug("%s: Can not add usb device into system.\r\n", __func__);
		return -1;
	}

	/* Now probe if the device is a hub */
	err = usb_hub_probe(dev, 0);
	if (err < 0)
		return err;

	return 0;
}
#endif

#ifdef __MS_VC__
int board_usb_init(int index, enum usb_init_type init)
#else
__weak
int board_usb_init(int index, enum usb_init_type init)
#endif
{
	return 0;
}

#ifdef __MS_VC__
int board_usb_cleanup(int index, enum usb_init_type init)
#else
__weak
int board_usb_cleanup(int index, enum usb_init_type init)
#endif
{
	return 0;
}

bool usb_device_has_child_on_port(struct usb_device *parent, int port)
{
#ifdef CONFIG_DM_USB
	return false;
#else
	return parent->children[port] != NULL;
#endif
}

/* EOF */
