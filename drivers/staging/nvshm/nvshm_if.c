/*
 * Copyright (C) 2012 NVIDIA Corporation.
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */

#include <linux/debugfs.h>

#include "nvshm_types.h"
#include "nvshm_if.h"
#include "nvshm_priv.h"
#include "nvshm_ipc.h"
#include "nvshm_queue.h"
#include "nvshm_iobuf.h"

struct nvshm_channel *nvshm_open_channel(int chan,
					 const struct nvshm_if_operations *ops,
					 void *interface_data)
{
	struct nvshm_handle *handle = nvshm_get_handle();

	pr_debug("%s\n", __func__);
	spin_lock(&handle->lock);
	if (handle->chan[chan].ops) {
		pr_err("%s: already registered on chan %d\n", __func__, chan);
		return NULL;
	}

	handle->chan[chan].ops = ops;
	handle->chan[chan].data = interface_data;
	spin_unlock(&handle->lock);
	return &handle->chan[chan];
}

void nvshm_close_channel(struct nvshm_channel *handle)
{
	struct nvshm_handle *priv = nvshm_get_handle();

	pr_debug("%s\n", __func__);

	flush_workqueue(priv->nvshm_wq);
	spin_lock(&priv->lock);
	priv->chan[handle->index].ops = NULL;
	priv->chan[handle->index].data = NULL;
	spin_unlock(&priv->lock);
}

int nvshm_write(struct nvshm_channel *handle, struct nvshm_iobuf *iob)
{
	struct nvshm_handle *priv = nvshm_get_handle();
	spin_lock_bh(&priv->lock);
	if (!priv->chan[handle->index].ops) {
		spin_unlock_bh(&priv->lock);
		return -EINVAL;
	}

	iob->chan = handle->index;
	/* TBD: update alloc/BW limit counter */
	iob->qnext = NULL;
	nvshm_queue_put(priv, iob);
	spin_unlock_bh(&priv->lock);
	nvshm_generate_ipc(priv);
	return 0;
}
