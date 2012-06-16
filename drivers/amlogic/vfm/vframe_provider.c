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
#include <linux/amports/vframe_provider.h>
#include <linux/amports/vframe_receiver.h>

/* Local headers */
#include "vfm.h"

static struct list_head phead = LIST_HEAD_INIT(phead);
static DEFINE_SPINLOCK(plist_lock);

bool is_provider_active(const char* provider_name)
{
    struct vframe_provider_s *p = NULL, *ptmp;
    int namelen = strlen(provider_name);
    list_for_each_entry_safe(p, ptmp, &phead, list) {
        if (!strncmp(p->name, provider_name, namelen)) {
            if( strlen(p->name) == namelen ||
                p->name[namelen] == '.'){
                return 1;
            }
        }
    }
    return 0;
}

void provider_list(void)
{
    struct vframe_provider_s *p = NULL, *ptmp;
    pr_info("\nprovider list:\n");
    list_for_each_entry_safe(p, ptmp, &phead, list) {
        pr_info("   %s\n", p->name);
    }
}
/*
 * get vframe provider from the provider list by receiver name.
 *
 */
struct vframe_provider_s * vf_get_provider(const char *receiver_name)
{
    struct vframe_provider_s *p = NULL, *ptmp;
    char* provider_name = NULL;

    if (list_empty(&phead)){
       return NULL;
    }

    if(vfm_mode){
        provider_name = vf_get_provider_name( receiver_name);
        if(provider_name){
            int namelen = strlen(provider_name);
            list_for_each_entry_safe(p, ptmp, &phead, list) {
                //pr_info("%s p:%s n:%s", __func__, p->name, provider_name);
                if (!strncmp(p->name, provider_name, namelen)) {
                    if( strlen(p->name) == namelen ||
                        p->name[namelen] == '.'){
                        break;
                    }
                }
            }
        }
    }
    else{
        /* get the last provider in the list */
        list_for_each_entry_reverse(p, &phead, list){
            break;
        }
    }
    return p;
}
EXPORT_SYMBOL(vf_get_provider);

int vf_notify_provider(const char* receiver_name, int event_type, void* data)
{
    int ret = -1;
    struct vframe_provider_s * provider = vf_get_provider(receiver_name);
    if(provider){
        if(provider->ops && provider->ops->event_cb){
            provider->ops->event_cb(event_type, data, provider->op_arg);
            ret = 0;
        }
    }
    else{
        //pr_err("Error: %s, fail to get provider of receiver %s\n", __func__, receiver_name);
    }
    return ret;
}
EXPORT_SYMBOL(vf_notify_provider);

struct vframe_provider_s *vf_provider_alloc(void)
{
    vframe_provider_t *p;
    p = kmalloc(sizeof(vframe_provider_t), GFP_KERNEL);
    return p;
}
EXPORT_SYMBOL(vf_provider_alloc);

void vf_provider_init(struct vframe_provider_s *prov,
    const char *name, const struct vframe_operations_s *ops, void* op_arg)
{
    if (!prov)
        return;
    memset(prov, 0, sizeof(struct vframe_provider_s));
    prov->name = name;
    prov->ops = ops;
    prov->op_arg = op_arg;
    INIT_LIST_HEAD(&prov->list);
}
EXPORT_SYMBOL(vf_provider_init);

void vf_provider_free(struct vframe_provider_s *prov)
{
    kfree(prov);
}
EXPORT_SYMBOL(vf_provider_free);

int vf_reg_provider(struct vframe_provider_s *prov)
{
    ulong flags;
    ulong fiq_flag;
    vframe_provider_t *p = NULL, *ptmp;
    vframe_receiver_t* receiver = NULL;

    if (!prov || !prov->name)
        return -1;

    list_for_each_entry_safe(p, ptmp, &phead, list) {
        if (!strcmp(p->name, prov->name)) {
            return -1;
        }
    }
    spin_lock_irqsave(&plist_lock, flags);
        raw_local_save_flags(fiq_flag);
        local_fiq_disable();
            list_add_tail(&prov->list, &phead);
        raw_local_irq_restore(fiq_flag);
    spin_unlock_irqrestore(&plist_lock, flags);

    receiver = vf_get_receiver(prov->name);
    if(receiver && receiver->ops && receiver->ops->event_cb){
        receiver->ops->event_cb(VFRAME_EVENT_PROVIDER_REG, prov->name, receiver->op_arg);
    }
    
    return 0;
}
EXPORT_SYMBOL(vf_reg_provider);

void vf_unreg_provider(struct vframe_provider_s *prov)
{
    ulong flags;
    ulong fiq_flag;
    vframe_receiver_t* receiver = NULL;
    struct vframe_provider_s *p, *ptmp;
    if(prov == NULL){
        pr_err("%s fails: input prov in NULL\n", __func__);
        return;
    }
    list_for_each_entry_safe(p, ptmp, &phead, list) {
        if (!strcmp(p->name, prov->name)) {
            spin_lock_irqsave(&plist_lock, flags);
                raw_local_save_flags(fiq_flag);
                local_fiq_disable();
                    list_del(&prov->list);
                raw_local_irq_restore(fiq_flag);
            spin_unlock_irqrestore(&plist_lock, flags);

            receiver = vf_get_receiver(p->name);
            if(receiver && receiver->ops && receiver->ops->event_cb){
                receiver->ops->event_cb(VFRAME_EVENT_PROVIDER_UNREG, NULL, receiver->op_arg);
            }
            break;
        }
    }
}
EXPORT_SYMBOL(vf_unreg_provider);

void vf_light_unreg_provider(struct vframe_provider_s *prov)
{
    ulong flags;
    vframe_receiver_t* receiver = NULL;
    struct vframe_provider_s *p, *ptmp;
    list_for_each_entry_safe(p, ptmp, &phead, list) {
        if (!strcmp(p->name, prov->name)) {
            spin_lock_irqsave(&plist_lock, flags);
            spin_unlock_irqrestore(&plist_lock, flags);
            pr_info("%s:%s\n", __func__, p->name);
            receiver = vf_get_receiver(p->name);
            if(receiver && receiver->ops && receiver->ops->event_cb){
                receiver->ops->event_cb(VFRAME_EVENT_PROVIDER_LIGHT_UNREG, NULL, receiver->op_arg);
            }
            break;
        }
    }
}
EXPORT_SYMBOL(vf_light_unreg_provider);

void vf_ext_light_unreg_provider(struct vframe_provider_s *prov)
{
    ulong flags;
    ulong fiq_flag;
    vframe_receiver_t* receiver = NULL;
    struct vframe_provider_s *p, *ptmp;
    list_for_each_entry_safe(p, ptmp, &phead, list) {
        if (!strcmp(p->name, prov->name)) {
            spin_lock_irqsave(&plist_lock, flags);
                raw_local_save_flags(fiq_flag);
                local_fiq_disable();
                    list_del(&prov->list);
                raw_local_irq_restore(fiq_flag);
            spin_unlock_irqrestore(&plist_lock, flags);
            pr_info("%s:%s\n", __func__, p->name);
            receiver = vf_get_receiver(p->name);
            if(receiver && receiver->ops && receiver->ops->event_cb){
                receiver->ops->event_cb(VFRAME_EVENT_PROVIDER_LIGHT_UNREG, NULL, receiver->op_arg);
            }
            break;
        }
    }
}
EXPORT_SYMBOL(vf_ext_light_unreg_provider);

struct vframe_s *vf_peek(const char* receiver)
{
    struct vframe_provider_s *vfp;
    vfp = vf_get_provider(receiver);
    if (!(vfp && vfp->ops && vfp->ops->peek))
        return NULL;
    return vfp->ops->peek(vfp->op_arg);
}
EXPORT_SYMBOL(vf_peek);

struct vframe_s *vf_get(const char* receiver)
{
    struct vframe_provider_s *vfp;
    vfp = vf_get_provider(receiver);
    if (!(vfp && vfp->ops && vfp->ops->get))
        return NULL;
    return vfp->ops->get(vfp->op_arg);
}
EXPORT_SYMBOL(vf_get);

void vf_put(struct vframe_s *vf, const char *receiver)
{
    struct vframe_provider_s *vfp;
    vfp = vf_get_provider(receiver);
    if (!(vfp && vfp->ops && vfp->ops->put))
        return;
    vfp->ops->put(vf, vfp->op_arg);
}
EXPORT_SYMBOL(vf_put);



