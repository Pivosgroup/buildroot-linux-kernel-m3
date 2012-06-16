//-----------------------------------------------------------------------------
// $Header: 
// (C) Copyright 2001 NXP Semiconductors, All rights reserved
//
// This source code and any compilation or derivative thereof is the sole
// property of NXP Corporation and is provided pursuant to a Software
// License Agreement.  This code is the proprietary information of NXP
// Corporation and is confidential in nature.  Its use and dissemination by
// any party other than NXP Corporation is strictly limited by the
// confidential information provisions of the Agreement referenced above.
//-----------------------------------------------------------------------------
// FILE NAME:    tmbslNT220xInstance.c
//
// DESCRIPTION:  define the static Objects
//
// DOCUMENT REF: DVP Software Coding Guidelines v1.14
//               DVP Board Support Library Architecture Specification v0.5
//
// NOTES:        
//-----------------------------------------------------------------------------
//

#include "tmNxTypes.h"
#include "tmCompId.h"
#include "tmFrontEnd.h"
#include "tmUnitParams.h"
#include "tmbslFrontEndTypes.h"

#ifdef TMBSL_NT220HN
 #include "tmbslNT220HN.h"
#else /* TMBSL_NT220HN */
 #include "tmbslNT220DN.h"
#endif /* TMBSL_NT220HN */

#include "tmbslNT220xlocal.h"
#include "tmbslNT220xInstance.h"
#include "tmbslNT220x_InstanceCustom.h"

//-----------------------------------------------------------------------------
// Global data:
//-----------------------------------------------------------------------------
//
//
// default instance
tmNT220xObject_t gNT220xInstance[] = 
{
    {
        (tmUnitSelect_t)(-1),                                   /* tUnit */
        (tmUnitSelect_t)(-1),                                   /* tUnit temporary */
        Null,                                                   /* pMutex */
        False,                                                  /* init (instance initialization default) */
        {                                                       /* sRWFunc */
            Null,
            Null
        },
        {                                                       /* sTime */
            Null,
            Null
        },
        {                                                       /* sDebug */
            Null
        },
        {                                                       /* sMutex */
            Null,
            Null,
            Null,
            Null
        },
#ifdef TMBSL_NT220HN
TMBSL_NT220x_INSTANCE_CUSTOM_MODE_PATH0
            {
TMBSL_NT220x_INSTANCE_CUSTOM_STD_DIGITAL_SELECTION_PATH0
TMBSL_NT220x_INSTANCE_CUSTOM_STD_ANALOG_SELECTION_PATH0
            }
#else
TMBSL_NT220x_INSTANCE_CUSTOM_MODE_PATH0
            {
TMBSL_NT220x_INSTANCE_CUSTOM_STD_DIGITAL_SELECTION_PATH0
            }
#endif
    },
    {
        (tmUnitSelect_t)(-1),                                   /* tUnit */
        (tmUnitSelect_t)(-1),                                   /* tUnit temporary */
        Null,                                                   /* pMutex */
        False,                                                  /* init (instance initialization default) */
        {                                                       /* sRWFunc */
            Null,
            Null
        },
        {                                                       /* sTime */
            Null,
            Null
        },
        {                                                       /* sDebug */
            Null
        },
        {                                                       /* sMutex */
            Null,
            Null,
            Null,
            Null
        },
#ifdef TMBSL_NT220HN
TMBSL_NT220x_INSTANCE_CUSTOM_MODE_PATH0
            {
TMBSL_NT220x_INSTANCE_CUSTOM_STD_DIGITAL_SELECTION_PATH0
TMBSL_NT220x_INSTANCE_CUSTOM_STD_ANALOG_SELECTION_PATH0
            }
#else
TMBSL_NT220x_INSTANCE_CUSTOM_MODE_PATH0
            {
TMBSL_NT220x_INSTANCE_CUSTOM_STD_DIGITAL_SELECTION_PATH0
            }
#endif
    },
    {
        (tmUnitSelect_t)(-1),                                   /* tUnit */
        (tmUnitSelect_t)(-1),                                   /* tUnit temporary */
        Null,                                                   /* pMutex */
        False,                                                  /* init (instance initialization default) */
        {                                                       /* sRWFunc */
            Null,
            Null
        },
        {                                                       /* sTime */
            Null,
            Null
        },
        {                                                       /* sDebug */
            Null
        },
        {                                                       /* sMutex */
            Null,
            Null,
            Null,
            Null
        },
#ifdef TMBSL_NT220HN
TMBSL_NT220x_INSTANCE_CUSTOM_MODE_PATH0
            {
TMBSL_NT220x_INSTANCE_CUSTOM_STD_DIGITAL_SELECTION_PATH0
TMBSL_NT220x_INSTANCE_CUSTOM_STD_ANALOG_SELECTION_PATH0
            }
#else
TMBSL_NT220x_INSTANCE_CUSTOM_MODE_PATH0
            {
TMBSL_NT220x_INSTANCE_CUSTOM_STD_DIGITAL_SELECTION_PATH0
            }
#endif
    },
    {
        (tmUnitSelect_t)(-1),                                   /* tUnit */
        (tmUnitSelect_t)(-1),                                   /* tUnit temporary */
        Null,                                                   /* pMutex */
        False,                                                  /* init (instance initialization default) */
        {                                                       /* sRWFunc */
            Null,
            Null
        },
        {                                                       /* sTime */
            Null,
            Null
        },
        {                                                       /* sDebug */
            Null
        },
        {                                                       /* sMutex */
            Null,
            Null,
            Null,
            Null
        },
#ifdef TMBSL_NT220HN
TMBSL_NT220x_INSTANCE_CUSTOM_MODE_PATH1
        {
TMBSL_NT220x_INSTANCE_CUSTOM_STD_DIGITAL_SELECTION_PATH1
TMBSL_NT220x_INSTANCE_CUSTOM_STD_ANALOG_SELECTION_PATH1
        }
#else
TMBSL_NT220x_INSTANCE_CUSTOM_MODE_PATH1
        {
TMBSL_NT220x_INSTANCE_CUSTOM_STD_DIGITAL_SELECTION_PATH1
        }
#endif
    }
};


//-----------------------------------------------------------------------------
// FUNCTION:    NT220xAllocInstance:
//
// DESCRIPTION: allocate new instance
//
// RETURN:      
//
// NOTES:       
//-----------------------------------------------------------------------------
//
tmErrorCode_t
NT220xAllocInstance
(
    tmUnitSelect_t          tUnit,      /* I: Unit number */
    pptmNT220xObject_t    ppDrvObject /* I: Device Object */
)
{ 
    tmErrorCode_t       err = NT220x_ERR_BAD_UNIT_NUMBER;
    ptmNT220xObject_t pObj = Null;
    UInt32              uLoopCounter = 0;    

    /* Find a free instance */
    for(uLoopCounter = 0; uLoopCounter<NT220x_MAX_UNITS; uLoopCounter++)
    {
        pObj = &gNT220xInstance[uLoopCounter];
        if(pObj->init == False)
        {
            pObj->tUnit = tUnit;
            pObj->tUnitW = tUnit;

            *ppDrvObject = pObj;
            err = TM_OK;
            break;
        }
    }

    /* return value */
    return err;
}

//-----------------------------------------------------------------------------
// FUNCTION:    NT220xDeAllocInstance:
//
// DESCRIPTION: deallocate instance
//
// RETURN:      always TM_OK
//
// NOTES:       
//-----------------------------------------------------------------------------
//
tmErrorCode_t
NT220xDeAllocInstance
(
    tmUnitSelect_t  tUnit   /* I: Unit number */
)
{     
    tmErrorCode_t       err = NT220x_ERR_BAD_UNIT_NUMBER;
    ptmNT220xObject_t pObj = Null;

    /* check input parameters */
    err = NT220xGetInstance(tUnit, &pObj);

    /* check driver state */
    if (err == TM_OK)
    {
        if (pObj == Null || pObj->init == False)
        {
            err = NT220x_ERR_NOT_INITIALIZED;
        }
    }

    if ((err == TM_OK) && (pObj != Null)) 
    {
        pObj->init = False;
    }

    /* return value */
    return err;
}

//-----------------------------------------------------------------------------
// FUNCTION:    NT220xGetInstance:
//
// DESCRIPTION: get the instance
//
// RETURN:      always True
//
// NOTES:       
//-----------------------------------------------------------------------------
//
tmErrorCode_t
NT220xGetInstance
(
    tmUnitSelect_t          tUnit,      /* I: Unit number */
    pptmNT220xObject_t    ppDrvObject /* I: Device Object */
)
{     
    tmErrorCode_t       err = NT220x_ERR_NOT_INITIALIZED;
    ptmNT220xObject_t pObj = Null;
    UInt32              uLoopCounter = 0;    

    /* get instance */
    for(uLoopCounter = 0; uLoopCounter<NT220x_MAX_UNITS; uLoopCounter++)
    {
        pObj = &gNT220xInstance[uLoopCounter];
        if(pObj->init == True && pObj->tUnit == GET_INDEX_TYPE_TUNIT(tUnit))
        {
            pObj->tUnitW = tUnit;
            
            *ppDrvObject = pObj;
            err = TM_OK;
            break;
        }
    }

    /* return value */
    return err;
}

