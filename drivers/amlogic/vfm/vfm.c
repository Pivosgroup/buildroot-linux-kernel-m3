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
#include <linux/types.h>
#include <linux/init.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/device.h>
#include <linux/string.h>
#include <linux/slab.h>
#include <linux/list.h>

/* Amlogic headers */
#include <linux/amports/vframe.h>
#include <linux/amports/vframe_provider.h>
#include <linux/amports/vframe_receiver.h>

/* Local headers */
#include "vfm.h"

#define DRV_NAME    "vfm"
#define DEV_NAME    "vfm"
#define BUS_NAME    "vfm"
#define CLS_NAME    "vfm"

#define VFM_NAME_LEN    100
#define VFM_MAP_SIZE    10
#define VFM_MAP_COUNT   20
typedef struct{
    char id[VFM_NAME_LEN];
    char name[VFM_MAP_SIZE][VFM_NAME_LEN];
    int vfm_map_size;
}vfm_map_t;

vfm_map_t* vfm_map[VFM_MAP_COUNT];

int vfm_mode = 1; //new mode

static DEFINE_SPINLOCK(vfm_lock);

static int get_vfm_map_index(const char* id)
{
    int index = -1;
    int i;
    for (i = 0; i < VFM_MAP_COUNT; i++){
        if (vfm_map[i] && (!strcmp(vfm_map[i]->id, id))){
            index = i;
            break;
        }
    }
    return index;
}

static int vfm_map_remove_by_index(int index)
{
    ulong flags;
    int i;
    int ret = 0;
    vfm_map_t *p;
    struct vframe_provider_s *vfp;
    for (i = 0; i < vfm_map[index]->vfm_map_size; i++) {
        if( (i < (vfm_map[index]->vfm_map_size - 1)) &&
            is_provider_active(vfm_map[index]->name[i])) {

            vfp = vf_get_provider(vfm_map[index]->name[i+1]);
            if(vfp && vfp->ops && vfp->ops->event_cb){
                vfp->ops->event_cb(VFRAME_EVENT_RECEIVER_FORCE_UNREG, NULL, vfp->op_arg);
                printk("%s: VFRAME_EVENT_RECEIVER_FORCE_UNREG %s\n", __func__, vfm_map[index]->name[i]);
            }
        }
    }

    for (i = 0; i < vfm_map[index]->vfm_map_size; i++) {
        if( (i < (vfm_map[index]->vfm_map_size - 1)) &&
            is_provider_active(vfm_map[index]->name[i])) {
            break;
        }
    }
    if (i == vfm_map[index]->vfm_map_size) {
    	p = vfm_map[index];
        vfm_map[index] = NULL;
        kfree(p);
    }
    else {
        pr_err("failed to remove vfm map %s with active provider %s.\n",
            vfm_map[index]->id, vfm_map[index]->name[i]);
        ret = -1;
    }
    return ret;

}

static int vfm_map_remove(char* id)
{
    int i;
    int index;
    int ret = 0;
    if (!strcmp(id, "all")) {
        for (i = 0; i < VFM_MAP_COUNT; i++) {
            if (vfm_map[i]) {
                ret = vfm_map_remove_by_index(i);
            }
        }
    }
    else{
        index = get_vfm_map_index(id);
        if (index >= 0) {
            ret = vfm_map_remove_by_index(index);
        }
    }
    return ret;
}

static int vfm_map_add(char* id,   char* name_chain)
{
    int i,j;
    int ret = -1;
    char* ptr, *token;
    vfm_map_t *p;
    for (i = 0; i < VFM_MAP_COUNT; i++) {
        if (vfm_map[i] == NULL){
            break;
        }
    }
    /* avoid to add the path with the same path name */
    for (j = 0; j < VFM_MAP_COUNT; j++) {
        if (vfm_map[j] != NULL){
            if (!strcmp(vfm_map[j]->name, id)) {

                pr_err("[vfm] failed to add the path due to same path name\n");
                return ret;
            }
        }
    }
    if (i < VFM_MAP_COUNT) {

        p = kmalloc(sizeof(vfm_map_t), GFP_KERNEL);
        if (p) {
            memset(p, 0, sizeof(vfm_map_t));
            memcpy(p->id, id, strlen(id));
            ptr = name_chain;
            while (1) {
                token = strsep(&ptr, " \n");
                if (token == NULL)
                    break;
                if (*token == '\0')
                    continue;
                memcpy(p->name[p->vfm_map_size],
                    token, strlen(token));
                p->vfm_map_size++;
            }
            ret = 0;
            vfm_map[i] = p;
        }
    }
    return ret;

}

char* vf_get_provider_name(const char* receiver_name)
{
    ulong flags = 0;
    int i,j;
    char* provider_name = NULL;
    unsigned char receiver_is_amvideo = 0;
    if((!strcmp(receiver_name, "amvideo"))||(!strcmp(receiver_name, "amvideo2"))){
        receiver_is_amvideo = 1; //do not call spin_lock_irqsave in fiq
    }
    if(receiver_is_amvideo==0){
        spin_lock_irqsave(&vfm_lock, flags);
    }

    for (i = 0; i < VFM_MAP_COUNT; i++) {
        if (vfm_map[i]) {
            for (j = 0; j < vfm_map[i]->vfm_map_size; j++) {
                if (!strcmp(vfm_map[i]->name[j], receiver_name)) {
                    if (j > 0 &&
                        is_provider_active(vfm_map[i]->name[j - 1])) {
                        provider_name = vfm_map[i]->name[j - 1];
                    }
                    break;
                }
            }
        }
        if (provider_name)
            break;
    }
    if(receiver_is_amvideo==0){
        spin_unlock_irqrestore(&vfm_lock, flags);
    }
    return provider_name;
}

char* vf_get_receiver_name(const char* provider_name)
{
    ulong flags;
    int i,j;
    char* receiver_name = NULL;
    int namelen;
    int provide_namelen = strlen(provider_name);

    spin_lock_irqsave(&vfm_lock, flags);

    for (i = 0; i < VFM_MAP_COUNT; i++) {
        if (vfm_map[i]){
            for (j = 0; j < vfm_map[i]->vfm_map_size; j++){
                namelen = strlen(vfm_map[i]->name[j]);
                if (!strncmp(vfm_map[i]->name[j], provider_name, namelen)) {
                    if (namelen == provide_namelen ||
                        provider_name[namelen] == '.') {
                        if ((j + 1) < vfm_map[i]->vfm_map_size &&
                            is_receiver_active(vfm_map[i]->name[j + 1])) {
                            receiver_name = vfm_map[i]->name[j + 1];
                        }
                        break;
                    }
                }
            }
        }
        if (receiver_name)
            break;
    }
    spin_unlock_irqrestore(&vfm_lock, flags);

    return receiver_name;
}

static void vfm_init(void)
{
#ifdef CONFIG_POST_PROCESS_MANAGER
    char def_id[] = "default";
    char def_name_chain[] = "decoder ppmgr amvideo";
#else
    char def_id[] = "default";
    char def_name_chain[] = "decoder amvideo";
#endif    
#ifdef CONFIG_AM_VIDEO2
    char def_ext_id[] = "default_ext";
    char def_ext_name_chain[] = "vdin amvideo2";
#else
#ifdef CONFIG_AMLOGIC_VIDEOIN_MANAGER
    char def_ext_id[] = "default_ext";
    char def_ext_name_chain[] = "vdin vm amvideo";
#endif    
#endif
    char def_osd_id[] = "default_osd";
    char def_osd_name_chain[] = "osd amvideo";
    
    int i;
    for(i=0; i<VFM_MAP_COUNT; i++){
        vfm_map[i] = NULL;
    }
    vfm_map_add(def_id, def_name_chain);
    vfm_map_add(def_osd_id, def_osd_name_chain);
#ifdef CONFIG_AMLOGIC_VIDEOIN_MANAGER
    vfm_map_add(def_ext_id, def_ext_name_chain);
#endif        

}

/*
 * cat /sys/class/vfm/map
*/
static ssize_t vfm_map_show(struct class *class,
    struct class_attribute *attr, char *buf)
{
    int i,j;
    int len = 0;

    for (i = 0; i < VFM_MAP_COUNT; i++){
        if (vfm_map[i]){
            len += sprintf(buf+len, "%s { ", vfm_map[i]->id);
            for (j = 0; j < vfm_map[i]->vfm_map_size; j++){
                if (j < (vfm_map[i]->vfm_map_size-1)){
                    len += sprintf(buf+len, "%s(%d) ", vfm_map[i]->name[j],
                        is_provider_active(vfm_map[i]->name[j]));
                }
                else{
                    len += sprintf(buf+len, "%s", vfm_map[i]->name[j]);
                }
            }
            len += sprintf(buf+len, "}\n");
        } 
    }
    provider_list();
    receiver_list();
    return len;
}

#define VFM_CMD_ADD 1
#define VFM_CMD_RM  2

/*
 * echo add <name> <node1 node2 ...> > /sys/class/vfm/map
 * echo rm <name>                    > /sys/class/vfm/map
 * echo rm all                       > /sys/class/vfm/map
 *
 * <name> the name of the path.
 * <node1 node2 ...> the name of the nodes in the path.
*/
static ssize_t vfm_map_store(struct class *class,
    struct class_attribute *attr, const char *buf, size_t count)
{
    char *buf_orig, *ps, *token;
    int i = 0;
    int cmd = 0;
    char* id = NULL;

	buf_orig = kstrdup(buf, GFP_KERNEL);
	ps = buf_orig;
	while (1) {
		token = strsep(&ps, " \n");
		if (token == NULL)
			break;
		if (*token == '\0')
			continue;
	  if (i == 0){ //command
	      if (!strcmp(token, "add")){

            cmd = VFM_CMD_ADD;
	      }
	      else if (!strcmp(token, "rm")){
            cmd = VFM_CMD_RM;
	      }
	      else{
	        break;
	      }
	  }
	  else if (i == 1){
	      id = token;
    	    if (cmd == VFM_CMD_ADD){
    	        //printk("vfm_map_add(%s,%s)\n",id,ps);
    	        vfm_map_add(id,  ps);
    	    }
    	    else if (cmd == VFM_CMD_RM){
    	        //printk("vfm_map_remove(%s)\n",id);
    	        if(vfm_map_remove(id)<0){
    	            count = 0;
    	        }
    	    }
    	    break;
	  }
    i++;
	}
	kfree(buf_orig);
	return count;
}

static CLASS_ATTR(map, 0644, vfm_map_show, vfm_map_store);

static struct class vfm_class = {
	.name = CLS_NAME,
};

static int __init vfm_class_init(void)
{
	int error;

	vfm_init();

	error = class_register(&vfm_class);
	if (error) {
		printk(KERN_ERR "%s: class_register failed\n", __func__);
		return error;
	}
	error = class_create_file(&vfm_class, &class_attr_map);
	if (error) {
		printk(KERN_ERR "%s: class_create_file failed\n", __func__);
		class_unregister(&vfm_class);
	}

	return error;

}
static void __exit vfm_class_exit(void)
{
	class_unregister(&vfm_class);
}

fs_initcall(vfm_class_init);
module_exit(vfm_class_exit);

MODULE_PARM_DESC(vfm_mode, "\n vfm_mode \n");
module_param(vfm_mode, int, 0664);

MODULE_DESCRIPTION("Amlogic video frame manager driver");
MODULE_LICENSE("GPL");
MODULE_AUTHOR("Bobby Yang <bo.yang@amlogic.com>");


