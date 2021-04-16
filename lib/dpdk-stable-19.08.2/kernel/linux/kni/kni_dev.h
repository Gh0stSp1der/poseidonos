/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright(c) 2010-2014 Intel Corporation.
 */

#ifndef _KNI_DEV_H_
#define _KNI_DEV_H_

#ifdef pr_fmt
#undef pr_fmt
#endif
#define pr_fmt(fmt) KBUILD_MODNAME ": " fmt

#define KNI_VERSION	"1.0"

#include "compat.h"

#include <linux/if.h>
#include <linux/wait.h>
#ifdef HAVE_SIGNAL_FUNCTIONS_OWN_HEADER
#include <linux/sched/signal.h>
#else
#include <linux/sched.h>
#endif
#include <linux/netdevice.h>
#include <linux/spinlock.h>
#include <linux/list.h>

#include <rte_kni_common.h>
#define KNI_KTHREAD_RESCHEDULE_INTERVAL 5 /* us */

#define MBUF_BURST_SZ 32

/* Default carrier state for created KNI network interfaces */
extern uint32_t dflt_carrier;

/**
 * A structure describing the private information for a kni device.
 */
struct kni_dev {
	/* kni list */
	struct list_head list;

	uint32_t core_id;            /* Core ID to bind */
	char name[RTE_KNI_NAMESIZE]; /* Network device name */
	struct task_struct *pthread;

	/* wait queue for req/resp */
	wait_queue_head_t wq;
	struct mutex sync_lock;

	/* kni device */
	struct net_device *net_dev;

	/* queue for packets to be sent out */
	struct rte_kni_fifo *tx_q;

	/* queue for the packets received */
	struct rte_kni_fifo *rx_q;

	/* queue for the allocated mbufs those can be used to save sk buffs */
	struct rte_kni_fifo *alloc_q;

	/* free queue for the mbufs to be freed */
	struct rte_kni_fifo *free_q;

	/* request queue */
	struct rte_kni_fifo *req_q;

	/* response queue */
	struct rte_kni_fifo *resp_q;

	void *sync_kva;
	void *sync_va;

	void *mbuf_kva;
	void *mbuf_va;

	/* mbuf size */
	uint32_t mbuf_size;

	/* buffers */
	void *pa[MBUF_BURST_SZ];
	void *va[MBUF_BURST_SZ];
	void *alloc_pa[MBUF_BURST_SZ];
	void *alloc_va[MBUF_BURST_SZ];
};

void kni_net_release_fifo_phy(struct kni_dev *kni);
void kni_net_rx(struct kni_dev *kni);
void kni_net_init(struct net_device *dev);
void kni_net_config_lo_mode(char *lo_str);
void kni_net_poll_resp(struct kni_dev *kni);

#endif