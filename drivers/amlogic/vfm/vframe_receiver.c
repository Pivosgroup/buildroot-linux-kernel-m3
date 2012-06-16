/*
 * Video Frame Manager For Provider and Receiver
 *
 * Author: Bobby Yang <bo.yang@amlogic.com>
 *
 *
 * Copyright (C) 2010 Amlogic Inc.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

/* Standard Linux headers */
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/slab.h>
#include <linux/list.h>
#include <linux/spinlock.h>

/* Amlogic headers */
#include <linux/amports/vframe.h>
#include <linux/amports/vframe_receiver.h>

/* Local headers */
#include "vfm.h"

static struct list_head rhead = LIST_HEAD_INIT(rhead);
static DEFINE_SPINLOCK(rlist_lock);

bool is_receiver_active(const char* receiver_name)
{
    struct vframe_receiver_s *p = NULL, *ptmp;
    list_for_each_entry_safe(p, ptmp, &rhead, list) {
        if (!strcmp(p->name, receiver_name)) {
            return 1;
        }
    }
    return 0;
}

void receiver_list(void)
{
    struct vframe_receiver_s *r = NULL, *rtmp;
    pr_info("\nreceiver list:\n");
    list_for_each_entry_safe(r, rtmp, &rhead, list) {
        pr_info("   %s\n", r->name);
    }
}
/*
 * get vframe provider from the provider list by receiver name.
 *
 */
struct vframe_receiver_s * vf_get_receiver(const char *provider_name)
{
    struct vframe_receiver_s *p = NULL, *ptmp;
    char* receiver_name = NULL;

    if (list_empty(&rhead)){
       pr_err("Error: no receiver registered\n");
       return NULL;
    }

    if(vfm_mode){
        receiver_name = vf_get_receiver_name( provider_name);
        if(receiver_name){
            list_for_each_entry_safe(p, ptmp, &rhead, list) {
                if (!strcmp(p->name, receiver_name)) {
                    //pr_info("%s:%s %s", __func__, p->name, receiver_name);
                    break;
                }
            }
        }
    }
    else{
        /* get the first receiver in the list */
        p = list_first_entry(&rhead, struct vframe_receiver_s, list);
    }
    return p;
}
EXPORT_SYMBOL(vf_get_receiver);

int vf_notify_receiver(const char* provider_name, int event_type, void* data)
{
    int ret = -1;
    vframe_receiver_t* receiver = NULL;

    receiver = vf_get_receiver(provider_name);
    if(receiver){
        if(receiver->ops && receiver->ops->event_cb){
            ret = receiver->ops->event_cb(event_type, data, receiver->op_arg);
        }
    }
    else{
        pr_err("Error: %s, fail to get receiver of provider %s\n", __func__, provider_name);
    }
    return ret;
}
EXPORT_SYMBOL(vf_notify_receiver);


struct vframe_receiver_s *vf_receiver_alloc(void)
{
    vframe_receiver_t *r;
    r = kmalloc(sizeof(vframe_receiver_t), GFP_KERNEL);
    return r;
}
EXPORT_SYMBOL(vf_receiver_alloc);

void vf_receiver_init(struct vframe_receiver_s *recv,
    const char *name, const struct vframe_receiver_op_s *ops, void* op_arg)
{
    if (!recv)
        return;
    memset(recv, 0, sizeof(struct vframe_receiver_s));
    recv->name = name;
    recv->ops = ops;
    recv->op_arg = op_arg;
    INIT_LIST_HEAD(&recv->list);
}
EXPORT_SYMBOL(vf_receiver_init);

void vf_receiver_free(struct vframe_receiver_s *recv)
{
    kfree(recv);
}
EXPORT_SYMBOL(vf_receiver_free);

int vf_reg_receiver(struct vframe_receiver_s *recv)
{
    ulong flags;
    vframe_receiver_t *r, *rtmp;

    if (!recv)
        return -1;

    spin_lock_irqsave(&rlist_lock, flags);
    list_for_each_entry_safe(r, rtmp, &rhead, list) {
        if (!strcmp(r->name, recv->name)) {
            spin_unlock_irqrestore(&rlist_lock, flags);
            return -1;
        }
    }
    list_add_tail(&recv->list, &rhead);
    spin_unlock_irqrestore(&rlist_lock, flags);
    return 0;
}
EXPORT_SYMBOL(vf_reg_receiver);

void vf_unreg_receiver(struct vframe_receiver_s *recv)
{
    ulong flags;

    struct vframe_receiver_s *r, *rtmp;
    spin_lock_irqsave(&rlist_lock, flags);
    list_for_each_entry_safe(r, rtmp, &rhead, list) {
        if (!strcmp(r->name, recv->name)) {
            list_del(&r->list);
            break;
        }
    }
    spin_unlock_irqrestore(&rlist_lock, flags);
}
EXPORT_SYMBOL(vf_unreg_receiver);



