/*
* TVIN Notifier
*
* Author: Bobby Yang <bo.yang@amlogic.com>
*
* Copyright (C) 2010 Amlogic Inc.
*
* This program is free software; you can redistribute it and/or modify
* it under the terms of the GNU General Public License version 2 as
* published by the Free Software Foundation.
*/

#include<linux/module.h>

#include "tvin_notifier.h"


/*
* source dec operations notifier
*/

static BLOCKING_NOTIFIER_HEAD(tvin_dec_notifier_list);


int tvin_dec_notifier_register(struct notifier_block *nb)
{
    return blocking_notifier_chain_register(&tvin_dec_notifier_list, nb);
}
EXPORT_SYMBOL_GPL(tvin_dec_notifier_register);


int tvin_dec_notifier_unregister(struct notifier_block *nb)
{
    return blocking_notifier_chain_unregister(&tvin_dec_notifier_list, nb);
}
EXPORT_SYMBOL_GPL(tvin_dec_notifier_unregister);


int tvin_dec_notifier_call(unsigned long val, void *v)
{
    return blocking_notifier_call_chain(&tvin_dec_notifier_list, val, v);
}
EXPORT_SYMBOL_GPL(tvin_dec_notifier_call);



static ATOMIC_NOTIFIER_HEAD(tvin_check_notifier_list);


int tvin_check_notifier_register(struct notifier_block *nb)
{
    return atomic_notifier_chain_register(&tvin_check_notifier_list, nb);
}
EXPORT_SYMBOL_GPL(tvin_check_notifier_register);


int tvin_check_notifier_unregister(struct notifier_block *nb)
{
    return atomic_notifier_chain_unregister(&tvin_check_notifier_list, nb);
}
EXPORT_SYMBOL_GPL(tvin_check_notifier_unregister);


int tvin_check_notifier_call(unsigned long val, void *v)
{
    return atomic_notifier_call_chain(&tvin_check_notifier_list, val, v);
}
EXPORT_SYMBOL_GPL(tvin_check_notifier_call);




