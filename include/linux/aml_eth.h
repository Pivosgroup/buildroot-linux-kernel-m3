/*
 * Author: AMLOGIC, Inc.
 * Copyright (C) 2010 Amlogic Inc.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */


#ifndef __AML_ETH_H
#define __AML_ETH_H

struct aml_eth_platform_data {
    void (*clock_enable)(int);
    void (*reset)(void);
};

#endif /* __AML_ETH_H */

