/*
  Copyright (C) 2006-2009 NXP B.V., All Rights Reserved.
  This source code and any compilation or derivative thereof is the proprietary
  information of NXP B.V. and is confidential in nature. Under no circumstances
  is this software to be  exposed to or placed under an Open Source License of
  any type without the expressed written permission of NXP B.V.
 *
 * \file          tmddNT220x.c
 *
 *                3
 *
 * \date          %modify_time%
 *
 * \brief         Describe briefly the purpose of this file.
 *
 * REFERENCE DOCUMENTS :
 *                TDA18254_Driver_User_Guide.pdf
 *
 * Detailed description may be added here.
 *
 * \section info Change Information
 *
*/

/*============================================================================*/
/* Standard include files:                                                    */
/*============================================================================*/
#include "tmNxTypes.h"
#include "tmCompId.h"
#include "tmFrontEnd.h"
#include "tmbslFrontEndTypes.h"
#include "tmUnitParams.h"

/*============================================================================*/
/* Project include files:                                                     */
/*============================================================================*/
#include "tmddNT220x.h"
#include "tmddNT220xlocal.h"

#include "tmddNT220xInstance.h"

/*============================================================================*/
/* Types and defines:                                                         */
/*============================================================================*/

/*============================================================================*/
/* Global data:                                                               */
/*============================================================================*/

/*============================================================================*/
/* Internal Prototypes:                                                       */
/*============================================================================*/

/*============================================================================*/
/* Exported functions:                                                        */
/*============================================================================*/


/*============================================================================*/
/* FUNCTION:    tmddNT220xInit                                              */
/*                                                                            */
/* DESCRIPTION: Create an instance of a NT220x Tuner                        */
/*                                                                            */
/* RETURN:      TM_OK if no error                                             */
/*                                                                            */
/* NOTES:                                                                     */
/*                                                                            */
/*============================================================================*/
tmErrorCode_t
tmddNT220xInit
(
    tmUnitSelect_t              tUnit,      /* I: Unit number */
    tmbslFrontEndDependency_t*  psSrvFunc   /* I: setup parameters */
)
{
    ptmddNT220xObject_t   pObj = Null;
    tmErrorCode_t           err  = TM_OK;

    if (psSrvFunc == Null)
    {
        err = ddNT220x_ERR_BAD_PARAMETER;
    }

    /* Get Instance Object */
    if(err == TM_OK)
    {
        err = ddNT220xGetInstance(tUnit, &pObj);
    }

    /* Check driver state */
    if (err == TM_OK || err == ddNT220x_ERR_NOT_INITIALIZED)
    {
        if (pObj != Null && pObj->init == True)
        {
            err = ddNT220x_ERR_NOT_INITIALIZED;
        }
        else 
        {
            /* Allocate the Instance Object */
            if (pObj == Null)
            {
                err = ddNT220xAllocInstance(tUnit, &pObj);
                if (err != TM_OK || pObj == Null)
                {
                    err = ddNT220x_ERR_NOT_INITIALIZED;        
                }
            }

            if(err == TM_OK)
            {
                /* initialize the Instance Object */
                pObj->sRWFunc = psSrvFunc->sIo;
                pObj->sTime = psSrvFunc->sTime;
                pObj->sDebug = psSrvFunc->sDebug;

                if(  psSrvFunc->sMutex.Init != Null
                    && psSrvFunc->sMutex.DeInit != Null
                    && psSrvFunc->sMutex.Acquire != Null
                    && psSrvFunc->sMutex.Release != Null)
                {
                    pObj->sMutex = psSrvFunc->sMutex;

                    err = pObj->sMutex.Init(&pObj->pMutex);
                }

                pObj->init = True;
                err = TM_OK;
            }
        }
    }

    return err;
}

/*============================================================================*/
/* FUNCTION:    tmddNT220xDeInit                                            */
/*                                                                            */
/* DESCRIPTION: Destroy an instance of a NT220x Tuner                       */
/*                                                                            */
/* RETURN:      TM_OK if no error                                             */
/*                                                                            */
/* NOTES:                                                                     */
/*                                                                            */
/*============================================================================*/
tmErrorCode_t 
tmddNT220xDeInit
(
    tmUnitSelect_t  tUnit   /* I: Unit number */
)
{
    tmErrorCode_t           err = TM_OK;
    ptmddNT220xObject_t   pObj = Null;

    /* Get Instance Object */
    err = ddNT220xGetInstance(tUnit, &pObj);

    tmDBGPRINTEx(DEBUGLVL_VERBOSE, "tmddNT220xDeInit(0x%08X)", tUnit);

    if(err == TM_OK)
    {
        if(pObj->sMutex.DeInit != Null)
        {
            if(pObj->pMutex != Null)
            {
                err = pObj->sMutex.DeInit(pObj->pMutex);
            }

            pObj->sMutex.Init = Null;
            pObj->sMutex.DeInit = Null;
            pObj->sMutex.Acquire = Null;
            pObj->sMutex.Release = Null;

            pObj->pMutex = Null;
        }
    }

    err = ddNT220xDeAllocInstance(tUnit);

    return err;
}

/*============================================================================*/
/* FUNCTION:    tmddNT220xGetSWVersion                                      */
/*                                                                            */
/* DESCRIPTION: Return the version of this device                             */
/*                                                                            */
/* RETURN:      TM_OK if no error                                             */
/*                                                                            */
/* NOTES:       Values defined in the tmddNT220xlocal.h file                */
/*                                                                            */
/*============================================================================*/
tmErrorCode_t
tmddNT220xGetSWVersion
(
    ptmSWVersion_t  pSWVersion  /* I: Receives SW Version */
)
{
    pSWVersion->compatibilityNr = NT220x_DD_COMP_NUM;
    pSWVersion->majorVersionNr  = NT220x_DD_MAJOR_VER;
    pSWVersion->minorVersionNr  = NT220x_DD_MINOR_VER;

    return TM_OK;
}

/*============================================================================*/
/* FUNCTION:    tmddNT220xReset                                             */
/*                                                                            */
/* DESCRIPTION: Initialize NT220x Hardware                                  */
/*                                                                            */
/* RETURN:      TM_OK if no error                                             */
/*                                                                            */
/* NOTES:                                                                     */
/*                                                                            */
/*============================================================================*/
tmErrorCode_t
tmddNT220xReset
(
    tmUnitSelect_t  tUnit   /* I: Unit number */
)
{   
    ptmddNT220xObject_t   pObj = Null;
    tmErrorCode_t           err = TM_OK;

    /* Get Instance Object */
    err = ddNT220xGetInstance(tUnit, &pObj);
    tmASSERTExT(err, TM_OK, (DEBUGLVL_ERROR, "ddNT220xGetInstance(0x%08X) failed.", tUnit));

    if(err == TM_OK)
    {
        err = ddNT220xMutexAcquire(pObj, ddNT220x_MUTEX_TIMEOUT);
    }

    if(err == TM_OK)
    {
        /****** I2C map initialization : begin *********/
        if(err == TM_OK)
        {
            /* read all bytes */
            err = ddNT220xRead(pObj, 0x00, NT220x_I2C_MAP_NB_BYTES);
            tmASSERTExT(err, TM_OK, (DEBUGLVL_ERROR, "ddNT220xRead(0x%08X) failed.", tUnit));
        }
        if(err == TM_OK)
        {
            /* RSSI_Ck_Speed    31,25 kHz   0 */
            err = tmddNT220xSetRSSI_Ck_Speed(tUnit, 0);
            tmASSERTExT(err, TM_OK, (DEBUGLVL_ERROR, "tmddNT220xSetRSSI_Ck_Speed(0x%08X, 0) failed.", tUnit));
        }
        if(err == TM_OK)
        {
            /* AGC1_Do_step    8,176 ms   2 */
            err = tmddNT220xSetAGC1_Do_step(tUnit, 2);
            tmASSERTExT(err, TM_OK, (DEBUGLVL_ERROR, "tmddNT220xSetAGC1_Do_step(0x%08X, 2) failed.", tUnit));
        }
        if(err == TM_OK)
        {
            /* AGC2_Do_step    8,176 ms   1 */
            err = tmddNT220xSetAGC2_Do_step(tUnit, 1);
            tmASSERTExT(err, TM_OK, (DEBUGLVL_ERROR, "tmddNT220xSetAGC2_Do_step(0x%08X, 1) failed.", tUnit));
        }
        if(err == TM_OK)
        {
            /* AGCs_Up_Step_assym       UP 12 Asym / 4 Asym  / 5 Asym   3 */
            err = tmddNT220xSetAGCs_Up_Step_assym(tUnit, 3);
            tmASSERTExT(err, TM_OK, (DEBUGLVL_ERROR, "tmddNT220xSetAGCs_Up_Step_assym(0x%08X, 3) failed.", tUnit));
        }
        if(err == TM_OK)
        {
            /* AGCs_Do_Step_assym       DO 12 Asym / 45 Sym   2 */
            err = tmddNT220xSetAGCs_Do_Step_assym(tUnit, 2);
            tmASSERTExT(err, TM_OK, (DEBUGLVL_ERROR, "tmddNT220xSetAGCs_Do_Step_assym(0x%08X, 2) failed.", tUnit));
        }
        /****** I2C map initialization : end *********/

        /*****************************************/
        /* Launch tuner calibration */
        /* State reached after 1.5 s max */
        if(err == TM_OK)
        {
            /* set IRQ_clear */
            err = tmddNT220xSetIRQ_Clear(tUnit, 0x1F);
            tmASSERTExT(err, TM_OK, (DEBUGLVL_ERROR, "tmddNT220xSetIRQ_clear(0x%08X, 0x1F) failed.", tUnit));
        }

        if(err == TM_OK)
        {
            /* set power state on */
            err = tmddNT220xSetPowerState(tUnit, tmddNT220x_PowerNormalMode);
            tmASSERTExT(err, TM_OK, (DEBUGLVL_ERROR, "tmddNT220xSetPowerState(0x%08X, PowerNormalMode) failed.", tUnit));
        }

        if(err == TM_OK)
        {
            /* set & trigger MSM */
            pObj->I2CMap.uBx19.MSM_byte_1 = 0x3B;
            pObj->I2CMap.uBx1A.MSM_byte_2 = 0x01;

            /* write bytes 0x19 to 0x1A */
            err = ddNT220xWrite(pObj, 0x19, 2);
            tmASSERTExT(err, TM_OK, (DEBUGLVL_ERROR, "ddNT220xWrite(0x%08X) failed.", tUnit));

            pObj->I2CMap.uBx1A.MSM_byte_2 = 0x00;

        }

        if(pObj->bIRQWait)
        {
            if(err == TM_OK)
            {
                err = ddNT220xWaitIRQ(pObj, 1500, 50, 0x1F);
                tmASSERTExT(err, TM_OK, (DEBUGLVL_ERROR, "ddNT220xWaitIRQ(0x%08X) failed.", tUnit));
            }
        }

        (void)ddNT220xMutexRelease(pObj);
    }

    return err;
}
/*============================================================================*/
/* FUNCTION:    tmddNT220xSetLPF_Gain_Mode                                  */
/*                                                                            */
/* DESCRIPTION: Free/Freeze LPF Gain                                          */
/*                                                                            */
/* RETURN:      TM_OK if no error                                             */
/*                                                                            */
/* NOTES:                                                                     */
/*                                                                            */
/*============================================================================*/
tmErrorCode_t
tmddNT220xSetLPF_Gain_Mode
(
    tmUnitSelect_t  tUnit,  /* I: Unit number */
    UInt8           uMode   /* I: Unknown/Free/Frozen */
)
{
    ptmddNT220xObject_t   pObj = Null;
    tmErrorCode_t           err  = TM_OK;

    /* Get Instance Object */
    err = ddNT220xGetInstance(tUnit, &pObj);
    tmASSERTExT(err, TM_OK, (DEBUGLVL_ERROR, "ddNT220xGetInstance(0x%08X) failed.", tUnit));

    if(err == TM_OK)
    {
        err = ddNT220xMutexAcquire(pObj, ddNT220x_MUTEX_TIMEOUT);
    }

    if(err == TM_OK)
    {
        switch(uMode)
        {
        case tmddNT220x_LPF_Gain_Unknown:
        default:
            tmASSERTExT(err, TM_OK, (DEBUGLVL_ERROR, "tmddNT220xSetLPF_Gain_Free(0x%08X, tmddNT220x_LPF_Gain_Unknown).", tUnit));
            break;

        case tmddNT220x_LPF_Gain_Free:
            err = tmddNT220xSetAGC5_loop_off(tUnit, False); /* Disable AGC5 loop off */
            tmASSERTExT(err, TM_OK, (DEBUGLVL_ERROR, "tmddNT220xSetAGC5_loop_off(0x%08X) failed.", tUnit));

            if(err == TM_OK)
            {
                err = tmddNT220xSetForce_AGC5_gain(tUnit, False); /* Do not force AGC5 gain */
                tmASSERTExT(err, TM_OK, (DEBUGLVL_ERROR, "tmddNT220xSetForce_AGC5_gain(0x%08X) failed.", tUnit));
            }
            break;

        case tmddNT220x_LPF_Gain_Frozen:
            err = tmddNT220xSetAGC5_loop_off(tUnit, True); /* Enable AGC5 loop off */
            tmASSERTExT(err, TM_OK, (DEBUGLVL_ERROR, "tmddNT220xSetAGC5_loop_off(0x%08X) failed.", tUnit));

            if(err == TM_OK)
            {
                err = tmddNT220xSetForce_AGC5_gain(tUnit, True); /* Force AGC5 gain */
                tmASSERTExT(err, TM_OK, (DEBUGLVL_ERROR, "tmddNT220xSetForce_AGC5_gain(0x%08X) failed.", tUnit));
            }

            if(err == TM_OK)
            {
                err = tmddNT220xSetAGC5_Gain(tUnit, 0);  /* Force gain to 0dB */
                tmASSERTExT(err, TM_OK, (DEBUGLVL_ERROR, "tmddNT220xSetAGC5_Gain(0x%08X) failed.", tUnit));
            }
            break;
        }

        (void)ddNT220xMutexRelease(pObj);
    }

    return err;
}

/*============================================================================*/
/* FUNCTION:    tmddNT220xGetLPF_Gain_Mode                                  */
/*                                                                            */
/* DESCRIPTION: Free/Freeze LPF Gain                                          */
/*                                                                            */
/* RETURN:      TM_OK if no error                                             */
/*                                                                            */
/* NOTES:                                                                     */
/*                                                                            */
/*============================================================================*/
tmErrorCode_t
tmddNT220xGetLPF_Gain_Mode
(
 tmUnitSelect_t  tUnit,  /* I: Unit number */
 UInt8           *puMode /* I/O: Unknown/Free/Frozen */
)
{
    ptmddNT220xObject_t   pObj = Null;
    tmErrorCode_t           err  = TM_OK;
    UInt8                   AGC5_loop_off = 0;
    UInt8                   Force_AGC5_gain = 0;
    UInt8                   AGC5_Gain = 0;

    /* Test the parameter */
    if (puMode == Null)
        err = ddNT220x_ERR_BAD_PARAMETER;

    if(err == TM_OK)
    {
        /* Get Instance Object */
        err = ddNT220xGetInstance(tUnit, &pObj);
        tmASSERTExT(err, TM_OK, (DEBUGLVL_ERROR, "ddNT220xGetInstance(0x%08X) failed.", tUnit));
    }

    if(err == TM_OK)
    {
        err = ddNT220xMutexAcquire(pObj, ddNT220x_MUTEX_TIMEOUT);
    }

    if(err == TM_OK)
    {
        *puMode = tmddNT220x_LPF_Gain_Unknown;

        err = tmddNT220xGetAGC5_loop_off(tUnit, &AGC5_loop_off);
        tmASSERTExT(err, TM_OK, (DEBUGLVL_ERROR, "tmddNT220xGetAGC5_loop_off(0x%08X) failed.", tUnit));

        if(err == TM_OK)
        {
            err = tmddNT220xGetForce_AGC5_gain(tUnit, &Force_AGC5_gain);
            tmASSERTExT(err, TM_OK, (DEBUGLVL_ERROR, "tmddNT220xGetForce_AGC5_gain(0x%08X) failed.", tUnit));
        }

        if(err == TM_OK)
        {
            err = tmddNT220xGetAGC5_Gain(tUnit, &AGC5_Gain);
            tmASSERTExT(err, TM_OK, (DEBUGLVL_ERROR, "tmddNT220xGetAGC5_Gain(0x%08X) failed.", tUnit));
        }

        if(err == TM_OK)
        {
            if(AGC5_loop_off==False && Force_AGC5_gain==False)
            {
                *puMode = tmddNT220x_LPF_Gain_Free;
            }
            else if(AGC5_loop_off==True && Force_AGC5_gain==True && AGC5_Gain==0)
            {
                *puMode = tmddNT220x_LPF_Gain_Frozen;
            }
        }

        (void)ddNT220xMutexRelease(pObj);
    }

    return err;
}

/*============================================================================*/
/* FUNCTION:    tmddNT220xWrite                                             */
/*                                                                            */
/* DESCRIPTION: Write in NT220x hardware                                    */
/*                                                                            */
/* RETURN:      TM_OK if no error                                             */
/*                                                                            */
/* NOTES:                                                                     */
/*                                                                            */
/*============================================================================*/
tmErrorCode_t
tmddNT220xWrite
(
    tmUnitSelect_t  tUnit,      /* I: Unit number */
    UInt32          uIndex,     /* I: Start index to write */
    UInt32          uNbBytes,   /* I: Number of bytes to write */
    UInt8*          puBytes     /* I: Pointer on an array of bytes */
)
{
    ptmddNT220xObject_t   pObj = Null;
    tmErrorCode_t           err = TM_OK;
    UInt32                  uCounter;
    UInt8*                  pI2CMap = Null;

    /* Get Instance Object */
    err = ddNT220xGetInstance(tUnit, &pObj);
    tmASSERTExT(err, TM_OK, (DEBUGLVL_ERROR, "ddNT220xGetInstance(0x%08X) failed.", tUnit));

    if(err == TM_OK)
    {
        err = ddNT220xMutexAcquire(pObj, ddNT220x_MUTEX_TIMEOUT);
    }

    if(err == TM_OK)
    {
        /* pI2CMap initialization */
        pI2CMap = &(pObj->I2CMap.uBx00.ID_byte_1) + uIndex;

        /* Save the values written in the Tuner */
        for (uCounter = 0; uCounter < uNbBytes; uCounter++)
        {
            *pI2CMap = puBytes[uCounter];
            pI2CMap ++;
        }

        /* Write in the Tuner */
        err = ddNT220xWrite(pObj,(UInt8)(uIndex),(UInt8)(uNbBytes));
        tmASSERTExT(err, TM_OK, (DEBUGLVL_ERROR, "ddNT220xWrite(0x%08X) failed.", tUnit));

        (void)ddNT220xMutexRelease(pObj);
    }

    return err;
}

/*============================================================================*/
/* FUNCTION:    tmddNT220xRead                                              */
/*                                                                            */
/* DESCRIPTION: Read in NT220x hardware                                     */
/*                                                                            */
/* RETURN:      TM_OK if no error                                             */
/*                                                                            */
/* NOTES:                                                                     */
/*                                                                            */
/*============================================================================*/
tmErrorCode_t
tmddNT220xRead
(
    tmUnitSelect_t  tUnit,      /* I: Unit number */
    UInt32          uIndex,     /* I: Start index to read */
    UInt32          uNbBytes,   /* I: Number of bytes to read */
    UInt8*          puBytes     /* I: Pointer on an array of bytes */
)
{
    ptmddNT220xObject_t   pObj = Null;
    tmErrorCode_t           err = TM_OK;
    UInt32                  uCounter = 0;
    UInt8*                  pI2CMap = Null;

    /* Test the parameters */
    if (uNbBytes > NT220x_I2C_MAP_NB_BYTES)
        err = ddNT220x_ERR_BAD_PARAMETER;

    if(err == TM_OK)
    {
        /* Get Instance Object */
        err = ddNT220xGetInstance(tUnit, &pObj);
        tmASSERTExT(err, TM_OK, (DEBUGLVL_ERROR, "ddNT220xGetInstance(0x%08X) failed.", tUnit));
    }

    if(err == TM_OK)
    {
        err = ddNT220xMutexAcquire(pObj, ddNT220x_MUTEX_TIMEOUT);
    }

    if(err == TM_OK)
    {
        /* pI2CMap initialization */
        pI2CMap = &(pObj->I2CMap.uBx00.ID_byte_1) + uIndex;

        /* Read from the Tuner */
        err = ddNT220xRead(pObj,(UInt8)(uIndex),(UInt8)(uNbBytes));
        tmASSERTExT(err, TM_OK, (DEBUGLVL_ERROR, "ddNT220xRead(0x%08X) failed.", tUnit));

        if(err == TM_OK)
        {
            /* Copy read values to puBytes */
            for (uCounter = 0; uCounter < uNbBytes; uCounter++)
            {
                *puBytes = (*pI2CMap);
                pI2CMap ++;
                puBytes ++;
            }
        }

        (void)ddNT220xMutexRelease(pObj);
    }

    return err;
}

/*============================================================================*/
/* FUNCTION:    tmddNT220xGetMS                                             */
/*                                                                            */
/* DESCRIPTION: Get the MS bit(s) status                                      */
/*                                                                            */
/* RETURN:      TM_OK if no error                                             */
/*                                                                            */
/* NOTES:                                                                     */
/*                                                                            */
/*============================================================================*/
tmErrorCode_t
tmddNT220xGetMS
(
    tmUnitSelect_t  tUnit,  /* I: Unit number */
    UInt8*          puValue /* I: Address of the variable to output item value */
)
{
    ptmddNT220xObject_t   pObj = Null;
    tmErrorCode_t           err  = TM_OK;

    /* Test the parameters */
    if (puValue == Null)
        err = ddNT220x_ERR_BAD_PARAMETER;

    if(err == TM_OK)
    {
        /* Get Instance Object */
        err = ddNT220xGetInstance(tUnit, &pObj);
        tmASSERTExT(err, TM_OK, (DEBUGLVL_ERROR, "ddNT220xGetInstance(0x%08X) failed.", tUnit));
    }

    if(err == TM_OK)
    {
        err = ddNT220xMutexAcquire(pObj, ddNT220x_MUTEX_TIMEOUT);
    }

    if(err == TM_OK)
    {
        /* Read byte 0x00 */
        err = ddNT220xRead(pObj, 0x00, 1);
        tmASSERTExT(err, TM_OK, (DEBUGLVL_ERROR, "ddNT220xRead(0x%08X) failed.", tUnit));

        /* Get value */
        *puValue = pObj->I2CMap.uBx00.bF.MS ;

        (void)ddNT220xMutexRelease(pObj);
    }

    return err;
}

/*============================================================================*/
/* FUNCTION:    tmddNT220xGetIdentity                                       */
/*                                                                            */
/* DESCRIPTION: Get the Identity bit(s) status                                */
/*                                                                            */
/* RETURN:      TM_OK if no error                                             */
/*                                                                            */
/* NOTES:                                                                     */
/*                                                                            */
/*============================================================================*/
tmErrorCode_t
tmddNT220xGetIdentity
(
    tmUnitSelect_t  tUnit,  /* I: Unit number */
    UInt16*         puValue /* I: Address of the variable to output item value */
)
{
    ptmddNT220xObject_t   pObj = Null;
    tmErrorCode_t           err  = TM_OK;

    /* Test the parameters */
    if (puValue == Null)
        err = ddNT220x_ERR_BAD_PARAMETER;

    if(err == TM_OK)
    {
        /* Get Instance Object */
        err = ddNT220xGetInstance(tUnit, &pObj);
        tmASSERTExT(err, TM_OK, (DEBUGLVL_ERROR, "ddNT220xGetInstance(0x%08X) failed.", tUnit));
    }

    if(err == TM_OK)
    {
        err = ddNT220xMutexAcquire(pObj, ddNT220x_MUTEX_TIMEOUT);
    }

    if(err == TM_OK)
    {
        /* Read byte 0x00-0x01 */
        err = ddNT220xRead(pObj, 0x00, 2);
        tmASSERTExT(err, TM_OK, (DEBUGLVL_ERROR, "ddNT220xRead(0x%08X) failed.", tUnit));

        if(err == TM_OK)
        {
            /* Get value */
            *puValue = pObj->I2CMap.uBx00.bF.Ident_1 << 8 |  pObj->I2CMap.uBx01.bF.Ident_2;
        }

        (void)ddNT220xMutexRelease(pObj);
    }

    return err;
}

/*============================================================================*/
/* FUNCTION:    tmddNT220xGetMinorRevision                                  */
/*                                                                            */
/* DESCRIPTION: Get the Revision bit(s) status                                */
/*                                                                            */
/* RETURN:      TM_OK if no error                                             */
/*                                                                            */
/* NOTES:                                                                     */
/*                                                                            */
/*============================================================================*/
tmErrorCode_t
tmddNT220xGetMinorRevision
(
    tmUnitSelect_t  tUnit,  /* I: Unit number */
    UInt8*          puValue /* I: Address of the variable to output item value */
)
{
    ptmddNT220xObject_t   pObj = Null;
    tmErrorCode_t           err  = TM_OK;

    /* Test the parameters */
    if (puValue == Null)
        err = ddNT220x_ERR_BAD_PARAMETER;

    if(err == TM_OK)
    {
        /* Get Instance Object */
        err = ddNT220xGetInstance(tUnit, &pObj);
        tmASSERTExT(err, TM_OK, (DEBUGLVL_ERROR, "ddNT220xGetInstance(0x%08X) failed.", tUnit));
    }

    if(err == TM_OK)
    {
        err = ddNT220xMutexAcquire(pObj, ddNT220x_MUTEX_TIMEOUT);
    }

    if(err == TM_OK)
    {
        /* Read byte 0x02 */
        err = ddNT220xRead(pObj, 0x02, 1);
        tmASSERTExT(err, TM_OK, (DEBUGLVL_ERROR, "ddNT220xRead(0x%08X) failed.", tUnit));

        if(err == TM_OK)
        {
            /* Get value */
            *puValue = pObj->I2CMap.uBx02.bF.Minor_rev;
        }

        (void)ddNT220xMutexRelease(pObj);
    }

    return err;
}

/*============================================================================*/
/* FUNCTION:    tmddNT220xGetMajorRevision                                  */
/*                                                                            */
/* DESCRIPTION: Get the Revision bit(s) status                                */
/*                                                                            */
/* RETURN:      TM_OK if no error                                             */
/*                                                                            */
/* NOTES:                                                                     */
/*                                                                            */
/*============================================================================*/
tmErrorCode_t
tmddNT220xGetMajorRevision
(
    tmUnitSelect_t  tUnit,  /* I: Unit number */
    UInt8*          puValue /* I: Address of the variable to output item value */
)
{
    ptmddNT220xObject_t   pObj = Null;
    tmErrorCode_t           err  = TM_OK;

    /* Test the parameters */
    if (puValue == Null)
        err = ddNT220x_ERR_BAD_PARAMETER;

    if(err == TM_OK)
    {
        /* Get Instance Object */
        err = ddNT220xGetInstance(tUnit, &pObj);
        tmASSERTExT(err, TM_OK, (DEBUGLVL_ERROR, "ddNT220xGetInstance(0x%08X) failed.", tUnit));
    }

    if(err == TM_OK)
    {
        err = ddNT220xMutexAcquire(pObj, ddNT220x_MUTEX_TIMEOUT);
    }

    if(err == TM_OK)
    {
        /* Read byte 0x02 */
        err = ddNT220xRead(pObj, 0x02, 1);
        tmASSERTExT(err, TM_OK, (DEBUGLVL_ERROR, "ddNT220xRead(0x%08X) failed.", tUnit));

        if(err == TM_OK)
        {
            /* Get value */
            *puValue = pObj->I2CMap.uBx02.bF.Major_rev;
        }

        (void)ddNT220xMutexRelease(pObj);
    }
    return err;
}

/*============================================================================*/
/* FUNCTION:    tmddNT220xGetLO_Lock                                        */
/*                                                                            */
/* DESCRIPTION: Get the LO_Lock bit(s) status                                 */
/*                                                                            */
/* RETURN:      TM_OK if no error                                             */
/*                                                                            */
/* NOTES:                                                                     */
/*                                                                            */
/*============================================================================*/
tmErrorCode_t
tmddNT220xGetLO_Lock
(
    tmUnitSelect_t  tUnit,  /* I: Unit number */
    UInt8*          puValue /* I: Address of the variable to output item value */
)
{
    ptmddNT220xObject_t   pObj = Null;
    tmErrorCode_t           err  = TM_OK;

    /* Test the parameters */
    if (puValue == Null)
        err = ddNT220x_ERR_BAD_PARAMETER;

    if(err == TM_OK)
    {
        /* Get Instance Object */
        err = ddNT220xGetInstance(tUnit, &pObj);
        tmASSERTExT(err, TM_OK, (DEBUGLVL_ERROR, "ddNT220xGetInstance(0x%08X) failed.", tUnit));
    }

    if(err == TM_OK)
    {
        err = ddNT220xMutexAcquire(pObj, ddNT220x_MUTEX_TIMEOUT);
    }

    if(err == TM_OK)
    {
        /* Read byte 0x05 */
        err = ddNT220xRead(pObj, 0x05, 1);
        tmASSERTExT(err, TM_OK, (DEBUGLVL_ERROR, "ddNT220xRead(0x%08X) failed.", tUnit));

        if(err == TM_OK)
        {
            /* Get value */
            *puValue = pObj->I2CMap.uBx05.bF.LO_Lock ;
        }

        (void)ddNT220xMutexRelease(pObj);
    }
    return err;
}

/*============================================================================*/
/* FUNCTION:    tmddNT220xSetPowerState                                     */
/*                                                                            */
/* DESCRIPTION: Set the power state of the NT220x                           */
/*                                                                            */
/* RETURN:      TM_OK if no error                                             */
/*                                                                            */
/* NOTES:                                                                     */
/*                                                                            */
/*============================================================================*/
tmErrorCode_t
tmddNT220xSetPowerState
(
    tmUnitSelect_t              tUnit,      /* I: Unit number */
    tmddNT220xPowerState_t    powerState  /* I: Power state of this device */
)
{
    ptmddNT220xObject_t   pObj = Null;
    tmErrorCode_t           err  = TM_OK;

    /* Get Instance Object */
    err = ddNT220xGetInstance(tUnit, &pObj);
    tmASSERTExT(err, TM_OK, (DEBUGLVL_ERROR, "ddNT220xGetInstance(0x%08X) failed.", tUnit));

    if(err == TM_OK)
    {
        err = ddNT220xMutexAcquire(pObj, ddNT220x_MUTEX_TIMEOUT);
    }

    if(err == TM_OK)
    {
        /* Read bytes 0x06-0x14 */
        err = ddNT220xRead(pObj, 0x06, 15);
        tmASSERTExT(err, TM_OK, (DEBUGLVL_ERROR, "ddNT220xRead(0x%08X) failed.", tUnit));

        /* Set digital clock mode*/
        if(err == TM_OK)
        {
            switch (powerState)
            {
            case tmddNT220x_PowerStandbyWithLNAOnAndWithXtalOnAndWithSyntheOn:
            case tmddNT220x_PowerStandbyWithLNAOnAndWithXtalOn:
            case tmddNT220x_PowerStandbyWithXtalOn:
            case tmddNT220x_PowerStandby:
                /* Set 16 Mhz Xtal clock */
                err = tmddNT220xSetDigital_Clock_Mode(tUnit, 0);
                tmASSERTExT(err, TM_OK, (DEBUGLVL_ERROR, "tmddNT220xSetDigital_Clock_Mode(0x%08X, 16 Mhz xtal clock) failed.", tUnit));
                break;

            default:
                break;
            }
        }

        /* Set power state */
        if(err == TM_OK)
        {
            switch (powerState)
            {
            case tmddNT220x_PowerNormalMode:          
                pObj->I2CMap.uBx06.bF.SM = 0x00;
                pObj->I2CMap.uBx06.bF.SM_Synthe = 0x00;
                pObj->I2CMap.uBx06.bF.SM_LT = 0x00;
                pObj->I2CMap.uBx06.bF.SM_XT = 0x00;
                break;

            case tmddNT220x_PowerStandbyWithLNAOnAndWithXtalOnAndWithSyntheOn:
                pObj->I2CMap.uBx06.bF.SM = 0x01;
                pObj->I2CMap.uBx06.bF.SM_Synthe = 0x00;
                pObj->I2CMap.uBx06.bF.SM_LT = 0x00;
                pObj->I2CMap.uBx06.bF.SM_XT = 0x00;
                break;

            case tmddNT220x_PowerStandbyWithLNAOnAndWithXtalOn:
                pObj->I2CMap.uBx06.bF.SM = 0x01;
                pObj->I2CMap.uBx06.bF.SM_Synthe = 0x01;
                pObj->I2CMap.uBx06.bF.SM_LT = 0x00;
                pObj->I2CMap.uBx06.bF.SM_XT = 0x00;
                break;

            case tmddNT220x_PowerStandbyWithXtalOn:
                pObj->I2CMap.uBx06.bF.SM = 0x01;
                pObj->I2CMap.uBx06.bF.SM_Synthe = 0x01;
                pObj->I2CMap.uBx06.bF.SM_LT = 0x01;
                pObj->I2CMap.uBx06.bF.SM_XT = 0x00;
                break;

            case tmddNT220x_PowerStandby:
                pObj->I2CMap.uBx06.bF.SM = 0x01;
                pObj->I2CMap.uBx06.bF.SM_Synthe = 0x01;
                pObj->I2CMap.uBx06.bF.SM_LT = 0x01;
                pObj->I2CMap.uBx06.bF.SM_XT = 0x01;
                break;

            default:
                /* Power state not supported*/
                return ddNT220x_ERR_NOT_SUPPORTED;
            }

            /* Write byte 0x06 */
            err = ddNT220xWrite(pObj, 0x06, 1);
            tmASSERTExT(err, TM_OK, (DEBUGLVL_ERROR, "ddNT220xWrite(0x%08X) failed.", tUnit));
        }

        /* Set digital clock mode*/
        if(err == TM_OK)
        {
            switch (powerState)
            {
            case tmddNT220x_PowerNormalMode:            
                /* Set sigma delta clock*/
                err = tmddNT220xSetDigital_Clock_Mode(tUnit, 1);
                tmASSERTExT(err, TM_OK, (DEBUGLVL_ERROR, "tmddNT220xSetDigital_Clock_Mode(0x%08X, sigma delta clock) failed.", tUnit));
                break;

            default:
                break;
            }
        }

        if(err == TM_OK)
        {
            /* Store powerstate */
            pObj->curPowerState = powerState;
        }

        (void)ddNT220xMutexRelease(pObj);
    }

    return err;
}

/*============================================================================*/
/* FUNCTION:    tmddNT220xGetPowerState                                     */
/*                                                                            */
/* DESCRIPTION: Get the power state of the NT220x                           */
/*                                                                            */
/* RETURN:      TM_OK if no error                                             */
/*                                                                            */
/* NOTES:                                                                     */
/*                                                                            */
/*============================================================================*/
tmErrorCode_t
tmddNT220xGetPowerState
(
    tmUnitSelect_t              tUnit,      /* I: Unit number */
    ptmddNT220xPowerState_t   pPowerState /* O: Power state of this device */
)
{
    ptmddNT220xObject_t   pObj = Null;
    tmErrorCode_t           err  = TM_OK;

    /* Get Instance Object */
    err = ddNT220xGetInstance(tUnit, &pObj);
    tmASSERTExT(err, TM_OK, (DEBUGLVL_ERROR, "ddNT220xGetInstance(0x%08X) failed.", tUnit));

    if(err == TM_OK)
    {
        err = ddNT220xMutexAcquire(pObj, ddNT220x_MUTEX_TIMEOUT);
    }

    if(err == TM_OK)
    {
        /* Get power state */
        if ((pObj->I2CMap.uBx06.bF.SM == 0x00) && (pObj->I2CMap.uBx06.bF.SM_Synthe == 0x00) && (pObj->I2CMap.uBx06.bF.SM_LT == 0x00) && (pObj->I2CMap.uBx06.bF.SM_XT == 0x00))
        {
            *pPowerState = tmddNT220x_PowerNormalMode;
        }
        else if ((pObj->I2CMap.uBx06.bF.SM == 0x01) && (pObj->I2CMap.uBx06.bF.SM_Synthe == 0x00) && (pObj->I2CMap.uBx06.bF.SM_LT == 0x00) && (pObj->I2CMap.uBx06.bF.SM_XT == 0x00))
        {
            *pPowerState = tmddNT220x_PowerStandbyWithLNAOnAndWithXtalOnAndWithSyntheOn;
        }
        else if ((pObj->I2CMap.uBx06.bF.SM == 0x01) && (pObj->I2CMap.uBx06.bF.SM_Synthe == 0x01) && (pObj->I2CMap.uBx06.bF.SM_LT == 0x00) && (pObj->I2CMap.uBx06.bF.SM_XT == 0x00))
        {
            *pPowerState = tmddNT220x_PowerStandbyWithLNAOnAndWithXtalOn;
        }
        else if ((pObj->I2CMap.uBx06.bF.SM == 0x01) && (pObj->I2CMap.uBx06.bF.SM_Synthe == 0x01) && (pObj->I2CMap.uBx06.bF.SM_LT == 0x01) && (pObj->I2CMap.uBx06.bF.SM_XT == 0x00))
        {
            *pPowerState = tmddNT220x_PowerStandbyWithXtalOn;
        }
        else if ((pObj->I2CMap.uBx06.bF.SM == 0x01) && (pObj->I2CMap.uBx06.bF.SM_Synthe == 0x01) && (pObj->I2CMap.uBx06.bF.SM_LT == 0x01) && (pObj->I2CMap.uBx06.bF.SM_XT == 0x01))
        {  
            *pPowerState = tmddNT220x_PowerStandby;
        }
        else
        {
            *pPowerState = tmddNT220x_PowerMax;
        }

        (void)ddNT220xMutexRelease(pObj);
    }

    return err;
}

/*============================================================================*/
/* FUNCTION:    tmddNT220xGetPower_Level                                    */
/*                                                                            */
/* DESCRIPTION: Get the Power_Level bit(s) status                             */
/*                                                                            */
/* RETURN:      TM_OK if no error                                             */
/*                                                                            */
/* NOTES:                                                                     */
/*                                                                            */
/*============================================================================*/
tmErrorCode_t
tmddNT220xGetPower_Level
(
    tmUnitSelect_t  tUnit,  /* I: Unit number */
    UInt8*          puValue /* I: Address of the variable to output item value */
)
{
    ptmddNT220xObject_t   pObj = Null;
    tmErrorCode_t           err  = TM_OK;
    UInt8                   uValue = 0;

    /* Test the parameters */
    if (puValue == Null)
        err = ddNT220x_ERR_BAD_PARAMETER;

    if(err == TM_OK)
    {
        /* Get Instance Object */
        err = ddNT220xGetInstance(tUnit, &pObj);
        tmASSERTExT(err, TM_OK, (DEBUGLVL_ERROR, "ddNT220xGetInstance(0x%08X) failed.", tUnit));
    }

    if(err == TM_OK)
    {
        err = ddNT220xMutexAcquire(pObj, ddNT220x_MUTEX_TIMEOUT);
    }
    if(err == TM_OK)
    {
        /* Set IRQ_clear*/
        err = tmddNT220xSetIRQ_Clear(tUnit, 0x10);
        tmASSERTExT(err, TM_OK, (DEBUGLVL_ERROR, "tmddNT220xSetIRQ_clear(0x%08X, 0x10) failed.", tUnit));
    }
    if(err == TM_OK)
    {
        /* Trigger RSSI_Meas */
        pObj->I2CMap.uBx19.MSM_byte_1 = 0x80;
        err = ddNT220xWrite(pObj, 0x19, 1);
        tmASSERTExT(err, TM_OK, (DEBUGLVL_ERROR, "tmddNT220xWrite(0x%08X) failed.", tUnit));

        if(err == TM_OK)
        {
            /*Trigger MSM_Launch */
            pObj->I2CMap.uBx1A.bF.MSM_Launch = 1;

            /* Write byte 0x1A */
            err = ddNT220xWrite(pObj, 0x1A, 1);
            tmASSERTExT(err, TM_OK, (DEBUGLVL_ERROR, "ddNT220xWrite(0x%08X) failed.", tUnit));

            pObj->I2CMap.uBx1A.bF.MSM_Launch = 0;
            if(pObj->bIRQWait)
            {
                if(err == TM_OK)
                {
                    err = ddNT220xWaitIRQ(pObj, 700, 1, 0x10);
                    tmASSERTExT(err, TM_OK, (DEBUGLVL_ERROR, "ddNT220xWaitIRQ(0x%08X) failed.", tUnit));
                }
            }
        }

        if(err == TM_OK)
        {
            /* Read byte 0x07 */
            err = ddNT220xRead(pObj, 0x07, 1);
            tmASSERTExT(err, TM_OK, (DEBUGLVL_ERROR, "ddNT220xRead(0x%08X) failed.", tUnit));
        }

        if(err == TM_OK)
        {
            /* Get value (limit range) */
            uValue = pObj->I2CMap.uBx07.bF.Power_Level;
            if (uValue < NT220x_POWER_LEVEL_MIN)
            {
                *puValue = 0x00;
            }
            else if (uValue > NT220x_POWER_LEVEL_MAX)
            {
                *puValue = 0xFF;
            }
            else
            {
                *puValue = uValue;
            }
        }

        (void)ddNT220xMutexRelease(pObj);
    }

    return err;
}

/*============================================================================*/
/* FUNCTION:    tmddNT220xGetIRQ_status                                     */
/*                                                                            */
/* DESCRIPTION: Get the IRQ_status bit(s) status                              */
/*                                                                            */
/* RETURN:      TM_OK if no error                                             */
/*                                                                            */
/* NOTES:                                                                     */
/*                                                                            */
/*============================================================================*/
tmErrorCode_t
tmddNT220xGetIRQ_status
(
    tmUnitSelect_t  tUnit,  /* I: Unit number */
    UInt8*          puValue /* I: Address of the variable to output item value */
)
{
    ptmddNT220xObject_t   pObj = Null;
    tmErrorCode_t           err  = TM_OK;

    /* Test the parameters */
    if (puValue == Null)
        err = ddNT220x_ERR_BAD_PARAMETER;

    if(err == TM_OK)
    {
        /* Get Instance Object */
        err = ddNT220xGetInstance(tUnit, &pObj);
        tmASSERTExT(err, TM_OK, (DEBUGLVL_ERROR, "ddNT220xGetInstance(0x%08X) failed.", tUnit));
    }

    if(err == TM_OK)
    {
        err = ddNT220xMutexAcquire(pObj, ddNT220x_MUTEX_TIMEOUT);
    }

    if(err == TM_OK)
    {
        /* Read byte 0x08 */
        err = ddNT220xGetIRQ_status(pObj, puValue);
        tmASSERTExT(err, TM_OK, (DEBUGLVL_ERROR, "ddNT220xGetIRQ_status(0x%08X) failed.", tUnit));

        (void)ddNT220xMutexRelease(pObj);
    }

    return err;
}

/*============================================================================*/
/* FUNCTION:    tmddNT220xGetMSM_XtalCal_End                                */
/*                                                                            */
/* DESCRIPTION: Get the MSM_XtalCal_End bit(s) status                         */
/*                                                                            */
/* RETURN:      TM_OK if no error                                             */
/*                                                                            */
/* NOTES:                                                                     */
/*                                                                            */
/*============================================================================*/
tmErrorCode_t
tmddNT220xGetMSM_XtalCal_End
(
    tmUnitSelect_t  tUnit,  /* I: Unit number */
    UInt8*          puValue /* I: Address of the variable to output item value */
)
{
    ptmddNT220xObject_t   pObj = Null;
    tmErrorCode_t           err  = TM_OK;

    /* Test the parameters */
    if (puValue == Null)
        err = ddNT220x_ERR_BAD_PARAMETER;

    if(err == TM_OK)
    {
        /* Get Instance Object */
        err = ddNT220xGetInstance(tUnit, &pObj);
        tmASSERTExT(err, TM_OK, (DEBUGLVL_ERROR, "ddNT220xGetInstance(0x%08X) failed.", tUnit));
    }

    if(err == TM_OK)
    {
        err = ddNT220xMutexAcquire(pObj, ddNT220x_MUTEX_TIMEOUT);
    }

    if(err == TM_OK)
    {
        /* Read byte 0x08 */
        err = ddNT220xGetMSM_XtalCal_End(pObj, puValue);

        (void)ddNT220xMutexRelease(pObj);
    }
    return err;
}

/*============================================================================*/
/* FUNCTION:    tmddNT220xSetIRQ_Clear                                      */
/*                                                                            */
/* DESCRIPTION: Set the IRQ_Clear bit(s) status                               */
/*                                                                            */
/* RETURN:      TM_OK if no error                                             */
/*                                                                            */
/* NOTES:                                                                     */
/*                                                                            */
/*============================================================================*/
tmErrorCode_t
tmddNT220xSetIRQ_Clear
(
    tmUnitSelect_t  tUnit,      /* I: Unit number */
    UInt8           irqStatus   /* I: IRQs to clear */
)
{
    ptmddNT220xObject_t   pObj = Null;
    tmErrorCode_t           err  = TM_OK;

    /* Get Instance Object */
    err = ddNT220xGetInstance(tUnit, &pObj);
    tmASSERTExT(err, TM_OK, (DEBUGLVL_ERROR, "ddNT220xGetInstance(0x%08X) failed.", tUnit));

    if(err == TM_OK)
    {
        err = ddNT220xMutexAcquire(pObj, ddNT220x_MUTEX_TIMEOUT);
    }

    if(err == TM_OK)
    {
        /* Set IRQ_Clear */
        /*pObj->I2CMap.uBx0A.bF.IRQ_Clear = 1; */
        pObj->I2CMap.uBx0A.IRQ_clear |= (0x80|(irqStatus&0x1F));

        /* Write byte 0x0A */
        err = ddNT220xWrite(pObj, 0x0A, 1);
        tmASSERTExT(err, TM_OK, (DEBUGLVL_ERROR, "ddNT220xWrite(0x%08X) failed.", tUnit));

        /* Reset IRQ_Clear (buffer only, no write) */
        /*pObj->I2CMap.uBx0A.bF.IRQ_Clear = 0;*/
        pObj->I2CMap.uBx0A.IRQ_clear &= (~(0x80|(irqStatus&0x1F)));

        (void)ddNT220xMutexRelease(pObj);
    }

    return err;
}

/*============================================================================*/
/* FUNCTION:    tmddNT220xSetAGC1_TOP                                       */
/*                                                                            */
/* DESCRIPTION: Set the AGC1_TOP bit(s) status                                */
/*                                                                            */
/* RETURN:      TM_OK if no error                                             */
/*                                                                            */
/* NOTES:                                                                     */
/*                                                                            */
/*============================================================================*/
tmErrorCode_t
tmddNT220xSetAGC1_TOP
(
    tmUnitSelect_t  tUnit,  /* I: Unit number */
    UInt8           uValue  /* I: Item value */
)
{
    ptmddNT220xObject_t   pObj = Null;
    tmErrorCode_t           err  = TM_OK;

    /* Get Instance Object */
    err = ddNT220xGetInstance(tUnit, &pObj);
    tmASSERTExT(err, TM_OK, (DEBUGLVL_ERROR, "ddNT220xGetInstance(0x%08X) failed.", tUnit));

    if(err == TM_OK)
    {
        err = ddNT220xMutexAcquire(pObj, ddNT220x_MUTEX_TIMEOUT);
    }

    if(err == TM_OK)
    {
        /* Set value */
        pObj->I2CMap.uBx0C.bF.AGC1_TOP = uValue;

        /* write byte 0x0C */
        err = ddNT220xWrite(pObj, 0x0C, 1);
        tmASSERTExT(err, TM_OK, (DEBUGLVL_ERROR, "ddNT220xWrite(0x%08X) failed.", tUnit));

        (void)ddNT220xMutexRelease(pObj);
    }

    return err;
}

/*============================================================================*/
/* FUNCTION:    tmddNT220xGetAGC1_TOP                                       */
/*                                                                            */
/* DESCRIPTION: Get the AGC1_TOP bit(s) status                                */
/*                                                                            */
/* RETURN:      TM_OK if no error                                             */
/*                                                                            */
/* NOTES:                                                                     */
/*                                                                            */
/*============================================================================*/
tmErrorCode_t
tmddNT220xGetAGC1_TOP
(
    tmUnitSelect_t  tUnit,  /* I: Unit number */
    UInt8*          puValue /* I: Address of the variable to output item value */
)
{
    ptmddNT220xObject_t   pObj = Null;
    tmErrorCode_t           err  = TM_OK;

    /* Test the parameters */
    if ( puValue == Null )
    {
        err = ddNT220x_ERR_BAD_PARAMETER;
    }       

    /* Get Instance Object */
    if(err == TM_OK)
    {
        err = ddNT220xGetInstance(tUnit, &pObj);
        tmASSERTExT(err, TM_OK, (DEBUGLVL_ERROR, "ddNT220xGetInstance(0x%08X) failed.", tUnit));
    }
    
    if(err == TM_OK)
    {
        err = ddNT220xMutexAcquire(pObj, ddNT220x_MUTEX_TIMEOUT);
        
        if(err == TM_OK)
        {
            /* Read byte 0x0C */
            err = ddNT220xRead(pObj, 0x0C, 1);
            tmASSERTExT(err, TM_OK, (DEBUGLVL_ERROR, "ddNT220xRead(0x%08X) failed.", tUnit));
    
            if(err == TM_OK)
            {
                /* Get value */
                *puValue = pObj->I2CMap.uBx0C.bF.AGC1_TOP;
            }
    
            (void)ddNT220xMutexRelease(pObj);
        }
    }
    return err;
}

/*============================================================================*/
/* FUNCTION:    tmddNT220xSetAGC2_TOP                                       */
/*                                                                            */
/* DESCRIPTION: Set the AGC2_TOP bit(s) status                                */
/*                                                                            */
/* RETURN:      TM_OK if no error                                             */
/*                                                                            */
/* NOTES:                                                                     */
/*                                                                            */
/*============================================================================*/
tmErrorCode_t
tmddNT220xSetAGC2_TOP
(
    tmUnitSelect_t  tUnit,  /* I: Unit number */
    UInt8           uValue  /* I: Item value */
)
{
    ptmddNT220xObject_t   pObj = Null;
    tmErrorCode_t           err  = TM_OK;

    /* Get Instance Object */
    err = ddNT220xGetInstance(tUnit, &pObj);
    tmASSERTExT(err, TM_OK, (DEBUGLVL_ERROR, "ddNT220xGetInstance(0x%08X) failed.", tUnit));

    if(err == TM_OK)
    {
        err = ddNT220xMutexAcquire(pObj, ddNT220x_MUTEX_TIMEOUT);
    }

    if(err == TM_OK)
    {
        /* set value */
        pObj->I2CMap.uBx0D.bF.AGC2_TOP = uValue;

        /* Write byte 0x0D */
        err = ddNT220xWrite(pObj, 0x0D, 1);
        tmASSERTExT(err, TM_OK, (DEBUGLVL_ERROR, "ddNT220xWrite(0x%08X) failed.", tUnit));

        (void)ddNT220xMutexRelease(pObj);
    }

    return err;
}

/*============================================================================*/
/* FUNCTION:    tmddNT220xGetAGC2_TOP                                       */
/*                                                                            */
/* DESCRIPTION: Get the AGC2_TOP bit(s) status                                */
/*                                                                            */
/* RETURN:      TM_OK if no error                                             */
/*                                                                            */
/* NOTES:                                                                     */
/*                                                                            */
/*============================================================================*/
tmErrorCode_t
tmddNT220xGetAGC2_TOP
(
    tmUnitSelect_t  tUnit,  /* I: Unit number */
    UInt8*          puValue /* I: Address of the variable to output item value */
)
{
    ptmddNT220xObject_t   pObj = Null;
    tmErrorCode_t           err  = TM_OK;

    /* Test the parameters */
    if (puValue == Null)
        err = ddNT220x_ERR_BAD_PARAMETER;

    if(err == TM_OK)
    {
        /* Get Instance Object */
        err = ddNT220xGetInstance(tUnit, &pObj);
        tmASSERTExT(err, TM_OK, (DEBUGLVL_ERROR, "ddNT220xGetInstance(0x%08X) failed.", tUnit));
    }

    if(err == TM_OK)
    {
        err = ddNT220xMutexAcquire(pObj, ddNT220x_MUTEX_TIMEOUT);
    }

    if(err == TM_OK)
    {
        /* Read byte 0x0D */
        err = ddNT220xRead(pObj, 0x0D, 1);
        tmASSERTExT(err, TM_OK, (DEBUGLVL_ERROR, "ddNT220xRead(0x%08X) failed.", tUnit));

        if(err == TM_OK)
        {
            /* Get value */
            *puValue = pObj->I2CMap.uBx0D.bF.AGC2_TOP;
        }

        (void)ddNT220xMutexRelease(pObj);
    }

    return err;
}

/*============================================================================*/
/* FUNCTION:    tmddNT220xSetAGCs_Up_Step                                   */
/*                                                                            */
/* DESCRIPTION: Set the AGCs_Up_Step bit(s) status                            */
/*                                                                            */
/* RETURN:      TM_OK if no error                                             */
/*                                                                            */
/* NOTES:                                                                     */
/*                                                                            */
/*============================================================================*/
tmErrorCode_t
tmddNT220xSetAGCs_Up_Step
(
    tmUnitSelect_t  tUnit,  /* I: Unit number */
    UInt8           uValue  /* I: Item value */
)
{
    ptmddNT220xObject_t   pObj = Null;
    tmErrorCode_t           err  = TM_OK;

    /* Get Instance Object */
    err = ddNT220xGetInstance(tUnit, &pObj);
    tmASSERTExT(err, TM_OK, (DEBUGLVL_ERROR, "ddNT220xGetInstance(0x%08X) failed.", tUnit));

    if(err == TM_OK)
    {
        err = ddNT220xMutexAcquire(pObj, ddNT220x_MUTEX_TIMEOUT);
    }

    if(err == TM_OK)
    {
        /* Set value */
        pObj->I2CMap.uBx0E.bF.AGCs_Up_Step = uValue;

        /* Write byte 0x0E */
        err = ddNT220xWrite(pObj, 0x0E, 1);
        tmASSERTExT(err, TM_OK, (DEBUGLVL_ERROR, "ddNT220xWrite(0x%08X) failed.", tUnit));

        (void)ddNT220xMutexRelease(pObj);
    }

    return err;
}

/*============================================================================*/
/* FUNCTION:    tmddNT220xGetAGCs_Up_Step                                   */
/*                                                                            */
/* DESCRIPTION: Get the AGCs_Up_Step bit(s) status                            */
/*                                                                            */
/* RETURN:      TM_OK if no error                                             */
/*                                                                            */
/* NOTES:                                                                     */
/*                                                                            */
/*============================================================================*/
tmErrorCode_t
tmddNT220xGetAGCs_Up_Step
(
    tmUnitSelect_t  tUnit,  /* I: Unit number */
    UInt8*          puValue /* I: Address of the variable to output item value */
)
{
    ptmddNT220xObject_t   pObj = Null;
    tmErrorCode_t           err  = TM_OK;

    /* Test the parameters */
    if (puValue == Null)
        err = ddNT220x_ERR_BAD_PARAMETER;

    if(err == TM_OK)
    {
        /* Get Instance Object */
        err = ddNT220xGetInstance(tUnit, &pObj);
        tmASSERTExT(err, TM_OK, (DEBUGLVL_ERROR, "ddNT220xGetInstance(0x%08X) failed.", tUnit));
    }

    if(err == TM_OK)
    {
        err = ddNT220xMutexAcquire(pObj, ddNT220x_MUTEX_TIMEOUT);
    }

    if(err == TM_OK)
    {
        /* Read byte 0x0E */
        err = ddNT220xRead(pObj, 0x0E, 1);
        tmASSERTExT(err, TM_OK, (DEBUGLVL_ERROR, "ddNT220xRead(0x%08X) failed.", tUnit));

        if(err == TM_OK)
        {
            /* Get value */
            *puValue = pObj->I2CMap.uBx0E.bF.AGCs_Up_Step;
        }

        (void)ddNT220xMutexRelease(pObj);
    }

    return err;
}

/*============================================================================*/
/* FUNCTION:    tmddNT220xSetAGCK_Step                                      */
/*                                                                            */
/* DESCRIPTION: Set the AGCK_Step bit(s) status                               */
/*                                                                            */
/* RETURN:      TM_OK if no error                                             */
/*                                                                            */
/* NOTES:                                                                     */
/*                                                                            */
/*============================================================================*/
tmErrorCode_t
tmddNT220xSetAGCK_Step
(
    tmUnitSelect_t  tUnit,  /* I: Unit number */
    UInt8           uValue  /* I: Item value */
)
{
    ptmddNT220xObject_t   pObj = Null;
    tmErrorCode_t           err  = TM_OK;

    /* Get Instance Object */
    err = ddNT220xGetInstance(tUnit, &pObj);
    tmASSERTExT(err, TM_OK, (DEBUGLVL_ERROR, "ddNT220xGetInstance(0x%08X) failed.", tUnit));

    if(err == TM_OK)
    {
        err = ddNT220xMutexAcquire(pObj, ddNT220x_MUTEX_TIMEOUT);
    }

    if(err == TM_OK)
    {
        /* Set value */
        pObj->I2CMap.uBx0E.bF.AGCK_Step = uValue;

        /* Write byte 0x0E */
        err = ddNT220xWrite(pObj, 0x0E, 1);
        tmASSERTExT(err, TM_OK, (DEBUGLVL_ERROR, "ddNT220xWrite(0x%08X) failed.", tUnit));

        (void)ddNT220xMutexRelease(pObj);
    }

    return err;
}

/*============================================================================*/
/* FUNCTION:    tmddNT220xGetAGCK_Step                                      */
/*                                                                            */
/* DESCRIPTION: Get the AGCK_Step bit(s) status                               */
/*                                                                            */
/* RETURN:      TM_OK if no error                                             */
/*                                                                            */
/* NOTES:                                                                     */
/*                                                                            */
/*============================================================================*/
tmErrorCode_t
tmddNT220xGetAGCK_Step
(
    tmUnitSelect_t  tUnit,  /* I: Unit number */
    UInt8*          puValue /* I: Address of the variable to output item value */
)
{
    ptmddNT220xObject_t   pObj = Null;
    tmErrorCode_t           err  = TM_OK;

    /* Test the parameters */
    if (puValue == Null)
        err = ddNT220x_ERR_BAD_PARAMETER;

    if(err == TM_OK)
    {
        /* Get Instance Object */
        err = ddNT220xGetInstance(tUnit, &pObj);
        tmASSERTExT(err, TM_OK, (DEBUGLVL_ERROR, "ddNT220xGetInstance(0x%08X) failed.", tUnit));
    }

    if(err == TM_OK)
    {
        err = ddNT220xMutexAcquire(pObj, ddNT220x_MUTEX_TIMEOUT);
    }

    if(err == TM_OK)
    {
        /* Read byte 0x0E */
        err = ddNT220xRead(pObj, 0x0E, 1);
        tmASSERTExT(err, TM_OK, (DEBUGLVL_ERROR, "ddNT220xRead(0x%08X) failed.", tUnit));

        if(err == TM_OK)
        {
            /* Get value */
            *puValue = pObj->I2CMap.uBx0E.bF.AGCK_Step;
        }

        (void)ddNT220xMutexRelease(pObj);
    }

    return err;
}

/*============================================================================*/
/* FUNCTION:    tmddNT220xSetAGCK_Mode                                      */
/*                                                                            */
/* DESCRIPTION: Set the AGCK_Mode bit(s) status                               */
/*                                                                            */
/* RETURN:      TM_OK if no error                                             */
/*                                                                            */
/* NOTES:                                                                     */
/*                                                                            */
/*============================================================================*/
tmErrorCode_t
tmddNT220xSetAGCK_Mode
(
    tmUnitSelect_t  tUnit,  /* I: Unit number */
    UInt8           uValue  /* I: Item value */
)
{
    ptmddNT220xObject_t   pObj = Null;
    tmErrorCode_t           err  = TM_OK;

    /* Get Instance Object */
    err = ddNT220xGetInstance(tUnit, &pObj);
    tmASSERTExT(err, TM_OK, (DEBUGLVL_ERROR, "ddNT220xGetInstance(0x%08X) failed.", tUnit));

    if(err == TM_OK)
    {
        err = ddNT220xMutexAcquire(pObj, ddNT220x_MUTEX_TIMEOUT);
    }

    if(err == TM_OK)
    {
        /* Set value */
        pObj->I2CMap.uBx0E.bF.AGCK_Mode = uValue;

        /* Write byte 0x0E */
        err = ddNT220xWrite(pObj, 0x0E, 1);
        tmASSERTExT(err, TM_OK, (DEBUGLVL_ERROR, "ddNT220xWrite(0x%08X) failed.", tUnit));

        (void)ddNT220xMutexRelease(pObj);
    }

    return err;
}

/*============================================================================*/
/* FUNCTION:    tmddNT220xGetAGCK_Mode                                      */
/*                                                                            */
/* DESCRIPTION: Get the AGCK_Mode bit(s) status                               */
/*                                                                            */
/* RETURN:      TM_OK if no error                                             */
/*                                                                            */
/* NOTES:                                                                     */
/*                                                                            */
/*============================================================================*/
tmErrorCode_t
tmddNT220xGetAGCK_Mode
(
    tmUnitSelect_t  tUnit,  /* I: Unit number */
    UInt8*          puValue /* I: Address of the variable to output item value */
)
{
    ptmddNT220xObject_t   pObj = Null;
    tmErrorCode_t           err  = TM_OK;

    /* Test the parameters */
    if (puValue == Null)
        err = ddNT220x_ERR_BAD_PARAMETER;

    if(err == TM_OK)
    {
        /* Get Instance Object */
        err = ddNT220xGetInstance(tUnit, &pObj);
        tmASSERTExT(err, TM_OK, (DEBUGLVL_ERROR, "ddNT220xGetInstance(0x%08X) failed.", tUnit));
    }

    if(err == TM_OK)
    {
        err = ddNT220xMutexAcquire(pObj, ddNT220x_MUTEX_TIMEOUT);
    }

    if(err == TM_OK)
    {
        /* Read byte 0x0E */
        err = ddNT220xRead(pObj, 0x0E, 1);
        tmASSERTExT(err, TM_OK, (DEBUGLVL_ERROR, "ddNT220xRead(0x%08X) failed.", tUnit));

        if(err == TM_OK)
        {
            /* Get value */
            *puValue = pObj->I2CMap.uBx0E.bF.AGCK_Mode;
        }

        (void)ddNT220xMutexRelease(pObj);
    }

    return err;
}

/*============================================================================*/
/* FUNCTION:    tmddNT220xSetPD_RFAGC_Adapt                                 */
/*                                                                            */
/* DESCRIPTION: Set the PD_RFAGC_Adapt bit(s) status                          */
/*                                                                            */
/* RETURN:      TM_OK if no error                                             */
/*                                                                            */
/* NOTES:                                                                     */
/*                                                                            */
/*============================================================================*/
tmErrorCode_t
tmddNT220xSetPD_RFAGC_Adapt
(
    tmUnitSelect_t  tUnit,  /* I: Unit number */
    UInt8           uValue  /* I: Item value */
)
{
    ptmddNT220xObject_t   pObj = Null;
    tmErrorCode_t           err  = TM_OK;

    /* Get Instance Object */
    err = ddNT220xGetInstance(tUnit, &pObj);
    tmASSERTExT(err, TM_OK, (DEBUGLVL_ERROR, "ddNT220xGetInstance(0x%08X) failed.", tUnit));

    if(err == TM_OK)
    {
        err = ddNT220xMutexAcquire(pObj, ddNT220x_MUTEX_TIMEOUT);
    }

    if(err == TM_OK)
    {
        /* Set value */
        pObj->I2CMap.uBx0F.bF.PD_RFAGC_Adapt = uValue;

        /* Write byte 0x0F */
        err = ddNT220xWrite(pObj, 0x0F, 1);
        tmASSERTExT(err, TM_OK, (DEBUGLVL_ERROR, "ddNT220xWrite(0x%08X) failed.", tUnit));

        (void)ddNT220xMutexRelease(pObj);
    }

    return err;
}

/*============================================================================*/
/* FUNCTION:    tmddNT220xGetPD_RFAGC_Adapt                                 */
/*                                                                            */
/* DESCRIPTION: Get the PD_RFAGC_Adapt bit(s) status                          */
/*                                                                            */
/* RETURN:      TM_OK if no error                                             */
/*                                                                            */
/* NOTES:                                                                     */
/*                                                                            */
/*============================================================================*/
tmErrorCode_t
tmddNT220xGetPD_RFAGC_Adapt
(
    tmUnitSelect_t  tUnit,  /* I: Unit number */
    UInt8*          puValue /* I: Address of the variable to output item value */
)
{
    ptmddNT220xObject_t   pObj = Null;
    tmErrorCode_t           err  = TM_OK;

    /* Test the parameters */
    if (puValue == Null)
        err = ddNT220x_ERR_BAD_PARAMETER;

    if(err == TM_OK)
    {
        /* Get Instance Object */
        err = ddNT220xGetInstance(tUnit, &pObj);
        tmASSERTExT(err, TM_OK, (DEBUGLVL_ERROR, "ddNT220xGetInstance(0x%08X) failed.", tUnit));
    }

    if(err == TM_OK)
    {
        err = ddNT220xMutexAcquire(pObj, ddNT220x_MUTEX_TIMEOUT);
    }

    if(err == TM_OK)
    {
        /* Read byte 0x0F */
        err = ddNT220xRead(pObj, 0x0F, 1);
        tmASSERTExT(err, TM_OK, (DEBUGLVL_ERROR, "ddNT220xRead(0x%08X) failed.", tUnit));

        if(err == TM_OK)
        {
            /* Get value */
            *puValue = pObj->I2CMap.uBx0F.bF.PD_RFAGC_Adapt;
        }

        (void)ddNT220xMutexRelease(pObj);
    }

    return err;
}

/*============================================================================*/
/* FUNCTION:    tmddNT220xSetRFAGC_Adapt_TOP                                */
/*                                                                            */
/* DESCRIPTION: Set the RFAGC_Adapt_TOP bit(s) status                         */
/*                                                                            */
/* RETURN:      TM_OK if no error                                             */
/*                                                                            */
/* NOTES:                                                                     */
/*                                                                            */
/*============================================================================*/
tmErrorCode_t
tmddNT220xSetRFAGC_Adapt_TOP
(
    tmUnitSelect_t  tUnit,  /* I: Unit number */
    UInt8           uValue  /* I: Item value */
)
{
    ptmddNT220xObject_t   pObj = Null;
    tmErrorCode_t           err  = TM_OK;

    /* Get Instance Object */
    err = ddNT220xGetInstance(tUnit, &pObj);
    tmASSERTExT(err, TM_OK, (DEBUGLVL_ERROR, "ddNT220xGetInstance(0x%08X) failed.", tUnit));

    if(err == TM_OK)
    {
        err = ddNT220xMutexAcquire(pObj, ddNT220x_MUTEX_TIMEOUT);
    }

    if(err == TM_OK)
    {
        /* Set value */
        pObj->I2CMap.uBx0F.bF.RFAGC_Adapt_TOP = uValue;

        /* Write byte 0x0F */
        err = ddNT220xWrite(pObj, 0x0F, 1);
        tmASSERTExT(err, TM_OK, (DEBUGLVL_ERROR, "ddNT220xWrite(0x%08X) failed.", tUnit));

        (void)ddNT220xMutexRelease(pObj);
    }

    return err;
}

/*============================================================================*/
/* FUNCTION:    tmddNT220xGetRFAGC_Adapt_TOP                                */
/*                                                                            */
/* DESCRIPTION: Get the RFAGC_Adapt_TOP bit(s) status                         */
/*                                                                            */
/* RETURN:      TM_OK if no error                                             */
/*                                                                            */
/* NOTES:                                                                     */
/*                                                                            */
/*============================================================================*/
tmErrorCode_t
tmddNT220xGetRFAGC_Adapt_TOP
(
    tmUnitSelect_t  tUnit,  /* I: Unit number */
    UInt8*          puValue /* I: Address of the variable to output item value */
)
{
    ptmddNT220xObject_t   pObj = Null;
    tmErrorCode_t           err  = TM_OK;

    /* Test the parameters */
    if (puValue == Null)
        err = ddNT220x_ERR_BAD_PARAMETER;

    if(err == TM_OK)
    {
        /* Get Instance Object */
        err = ddNT220xGetInstance(tUnit, &pObj);
        tmASSERTExT(err, TM_OK, (DEBUGLVL_ERROR, "ddNT220xGetInstance(0x%08X) failed.", tUnit));
    }

    if(err == TM_OK)
    {
        err = ddNT220xMutexAcquire(pObj, ddNT220x_MUTEX_TIMEOUT);
    }

    if(err == TM_OK)
    {
        /* Read byte 0x0F */
        err = ddNT220xRead(pObj, 0x0F, 1);
        tmASSERTExT(err, TM_OK, (DEBUGLVL_ERROR, "ddNT220xRead(0x%08X) failed.", tUnit));

        if(err == TM_OK)
        {
            /* Get value */
            *puValue = pObj->I2CMap.uBx0F.bF.RFAGC_Adapt_TOP;
        }

        (void)ddNT220xMutexRelease(pObj);
    }

    return err;
}

/*============================================================================*/
/* FUNCTION:    tmddNT220xSetRF_Atten_3dB                                   */
/*                                                                            */
/* DESCRIPTION: Set the RF_Atten_3dB bit(s) status                            */
/*                                                                            */
/* RETURN:      TM_OK if no error                                             */
/*                                                                            */
/* NOTES:                                                                     */
/*                                                                            */
/*============================================================================*/
tmErrorCode_t
tmddNT220xSetRF_Atten_3dB
(
    tmUnitSelect_t  tUnit,  /* I: Unit number */
    UInt8           uValue  /* I: Item value */
)
{
    ptmddNT220xObject_t   pObj = Null;
    tmErrorCode_t           err  = TM_OK;

    /* Get Instance Object */
    err = ddNT220xGetInstance(tUnit, &pObj);
    tmASSERTExT(err, TM_OK, (DEBUGLVL_ERROR, "ddNT220xGetInstance(0x%08X) failed.", tUnit));

    if(err == TM_OK)
    {
        err = ddNT220xMutexAcquire(pObj, ddNT220x_MUTEX_TIMEOUT);
    }

    if(err == TM_OK)
    {
        /* Set value */
        pObj->I2CMap.uBx0F.bF.RF_Atten_3dB = uValue;

        /* Write byte 0x0F */
        err = ddNT220xWrite(pObj, 0x0F, 1);
        tmASSERTExT(err, TM_OK, (DEBUGLVL_ERROR, "ddNT220xWrite(0x%08X) failed.", tUnit));

        (void)ddNT220xMutexRelease(pObj);
    }

    return err;
}

/*============================================================================*/
/* FUNCTION:    tmddNT220xGetRF_Atten_3dB                                   */
/*                                                                            */
/* DESCRIPTION: Get the RF_Atten_3dB bit(s) status                            */
/*                                                                            */
/* RETURN:      TM_OK if no error                                             */
/*                                                                            */
/* NOTES:                                                                     */
/*                                                                            */
/*============================================================================*/
tmErrorCode_t
tmddNT220xGetRF_Atten_3dB
(
    tmUnitSelect_t  tUnit,  /* I: Unit number */
    UInt8*          puValue /* I: Address of the variable to output item value */
)
{
    ptmddNT220xObject_t   pObj = Null;
    tmErrorCode_t           err  = TM_OK;

    /* Test the parameters */
    if (puValue == Null)
        err = ddNT220x_ERR_BAD_PARAMETER;

    if(err == TM_OK)
    {
        /* Get Instance Object */
        err = ddNT220xGetInstance(tUnit, &pObj);
        tmASSERTExT(err, TM_OK, (DEBUGLVL_ERROR, "ddNT220xGetInstance(0x%08X) failed.", tUnit));
    }

    if(err == TM_OK)
    {
        err = ddNT220xMutexAcquire(pObj, ddNT220x_MUTEX_TIMEOUT);
    }

    if(err == TM_OK)
    {
        /* Read byte 0x0F */
        err = ddNT220xRead(pObj, 0x0F, 1);
        tmASSERTExT(err, TM_OK, (DEBUGLVL_ERROR, "ddNT220xRead(0x%08X) failed.", tUnit));

        if(err == TM_OK)
        {
            /* Get value */
            *puValue = pObj->I2CMap.uBx0F.bF.RF_Atten_3dB;
        }

        (void)ddNT220xMutexRelease(pObj);
    }

    return err;
}

/*============================================================================*/
/* FUNCTION:    tmddNT220xSetRFAGC_Top                                      */
/*                                                                            */
/* DESCRIPTION: Set the RFAGC_Top bit(s) status                               */
/*                                                                            */
/* RETURN:      TM_OK if no error                                             */
/*                                                                            */
/* NOTES:                                                                     */
/*                                                                            */
/*============================================================================*/
tmErrorCode_t
tmddNT220xSetRFAGC_Top
(
    tmUnitSelect_t  tUnit,  /* I: Unit number */
    UInt8           uValue  /* I: Item value */
)
{
    ptmddNT220xObject_t   pObj = Null;
    tmErrorCode_t           err  = TM_OK;

    /* Get Instance Object */
    err = ddNT220xGetInstance(tUnit, &pObj);
    tmASSERTExT(err, TM_OK, (DEBUGLVL_ERROR, "ddNT220xGetInstance(0x%08X) failed.", tUnit));

    if(err == TM_OK)
    {
        err = ddNT220xMutexAcquire(pObj, ddNT220x_MUTEX_TIMEOUT);
    }

    if(err == TM_OK)
    {
        /* Set value */
        pObj->I2CMap.uBx0F.bF.RFAGC_Top = uValue;

        /* Write byte 0x0F */
        err = ddNT220xWrite(pObj, 0x0F, 1);
        tmASSERTExT(err, TM_OK, (DEBUGLVL_ERROR, "ddNT220xWrite(0x%08X) failed.", tUnit));

        (void)ddNT220xMutexRelease(pObj);
    }

    return err;
}

/*============================================================================*/
/* FUNCTION:    tmddNT220xGetRFAGC_Top                                      */
/*                                                                            */
/* DESCRIPTION: Get the RFAGC_Top bit(s) status                               */
/*                                                                            */
/* RETURN:      TM_OK if no error                                             */
/*                                                                            */
/* NOTES:                                                                     */
/*                                                                            */
/*============================================================================*/
tmErrorCode_t
tmddNT220xGetRFAGC_Top
(
    tmUnitSelect_t  tUnit,  /* I: Unit number */
    UInt8*          puValue /* I: Address of the variable to output item value */
)
{
    ptmddNT220xObject_t   pObj = Null;
    tmErrorCode_t           err  = TM_OK;

    /* Test the parameters */
    if (puValue == Null)
        err = ddNT220x_ERR_BAD_PARAMETER;

    if(err == TM_OK)
    {
        /* Get Instance Object */
        err = ddNT220xGetInstance(tUnit, &pObj);
        tmASSERTExT(err, TM_OK, (DEBUGLVL_ERROR, "ddNT220xGetInstance(0x%08X) failed.", tUnit));
    }

    if(err == TM_OK)
    {
        err = ddNT220xMutexAcquire(pObj, ddNT220x_MUTEX_TIMEOUT);
    }

    if(err == TM_OK)
    {
        /* Read byte 0x0F */
        err = ddNT220xRead(pObj, 0x0F, 1);
        tmASSERTExT(err, TM_OK, (DEBUGLVL_ERROR, "ddNT220xRead(0x%08X) failed.", tUnit));

        if(err == TM_OK)
        {
            /* Get value */
            *puValue = pObj->I2CMap.uBx0F.bF.RFAGC_Top;
        }

        (void)ddNT220xMutexRelease(pObj);
    }

    return err;
}

/*============================================================================*/
/* FUNCTION:    tmddNT220xSetIR_Mixer_Top                                   */
/*                                                                            */
/* DESCRIPTION: Set the IR_Mixer_Top bit(s) status                            */
/*                                                                            */
/* RETURN:      TM_OK if no error                                             */
/*                                                                            */
/* NOTES:                                                                     */
/*                                                                            */
/*============================================================================*/
tmErrorCode_t
tmddNT220xSetIR_Mixer_Top
(
    tmUnitSelect_t  tUnit,  /* I: Unit number */
    UInt8           uValue  /* I: Item value */
)
{
    ptmddNT220xObject_t   pObj = Null;
    tmErrorCode_t           err  = TM_OK;

    /* Get Instance Object */
    err = ddNT220xGetInstance(tUnit, &pObj);
    tmASSERTExT(err, TM_OK, (DEBUGLVL_ERROR, "ddNT220xGetInstance(0x%08X) failed.", tUnit));

    if(err == TM_OK)
    {
        err = ddNT220xMutexAcquire(pObj, ddNT220x_MUTEX_TIMEOUT);
    }

    if(err == TM_OK)
    {
        /* Set value */
        pObj->I2CMap.uBx10.bF.IR_Mixer_Top = uValue;

        /* Write byte 0x10 */
        err = ddNT220xWrite(pObj, 0x10, 1);
        tmASSERTExT(err, TM_OK, (DEBUGLVL_ERROR, "ddNT220xWrite(0x%08X) failed.", tUnit));

        (void)ddNT220xMutexRelease(pObj);
    }

    return err;
}

/*============================================================================*/
/* FUNCTION:    tmddNT220xGetIR_Mixer_Top                                   */
/*                                                                            */
/* DESCRIPTION: Get the IR_Mixer_Top bit(s) status                            */
/*                                                                            */
/* RETURN:      TM_OK if no error                                             */
/*                                                                            */
/* NOTES:                                                                     */
/*                                                                            */
/*============================================================================*/
tmErrorCode_t
tmddNT220xGetIR_Mixer_Top
(
    tmUnitSelect_t  tUnit,  /* I: Unit number */
    UInt8*          puValue /* I: Address of the variable to output item value */
)
{
    ptmddNT220xObject_t   pObj = Null;
    tmErrorCode_t           err  = TM_OK;

    /* Test the parameters */
    if (puValue == Null)
        err = ddNT220x_ERR_BAD_PARAMETER;

    if(err == TM_OK)
    {
        /* Get Instance Object */
        err = ddNT220xGetInstance(tUnit, &pObj);
        tmASSERTExT(err, TM_OK, (DEBUGLVL_ERROR, "ddNT220xGetInstance(0x%08X) failed.", tUnit));
    }

    if(err == TM_OK)
    {
        err = ddNT220xMutexAcquire(pObj, ddNT220x_MUTEX_TIMEOUT);
    }

    if(err == TM_OK)
    {
        /* Read byte 0x10 */
        err = ddNT220xRead(pObj, 0x10, 1);
        tmASSERTExT(err, TM_OK, (DEBUGLVL_ERROR, "ddNT220xRead(0x%08X) failed.", tUnit));

        if(err == TM_OK)
        {
            /* Get value */
            *puValue = pObj->I2CMap.uBx10.bF.IR_Mixer_Top;
        }

        (void)ddNT220xMutexRelease(pObj);
    }

    return err;
}

/*============================================================================*/
/* FUNCTION:    tmddNT220xSetAGC5_Ana                                       */
/*                                                                            */
/* DESCRIPTION: Set the AGC5_Ana bit(s) status                                */
/*                                                                            */
/* RETURN:      TM_OK if no error                                             */
/*                                                                            */
/* NOTES:                                                                     */
/*                                                                            */
/*============================================================================*/
tmErrorCode_t
tmddNT220xSetAGC5_Ana
(
    tmUnitSelect_t  tUnit,  /* I: Unit number */
    UInt8           uValue  /* I: Item value */
)
{
    ptmddNT220xObject_t   pObj = Null;
    tmErrorCode_t           err  = TM_OK;

    /* Get Instance Object */
    err = ddNT220xGetInstance(tUnit, &pObj);
    tmASSERTExT(err, TM_OK, (DEBUGLVL_ERROR, "ddNT220xGetInstance(0x%08X) failed.", tUnit));

    if(err == TM_OK)
    {
        err = ddNT220xMutexAcquire(pObj, ddNT220x_MUTEX_TIMEOUT);
    }

    if(err == TM_OK)
    {
        /* Set value */
        pObj->I2CMap.uBx11.bF.AGC5_Ana = uValue;

        /* Write byte 0x11 */
        err = ddNT220xWrite(pObj, 0x11, 1);
        tmASSERTExT(err, TM_OK, (DEBUGLVL_ERROR, "ddNT220xWrite(0x%08X) failed.", tUnit));

        (void)ddNT220xMutexRelease(pObj);
    }

    return err;
}

/*============================================================================*/
/* FUNCTION:    tmddNT220xGetAGC5_Ana                                       */
/*                                                                            */
/* DESCRIPTION: Get the AGC5_Ana bit(s) status                                */
/*                                                                            */
/* RETURN:      TM_OK if no error                                             */
/*                                                                            */
/* NOTES:                                                                     */
/*                                                                            */
/*============================================================================*/
tmErrorCode_t
tmddNT220xGetAGC5_Ana
(
    tmUnitSelect_t  tUnit,  /* I: Unit number */
    UInt8*          puValue /* I: Address of the variable to output item value */
)
{
    ptmddNT220xObject_t   pObj = Null;
    tmErrorCode_t           err  = TM_OK;

    /* Test the parameters */
    if (puValue == Null)
        err = ddNT220x_ERR_BAD_PARAMETER;

    if(err == TM_OK)
    {
        /* Get Instance Object */
        err = ddNT220xGetInstance(tUnit, &pObj);
        tmASSERTExT(err, TM_OK, (DEBUGLVL_ERROR, "ddNT220xGetInstance(0x%08X) failed.", tUnit));
    }

    if(err == TM_OK)
    {
        err = ddNT220xMutexAcquire(pObj, ddNT220x_MUTEX_TIMEOUT);
    }

    if(err == TM_OK)
    {
        /* Read byte 0x11 */
        err = ddNT220xRead(pObj, 0x11, 1);
        tmASSERTExT(err, TM_OK, (DEBUGLVL_ERROR, "ddNT220xRead(0x%08X) failed.", tUnit));

        if(err == TM_OK)
        {
            /* Get value */
            *puValue = pObj->I2CMap.uBx11.bF.AGC5_Ana;
        }

        (void)ddNT220xMutexRelease(pObj);
    }
    return err;
}

/*============================================================================*/
/* FUNCTION:    tmddNT220xSetAGC5_TOP                                       */
/*                                                                            */
/* DESCRIPTION: Set the AGC5_TOP bit(s) status                                */
/*                                                                            */
/* RETURN:      TM_OK if no error                                             */
/*                                                                            */
/* NOTES:                                                                     */
/*                                                                            */
/*============================================================================*/
tmErrorCode_t
tmddNT220xSetAGC5_TOP
(
    tmUnitSelect_t  tUnit,  /* I: Unit number */
    UInt8           uValue  /* I: Item value */
)
{
    ptmddNT220xObject_t   pObj = Null;
    tmErrorCode_t           err  = TM_OK;

    /* Get Instance Object */
    err = ddNT220xGetInstance(tUnit, &pObj);
    tmASSERTExT(err, TM_OK, (DEBUGLVL_ERROR, "ddNT220xGetInstance(0x%08X) failed.", tUnit));

    if(err == TM_OK)
    {
        err = ddNT220xMutexAcquire(pObj, ddNT220x_MUTEX_TIMEOUT);
    }

    if(err == TM_OK)
    {
        /* Set value */
        pObj->I2CMap.uBx11.bF.AGC5_TOP = uValue;

        /* Write byte 0x11 */
        err = ddNT220xWrite(pObj, 0x11, 1);
        tmASSERTExT(err, TM_OK, (DEBUGLVL_ERROR, "ddNT220xWrite(0x%08X) failed.", tUnit));

        (void)ddNT220xMutexRelease(pObj);
    }

    return err;
}

/*============================================================================*/
/* FUNCTION:    tmddNT220xGetAGC5_TOP                                       */
/*                                                                            */
/* DESCRIPTION: Get the AGC5_TOP bit(s) status                                */
/*                                                                            */
/* RETURN:      TM_OK if no error                                             */
/*                                                                            */
/* NOTES:                                                                     */
/*                                                                            */
/*============================================================================*/
tmErrorCode_t
tmddNT220xGetAGC5_TOP
(
    tmUnitSelect_t  tUnit,  /* I: Unit number */
    UInt8*          puValue /* I: Address of the variable to output item value */
)
{
    ptmddNT220xObject_t   pObj = Null;
    tmErrorCode_t           err  = TM_OK;

    /* Test the parameters */
    if (puValue == Null)
        err = ddNT220x_ERR_BAD_PARAMETER;

    if(err == TM_OK)
    {
        /* Get Instance Object */
        err = ddNT220xGetInstance(tUnit, &pObj);
        tmASSERTExT(err, TM_OK, (DEBUGLVL_ERROR, "ddNT220xGetInstance(0x%08X) failed.", tUnit));
    }

    if(err == TM_OK)
    {
        err = ddNT220xMutexAcquire(pObj, ddNT220x_MUTEX_TIMEOUT);
    }

    if(err == TM_OK)
    {
        /* Read byte 0x11 */
        err = ddNT220xRead(pObj, 0x11, 1);
        tmASSERTExT(err, TM_OK, (DEBUGLVL_ERROR, "ddNT220xRead(0x%08X) failed.", tUnit));

        if(err == TM_OK)
        {
            /* Get value */
            *puValue = pObj->I2CMap.uBx11.bF.AGC5_TOP;
        }

        (void)ddNT220xMutexRelease(pObj);
    }

    return err;
}

/*============================================================================*/
/* FUNCTION:    tmddNT220xSetIF_Level                                       */
/*                                                                            */
/* DESCRIPTION: Set the IF_level bit(s) status                                */
/*                                                                            */
/* RETURN:      TM_OK if no error                                             */
/*                                                                            */
/* NOTES:                                                                     */
/*                                                                            */
/*============================================================================*/
tmErrorCode_t
tmddNT220xSetIF_Level
(
    tmUnitSelect_t  tUnit,  /* I: Unit number */
    UInt8           uValue  /* I: Item value */
)
{
    ptmddNT220xObject_t   pObj = Null;
    tmErrorCode_t           err  = TM_OK;

    /* Get Instance Object */
    err = ddNT220xGetInstance(tUnit, &pObj);
    tmASSERTExT(err, TM_OK, (DEBUGLVL_ERROR, "ddNT220xGetInstance(0x%08X) failed.", tUnit));

    if(err == TM_OK)
    {
        err = ddNT220xMutexAcquire(pObj, ddNT220x_MUTEX_TIMEOUT);
    }

    if(err == TM_OK)
    {
        /* Set value */
        pObj->I2CMap.uBx12.bF.IF_level = uValue;

        /* Write byte 0x12 */
        err = ddNT220xWrite(pObj, 0x12, 1);
        tmASSERTExT(err, TM_OK, (DEBUGLVL_ERROR, "ddNT220xWrite(0x%08X) failed.", tUnit));

        (void)ddNT220xMutexRelease(pObj);
    }

    return err;
}

/*============================================================================*/
/* FUNCTION:    tmddNT220xGetIF_Level                                       */
/*                                                                            */
/* DESCRIPTION: Get the IF_level bit(s) status                                */
/*                                                                            */
/* RETURN:      TM_OK if no error                                             */
/*                                                                            */
/* NOTES:                                                                     */
/*                                                                            */
/*============================================================================*/
tmErrorCode_t
tmddNT220xGetIF_Level
(
    tmUnitSelect_t  tUnit,  /* I: Unit number */
    UInt8*          puValue /* I: Address of the variable to output item value */
)
{
    ptmddNT220xObject_t   pObj = Null;
    tmErrorCode_t           err  = TM_OK;

    /* Test the parameters */
    if (puValue == Null)
        err = ddNT220x_ERR_BAD_PARAMETER;

    if(err == TM_OK)
    {
        /* Get Instance Object */
        err = ddNT220xGetInstance(tUnit, &pObj);
        tmASSERTExT(err, TM_OK, (DEBUGLVL_ERROR, "ddNT220xGetInstance(0x%08X) failed.", tUnit));
    }

    if(err == TM_OK)
    {
        err = ddNT220xMutexAcquire(pObj, ddNT220x_MUTEX_TIMEOUT);
    }

    if(err == TM_OK)
    {
        /* Read byte 0x12 */
        err = ddNT220xRead(pObj, 0x12, 1);
        tmASSERTExT(err, TM_OK, (DEBUGLVL_ERROR, "ddNT220xRead(0x%08X) failed.", tUnit));

        if(err == TM_OK)
        {
            /* Get value */
            *puValue = pObj->I2CMap.uBx12.bF.IF_level;
        }

        (void)ddNT220xMutexRelease(pObj);
    }

    return err;
}

/*============================================================================*/
/* FUNCTION:    tmddNT220xSetIF_HP_Fc                                       */
/*                                                                            */
/* DESCRIPTION: Set the IF_HP_Fc bit(s) status                                */
/*                                                                            */
/* RETURN:      TM_OK if no error                                             */
/*                                                                            */
/* NOTES:                                                                     */
/*                                                                            */
/*============================================================================*/
tmErrorCode_t
tmddNT220xSetIF_HP_Fc
(
    tmUnitSelect_t  tUnit,  /* I: Unit number */
    UInt8           uValue  /* I: Item value */
)
{
    ptmddNT220xObject_t   pObj = Null;
    tmErrorCode_t           err  = TM_OK;

    /* Get Instance Object */
    err = ddNT220xGetInstance(tUnit, &pObj);
    tmASSERTExT(err, TM_OK, (DEBUGLVL_ERROR, "ddNT220xGetInstance(0x%08X) failed.", tUnit));

    if(err == TM_OK)
    {
        err = ddNT220xMutexAcquire(pObj, ddNT220x_MUTEX_TIMEOUT);
    }

    if(err == TM_OK)
    {
        /* Set value */
        pObj->I2CMap.uBx13.bF.IF_HP_Fc = uValue;

        /* Write byte 0x13 */
        err = ddNT220xWrite(pObj, 0x13, 1);
        tmASSERTExT(err, TM_OK, (DEBUGLVL_ERROR, "ddNT220xWrite(0x%08X) failed.", tUnit));

        (void)ddNT220xMutexRelease(pObj);
    }

    return err;
}

/*============================================================================*/
/* FUNCTION:    tmddNT220xGetIF_HP_Fc                                       */
/*                                                                            */
/* DESCRIPTION: Get the IF_HP_Fc bit(s) status                                */
/*                                                                            */
/* RETURN:      TM_OK if no error                                             */
/*                                                                            */
/* NOTES:                                                                     */
/*                                                                            */
/*============================================================================*/
tmErrorCode_t
tmddNT220xGetIF_HP_Fc
(
    tmUnitSelect_t  tUnit,  /* I: Unit number */
    UInt8*          puValue /* I: Address of the variable to output item value */
)
{
    ptmddNT220xObject_t   pObj = Null;
    tmErrorCode_t           err  = TM_OK;

    /* Test the parameters */
    if (puValue == Null)
        err = ddNT220x_ERR_BAD_PARAMETER;

    if(err == TM_OK)
    {
        /* Get Instance Object */
        err = ddNT220xGetInstance(tUnit, &pObj);
        tmASSERTExT(err, TM_OK, (DEBUGLVL_ERROR, "ddNT220xGetInstance(0x%08X) failed.", tUnit));
    }

    if(err == TM_OK)
    {
        err = ddNT220xMutexAcquire(pObj, ddNT220x_MUTEX_TIMEOUT);
    }

    if(err == TM_OK)
    {
        /* Read byte 0x13 */
        err = ddNT220xRead(pObj, 0x13, 1);
        tmASSERTExT(err, TM_OK, (DEBUGLVL_ERROR, "ddNT220xRead(0x%08X) failed.", tUnit));

        if(err == TM_OK)
        {
            /* Get value */
            *puValue = pObj->I2CMap.uBx13.bF.IF_HP_Fc;
        }

        (void)ddNT220xMutexRelease(pObj);
    }

    return err;
}

/*============================================================================*/
/* FUNCTION:    tmddNT220xSetIF_ATSC_Notch                                  */
/*                                                                            */
/* DESCRIPTION: Set the IF_ATSC_Notch bit(s) status                           */
/*                                                                            */
/* RETURN:      TM_OK if no error                                             */
/*                                                                            */
/* NOTES:                                                                     */
/*                                                                            */
/*============================================================================*/
tmErrorCode_t
tmddNT220xSetIF_ATSC_Notch
(
    tmUnitSelect_t  tUnit,  /* I: Unit number */
    UInt8           uValue  /* I: Item value */
)
{
    ptmddNT220xObject_t   pObj = Null;
    tmErrorCode_t           err  = TM_OK;

    /* Get Instance Object */
    err = ddNT220xGetInstance(tUnit, &pObj);
    tmASSERTExT(err, TM_OK, (DEBUGLVL_ERROR, "ddNT220xGetInstance(0x%08X) failed.", tUnit));

    if(err == TM_OK)
    {
        err = ddNT220xMutexAcquire(pObj, ddNT220x_MUTEX_TIMEOUT);
    }

    if(err == TM_OK)
    {
        /* Set value */
        pObj->I2CMap.uBx13.bF.IF_ATSC_Notch = uValue;

        /* Write byte 0x13 */
        err = ddNT220xWrite(pObj, 0x13, 1);
        tmASSERTExT(err, TM_OK, (DEBUGLVL_ERROR, "ddNT220xWrite(0x%08X) failed.", tUnit));

        (void)ddNT220xMutexRelease(pObj);
    }

    return err;
}

/*============================================================================*/
/* FUNCTION:    tmddNT220xGetIF_ATSC_Notch                                  */
/*                                                                            */
/* DESCRIPTION: Get the IF_ATSC_Notch bit(s) status                           */
/*                                                                            */
/* RETURN:      TM_OK if no error                                             */
/*                                                                            */
/* NOTES:                                                                     */
/*                                                                            */
/*============================================================================*/
tmErrorCode_t
tmddNT220xGetIF_ATSC_Notch
(
    tmUnitSelect_t  tUnit,  /* I: Unit number */
    UInt8*          puValue /* I: Address of the variable to output item value */
)
{
    ptmddNT220xObject_t   pObj = Null;
    tmErrorCode_t           err  = TM_OK;

    /* Test the parameters */
    if (puValue == Null)
        err = ddNT220x_ERR_BAD_PARAMETER;

    if(err == TM_OK)
    {
        /* Get Instance Object */
        err = ddNT220xGetInstance(tUnit, &pObj);
        tmASSERTExT(err, TM_OK, (DEBUGLVL_ERROR, "ddNT220xGetInstance(0x%08X) failed.", tUnit));
    }

    if(err == TM_OK)
    {
        err = ddNT220xMutexAcquire(pObj, ddNT220x_MUTEX_TIMEOUT);
    }

    if(err == TM_OK)
    {
        /* Read byte 0x13 */
        err = ddNT220xRead(pObj, 0x13, 1);
        tmASSERTExT(err, TM_OK, (DEBUGLVL_ERROR, "ddNT220xRead(0x%08X) failed.", tUnit));

        if(err == TM_OK)
        {
            /* Get value */
            *puValue = pObj->I2CMap.uBx13.bF.IF_ATSC_Notch;
        }

        (void)ddNT220xMutexRelease(pObj);
    }
    return err;
}

/*============================================================================*/
/* FUNCTION:    tmddNT220xSetLP_FC_Offset                                   */
/*                                                                            */
/* DESCRIPTION: Set the LP_FC_Offset bit(s) status                            */
/*                                                                            */
/* RETURN:      TM_OK if no error                                             */
/*                                                                            */
/* NOTES:                                                                     */
/*                                                                            */
/*============================================================================*/
tmErrorCode_t
tmddNT220xSetLP_FC_Offset
(
    tmUnitSelect_t  tUnit,  /* I: Unit number */
    UInt8           uValue  /* I: Item value */
)
{
    ptmddNT220xObject_t   pObj = Null;
    tmErrorCode_t           err  = TM_OK;

    /* Get Instance Object */
    err = ddNT220xGetInstance(tUnit, &pObj);
    tmASSERTExT(err, TM_OK, (DEBUGLVL_ERROR, "ddNT220xGetInstance(0x%08X) failed.", tUnit));

    if(err == TM_OK)
    {
        err = ddNT220xMutexAcquire(pObj, ddNT220x_MUTEX_TIMEOUT);
    }

    if(err == TM_OK)
    {
        /* Set value */
        pObj->I2CMap.uBx13.bF.LP_FC_Offset = uValue;

        /* Write byte 0x13 */
        err = ddNT220xWrite(pObj, 0x13, 1);
        tmASSERTExT(err, TM_OK, (DEBUGLVL_ERROR, "ddNT220xWrite(0x%08X) failed.", tUnit));

        (void)ddNT220xMutexRelease(pObj);
    }

    return err;
}

/*============================================================================*/
/* FUNCTION:    tmddNT220xGetLP_FC_Offset                                   */
/*                                                                            */
/* DESCRIPTION: Get the LP_FC_Offset bit(s) status                            */
/*                                                                            */
/* RETURN:      TM_OK if no error                                             */
/*                                                                            */
/* NOTES:                                                                     */
/*                                                                            */
/*============================================================================*/
tmErrorCode_t
tmddNT220xGetLP_FC_Offset
(
    tmUnitSelect_t  tUnit,  /* I: Unit number */
    UInt8*          puValue /* I: Address of the variable to output item value */
)
{
    ptmddNT220xObject_t   pObj = Null;
    tmErrorCode_t           err  = TM_OK;

    /* Test the parameters */
    if (puValue == Null)
        err = ddNT220x_ERR_BAD_PARAMETER;

    if(err == TM_OK)
    {
        /* Get Instance Object */
        err = ddNT220xGetInstance(tUnit, &pObj);
        tmASSERTExT(err, TM_OK, (DEBUGLVL_ERROR, "ddNT220xGetInstance(0x%08X) failed.", tUnit));
    }

    if(err == TM_OK)
    {
        err = ddNT220xMutexAcquire(pObj, ddNT220x_MUTEX_TIMEOUT);
    }

    if(err == TM_OK)
    {
        /* Read byte 0x13 */
        err = ddNT220xRead(pObj, 0x13, 1);
        tmASSERTExT(err, TM_OK, (DEBUGLVL_ERROR, "ddNT220xRead(0x%08X) failed.", tUnit));

        if(err == TM_OK)
        {
            /* Get value */
            *puValue = pObj->I2CMap.uBx13.bF.LP_FC_Offset;
        }

        (void)ddNT220xMutexRelease(pObj);
    }
    return err;
}

/*============================================================================*/
/* FUNCTION:    tmddNT220xSetLP_FC                                          */
/*                                                                            */
/* DESCRIPTION: Set the LP_Fc bit(s) status                                   */
/*                                                                            */
/* RETURN:      TM_OK if no error                                             */
/*                                                                            */
/* NOTES:                                                                     */
/*                                                                            */
/*============================================================================*/
tmErrorCode_t
tmddNT220xSetLP_FC
(
    tmUnitSelect_t  tUnit,  /* I: Unit number */
    UInt8           uValue  /* I: Item value */
)
{
    ptmddNT220xObject_t   pObj = Null;
    tmErrorCode_t           err  = TM_OK;

    /* Get Instance Object */
    err = ddNT220xGetInstance(tUnit, &pObj);
    tmASSERTExT(err, TM_OK, (DEBUGLVL_ERROR, "ddNT220xGetInstance(0x%08X) failed.", tUnit));

    if(err == TM_OK)
    {
        err = ddNT220xMutexAcquire(pObj, ddNT220x_MUTEX_TIMEOUT);
    }

    if(err == TM_OK)
    {
        /* Set value */
        pObj->I2CMap.uBx13.bF.LP_Fc = uValue;

        /* Write byte 0x13 */
        err = ddNT220xWrite(pObj, 0x13, 1);
        tmASSERTExT(err, TM_OK, (DEBUGLVL_ERROR, "ddNT220xWrite(0x%08X) failed.", tUnit));

        (void)ddNT220xMutexRelease(pObj);
    }

    return err;
}

/*============================================================================*/
/* FUNCTION:    tmddNT220xGetLP_FC                                          */
/*                                                                            */
/* DESCRIPTION: Get the LP_Fc bit(s) status                                   */
/*                                                                            */
/* RETURN:      TM_OK if no error                                             */
/*                                                                            */
/* NOTES:                                                                     */
/*                                                                            */
/*============================================================================*/
tmErrorCode_t
tmddNT220xGetLP_FC
(
    tmUnitSelect_t  tUnit,  /* I: Unit number */
    UInt8*          puValue /* I: Address of the variable to output item value */
)
{
    ptmddNT220xObject_t   pObj = Null;
    tmErrorCode_t           err  = TM_OK;

    /* Test the parameters */
    if (puValue == Null)
        err = ddNT220x_ERR_BAD_PARAMETER;

    if(err == TM_OK)
    {
        /* Get Instance Object */
        err = ddNT220xGetInstance(tUnit, &pObj);
        tmASSERTExT(err, TM_OK, (DEBUGLVL_ERROR, "ddNT220xGetInstance(0x%08X) failed.", tUnit));
    }

    if(err == TM_OK)
    {
        err = ddNT220xMutexAcquire(pObj, ddNT220x_MUTEX_TIMEOUT);
    }

    if(err == TM_OK)
    {
        /* Read byte 0x13 */
        err = ddNT220xRead(pObj, 0x13, 1);
        tmASSERTExT(err, TM_OK, (DEBUGLVL_ERROR, "ddNT220xRead(0x%08X) failed.", tUnit));

        if(err == TM_OK)
        {
            /* Get value */
            *puValue = pObj->I2CMap.uBx13.bF.LP_Fc;
        }

        (void)ddNT220xMutexRelease(pObj);
    }

    return err;
}

/*============================================================================*/
/* FUNCTION:    tmddNT220xSetDigital_Clock_Mode                             */
/*                                                                            */
/* DESCRIPTION: Set the Digital_Clock_Mode bit(s) status                      */
/*                                                                            */
/* RETURN:      TM_OK if no error                                             */
/*                                                                            */
/* NOTES:                                                                     */
/*                                                                            */
/*============================================================================*/
tmErrorCode_t
tmddNT220xSetDigital_Clock_Mode
(
    tmUnitSelect_t  tUnit,  /* I: Unit number */
    UInt8           uValue  /* I: Item value */
)
{
    ptmddNT220xObject_t   pObj = Null;
    tmErrorCode_t           err  = TM_OK;

    /* Get Instance Object */
    err = ddNT220xGetInstance(tUnit, &pObj);
    tmASSERTExT(err, TM_OK, (DEBUGLVL_ERROR, "ddNT220xGetInstance(0x%08X) failed.", tUnit));

    if(err == TM_OK)
    {
        err = ddNT220xMutexAcquire(pObj, ddNT220x_MUTEX_TIMEOUT);
    }

    if(err == TM_OK)
    {
        /* Set value */
        pObj->I2CMap.uBx14.bF.Digital_Clock_Mode = uValue;

        /* Write byte 0x14 */
        err = ddNT220xWrite(pObj, 0x14, 1);
        tmASSERTExT(err, TM_OK, (DEBUGLVL_ERROR, "ddNT220xWrite(0x%08X) failed.", tUnit));

        (void)ddNT220xMutexRelease(pObj);
    }

    return err;
}

/*============================================================================*/
/* FUNCTION:    tmddNT220xGetDigital_Clock_Mode                             */
/*                                                                            */
/* DESCRIPTION: Get the Digital_Clock_Mode bit(s) status                      */
/*                                                                            */
/* RETURN:      TM_OK if no error                                             */
/*                                                                            */
/* NOTES:                                                                     */
/*                                                                            */
/*============================================================================*/
tmErrorCode_t
tmddNT220xGetDigital_Clock_Mode
(
    tmUnitSelect_t  tUnit,  /* I: Unit number */
    UInt8*          puValue /* I: Address of the variable to output item value */
)
{
    ptmddNT220xObject_t   pObj = Null;
    tmErrorCode_t           err  = TM_OK;

    /* Test the parameters */
    if (puValue == Null)
        err = ddNT220x_ERR_BAD_PARAMETER;

    if(err == TM_OK)
    {
        /* Get Instance Object */
        err = ddNT220xGetInstance(tUnit, &pObj);
        tmASSERTExT(err, TM_OK, (DEBUGLVL_ERROR, "ddNT220xGetInstance(0x%08X) failed.", tUnit));
    }

    if(err == TM_OK)
    {
        err = ddNT220xMutexAcquire(pObj, ddNT220x_MUTEX_TIMEOUT);
    }

    if(err == TM_OK)
    {
        /* Read byte 0x14 */
        err = ddNT220xRead(pObj, 0x14, 1);
        tmASSERTExT(err, TM_OK, (DEBUGLVL_ERROR, "ddNT220xRead(0x%08X) failed.", tUnit));

        if(err == TM_OK)
        {
            /* Get value */
            *puValue = pObj->I2CMap.uBx14.bF.Digital_Clock_Mode;
        }

        (void)ddNT220xMutexRelease(pObj);
    }

    return err;
}

/*============================================================================*/
/* FUNCTION:    tmddNT220xSetIF_Freq                                        */
/*                                                                            */
/* DESCRIPTION: Set the IF_Freq bit(s) status                                 */
/*                                                                            */
/* RETURN:      TM_OK if no error                                             */
/*                                                                            */
/* NOTES:                                                                     */
/*                                                                            */
/*============================================================================*/
tmErrorCode_t
tmddNT220xSetIF_Freq
(
    tmUnitSelect_t  tUnit,  /* I: Unit number */
    UInt32          uValue  /* I: Item value */
)
{
    ptmddNT220xObject_t   pObj = Null;
    tmErrorCode_t           err  = TM_OK;

    /* Get Instance Object */
    err = ddNT220xGetInstance(tUnit, &pObj);
    tmASSERTExT(err, TM_OK, (DEBUGLVL_ERROR, "ddNT220xGetInstance(0x%08X) failed.", tUnit));

    if(err == TM_OK)
    {
        err = ddNT220xMutexAcquire(pObj, ddNT220x_MUTEX_TIMEOUT);
    }

    if(err == TM_OK)
    {
        /* Set value */
        pObj->I2CMap.uBx15.bF.IF_Freq = (UInt8)(uValue / 50000);

        /* Write byte 0x15 */
        err = ddNT220xWrite(pObj, 0x15, 1);
        tmASSERTExT(err, TM_OK, (DEBUGLVL_ERROR, "ddNT220xWrite(0x%08X) failed.", tUnit));

        (void)ddNT220xMutexRelease(pObj);
    }

    return err;
}

/*============================================================================*/
/* FUNCTION:    tmddNT220xGetIF_Freq                                        */
/*                                                                            */
/* DESCRIPTION: Get the IF_Freq bit(s) status                                 */
/*                                                                            */
/* RETURN:      TM_OK if no error                                             */
/*                                                                            */
/* NOTES:                                                                     */
/*                                                                            */
/*============================================================================*/
tmErrorCode_t
tmddNT220xGetIF_Freq
(
    tmUnitSelect_t  tUnit,  /* I: Unit number */
    UInt32*         puValue /* I: Address of the variable to output item value */
)
{
    ptmddNT220xObject_t   pObj = Null;
    tmErrorCode_t           err  = TM_OK;

    /* Test the parameters */
    if (puValue == Null)
        err = ddNT220x_ERR_BAD_PARAMETER;

    if(err == TM_OK)
    {
        /* Get Instance Object */
        err = ddNT220xGetInstance(tUnit, &pObj);
        tmASSERTExT(err, TM_OK, (DEBUGLVL_ERROR, "ddNT220xGetInstance(0x%08X) failed.", tUnit));
    }

    if(err == TM_OK)
    {
        err = ddNT220xMutexAcquire(pObj, ddNT220x_MUTEX_TIMEOUT);
    }

    if(err == TM_OK)
    {
        /* Read byte 0x15 */
        err = ddNT220xRead(pObj, 0x15, 1);
        tmASSERTExT(err, TM_OK, (DEBUGLVL_ERROR, "ddNT220xRead(0x%08X) failed.", tUnit));

        if(err == TM_OK)
        {
            /* Get value */
            *puValue = pObj->I2CMap.uBx15.bF.IF_Freq * 50000;
        }

        (void)ddNT220xMutexRelease(pObj);
    }

    return err;
}

/*============================================================================*/
/* FUNCTION:    tmddNT220xSetRF_Freq                                        */
/*                                                                            */
/* DESCRIPTION: Set the RF_Freq bit(s) status                                 */
/*                                                                            */
/* RETURN:      TM_OK if no error                                             */
/*                                                                            */
/* NOTES:                                                                     */
/*                                                                            */
/*============================================================================*/
tmErrorCode_t
tmddNT220xSetRF_Freq
(
    tmUnitSelect_t  tUnit,  /* I: Unit number */
    UInt32          uValue  /* I: Item value */
)
{
    ptmddNT220xObject_t   pObj = Null;
    tmErrorCode_t           err  = TM_OK;
    UInt32                  uRF = 0;
	UInt8 uRF_Filter_Gv = 0;

    /* Get Instance Object */
    err = ddNT220xGetInstance(tUnit, &pObj);
    tmASSERTExT(err, TM_OK, (DEBUGLVL_ERROR, "ddNT220xGetInstance(0x%08X) failed.", tUnit));

    if(err == TM_OK)
    {
        err = ddNT220xMutexAcquire(pObj, ddNT220x_MUTEX_TIMEOUT);
    }

    if(err == TM_OK)
    {
        //***************************************
        // Configures the settings that depends on the chosen received TV standard 
        // (standard and demodulator dependent)

        //pObj->I2CMap.uBx0E.AGCK_byte_1 = 0x1F;
        //pObj->I2CMap.uBx0F.RF_AGC_byte = 0x15;
        //pObj->I2CMap.uBx10.IR_Mixer_byte_1 = 0x0A;
        //pObj->I2CMap.uBx11.AGC5_byte_1 = 0x0A;
        //pObj->I2CMap.uBx12.IF_AGC_byte = 0x00;
        //pObj->I2CMap.uBx13.IF_Byte_1 = 0xE0;
        //pObj->I2CMap.uBx14.Reference_Byte = 0x03;
        //pObj->I2CMap.uBx15.IF_Frequency_byte = 0x00;

        ///* Write bytes 0x0E to 0x15
        //err = ddNT220xWrite(pObj, 0x0E, 8);
        //tmASSERTExT(err, TM_OK, (DEBUGLVL_ERROR, "ddNT220xWrite(0x%08X) failed.", tUnit));  
        //***************************************

        /*****************************************/
        /* Tune the settings that depend on the RF input frequency, expressed in kHz.*/
        /* RF filters tuning, PLL locking*/
        /* State reached after 5ms*/

        if(err == TM_OK)
        {
            /* Set IRQ_clear */
            err = tmddNT220xSetIRQ_Clear(tUnit, 0x0C);
            tmASSERTExT(err, TM_OK, (DEBUGLVL_ERROR, "tmddNT220xSetIRQ_clear(0x%08X, 0x0C) failed.", tUnit));
        }

        if(err == TM_OK)
        {
            /* Set power state ON */
            err = tmddNT220xSetPowerState(tUnit, tmddNT220x_PowerNormalMode);
            tmASSERTExT(err, TM_OK, (DEBUGLVL_ERROR, "tmddNT220xSetPowerState(0x%08X, PowerNormalMode) failed.", tUnit));
        }
		/* save RF_Filter_Gv current value */
        if(err == TM_OK)
        {
            err = tmddNT220xGetAGC2_Gain_Read (tUnit, &uRF_Filter_Gv);
            tmASSERTExT(err, TM_OK, (DEBUGLVL_ERROR, "tmddNT220xGetRF_Filter_Gv(0x%08X) failed.", tUnit));
		}
        if(err == TM_OK)
        {	     
			err = tmddNT220xSetRF_Filter_Gv(tUnit, uRF_Filter_Gv );
		}
		if(err == TM_OK)
        {
			err = tmddNT220xSetForce_AGC2_gain(tUnit, 0x1);
		}
		/* smooth RF_Filter_Gv to min value */
        if((err == TM_OK)&&(uRF_Filter_Gv != 0))
        {
			do
			{
			    err = tmddNT220xSetRF_Filter_Gv(tUnit, (UInt8)(uRF_Filter_Gv -1) );
				tmASSERTExT(err, TM_OK, (DEBUGLVL_ERROR, "tmddNT220xGetRF_Filter_Gv(0x%08X) failed.", tUnit));
				if(err == TM_OK)
				{
					err = ddNT220xWait(pObj, 10);
					tmASSERTExT(err, TM_OK, (DEBUGLVL_ERROR, "tmddNT220xSetRF_Filter_Cap(0x%08X) failed.", tUnit));
				}
				uRF_Filter_Gv = uRF_Filter_Gv -1 ;
			}
			while ( uRF_Filter_Gv > 0);
		}
        if(err == TM_OK)
        {
            err = tmddNT220xSetRF_Atten_3dB (tUnit, 0x01);
            tmASSERTExT(err, TM_OK, (DEBUGLVL_ERROR, "tmddNT220xGetRF_Filter_Gv(0x%08X) failed.", tUnit));
		}
        if(err == TM_OK)
        {
            /* Set RF frequency expressed in kHz */
            uRF = uValue / 1000;
            pObj->I2CMap.uBx16.bF.RF_Freq_1 = (UInt8)((uRF & 0x00FF0000) >> 16);
            pObj->I2CMap.uBx17.bF.RF_Freq_2 = (UInt8)((uRF & 0x0000FF00) >> 8);
            pObj->I2CMap.uBx18.bF.RF_Freq_3 = (UInt8)(uRF & 0x000000FF);

            /* write bytes 0x16 to 0x18*/
            err = ddNT220xWrite(pObj, 0x16, 3);
            tmASSERTExT(err, TM_OK, (DEBUGLVL_ERROR, "ddNT220xWrite(0x%08X) failed.", tUnit));
        }

        if(err == TM_OK)
        {
            /* Set & trigger MSM */
            pObj->I2CMap.uBx19.MSM_byte_1 = 0x41;
            pObj->I2CMap.uBx1A.MSM_byte_2 = 0x01;

            /* Write bytes 0x19 to 0x1A */
            err = ddNT220xWrite(pObj, 0x19, 2);
            tmASSERTExT(err, TM_OK, (DEBUGLVL_ERROR, "ddNT220xWrite(0x%08X) failed.", tUnit));

            pObj->I2CMap.uBx1A.MSM_byte_2 = 0x00;
        }
        if(pObj->bIRQWait)
        {
            if(err == TM_OK)
            {
                err = ddNT220xWaitIRQ(pObj, 50, 5, 0x0C);
                tmASSERTExT(err, TM_OK, (DEBUGLVL_ERROR, "ddNT220xWaitIRQ(0x%08X) failed.", tUnit));
            }
        }
        if(err == TM_OK)
        {
            err = tmddNT220xSetRF_Atten_3dB (tUnit, 0x00);
            tmASSERTExT(err, TM_OK, (DEBUGLVL_ERROR, "tmddNT220xGetRF_Filter_Gv(0x%08X) failed.", tUnit));
		}
		err = ddNT220xWait(pObj, 50);
        if(err == TM_OK)
        {
			tmddNT220xSetForce_AGC2_gain(tUnit, 0x0);
		}
        /*****************************************/


        (void)ddNT220xMutexRelease(pObj);
    }

    return err;
}

/*============================================================================*/
/* FUNCTION:    tmddNT220xGetRF_Freq                                        */
/*                                                                            */
/* DESCRIPTION: Get the RF_Freq bit(s) status                                 */
/*                                                                            */
/* RETURN:      TM_OK if no error                                             */
/*                                                                            */
/* NOTES:                                                                     */
/*                                                                            */
/*============================================================================*/
tmErrorCode_t
tmddNT220xGetRF_Freq
(
    tmUnitSelect_t  tUnit,  /* I: Unit number */
    UInt32*         puValue /* I: Address of the variable to output item value */
)
{
    ptmddNT220xObject_t   pObj = Null;
    tmErrorCode_t           err  = TM_OK;

    /* Test the parameters */
    if (puValue == Null)
        err = ddNT220x_ERR_BAD_PARAMETER;

    if(err == TM_OK)
    {
        /* Get Instance Object */
        err = ddNT220xGetInstance(tUnit, &pObj);
        tmASSERTExT(err, TM_OK, (DEBUGLVL_ERROR, "ddNT220xGetInstance(0x%08X) failed.", tUnit));
    }

    if(err == TM_OK)
    {
        err = ddNT220xMutexAcquire(pObj, ddNT220x_MUTEX_TIMEOUT);
    }

    if(err == TM_OK)
    {
        /* Read bytes 0x16 to 0x18 */
        err = ddNT220xRead(pObj, 0x16, 3);
        tmASSERTExT(err, TM_OK, (DEBUGLVL_ERROR, "ddNT220xRead(0x%08X) failed.", tUnit));

        if(err == TM_OK)
        {
            /* Get value */
            *puValue = (pObj->I2CMap.uBx16.bF.RF_Freq_1 << 16) | (pObj->I2CMap.uBx17.bF.RF_Freq_2 << 8) | pObj->I2CMap.uBx18.bF.RF_Freq_3;
            *puValue = *puValue * 1000;
        }

        (void)ddNT220xMutexRelease(pObj);
    }

    return err;
}

/*============================================================================*/
/* FUNCTION:    tmddNT220xSetMSM_Launch                                     */
/*                                                                            */
/* DESCRIPTION: Set the MSM_Launch bit(s) status                              */
/*                                                                            */
/* RETURN:      TM_OK if no error                                             */
/*                                                                            */
/* NOTES:                                                                     */
/*                                                                            */
/*============================================================================*/
tmErrorCode_t
tmddNT220xSetMSM_Launch
(
    tmUnitSelect_t  tUnit   /* I: Unit number */
)
{
    ptmddNT220xObject_t   pObj = Null;
    tmErrorCode_t           err  = TM_OK;

    /* Get Instance Object */
    err = ddNT220xGetInstance(tUnit, &pObj);
    tmASSERTExT(err, TM_OK, (DEBUGLVL_ERROR, "ddNT220xGetInstance(0x%08X) failed.", tUnit));

    if(err == TM_OK)
    {
        err = ddNT220xMutexAcquire(pObj, ddNT220x_MUTEX_TIMEOUT);
    }

    if(err == TM_OK)
    {
        /* Set value */
        pObj->I2CMap.uBx1A.bF.MSM_Launch = 1;

        /* Write byte 0x1A */
        err = ddNT220xWrite(pObj, 0x1A, 1);
        tmASSERTExT(err, TM_OK, (DEBUGLVL_ERROR, "ddNT220xWrite(0x%08X) failed.", tUnit));

        /* reset MSM_Launch (buffer only, no write) */
        pObj->I2CMap.uBx1A.bF.MSM_Launch = 0x00;

        (void)ddNT220xMutexRelease(pObj);
    }

    return err;
}

/*============================================================================*/
/* FUNCTION:    tmddNT220xGetMSM_Launch                                     */
/*                                                                            */
/* DESCRIPTION: Get the MSM_Launch bit(s) status                              */
/*                                                                            */
/* RETURN:      TM_OK if no error                                             */
/*                                                                            */
/* NOTES:                                                                     */
/*                                                                            */
/*============================================================================*/
tmErrorCode_t
tmddNT220xGetMSM_Launch
(
    tmUnitSelect_t  tUnit,  /* I: Unit number */
    UInt8*          puValue /* I: Address of the variable to output item value */
)
{
    ptmddNT220xObject_t   pObj = Null;
    tmErrorCode_t           err  = TM_OK;

    /* Test the parameters */
    if (puValue == Null)
        err = ddNT220x_ERR_BAD_PARAMETER;

    if(err == TM_OK)
    {
        /* Get Instance Object */
        err = ddNT220xGetInstance(tUnit, &pObj);
        tmASSERTExT(err, TM_OK, (DEBUGLVL_ERROR, "ddNT220xGetInstance(0x%08X) failed.", tUnit));
    }

    if(err == TM_OK)
    {
        err = ddNT220xMutexAcquire(pObj, ddNT220x_MUTEX_TIMEOUT);
    }

    if(err == TM_OK)
    {
        /* Read byte 0x1A */
        err = ddNT220xRead(pObj, 0x1A, 1);
        tmASSERTExT(err, TM_OK, (DEBUGLVL_ERROR, "ddNT220xRead(0x%08X) failed.", tUnit));

        if(err == TM_OK)
        {
            /* Get value */
            *puValue = pObj->I2CMap.uBx1A.bF.MSM_Launch;
        }

        (void)ddNT220xMutexRelease(pObj);
    }

    return err;
}

/*============================================================================*/
/* FUNCTION:    tmddNT220xSetPSM_StoB                                       */
/*                                                                            */
/* DESCRIPTION: Set the PSM_StoB bit(s) status                                */
/*                                                                            */
/* RETURN:      TM_OK if no error                                             */
/*                                                                            */
/* NOTES:                                                                     */
/*                                                                            */
/*============================================================================*/
tmErrorCode_t
tmddNT220xSetPSM_StoB
(
    tmUnitSelect_t  tUnit,  /* I: Unit number */
    UInt8           uValue  /* I: Item value */
)
{
    ptmddNT220xObject_t   pObj = Null;
    tmErrorCode_t           err  = TM_OK;

    /* Get Instance Object */
    err = ddNT220xGetInstance(tUnit, &pObj);
    tmASSERTExT(err, TM_OK, (DEBUGLVL_ERROR, "ddNT220xGetInstance(0x%08X) failed.", tUnit));

    if(err == TM_OK)
    {
        err = ddNT220xMutexAcquire(pObj, ddNT220x_MUTEX_TIMEOUT);
    }

    if(err == TM_OK)
    {
        /* Set value */
        pObj->I2CMap.uBx1B.bF.PSM_StoB = uValue;

        /* Read byte 0x1B */
        err = ddNT220xWrite(pObj, 0x1B, 1);
        tmASSERTExT(err, TM_OK, (DEBUGLVL_ERROR, "ddNT220xWrite(0x%08X) failed.", tUnit));

        (void)ddNT220xMutexRelease(pObj);
    }

    return err;
}

/*============================================================================*/
/* FUNCTION:    tmddNT220xGetPSM_StoB                                       */
/*                                                                            */
/* DESCRIPTION: Get the PSM_StoB bit(s) status                                */
/*                                                                            */
/* RETURN:      TM_OK if no error                                             */
/*                                                                            */
/* NOTES:                                                                     */
/*                                                                            */
/*============================================================================*/
tmErrorCode_t
tmddNT220xGetPSM_StoB
(
    tmUnitSelect_t  tUnit,  /* I: Unit number */
    UInt8*          puValue /* I: Address of the variable to output item value */
)
{
    ptmddNT220xObject_t   pObj = Null;
    tmErrorCode_t           err  = TM_OK;

    /* Test the parameters */
    if (puValue == Null)
        err = ddNT220x_ERR_BAD_PARAMETER;

    if(err == TM_OK)
    {
        /* Get Instance Object */
        err = ddNT220xGetInstance(tUnit, &pObj);
        tmASSERTExT(err, TM_OK, (DEBUGLVL_ERROR, "ddNT220xGetInstance(0x%08X) failed.", tUnit));
    }

    if(err == TM_OK)
    {
        err = ddNT220xMutexAcquire(pObj, ddNT220x_MUTEX_TIMEOUT);
    }

    if(err == TM_OK)
    {
        /* Read byte 0x1B */
        err = ddNT220xRead(pObj, 0x1B, 1);
        tmASSERTExT(err, TM_OK, (DEBUGLVL_ERROR, "ddNT220xRead(0x%08X) failed.", tUnit));

        if(err == TM_OK)
        {
            /* Get value */
            *puValue = pObj->I2CMap.uBx1B.bF.PSM_StoB;
        }

        (void)ddNT220xMutexRelease(pObj);
    }

    return err;
}

/*============================================================================*/
/* FUNCTION:    tmddNT220xSetFmax_Lo                                        */
/*                                                                            */
/* DESCRIPTION: Set the Fmax_Lo bit(s) status                                 */
/*                                                                            */
/* RETURN:      TM_OK if no error                                             */
/*                                                                            */
/* NOTES:                                                                     */
/*                                                                            */
/*============================================================================*/
tmErrorCode_t
tmddNT220xSetFmax_Lo
(
    tmUnitSelect_t  tUnit,  //  I: Unit number
    UInt8           uValue  //  I: Item value
)
{
    ptmddNT220xObject_t   pObj = Null;
    tmErrorCode_t           err  = TM_OK;

    /* Get Instance Object */
    err = ddNT220xGetInstance(tUnit, &pObj);
    tmASSERTExT(err, TM_OK, (DEBUGLVL_ERROR, "ddNT220xGetInstance(0x%08X) failed.", tUnit));

    if(err == TM_OK)
    {
        err = ddNT220xMutexAcquire(pObj, ddNT220x_MUTEX_TIMEOUT);
    }

    if(err == TM_OK)
    {
        // set value
        pObj->I2CMap.uBx1D.bF.Fmax_Lo = uValue;

        // read byte 0x1D
        err = ddNT220xWrite(pObj, 0x1D, 1);
        tmASSERTExT(err, TM_OK, (DEBUGLVL_ERROR, "ddNT220xWrite(0x%08X) failed.", tUnit));

        (void)ddNT220xMutexRelease(pObj);
    }

    return err;
}

/*============================================================================*/
/* FUNCTION:    tmddNT220xGetFmax_Lo                                        */
/*                                                                            */
/* DESCRIPTION: Get the Fmax_Lo bit(s) status                                 */
/*                                                                            */
/* RETURN:      TM_OK if no error                                             */
/*                                                                            */
/* NOTES:                                                                     */
/*                                                                            */
/*============================================================================*/
tmErrorCode_t
tmddNT220xGetFmax_Lo
(
 tmUnitSelect_t  tUnit,  //  I: Unit number
 UInt8*          puValue //  I: Address of the variable to output item value
 )
{
    ptmddNT220xObject_t   pObj = Null;
    tmErrorCode_t           err  = TM_OK;

    // test the parameter
    if (puValue == Null)
        err = ddNT220x_ERR_BAD_PARAMETER;

    if(err == TM_OK)
    {
        /* Get Instance Object */
        err = ddNT220xGetInstance(tUnit, &pObj);
        tmASSERTExT(err, TM_OK, (DEBUGLVL_ERROR, "ddNT220xGetInstance(0x%08X) failed.", tUnit));
    }

    if(err == TM_OK)
    {
        err = ddNT220xMutexAcquire(pObj, ddNT220x_MUTEX_TIMEOUT);
    }

    if(err == TM_OK)
    {
        // read byte 0x1D
        err = ddNT220xRead(pObj, 0x1D, 1);
        tmASSERTExT(err, TM_OK, (DEBUGLVL_ERROR, "ddNT220xRead(0x%08X) failed.", tUnit));

        if(err == TM_OK)
        {
            // get value
            *puValue = pObj->I2CMap.uBx1D.bF.Fmax_Lo;
        }

        (void)ddNT220xMutexRelease(pObj);
    }

    return err;
}



/*============================================================================*/
/* FUNCTION:    tmddNT220xSetIR_Loop                                        */
/*                                                                            */
/* DESCRIPTION: Set the IR_Loop bit(s) status                                 */
/*                                                                            */
/* RETURN:      TM_OK if no error                                             */
/*                                                                            */
/* NOTES:                                                                     */
/*                                                                            */
/*============================================================================*/
tmErrorCode_t
tmddNT220xSetIR_Loop
(
    tmUnitSelect_t  tUnit,  /* I: Unit number */
    UInt8           uValue  /* I: Item value */
)
{
    ptmddNT220xObject_t   pObj = Null;
    tmErrorCode_t           err  = TM_OK;

    /* Get Instance Object */
    err = ddNT220xGetInstance(tUnit, &pObj);
    tmASSERTExT(err, TM_OK, (DEBUGLVL_ERROR, "ddNT220xGetInstance(0x%08X) failed.", tUnit));

    if(err == TM_OK)
    {
        err = ddNT220xMutexAcquire(pObj, ddNT220x_MUTEX_TIMEOUT);
    }

    if(err == TM_OK)
    {
        /* Set value */
        pObj->I2CMap.uBx1E.bF.IR_Loop = uValue - 4;

        /* Read byte 0x1E */
        err = ddNT220xWrite(pObj, 0x1E, 1);
        tmASSERTExT(err, TM_OK, (DEBUGLVL_ERROR, "ddNT220xWrite(0x%08X) failed.", tUnit));

        (void)ddNT220xMutexRelease(pObj);
    }

    return err;
}

/*============================================================================*/
/* FUNCTION:    tmddNT220xGetIR_Loop                                        */
/*                                                                            */
/* DESCRIPTION: Get the IR_Loop bit(s) status                                 */
/*                                                                            */
/* RETURN:      TM_OK if no error                                             */
/*                                                                            */
/* NOTES:                                                                     */
/*                                                                            */
/*============================================================================*/
tmErrorCode_t
tmddNT220xGetIR_Loop
(
    tmUnitSelect_t  tUnit,  /* I: Unit number */
    UInt8*          puValue /* I: Address of the variable to output item value */
)
{
    ptmddNT220xObject_t   pObj = Null;
    tmErrorCode_t           err  = TM_OK;

    /* Test the parameters */
    if (puValue == Null)
        err = ddNT220x_ERR_BAD_PARAMETER;

    if(err == TM_OK)
    {
        /* Get Instance Object */
        err = ddNT220xGetInstance(tUnit, &pObj);
        tmASSERTExT(err, TM_OK, (DEBUGLVL_ERROR, "ddNT220xGetInstance(0x%08X) failed.", tUnit));
    }

    if(err == TM_OK)
    {
        err = ddNT220xMutexAcquire(pObj, ddNT220x_MUTEX_TIMEOUT);
    }

    if(err == TM_OK)
    {
        /* Read byte 0x1E */
        err = ddNT220xRead(pObj, 0x1E, 1);
        tmASSERTExT(err, TM_OK, (DEBUGLVL_ERROR, "ddNT220xRead(0x%08X) failed.", tUnit));

        if(err == TM_OK)
        {
            /* Get value */
            *puValue = pObj->I2CMap.uBx1E.bF.IR_Loop + 4;
        }

        (void)ddNT220xMutexRelease(pObj);
    }

    return err;
}

/*============================================================================*/
/* FUNCTION:    tmddNT220xSetIR_Target                                      */
/*                                                                            */
/* DESCRIPTION: Set the IR_Target bit(s) status                               */
/*                                                                            */
/* RETURN:      TM_OK if no error                                             */
/*                                                                            */
/* NOTES:                                                                     */
/*                                                                            */
/*============================================================================*/
tmErrorCode_t
tmddNT220xSetIR_Target
(
    tmUnitSelect_t  tUnit,  /* I: Unit number */
    UInt8           uValue  /* I: Item value */
)
{
    ptmddNT220xObject_t   pObj = Null;
    tmErrorCode_t           err  = TM_OK;

    /* Get Instance Object */
    err = ddNT220xGetInstance(tUnit, &pObj);
    tmASSERTExT(err, TM_OK, (DEBUGLVL_ERROR, "ddNT220xGetInstance(0x%08X) failed.", tUnit));

    if(err == TM_OK)
    {
        err = ddNT220xMutexAcquire(pObj, ddNT220x_MUTEX_TIMEOUT);
    }

    if(err == TM_OK)
    {
        /* Set value */
        pObj->I2CMap.uBx1E.bF.IR_Target = uValue;

        /* Read byte 0x1E */
        err = ddNT220xWrite(pObj, 0x1E, 1);
        tmASSERTExT(err, TM_OK, (DEBUGLVL_ERROR, "ddNT220xWrite(0x%08X) failed.", tUnit));

        (void)ddNT220xMutexRelease(pObj);
    }

    return err;
}

/*============================================================================*/
/* FUNCTION:    tmddNT220xGetIR_Target                                      */
/*                                                                            */
/* DESCRIPTION: Get the IR_Target bit(s) status                               */
/*                                                                            */
/* RETURN:      TM_OK if no error                                             */
/*                                                                            */
/* NOTES:                                                                     */
/*                                                                            */
/*============================================================================*/
tmErrorCode_t
tmddNT220xGetIR_Target
(
    tmUnitSelect_t  tUnit,  /* I: Unit number */
    UInt8*          puValue /* I: Address of the variable to output item value */
)
{
    ptmddNT220xObject_t   pObj = Null;
    tmErrorCode_t           err  = TM_OK;

    /* Test the parameters */
    if (puValue == Null)
        err = ddNT220x_ERR_BAD_PARAMETER;

    if(err == TM_OK)
    {
        /* Get Instance Object */
        err = ddNT220xGetInstance(tUnit, &pObj);
        tmASSERTExT(err, TM_OK, (DEBUGLVL_ERROR, "ddNT220xGetInstance(0x%08X) failed.", tUnit));
    }

    if(err == TM_OK)
    {
        err = ddNT220xMutexAcquire(pObj, ddNT220x_MUTEX_TIMEOUT);
    }

    if(err == TM_OK)
    {
        /* Read byte 0x1E */
        err = ddNT220xRead(pObj, 0x1E, 1);
        tmASSERTExT(err, TM_OK, (DEBUGLVL_ERROR, "ddNT220xRead(0x%08X) failed.", tUnit));

        if(err == TM_OK)
        {
            /* Get value */
            *puValue = pObj->I2CMap.uBx1E.bF.IR_Target;
        }

        (void)ddNT220xMutexRelease(pObj);
    }

    return err;
}

/*============================================================================*/
/* FUNCTION:    tmddNT220xSetIR_Corr_Boost                                  */
/*                                                                            */
/* DESCRIPTION: Set the IR_Corr_Boost bit(s) status                           */
/*                                                                            */
/* RETURN:      TM_OK if no error                                             */
/*                                                                            */
/* NOTES:                                                                     */
/*                                                                            */
/*============================================================================*/
tmErrorCode_t
tmddNT220xSetIR_Corr_Boost
(
    tmUnitSelect_t  tUnit,  /* I: Unit number */
    UInt8           uValue  /* I: Item value */
)
{
    ptmddNT220xObject_t   pObj = Null;
    tmErrorCode_t           err  = TM_OK;

    /* Get Instance Object */
    err = ddNT220xGetInstance(tUnit, &pObj);
    tmASSERTExT(err, TM_OK, (DEBUGLVL_ERROR, "ddNT220xGetInstance(0x%08X) failed.", tUnit));

    if(err == TM_OK)
    {
        err = ddNT220xMutexAcquire(pObj, ddNT220x_MUTEX_TIMEOUT);
    }

    if(err == TM_OK)
    {
        /* Set value */
        pObj->I2CMap.uBx1F.bF.IR_Corr_Boost = uValue;

        /* Read byte 0x1F */
        err = ddNT220xWrite(pObj, 0x1F, 1);
        tmASSERTExT(err, TM_OK, (DEBUGLVL_ERROR, "ddNT220xWrite(0x%08X) failed.", tUnit));

        (void)ddNT220xMutexRelease(pObj);
    }

    return err;
}

/*============================================================================*/
/* FUNCTION:    tmddNT220xGetIR_Corr_Boost                                  */
/*                                                                            */
/* DESCRIPTION: Get the IR_Corr_Boost bit(s) status                           */
/*                                                                            */
/* RETURN:      TM_OK if no error                                             */
/*                                                                            */
/* NOTES:                                                                     */
/*                                                                            */
/*============================================================================*/
tmErrorCode_t
tmddNT220xGetIR_Corr_Boost
(
    tmUnitSelect_t  tUnit,  /* I: Unit number */
    UInt8*          puValue /* I: Address of the variable to output item value */
)
{
    ptmddNT220xObject_t   pObj = Null;
    tmErrorCode_t           err  = TM_OK;

    /* Test the parameters */
    if (puValue == Null)
        err = ddNT220x_ERR_BAD_PARAMETER;

    if(err == TM_OK)
    {
        /* Get Instance Object */
        err = ddNT220xGetInstance(tUnit, &pObj);
        tmASSERTExT(err, TM_OK, (DEBUGLVL_ERROR, "ddNT220xGetInstance(0x%08X) failed.", tUnit));
    }

    if(err == TM_OK)
    {
        err = ddNT220xMutexAcquire(pObj, ddNT220x_MUTEX_TIMEOUT);
    }

    if(err == TM_OK)
    {
        /* Read byte 0x1F */
        err = ddNT220xRead(pObj, 0x1F, 1);
        tmASSERTExT(err, TM_OK, (DEBUGLVL_ERROR, "ddNT220xRead(0x%08X) failed.", tUnit));

        if(err == TM_OK)
        {
            /* Get value */
            *puValue = pObj->I2CMap.uBx1F.bF.IR_Corr_Boost;
        }

        (void)ddNT220xMutexRelease(pObj);
    }

    return err;
}

/*============================================================================*/
/* FUNCTION:    tmddNT220xSetIR_mode_ram_store                              */
/*                                                                            */
/* DESCRIPTION: Set the IR_mode_ram_store bit(s) status                       */
/*                                                                            */
/* RETURN:      TM_OK if no error                                             */
/*                                                                            */
/* NOTES:                                                                     */
/*                                                                            */
/*============================================================================*/
tmErrorCode_t
tmddNT220xSetIR_mode_ram_store
(
    tmUnitSelect_t  tUnit,  /* I: Unit number */
    UInt8           uValue  /* I: Item value */
)
{
    ptmddNT220xObject_t   pObj = Null;
    tmErrorCode_t           err  = TM_OK;

    /* Get Instance Object */
    err = ddNT220xGetInstance(tUnit, &pObj);
    tmASSERTExT(err, TM_OK, (DEBUGLVL_ERROR, "ddNT220xGetInstance(0x%08X) failed.", tUnit));

    if(err == TM_OK)
    {
        err = ddNT220xMutexAcquire(pObj, ddNT220x_MUTEX_TIMEOUT);
    }

    if(err == TM_OK)
    {
        /* Set value */
        pObj->I2CMap.uBx1F.bF.IR_mode_ram_store = uValue;

        /* Write byte 0x1F */
        err = ddNT220xWrite(pObj, 0x1F, 1);
        tmASSERTExT(err, TM_OK, (DEBUGLVL_ERROR, "ddNT220xWrite(0x%08X) failed.", tUnit));

        (void)ddNT220xMutexRelease(pObj);
    }

    return err;
}

/*============================================================================*/
/* FUNCTION:    tmddNT220xGetIR_mode_ram_store                              */
/*                                                                            */
/* DESCRIPTION: Get the IR_mode_ram_store bit(s) status                       */
/*                                                                            */
/* RETURN:      TM_OK if no error                                             */
/*                                                                            */
/* NOTES:                                                                     */
/*                                                                            */
/*============================================================================*/
tmErrorCode_t
tmddNT220xGetIR_mode_ram_store
(
    tmUnitSelect_t  tUnit,  /* I: Unit number */
    UInt8*          puValue /* I: Address of the variable to output item value */
)
{
    ptmddNT220xObject_t   pObj = Null;
    tmErrorCode_t           err  = TM_OK;

    /* Test the parameters */
    if (puValue == Null)
        err = ddNT220x_ERR_BAD_PARAMETER;

    if(err == TM_OK)
    {
        /* Get Instance Object */
        err = ddNT220xGetInstance(tUnit, &pObj);
        tmASSERTExT(err, TM_OK, (DEBUGLVL_ERROR, "ddNT220xGetInstance(0x%08X) failed.", tUnit));
    }

    if(err == TM_OK)
    {
        err = ddNT220xMutexAcquire(pObj, ddNT220x_MUTEX_TIMEOUT);
    }

    if(err == TM_OK)
    {
        /* Read byte 0x1F */
        err = ddNT220xRead(pObj, 0x1F, 1);
        tmASSERTExT(err, TM_OK, (DEBUGLVL_ERROR, "ddNT220xRead(0x%08X) failed.", tUnit));

        if(err == TM_OK)
        {
            /* Get value */
            *puValue = pObj->I2CMap.uBx1F.bF.IR_mode_ram_store;
        }

        (void)ddNT220xMutexRelease(pObj);
    }

    return err;
}

/*============================================================================*/
/* FUNCTION:    tmddNT220xSetPD_Udld                                        */
/*                                                                            */
/* DESCRIPTION: Set the PD_Udld bit(s) status                                 */
/*                                                                            */
/* RETURN:      TM_OK if no error                                             */
/*                                                                            */
/* NOTES:                                                                     */
/*                                                                            */
/*============================================================================*/
tmErrorCode_t
tmddNT220xSetPD_Udld
(
    tmUnitSelect_t  tUnit,  /* I: Unit number */
    UInt8           uValue  /* I: Item value */
)
{
    ptmddNT220xObject_t   pObj = Null;
    tmErrorCode_t           err  = TM_OK;

    /* Get Instance Object */
    err = ddNT220xGetInstance(tUnit, &pObj);
    tmASSERTExT(err, TM_OK, (DEBUGLVL_ERROR, "ddNT220xGetInstance(0x%08X) failed.", tUnit));

    if(err == TM_OK)
    {
        err = ddNT220xMutexAcquire(pObj, ddNT220x_MUTEX_TIMEOUT);
    }

    if(err == TM_OK)
    {
        /* Set value */
        pObj->I2CMap.uBx22.bF.PD_Udld = uValue;

        /* Write byte 0x22 */
        err = ddNT220xWrite(pObj, 0x22, 1);
        tmASSERTExT(err, TM_OK, (DEBUGLVL_ERROR, "ddNT220xWrite(0x%08X) failed.", tUnit));

        (void)ddNT220xMutexRelease(pObj);
    }

    return err;
}

/*============================================================================*/
/* FUNCTION:    tmddNT220xGetPD_Udld                                        */
/*                                                                            */
/* DESCRIPTION: Get the PD_Udld bit(s) status                                 */
/*                                                                            */
/* RETURN:      TM_OK if no error                                             */
/*                                                                            */
/* NOTES:                                                                     */
/*                                                                            */
/*============================================================================*/
tmErrorCode_t
tmddNT220xGetPD_Udld
(
    tmUnitSelect_t  tUnit,  /* I: Unit number */
    UInt8*          puValue /* I: Address of the variable to output item value */
)
{
    ptmddNT220xObject_t   pObj = Null;
    tmErrorCode_t           err  = TM_OK;

    /* Test the parameters */
    if (puValue == Null)
        err = ddNT220x_ERR_BAD_PARAMETER;

    if(err == TM_OK)
    {
        /* Get Instance Object */
        err = ddNT220xGetInstance(tUnit, &pObj);
        tmASSERTExT(err, TM_OK, (DEBUGLVL_ERROR, "ddNT220xGetInstance(0x%08X) failed.", tUnit));
    }

    if(err == TM_OK)
    {
        err = ddNT220xMutexAcquire(pObj, ddNT220x_MUTEX_TIMEOUT);
    }

    if(err == TM_OK)
    {
        /* Read byte 0x22 */
        err = ddNT220xRead(pObj, 0x22, 1);
        tmASSERTExT(err, TM_OK, (DEBUGLVL_ERROR, "ddNT220xRead(0x%08X) failed.", tUnit));

        if(err == TM_OK)
        {
            /* Get value */
            *puValue = pObj->I2CMap.uBx22.bF.PD_Udld;
        }

        (void)ddNT220xMutexRelease(pObj);
    }

    return err;
}

/*============================================================================*/
/* FUNCTION:    tmddNT220xSetAGC_Ovld_TOP                                   */
/*                                                                            */
/* DESCRIPTION: Set the AGC_Ovld_TOP bit(s) status                            */
/*                                                                            */
/* RETURN:      TM_OK if no error                                             */
/*                                                                            */
/* NOTES:                                                                     */
/*                                                                            */
/*============================================================================*/
tmErrorCode_t
tmddNT220xSetAGC_Ovld_TOP
(
    tmUnitSelect_t  tUnit,  /* I: Unit number */
    UInt8           uValue  /* I: Item value */
)
{
    ptmddNT220xObject_t   pObj = Null;
    tmErrorCode_t           err  = TM_OK;

    /* Get Instance Object */
    err = ddNT220xGetInstance(tUnit, &pObj);
    tmASSERTExT(err, TM_OK, (DEBUGLVL_ERROR, "ddNT220xGetInstance(0x%08X) failed.", tUnit));

    if(err == TM_OK)
    {
        err = ddNT220xMutexAcquire(pObj, ddNT220x_MUTEX_TIMEOUT);
    }

    if(err == TM_OK)
    {
        /* Set value */
        pObj->I2CMap.uBx22.bF.AGC_Ovld_TOP = uValue;

        /* Write byte 0x22 */
        err = ddNT220xWrite(pObj, 0x22, 1);
        tmASSERTExT(err, TM_OK, (DEBUGLVL_ERROR, "ddNT220xWrite(0x%08X) failed.", tUnit));

        (void)ddNT220xMutexRelease(pObj);
    }

    return err;
}

/*============================================================================*/
/* FUNCTION:    tmddNT220xGetAGC_Ovld_TOP                                   */
/*                                                                            */
/* DESCRIPTION: Get the AGC_Ovld_TOP bit(s) status                            */
/*                                                                            */
/* RETURN:      TM_OK if no error                                             */
/*                                                                            */
/* NOTES:                                                                     */
/*                                                                            */
/*============================================================================*/
tmErrorCode_t
tmddNT220xGetAGC_Ovld_TOP
(
    tmUnitSelect_t  tUnit,  /* I: Unit number */
    UInt8*          puValue /* I: Address of the variable to output item value */
)
{
    ptmddNT220xObject_t   pObj = Null;
    tmErrorCode_t           err  = TM_OK;

    /* Test the parameters */
    if (puValue == Null)
        err = ddNT220x_ERR_BAD_PARAMETER;

    if(err == TM_OK)
    {
        /* Get Instance Object */
        err = ddNT220xGetInstance(tUnit, &pObj);
        tmASSERTExT(err, TM_OK, (DEBUGLVL_ERROR, "ddNT220xGetInstance(0x%08X) failed.", tUnit));
    }

    if(err == TM_OK)
    {
        err = ddNT220xMutexAcquire(pObj, ddNT220x_MUTEX_TIMEOUT);
    }

    if(err == TM_OK)
    {
        /* Read byte 0x22 */
        err = ddNT220xRead(pObj, 0x22, 1);
        tmASSERTExT(err, TM_OK, (DEBUGLVL_ERROR, "ddNT220xRead(0x%08X) failed.", tUnit));

        if(err == TM_OK)
        {
            /* Get value */
            *puValue = pObj->I2CMap.uBx22.bF.AGC_Ovld_TOP;
        }

        (void)ddNT220xMutexRelease(pObj);
    }

    return err;
}

/*============================================================================*/
/* FUNCTION:    tmddNT220xSetHi_Pass                                        */
/*                                                                            */
/* DESCRIPTION: Set the Hi_Pass bit(s) status                                 */
/*                                                                            */
/* RETURN:      TM_OK if no error                                             */
/*                                                                            */
/* NOTES:                                                                     */
/*                                                                            */
/*============================================================================*/
tmErrorCode_t
tmddNT220xSetHi_Pass
(
    tmUnitSelect_t  tUnit,  /* I: Unit number */
    UInt8           uValue  /* I: Item value */
)
{
    ptmddNT220xObject_t   pObj = Null;
    tmErrorCode_t           err  = TM_OK;

    /* Get Instance Object */
    err = ddNT220xGetInstance(tUnit, &pObj);
    tmASSERTExT(err, TM_OK, (DEBUGLVL_ERROR, "ddNT220xGetInstance(0x%08X) failed.", tUnit));

    if(err == TM_OK)
    {
        err = ddNT220xMutexAcquire(pObj, ddNT220x_MUTEX_TIMEOUT);
    }

    if(err == TM_OK)
    {
        /* Set value */
        pObj->I2CMap.uBx23.bF.Hi_Pass = uValue;

        /* Read byte 0x23 */
        err = ddNT220xWrite(pObj, 0x23, 1);
        tmASSERTExT(err, TM_OK, (DEBUGLVL_ERROR, "ddNT220xWrite(0x%08X) failed.", tUnit));

        (void)ddNT220xMutexRelease(pObj);
    }

    return err;
}

/*============================================================================*/
/* FUNCTION:    tmddNT220xGetHi_Pass                                        */
/*                                                                            */
/* DESCRIPTION: Get the Hi_Pass bit(s) status                                 */
/*                                                                            */
/* RETURN:      TM_OK if no error                                             */
/*                                                                            */
/* NOTES:                                                                     */
/*                                                                            */
/*============================================================================*/
tmErrorCode_t
tmddNT220xGetHi_Pass
(
    tmUnitSelect_t  tUnit,  /* I: Unit number */
    UInt8*          puValue /* I: Address of the variable to output item value */
)
{
    ptmddNT220xObject_t   pObj = Null;
    tmErrorCode_t           err  = TM_OK;

    /* Test the parameters */
    if (puValue == Null)
        err = ddNT220x_ERR_BAD_PARAMETER;

    if(err == TM_OK)
    {
        /* Get Instance Object */
        err = ddNT220xGetInstance(tUnit, &pObj);
        tmASSERTExT(err, TM_OK, (DEBUGLVL_ERROR, "ddNT220xGetInstance(0x%08X) failed.", tUnit));
    }

    if(err == TM_OK)
    {
        err = ddNT220xMutexAcquire(pObj, ddNT220x_MUTEX_TIMEOUT);
    }

    if(err == TM_OK)
    {
        /* Read byte 0x23 */
        err = ddNT220xRead(pObj, 0x23, 1);
        tmASSERTExT(err, TM_OK, (DEBUGLVL_ERROR, "ddNT220xRead(0x%08X) failed.", tUnit));

        if(err == TM_OK)
        {
            /* Get value */
            *puValue = pObj->I2CMap.uBx23.bF.Hi_Pass;
        }

        (void)ddNT220xMutexRelease(pObj);
    }

    return err;
}

/*============================================================================*/
/* FUNCTION:    tmddNT220xSetIF_Notch                                       */
/*                                                                            */
/* DESCRIPTION: Set the IF_Notch bit(s) status                                */
/*                                                                            */
/* RETURN:      TM_OK if no error                                             */
/*                                                                            */
/* NOTES:                                                                     */
/*                                                                            */
/*============================================================================*/
tmErrorCode_t
tmddNT220xSetIF_Notch
(
    tmUnitSelect_t  tUnit,  /* I: Unit number */
    UInt8           uValue  /* I: Item value */
)
{
    ptmddNT220xObject_t   pObj = Null;
    tmErrorCode_t           err  = TM_OK;

    /* Get Instance Object */
    err = ddNT220xGetInstance(tUnit, &pObj);
    tmASSERTExT(err, TM_OK, (DEBUGLVL_ERROR, "ddNT220xGetInstance(0x%08X) failed.", tUnit));

    if(err == TM_OK)
    {
        err = ddNT220xMutexAcquire(pObj, ddNT220x_MUTEX_TIMEOUT);
    }

    if(err == TM_OK)
    {
        /* Set value */
        pObj->I2CMap.uBx23.bF.IF_Notch = uValue;

        /* Read byte 0x23 */
        err = ddNT220xWrite(pObj, 0x23, 1);
        tmASSERTExT(err, TM_OK, (DEBUGLVL_ERROR, "ddNT220xWrite(0x%08X) failed.", tUnit));

        (void)ddNT220xMutexRelease(pObj);
    }

    return err;
}

/*============================================================================*/
/* FUNCTION:    tmddNT220xGetIF_Notch                                       */
/*                                                                            */
/* DESCRIPTION: Get the IF_Notch bit(s) status                                */
/*                                                                            */
/* RETURN:      TM_OK if no error                                             */
/*                                                                            */
/* NOTES:                                                                     */
/*                                                                            */
/*============================================================================*/
tmErrorCode_t
tmddNT220xGetIF_Notch
(
    tmUnitSelect_t  tUnit,  /* I: Unit number */
    UInt8*          puValue /* I: Address of the variable to output item value */
)
{
    ptmddNT220xObject_t   pObj = Null;
    tmErrorCode_t           err  = TM_OK;

    /* Test the parameters */
    if (puValue == Null)
        err = ddNT220x_ERR_BAD_PARAMETER;

    if(err == TM_OK)
    {
        /* Get Instance Object */
        err = ddNT220xGetInstance(tUnit, &pObj);
        tmASSERTExT(err, TM_OK, (DEBUGLVL_ERROR, "ddNT220xGetInstance(0x%08X) failed.", tUnit));
    }

    if(err == TM_OK)
    {
        err = ddNT220xMutexAcquire(pObj, ddNT220x_MUTEX_TIMEOUT);
    }

    if(err == TM_OK)
    {
        /* Read byte 0x23 */
        err = ddNT220xRead(pObj, 0x23, 1);
        tmASSERTExT(err, TM_OK, (DEBUGLVL_ERROR, "ddNT220xRead(0x%08X) failed.", tUnit));

        if(err == TM_OK)
        {
            /* Get value */
            *puValue = pObj->I2CMap.uBx23.bF.IF_Notch;
        }

        (void)ddNT220xMutexRelease(pObj);
    }

    return err;
}

/*============================================================================*/
/* FUNCTION:    tmddNT220xSetAGC5_loop_off                                  */
/*                                                                            */
/* DESCRIPTION: Set the AGC5_loop_off bit(s) status                           */
/*                                                                            */
/* RETURN:      TM_OK if no error                                             */
/*                                                                            */
/* NOTES:                                                                     */
/*                                                                            */
/*============================================================================*/
tmErrorCode_t
tmddNT220xSetAGC5_loop_off
(
    tmUnitSelect_t  tUnit,  /* I: Unit number */
    UInt8           uValue  /* I: Item value */
)
{
    ptmddNT220xObject_t   pObj = Null;
    tmErrorCode_t           err  = TM_OK;

    /* Get Instance Object */
    err = ddNT220xGetInstance(tUnit, &pObj);
    tmASSERTExT(err, TM_OK, (DEBUGLVL_ERROR, "ddNT220xGetInstance(0x%08X) failed.", tUnit));

    if(err == TM_OK)
    {
        err = ddNT220xMutexAcquire(pObj, ddNT220x_MUTEX_TIMEOUT);
    }

    if(err == TM_OK)
    {
        /* Set value */
        pObj->I2CMap.uBx25.bF.AGC5_loop_off = uValue;

        /* Read byte 0x25 */
        err = ddNT220xWrite(pObj, 0x25, 1);
        tmASSERTExT(err, TM_OK, (DEBUGLVL_ERROR, "ddNT220xWrite(0x%08X) failed.", tUnit));

        (void)ddNT220xMutexRelease(pObj);
    }

    return err;
}

/*============================================================================*/
/* FUNCTION:    tmddNT220xGetAGC5_loop_off                                  */
/*                                                                            */
/* DESCRIPTION: Get the AGC5_loop_off bit(s) status                           */
/*                                                                            */
/* RETURN:      TM_OK if no error                                             */
/*                                                                            */
/* NOTES:                                                                     */
/*                                                                            */
/*============================================================================*/
tmErrorCode_t
tmddNT220xGetAGC5_loop_off
(
    tmUnitSelect_t  tUnit,  /* I: Unit number */
    UInt8*          puValue /* I: Address of the variable to output item value */
)
{
    ptmddNT220xObject_t   pObj = Null;
    tmErrorCode_t           err  = TM_OK;

    /* Test the parameters */
    if (puValue == Null)
        err = ddNT220x_ERR_BAD_PARAMETER;

    if(err == TM_OK)
    {
        /* Get Instance Object */
        err = ddNT220xGetInstance(tUnit, &pObj);
        tmASSERTExT(err, TM_OK, (DEBUGLVL_ERROR, "ddNT220xGetInstance(0x%08X) failed.", tUnit));
    }

    if(err == TM_OK)
    {
        err = ddNT220xMutexAcquire(pObj, ddNT220x_MUTEX_TIMEOUT);
    }

    if(err == TM_OK)
    {
        /* Read byte 0x25 */
        err = ddNT220xRead(pObj, 0x25, 1);
        tmASSERTExT(err, TM_OK, (DEBUGLVL_ERROR, "ddNT220xRead(0x%08X) failed.", tUnit));

        if(err == TM_OK)
        {
            /* Get value */
            *puValue = pObj->I2CMap.uBx25.bF.AGC5_loop_off;
        }

        (void)ddNT220xMutexRelease(pObj);
    }
    return err;
}

/*============================================================================*/
/* FUNCTION:    tmddNT220xSetAGC5_Do_step                                   */
/*                                                                            */
/* DESCRIPTION: Set the AGC5_Do_step bit(s) status                            */
/*                                                                            */
/* RETURN:      TM_OK if no error                                             */
/*                                                                            */
/* NOTES:                                                                     */
/*                                                                            */
/*============================================================================*/
tmErrorCode_t
tmddNT220xSetAGC5_Do_step
(
    tmUnitSelect_t  tUnit,  /* I: Unit number */
    UInt8           uValue  /* I: Item value */
)
{
    ptmddNT220xObject_t   pObj = Null;
    tmErrorCode_t           err  = TM_OK;

    /* Get Instance Object */
    err = ddNT220xGetInstance(tUnit, &pObj);
    tmASSERTExT(err, TM_OK, (DEBUGLVL_ERROR, "ddNT220xGetInstance(0x%08X) failed.", tUnit));

    if(err == TM_OK)
    {
        err = ddNT220xMutexAcquire(pObj, ddNT220x_MUTEX_TIMEOUT);
    }

    if(err == TM_OK)
    {
        /* Set value */
        pObj->I2CMap.uBx25.bF.AGC5_Do_step = uValue;

        /* Read byte 0x25 */
        err = ddNT220xWrite(pObj, 0x25, 1);
        tmASSERTExT(err, TM_OK, (DEBUGLVL_ERROR, "ddNT220xWrite(0x%08X) failed.", tUnit));

        (void)ddNT220xMutexRelease(pObj);
    }

    return err;
}

/*============================================================================*/
/* FUNCTION:    tmddNT220xGetAGC5_Do_step                                   */
/*                                                                            */
/* DESCRIPTION: Get the AGC5_Do_step bit(s) status                            */
/*                                                                            */
/* RETURN:      TM_OK if no error                                             */
/*                                                                            */
/* NOTES:                                                                     */
/*                                                                            */
/*============================================================================*/
tmErrorCode_t
tmddNT220xGetAGC5_Do_step
(
    tmUnitSelect_t  tUnit,  /* I: Unit number */
    UInt8*          puValue /* I: Address of the variable to output item value */
)
{
    ptmddNT220xObject_t   pObj = Null;
    tmErrorCode_t           err  = TM_OK;

    /* Test the parameters */
    if (puValue == Null)
        err = ddNT220x_ERR_BAD_PARAMETER;

    if(err == TM_OK)
    {
        /* Get Instance Object */
        err = ddNT220xGetInstance(tUnit, &pObj);
        tmASSERTExT(err, TM_OK, (DEBUGLVL_ERROR, "ddNT220xGetInstance(0x%08X) failed.", tUnit));
    }

    if(err == TM_OK)
    {
        err = ddNT220xMutexAcquire(pObj, ddNT220x_MUTEX_TIMEOUT);
    }

    if(err == TM_OK)
    {
        /* Read byte 0x25 */
        err = ddNT220xRead(pObj, 0x25, 1);
        tmASSERTExT(err, TM_OK, (DEBUGLVL_ERROR, "ddNT220xRead(0x%08X) failed.", tUnit));

        if(err == TM_OK)
        {
            /* Get value */
            *puValue = pObj->I2CMap.uBx25.bF.AGC5_Do_step;
        }

        (void)ddNT220xMutexRelease(pObj);
    }

    return err;
}

/*============================================================================*/
/* FUNCTION:    tmddNT220xSetForce_AGC5_gain                                */
/*                                                                            */
/* DESCRIPTION: Set the Force_AGC5_gain bit(s) status                         */
/*                                                                            */
/* RETURN:      TM_OK if no error                                             */
/*                                                                            */
/* NOTES:                                                                     */
/*                                                                            */
/*============================================================================*/
tmErrorCode_t
tmddNT220xSetForce_AGC5_gain
(
    tmUnitSelect_t  tUnit,  /* I: Unit number */
    UInt8           uValue  /* I: Item value */
)
{
    ptmddNT220xObject_t   pObj = Null;
    tmErrorCode_t           err  = TM_OK;

    /* Get Instance Object */
    err = ddNT220xGetInstance(tUnit, &pObj);
    tmASSERTExT(err, TM_OK, (DEBUGLVL_ERROR, "ddNT220xGetInstance(0x%08X) failed.", tUnit));

    if(err == TM_OK)
    {
        err = ddNT220xMutexAcquire(pObj, ddNT220x_MUTEX_TIMEOUT);
    }

    if(err == TM_OK)
    {
        /* Set value */
        pObj->I2CMap.uBx25.bF.Force_AGC5_gain = uValue;

        /* Read byte 0x25 */
        err = ddNT220xWrite(pObj, 0x25, 1);
        tmASSERTExT(err, TM_OK, (DEBUGLVL_ERROR, "ddNT220xWrite(0x%08X) failed.", tUnit));

        (void)ddNT220xMutexRelease(pObj);
    }

    return err;
}

/*============================================================================*/
/* FUNCTION:    tmddNT220xGetForce_AGC5_gain                                */
/*                                                                            */
/* DESCRIPTION: Get the Force_AGC5_gain bit(s) status                         */
/*                                                                            */
/* RETURN:      TM_OK if no error                                             */
/*                                                                            */
/* NOTES:                                                                     */
/*                                                                            */
/*============================================================================*/
tmErrorCode_t
tmddNT220xGetForce_AGC5_gain
(
    tmUnitSelect_t  tUnit,  /* I: Unit number */
    UInt8*          puValue /* I: Address of the variable to output item value */
)
{
    ptmddNT220xObject_t   pObj = Null;
    tmErrorCode_t           err  = TM_OK;

    /* Test the parameters */
    if (puValue == Null)
        err = ddNT220x_ERR_BAD_PARAMETER;

    if(err == TM_OK)
    {
        /* Get Instance Object */
        err = ddNT220xGetInstance(tUnit, &pObj);
        tmASSERTExT(err, TM_OK, (DEBUGLVL_ERROR, "ddNT220xGetInstance(0x%08X) failed.", tUnit));
    }

    if(err == TM_OK)
    {
        err = ddNT220xMutexAcquire(pObj, ddNT220x_MUTEX_TIMEOUT);
    }

    if(err == TM_OK)
    {
        /* Read byte 0x25 */
        err = ddNT220xRead(pObj, 0x25, 1);
        tmASSERTExT(err, TM_OK, (DEBUGLVL_ERROR, "ddNT220xRead(0x%08X) failed.", tUnit));

        if(err == TM_OK)
        {
            /* Get value */
            *puValue = pObj->I2CMap.uBx25.bF.Force_AGC5_gain;
        }

        (void)ddNT220xMutexRelease(pObj);
    }

    return err;
}

/*============================================================================*/
/* FUNCTION:    tmddNT220xSetAGC5_Gain                                      */
/*                                                                            */
/* DESCRIPTION: Set the AGC5_Gain bit(s) status                               */
/*                                                                            */
/* RETURN:      TM_OK if no error                                             */
/*                                                                            */
/* NOTES:                                                                     */
/*                                                                            */
/*============================================================================*/
tmErrorCode_t
tmddNT220xSetAGC5_Gain
(
    tmUnitSelect_t  tUnit,  /* I: Unit number */
    UInt8           uValue  /* I: Item value */
)
{
    ptmddNT220xObject_t   pObj = Null;
    tmErrorCode_t           err  = TM_OK;

    /* Get Instance Object */
    err = ddNT220xGetInstance(tUnit, &pObj);
    tmASSERTExT(err, TM_OK, (DEBUGLVL_ERROR, "ddNT220xGetInstance(0x%08X) failed.", tUnit));

    if(err == TM_OK)
    {
        err = ddNT220xMutexAcquire(pObj, ddNT220x_MUTEX_TIMEOUT);
    }

    if(err == TM_OK)
    {
        /* Set value */
        pObj->I2CMap.uBx25.bF.AGC5_Gain = uValue;

        /* Read byte 0x25 */
        err = ddNT220xWrite(pObj, 0x25, 1);
        tmASSERTExT(err, TM_OK, (DEBUGLVL_ERROR, "ddNT220xWrite(0x%08X) failed.", tUnit));

        (void)ddNT220xMutexRelease(pObj);
    }

    return err;
}

/*============================================================================*/
/* FUNCTION:    tmddNT220xGetAGC5_Gain                                      */
/*                                                                            */
/* DESCRIPTION: Get the AGC5_Gain bit(s) status                               */
/*                                                                            */
/* RETURN:      TM_OK if no error                                             */
/*                                                                            */
/* NOTES:                                                                     */
/*                                                                            */
/*============================================================================*/
tmErrorCode_t
tmddNT220xGetAGC5_Gain
(
    tmUnitSelect_t  tUnit,  /* I: Unit number */
    UInt8*          puValue /* I: Address of the variable to output item value */
)
{
    ptmddNT220xObject_t   pObj = Null;
    tmErrorCode_t           err  = TM_OK;

    /* Test the parameters */
    if (puValue == Null)
        err = ddNT220x_ERR_BAD_PARAMETER;

    if(err == TM_OK)
    {
        /* Get Instance Object */
        err = ddNT220xGetInstance(tUnit, &pObj);
        tmASSERTExT(err, TM_OK, (DEBUGLVL_ERROR, "ddNT220xGetInstance(0x%08X) failed.", tUnit));
    }

    if(err == TM_OK)
    {
        err = ddNT220xMutexAcquire(pObj, ddNT220x_MUTEX_TIMEOUT);
    }

    if(err == TM_OK)
    {
        /* Read byte 0x25 */
        err = ddNT220xRead(pObj, 0x25, 1);
        tmASSERTExT(err, TM_OK, (DEBUGLVL_ERROR, "ddNT220xRead(0x%08X) failed.", tUnit));

        if(err == TM_OK)
        {
            /* Get value */
            *puValue = pObj->I2CMap.uBx25.bF.AGC5_Gain;
        }

        (void)ddNT220xMutexRelease(pObj);
    }

    return err;
}

/*============================================================================*/
/* FUNCTION:    tmddNT220xSetRF_Filter_Bypass                               */
/*                                                                            */
/* DESCRIPTION: Set the RF_Filter_Bypass bit(s) status                        */
/*                                                                            */
/* RETURN:      TM_OK if no error                                             */
/*                                                                            */
/* NOTES:                                                                     */
/*                                                                            */
/*============================================================================*/
tmErrorCode_t
tmddNT220xSetRF_Filter_Bypass
(
    tmUnitSelect_t  tUnit,  /* I: Unit number */
    UInt8           uValue  /* I: Item value */
)
{
    ptmddNT220xObject_t   pObj = Null;
    tmErrorCode_t           err  = TM_OK;

    /* Get Instance Object */
    err = ddNT220xGetInstance(tUnit, &pObj);
    tmASSERTExT(err, TM_OK, (DEBUGLVL_ERROR, "ddNT220xGetInstance(0x%08X) failed.", tUnit));

    if(err == TM_OK)
    {
        err = ddNT220xMutexAcquire(pObj, ddNT220x_MUTEX_TIMEOUT);
    }

    if(err == TM_OK)
    {
        /* Set value */
        pObj->I2CMap.uBx2C.bF.RF_Filter_Bypass = uValue;

        /* Read byte 0x2C */
        err = ddNT220xWrite(pObj, 0x2C, 1);
        tmASSERTExT(err, TM_OK, (DEBUGLVL_ERROR, "ddNT220xWrite(0x%08X) failed.", tUnit));

        (void)ddNT220xMutexRelease(pObj);
    }

    return err;
}

/*============================================================================*/
/* FUNCTION:    tmddNT220xGetRF_Filter_Bypass                               */
/*                                                                            */
/* DESCRIPTION: Get the RF_Filter_Bypass bit(s) status                        */
/*                                                                            */
/* RETURN:      TM_OK if no error                                             */
/*                                                                            */
/* NOTES:                                                                     */
/*                                                                            */
/*============================================================================*/
tmErrorCode_t
tmddNT220xGetRF_Filter_Bypass
(
    tmUnitSelect_t  tUnit,  /* I: Unit number */
    UInt8*          puValue /* I: Address of the variable to output item value */
)
{
    ptmddNT220xObject_t   pObj = Null;
    tmErrorCode_t           err  = TM_OK;

    /* Test the parameters */
    if (puValue == Null)
        err = ddNT220x_ERR_BAD_PARAMETER;

    if(err == TM_OK)
    {
        /* Get Instance Object */
        err = ddNT220xGetInstance(tUnit, &pObj);
        tmASSERTExT(err, TM_OK, (DEBUGLVL_ERROR, "ddNT220xGetInstance(0x%08X) failed.", tUnit));
    }

    if(err == TM_OK)
    {
        err = ddNT220xMutexAcquire(pObj, ddNT220x_MUTEX_TIMEOUT);
    }

    if(err == TM_OK)
    {
        /* Read byte 0x2C */
        err = ddNT220xRead(pObj, 0x2C, 1);
        tmASSERTExT(err, TM_OK, (DEBUGLVL_ERROR, "ddNT220xRead(0x%08X) failed.", tUnit));

        if(err == TM_OK)
        {
            /* Get value */
            *puValue = pObj->I2CMap.uBx2C.bF.RF_Filter_Bypass;
        }

        (void)ddNT220xMutexRelease(pObj);
    }

    return err;
}

/*============================================================================*/
/* FUNCTION:    tmddNT220xSetRF_Filter_Band                                 */
/*                                                                            */
/* DESCRIPTION: Set the RF_Filter_Band bit(s) status                          */
/*                                                                            */
/* RETURN:      TM_OK if no error                                             */
/*                                                                            */
/* NOTES:                                                                     */
/*                                                                            */
/*============================================================================*/
tmErrorCode_t
tmddNT220xSetRF_Filter_Band
(
    tmUnitSelect_t  tUnit,  /* I: Unit number */
    UInt8           uValue  /* I: Item value */
)
{
    ptmddNT220xObject_t   pObj = Null;
    tmErrorCode_t           err  = TM_OK;

    /* Get Instance Object */
    err = ddNT220xGetInstance(tUnit, &pObj);
    tmASSERTExT(err, TM_OK, (DEBUGLVL_ERROR, "ddNT220xGetInstance(0x%08X) failed.", tUnit));

    if(err == TM_OK)
    {
        err = ddNT220xMutexAcquire(pObj, ddNT220x_MUTEX_TIMEOUT);
    }

    if(err == TM_OK)
    {
        /* Set value */
        pObj->I2CMap.uBx2C.bF.RF_Filter_Band = uValue;

        /* Read byte 0x2C */
        err = ddNT220xWrite(pObj, 0x2C, 1);
        tmASSERTExT(err, TM_OK, (DEBUGLVL_ERROR, "ddNT220xWrite(0x%08X) failed.", tUnit));

        (void)ddNT220xMutexRelease(pObj);
    }

    return err;
}

/*============================================================================*/
/* FUNCTION:    tmddNT220xGetRF_Filter_Band                                 */
/*                                                                            */
/* DESCRIPTION: Get the RF_Filter_Band bit(s) status                          */
/*                                                                            */
/* RETURN:      TM_OK if no error                                             */
/*                                                                            */
/* NOTES:                                                                     */
/*                                                                            */
/*============================================================================*/
tmErrorCode_t
tmddNT220xGetRF_Filter_Band
(
    tmUnitSelect_t  tUnit,  /* I: Unit number */
    UInt8*          puValue /* I: Address of the variable to output item value */
)
{
    ptmddNT220xObject_t   pObj = Null;
    tmErrorCode_t           err  = TM_OK;

    /* Test the parameters */
    if (puValue == Null)
        err = ddNT220x_ERR_BAD_PARAMETER;

    if(err == TM_OK)
    {
        /* Get Instance Object */
        err = ddNT220xGetInstance(tUnit, &pObj);
        tmASSERTExT(err, TM_OK, (DEBUGLVL_ERROR, "ddNT220xGetInstance(0x%08X) failed.", tUnit));
    }

    if(err == TM_OK)
    {
        err = ddNT220xMutexAcquire(pObj, ddNT220x_MUTEX_TIMEOUT);
    }

    if(err == TM_OK)
    {
        /* Read byte 0x2C */
        err = ddNT220xRead(pObj, 0x2C, 1);
        tmASSERTExT(err, TM_OK, (DEBUGLVL_ERROR, "ddNT220xRead(0x%08X) failed.", tUnit));

        if(err == TM_OK)
        {
            /* Get value */
            *puValue = pObj->I2CMap.uBx2C.bF.RF_Filter_Band;
        }

        (void)ddNT220xMutexRelease(pObj);
    }

    return err;
}

/*============================================================================*/
/* FUNCTION:    tmddNT220xSetRF_Filter_Cap                                  */
/*                                                                            */
/* DESCRIPTION: Set the RF_Filter_Cap bit(s) status                           */
/*                                                                            */
/* RETURN:      TM_OK if no error                                             */
/*                                                                            */
/* NOTES:                                                                     */
/*                                                                            */
/*============================================================================*/
tmErrorCode_t
tmddNT220xSetRF_Filter_Cap
(
    tmUnitSelect_t  tUnit,  /* I: Unit number */
    UInt8           uValue  /* I: Item value */
)
{
    ptmddNT220xObject_t   pObj = Null;
    tmErrorCode_t           err  = TM_OK;

    /* Get Instance Object */
    err = ddNT220xGetInstance(tUnit, &pObj);
    tmASSERTExT(err, TM_OK, (DEBUGLVL_ERROR, "ddNT220xGetInstance(0x%08X) failed.", tUnit));

    if(err == TM_OK)
    {
        err = ddNT220xMutexAcquire(pObj, ddNT220x_MUTEX_TIMEOUT);
    }

    if(err == TM_OK)
    {
        /* Set value */
        pObj->I2CMap.uBx2D.bF.RF_Filter_Cap = uValue;

        /* Read byte 0x2D */
        err = ddNT220xWrite(pObj, 0x2D, 1);
        tmASSERTExT(err, TM_OK, (DEBUGLVL_ERROR, "ddNT220xWrite(0x%08X) failed.", tUnit));

        (void)ddNT220xMutexRelease(pObj);
    }

    return err;
}

/*============================================================================*/
/* FUNCTION:    tmddNT220xGetRF_Filter_Cap                                  */
/*                                                                            */
/* DESCRIPTION: Get the RF_Filter_Cap bit(s) status                           */
/*                                                                            */
/* RETURN:      TM_OK if no error                                             */
/*                                                                            */
/* NOTES:                                                                     */
/*                                                                            */
/*============================================================================*/
tmErrorCode_t
tmddNT220xGetRF_Filter_Cap
(
    tmUnitSelect_t  tUnit,  /* I: Unit number */
    UInt8*          puValue /* I: Address of the variable to output item value */
)
{
    ptmddNT220xObject_t   pObj = Null;
    tmErrorCode_t           err  = TM_OK;

    /* Test the parameters */
    if (puValue == Null)
        err = ddNT220x_ERR_BAD_PARAMETER;

    if(err == TM_OK)
    {
        /* Get Instance Object */
        err = ddNT220xGetInstance(tUnit, &pObj);
        tmASSERTExT(err, TM_OK, (DEBUGLVL_ERROR, "ddNT220xGetInstance(0x%08X) failed.", tUnit));
    }

    if(err == TM_OK)
    {
        err = ddNT220xMutexAcquire(pObj, ddNT220x_MUTEX_TIMEOUT);
    }

    if(err == TM_OK)
    {
        /* Read byte 0x2D */
        err = ddNT220xRead(pObj, 0x2D, 1);
        tmASSERTExT(err, TM_OK, (DEBUGLVL_ERROR, "ddNT220xRead(0x%08X) failed.", tUnit));

        if(err == TM_OK)
        {
            /* Get value */
            *puValue = pObj->I2CMap.uBx2D.bF.RF_Filter_Cap;
        }

        (void)ddNT220xMutexRelease(pObj);
    }

    return err;
}

/*============================================================================*/
/* FUNCTION:    tmddNT220xSetGain_Taper                                     */
/*                                                                            */
/* DESCRIPTION: Set the Gain_Taper bit(s) status                              */
/*                                                                            */
/* RETURN:      TM_OK if no error                                             */
/*                                                                            */
/* NOTES:                                                                     */
/*                                                                            */
/*============================================================================*/
tmErrorCode_t
tmddNT220xSetGain_Taper
(
    tmUnitSelect_t  tUnit,  /* I: Unit number */
    UInt8           uValue  /* I: Item value */
)
{
    ptmddNT220xObject_t   pObj = Null;
    tmErrorCode_t           err  = TM_OK;

    /* Get Instance Object */
    err = ddNT220xGetInstance(tUnit, &pObj);
    tmASSERTExT(err, TM_OK, (DEBUGLVL_ERROR, "ddNT220xGetInstance(0x%08X) failed.", tUnit));

    if(err == TM_OK)
    {
        err = ddNT220xMutexAcquire(pObj, ddNT220x_MUTEX_TIMEOUT);
    }

    if(err == TM_OK)
    {
        /* Set value */
        pObj->I2CMap.uBx2E.bF.Gain_Taper = uValue;

        /* Read byte 0x2E */
        err = ddNT220xWrite(pObj, 0x2E, 1);
        tmASSERTExT(err, TM_OK, (DEBUGLVL_ERROR, "ddNT220xWrite(0x%08X) failed.", tUnit));

        (void)ddNT220xMutexRelease(pObj);
    }

    return err;
}

/*============================================================================*/
/* FUNCTION:    tmddNT220xGetGain_Taper                                     */
/*                                                                            */
/* DESCRIPTION: Get the Gain_Taper bit(s) status                              */
/*                                                                            */
/* RETURN:      TM_OK if no error                                             */
/*                                                                            */
/* NOTES:                                                                     */
/*                                                                            */
/*============================================================================*/
tmErrorCode_t
tmddNT220xGetGain_Taper
(
    tmUnitSelect_t  tUnit,  /* I: Unit number */
    UInt8*          puValue /* I: Address of the variable to output item value */
)
{
    ptmddNT220xObject_t   pObj = Null;
    tmErrorCode_t           err  = TM_OK;

    /* Test the parameters */
    if( puValue == Null )
    {
        err = ddNT220x_ERR_BAD_PARAMETER;
    }

    /* Get Instance Object */
    if(err == TM_OK)
    {
        err = ddNT220xGetInstance(tUnit, &pObj);
        tmASSERTExT(err, TM_OK, (DEBUGLVL_ERROR, "ddNT220xGetInstance(0x%08X) failed.", tUnit));
    }

    if(err == TM_OK)
    {
        err = ddNT220xMutexAcquire(pObj, ddNT220x_MUTEX_TIMEOUT);

        if(err == TM_OK)
        {
            /* Read byte 0x2E */
            err = ddNT220xRead(pObj, 0x2E, 1);
            tmASSERTExT(err, TM_OK, (DEBUGLVL_ERROR, "ddNT220xRead(0x%08X) failed.", tUnit));
    
            if(err == TM_OK)
            {
                /* Get value */
                *puValue = pObj->I2CMap.uBx2E.bF.Gain_Taper;
            }
    
            (void)ddNT220xMutexRelease(pObj);
        }
    }
    
    return err;
}

/*============================================================================*/
/* FUNCTION:    tmddNT220xSetN_CP_Current                                   */
/*                                                                            */
/* DESCRIPTION: Set the N_CP_Current bit(s) status                            */
/*                                                                            */
/* RETURN:      TM_OK if no error                                             */
/*                                                                            */
/* NOTES:                                                                     */
/*                                                                            */
/*============================================================================*/
tmErrorCode_t
tmddNT220xSetN_CP_Current
(
    tmUnitSelect_t  tUnit,  /* I: Unit number */
    UInt8           uValue  /* I: Item value */
)
{
    ptmddNT220xObject_t   pObj = Null;
    tmErrorCode_t           err  = TM_OK;

    /* Get Instance Object */
    err = ddNT220xGetInstance(tUnit, &pObj);
    tmASSERTExT(err, TM_OK, (DEBUGLVL_ERROR, "ddNT220xGetInstance(0x%08X) failed.", tUnit));

    if(err == TM_OK)
    {
        err = ddNT220xMutexAcquire(pObj, ddNT220x_MUTEX_TIMEOUT);
    }

    if(err == TM_OK)
    {
        /* Set value */
        pObj->I2CMap.uBx30.bF.N_CP_Current = uValue;

        /* Read byte 0x30 */
        err = ddNT220xWrite(pObj, 0x30, 1);
        tmASSERTExT(err, TM_OK, (DEBUGLVL_ERROR, "ddNT220xWrite(0x%08X) failed.", tUnit));

        (void)ddNT220xMutexRelease(pObj);
    }

    return err;
}

/*============================================================================*/
/* FUNCTION:    tmddNT220xGetN_CP_Current                                   */
/*                                                                            */
/* DESCRIPTION: Get the N_CP_Current bit(s) status                            */
/*                                                                            */
/* RETURN:      TM_OK if no error                                             */
/*                                                                            */
/* NOTES:                                                                     */
/*                                                                            */
/*============================================================================*/
tmErrorCode_t
tmddNT220xGetN_CP_Current
(
    tmUnitSelect_t  tUnit,  /* I: Unit number */
    UInt8*          puValue /* I: Address of the variable to output item value */
)
{
    ptmddNT220xObject_t   pObj = Null;
    tmErrorCode_t           err  = TM_OK;

    /* Test the parameters */
    if (puValue == Null)
        err = ddNT220x_ERR_BAD_PARAMETER;

    if(err == TM_OK)
    {
        /* Get Instance Object */
        err = ddNT220xGetInstance(tUnit, &pObj);
        tmASSERTExT(err, TM_OK, (DEBUGLVL_ERROR, "ddNT220xGetInstance(0x%08X) failed.", tUnit));
    }

    if(err == TM_OK)
    {
        err = ddNT220xMutexAcquire(pObj, ddNT220x_MUTEX_TIMEOUT);
    }

    if(err == TM_OK)
    {
        /* Read byte 0x30 */
        err = ddNT220xRead(pObj, 0x30, 1);
        tmASSERTExT(err, TM_OK, (DEBUGLVL_ERROR, "ddNT220xRead(0x%08X) failed.", tUnit));

        if(err == TM_OK)
        {
            /* Get value */
            *puValue = pObj->I2CMap.uBx30.bF.N_CP_Current;
        }

        (void)ddNT220xMutexRelease(pObj);
    }

    return err;
}

/*============================================================================*/
/* FUNCTION:    tmddNT220xSetRSSI_Ck_Speed                                  */
/*                                                                            */
/* DESCRIPTION: Set the RSSI_Ck_Speed bit(s) status                           */
/*                                                                            */
/* RETURN:      TM_OK if no error                                             */
/*                                                                            */
/* NOTES:                                                                     */
/*                                                                            */
/*============================================================================*/
tmErrorCode_t
tmddNT220xSetRSSI_Ck_Speed
(
    tmUnitSelect_t  tUnit,  /* I: Unit number */
    UInt8           uValue  /* I: Item value */
)
{
    ptmddNT220xObject_t   pObj = Null;
    tmErrorCode_t           err  = TM_OK;

    /* Get Instance Object */
    err = ddNT220xGetInstance(tUnit, &pObj);
    tmASSERTExT(err, TM_OK, (DEBUGLVL_ERROR, "ddNT220xGetInstance(0x%08X) failed.", tUnit));

    if(err == TM_OK)
    {
        err = ddNT220xMutexAcquire(pObj, ddNT220x_MUTEX_TIMEOUT);
    }

    if(err == TM_OK)
    {
        /* Set value */
        pObj->I2CMap.uBx36.bF.RSSI_Ck_Speed = uValue;

        /* Write byte 0x36 */
        err = ddNT220xWrite(pObj, 0x36, 1);
        tmASSERTExT(err, TM_OK, (DEBUGLVL_ERROR, "ddNT220xWrite(0x%08X) failed.", tUnit));

        (void)ddNT220xMutexRelease(pObj);
    }

    return err;
}

/*============================================================================*/
/* FUNCTION:    tmddNT220xGetRSSI_Ck_Speed                                  */
/*                                                                            */
/* DESCRIPTION: Get the RSSI_Ck_Speed bit(s) status                           */
/*                                                                            */
/* RETURN:      TM_OK if no error                                             */
/*                                                                            */
/* NOTES:                                                                     */
/*                                                                            */
/*============================================================================*/
tmErrorCode_t
tmddNT220xGetRSSI_Ck_Speed
(
    tmUnitSelect_t  tUnit,  /* I: Unit number */
    UInt8*          puValue /* I: Address of the variable to output item value */
)
{
    ptmddNT220xObject_t   pObj = Null;
    tmErrorCode_t           err  = TM_OK;

    /* Test the parameters */
    if (puValue == Null)
        err = ddNT220x_ERR_BAD_PARAMETER;

    if(err == TM_OK)
    {
        /* Get Instance Object */
        err = ddNT220xGetInstance(tUnit, &pObj);
        tmASSERTExT(err, TM_OK, (DEBUGLVL_ERROR, "ddNT220xGetInstance(0x%08X) failed.", tUnit));
    }

    if(err == TM_OK)
    {
        err = ddNT220xMutexAcquire(pObj, ddNT220x_MUTEX_TIMEOUT);
    }

    if(err == TM_OK)
    {
        /* Read byte 0x36 */
        err = ddNT220xRead(pObj, 0x36, 1);
        tmASSERTExT(err, TM_OK, (DEBUGLVL_ERROR, "ddNT220xRead(0x%08X) failed.", tUnit));

        if(err == TM_OK)
        {
            /* Get value */
            *puValue = pObj->I2CMap.uBx36.bF.RSSI_Ck_Speed;
        }

        (void)ddNT220xMutexRelease(pObj);
    }

    return err;
}

/*============================================================================*/
/* FUNCTION:    tmddNT220xSetRFCAL_Phi2                                     */
/*                                                                            */
/* DESCRIPTION: Set the RFCAL_Phi2 bit(s) status                              */
/*                                                                            */
/* RETURN:      TM_OK if no error                                             */
/*                                                                            */
/* NOTES:                                                                     */
/*                                                                            */
/*============================================================================*/
tmErrorCode_t
tmddNT220xSetRFCAL_Phi2
(
    tmUnitSelect_t  tUnit,  /* I: Unit number */
    UInt8           uValue  /* I: Item value */
)
{
    ptmddNT220xObject_t   pObj = Null;
    tmErrorCode_t           err  = TM_OK;

    /* Get Instance Object */
    err = ddNT220xGetInstance(tUnit, &pObj);
    tmASSERTExT(err, TM_OK, (DEBUGLVL_ERROR, "ddNT220xGetInstance(0x%08X) failed.", tUnit));

    if(err == TM_OK)
    {
        err = ddNT220xMutexAcquire(pObj, ddNT220x_MUTEX_TIMEOUT);
    }

    if(err == TM_OK)
    {
        /* Set value */
        pObj->I2CMap.uBx37.bF.RFCAL_Phi2 = uValue;

        /* Write byte 0x37 */
        err = ddNT220xWrite(pObj, 0x37, 1);
        tmASSERTExT(err, TM_OK, (DEBUGLVL_ERROR, "ddNT220xWrite(0x%08X) failed.", tUnit));

        (void)ddNT220xMutexRelease(pObj);
    }

    return err;
}

/*============================================================================*/
/* FUNCTION:    tmddNT220xGetRFCAL_Phi2                                     */
/*                                                                            */
/* DESCRIPTION: Get the RFCAL_Phi2 bit(s) status                              */
/*                                                                            */
/* RETURN:      TM_OK if no error                                             */
/*                                                                            */
/* NOTES:                                                                     */
/*                                                                            */
/*============================================================================*/
tmErrorCode_t
tmddNT220xGetRFCAL_Phi2
(
    tmUnitSelect_t  tUnit,  /* I: Unit number */
    UInt8*          puValue /* I: Address of the variable to output item value */
)
{
    ptmddNT220xObject_t   pObj = Null;
    tmErrorCode_t           err  = TM_OK;

    /* Test the parameters */
    if (puValue == Null)
        err = ddNT220x_ERR_BAD_PARAMETER;

    if(err == TM_OK)
    {
        /* Get Instance Object */
        err = ddNT220xGetInstance(tUnit, &pObj);
        tmASSERTExT(err, TM_OK, (DEBUGLVL_ERROR, "ddNT220xGetInstance(0x%08X) failed.", tUnit));
    }

    if(err == TM_OK)
    {
        err = ddNT220xMutexAcquire(pObj, ddNT220x_MUTEX_TIMEOUT);
    }

    if(err == TM_OK)
    {
        /* Read byte 0x37 */
        err = ddNT220xRead(pObj, 0x37, 1);
        tmASSERTExT(err, TM_OK, (DEBUGLVL_ERROR, "ddNT220xRead(0x%08X) failed.", tUnit));

        if(err == TM_OK)
        {
            /* Get value */
            *puValue = pObj->I2CMap.uBx37.bF.RFCAL_Phi2;
        }

        (void)ddNT220xMutexRelease(pObj);
    }

    return err;
}

/*============================================================================*/
/* FUNCTION:    tmddNT220xWaitIRQ                                           */
/*                                                                            */
/* DESCRIPTION: Wait the IRQ to trigger                                       */
/*                                                                            */
/* RETURN:      TM_OK if no error                                             */
/*                                                                            */
/* NOTES:                                                                     */
/*                                                                            */
/*============================================================================*/
tmErrorCode_t
tmddNT220xWaitIRQ
(
    tmUnitSelect_t  tUnit,      /* I: Unit number */
    UInt32          timeOut,    /* I: timeout */
    UInt32          waitStep,   /* I: wait step */
    UInt8           irqStatus   /* I: IRQs to wait */
)
{
    ptmddNT220xObject_t   pObj = Null;
    tmErrorCode_t           err  = TM_OK;

    /* Get Instance Object */
    err = ddNT220xGetInstance(tUnit, &pObj);
    tmASSERTExT(err, TM_OK, (DEBUGLVL_ERROR, "ddNT220xGetInstance(0x%08X) failed.", tUnit));

    if(err == TM_OK)
    {
        err = ddNT220xMutexAcquire(pObj, ddNT220x_MUTEX_TIMEOUT);
    }

    if(err == TM_OK)
    {
        err = ddNT220xWaitIRQ(pObj, timeOut, waitStep, irqStatus);
        tmASSERTExT(err, TM_OK, (DEBUGLVL_ERROR, "ddNT220xWaitIRQ(0x%08X) failed.", tUnit));

        (void)ddNT220xMutexRelease(pObj);
    }

    return err;
}

/*============================================================================*/
/* FUNCTION:    tmddNT220xWaitXtalCal_End                                   */
/*                                                                            */
/* DESCRIPTION: Wait the MSM_XtalCal_End to trigger                           */
/*                                                                            */
/* RETURN:      TM_OK if no error                                             */
/*                                                                            */
/* NOTES:                                                                     */
/*                                                                            */
/*============================================================================*/
tmErrorCode_t
tmddNT220xWaitXtalCal_End
(
    tmUnitSelect_t  tUnit,      /* I: Unit number */
    UInt32          timeOut,    /* I: timeout */
    UInt32          waitStep    /* I: wait step */
)
{
    ptmddNT220xObject_t   pObj = Null;
    tmErrorCode_t           err  = TM_OK;

    /* Get Instance Object */
    err = ddNT220xGetInstance(tUnit, &pObj);
    tmASSERTExT(err, TM_OK, (DEBUGLVL_ERROR, "ddNT220xGetInstance(0x%08X) failed.", tUnit));

    if(err == TM_OK)
    {
        err = ddNT220xMutexAcquire(pObj, ddNT220x_MUTEX_TIMEOUT);
    }

    if(err == TM_OK)
    {
        err = ddNT220xWaitXtalCal_End(pObj, timeOut, waitStep);
        tmASSERTExT(err, TM_OK, (DEBUGLVL_ERROR, "ddNT220xWaitXtalCal_End(0x%08X) failed.", tUnit));

        (void)ddNT220xMutexRelease(pObj);
    }

    return err;
}

/*============================================================================*/
/* FUNCTION:    tmddNT220xSetIRQWait                                        */
/*                                                                            */
/* DESCRIPTION: Set whether wait IRQ in driver or not                         */
/*                                                                            */
/* RETURN:      TM_OK if no error                                             */
/*                                                                            */
/* NOTES:                                                                     */
/*                                                                            */
/*============================================================================*/
tmErrorCode_t
tmddNT220xSetIRQWait
(
    tmUnitSelect_t  tUnit,  /* I: Unit number */
    Bool            bWait   /* I: Determine if we need to wait IRQ in driver functions */
)
{
    ptmddNT220xObject_t   pObj = Null;
    tmErrorCode_t           err  = TM_OK;

    /* Get Instance Object */
    err = ddNT220xGetInstance(tUnit, &pObj);
    tmASSERTExT(err, TM_OK, (DEBUGLVL_ERROR, "ddNT220xGetInstance(0x%08X) failed.", tUnit));

    if(err == TM_OK)
    {
        err = ddNT220xMutexAcquire(pObj, ddNT220x_MUTEX_TIMEOUT);
    }

    if(err == TM_OK)
    {
        pObj->bIRQWait = bWait;

        (void)ddNT220xMutexRelease(pObj);
    }

    return err;
}

/*============================================================================*/
/* FUNCTION:    tmddNT220xGetIRQWait                                        */
/*                                                                            */
/* DESCRIPTION: Get whether wait IRQ in driver or not                         */
/*                                                                            */
/* RETURN:      TM_OK if no error                                             */
/*                                                                            */
/* NOTES:                                                                     */
/*                                                                            */
/*============================================================================*/
tmErrorCode_t
tmddNT220xGetIRQWait
(
    tmUnitSelect_t  tUnit,  /* I: Unit number */
    Bool*           pbWait  /* O: Determine if we need to wait IRQ in driver functions */
)
{     
    ptmddNT220xObject_t   pObj = Null;
    tmErrorCode_t           err  = TM_OK;

    /* Test the parameters */
    if (pbWait == Null)
        err = ddNT220x_ERR_BAD_PARAMETER;

    if(err == TM_OK)
    {
        /* Get Instance Object */
        err = ddNT220xGetInstance(tUnit, &pObj);
        tmASSERTExT(err, TM_OK, (DEBUGLVL_ERROR, "ddNT220xGetInstance(0x%08X) failed.", tUnit));
    }

    if(err == TM_OK)
    {
        err = ddNT220xMutexAcquire(pObj, ddNT220x_MUTEX_TIMEOUT);
    }

    if(err == TM_OK)
    {
        *pbWait = pObj->bIRQWait;

        (void)ddNT220xMutexRelease(pObj);
    }

    return err;
}
/*============================================================================*/
/* FUNCTION:    ddNT220xGetIRQ_status                                       */
/*                                                                            */
/* DESCRIPTION: Get IRQ status                                                */
/*                                                                            */
/* RETURN:      TM_OK if no error                                             */
/*                                                                            */
/* NOTES:                                                                     */
/*                                                                            */
/*============================================================================*/
tmErrorCode_t
ddNT220xGetIRQ_status
(
    ptmddNT220xObject_t   pObj,   /* I: Instance object */
    UInt8*                  puValue /* I: Address of the variable to output item value */
)
{     
    tmErrorCode_t   err  = TM_OK;

    /* Read byte 0x08 */
    err = ddNT220xRead(pObj, 0x08, 1);
    tmASSERTExT(err, TM_OK, (DEBUGLVL_ERROR, "ddNT220xRead(0x%08X) failed.", pObj->tUnit));

    if(err == TM_OK)
    {
        /* Get value */
        *puValue = pObj->I2CMap.uBx08.bF.IRQ_status;
    }

    return err;
}

/*============================================================================*/
/* FUNCTION:    ddNT220xGetMSM_XtalCal_End                                  */
/*                                                                            */
/* DESCRIPTION: Get MSM_XtalCal_End bit(s) status                             */
/*                                                                            */
/* RETURN:      TM_OK if no error                                             */
/*                                                                            */
/* NOTES:                                                                     */
/*                                                                            */
/*============================================================================*/
tmErrorCode_t
ddNT220xGetMSM_XtalCal_End
(
    ptmddNT220xObject_t   pObj,   /* I: Instance object */
    UInt8*                  puValue /* I: Address of the variable to output item value */
)
{
    tmErrorCode_t           err  = TM_OK;

    /* Test the parameters */
    if (puValue == Null)
        err = ddNT220x_ERR_BAD_PARAMETER;

    if(err == TM_OK)
    {
        /* Read byte 0x08 */
        err = ddNT220xRead(pObj, 0x08, 1);
        tmASSERTExT(err, TM_OK, (DEBUGLVL_ERROR, "ddNT220xRead(0x%08X) failed.", pObj->tUnit));

        if(err == TM_OK)
        {
            /* Get value */
            *puValue = pObj->I2CMap.uBx08.bF.MSM_XtalCal_End;
        }
    }
    return err;
}

/*============================================================================*/
/* FUNCTION:    ddNT220xWaitIRQ                                             */
/*                                                                            */
/* DESCRIPTION: Wait for IRQ to trigger                                       */
/*                                                                            */
/* RETURN:      TM_OK if no error                                             */
/*                                                                            */
/* NOTES:                                                                     */
/*                                                                            */
/*============================================================================*/
tmErrorCode_t
ddNT220xWaitIRQ
(
    ptmddNT220xObject_t   pObj,       /* I: Instance object */
    UInt32                  timeOut,    /* I: timeout */
    UInt32                  waitStep,   /* I: wait step */
    UInt8                   irqStatus   /* I: IRQs to wait */
)
{     
    tmErrorCode_t   err  = TM_OK;
    UInt32          counter = timeOut/waitStep; /* Wait max timeOut/waitStep ms */
    UInt8           uIRQ = 0;
    UInt8           uIRQStatus = 0;
    Bool            bIRQTriggered = False;

    while(err == TM_OK && (--counter)>0)
    {
        err = ddNT220xGetIRQ_status(pObj, &uIRQ);

        if(err == TM_OK && uIRQ == 1)
        {
            bIRQTriggered = True;
        }

        if(bIRQTriggered)
        {
            /* IRQ triggered => Exit */
            break;
        }

        if(err == TM_OK && irqStatus != 0x00)
        {
            uIRQStatus = ((pObj->I2CMap.uBx08.IRQ_status)&0x1F);

            if(irqStatus == uIRQStatus)
            {
                bIRQTriggered = True;
            }
        }

        err = ddNT220xWait(pObj, waitStep);
    }

    if(counter == 0)
    {
        err = ddNT220x_ERR_NOT_READY;
    }

    return err;
}

/*============================================================================*/
/* FUNCTION:    ddNT220xWaitXtalCal_End                                     */
/*                                                                            */
/* DESCRIPTION: Wait for MSM_XtalCal_End to trigger                           */
/*                                                                            */
/* RETURN:      TM_OK if no error                                             */
/*                                                                            */
/* NOTES:                                                                     */
/*                                                                            */
/*============================================================================*/
tmErrorCode_t
ddNT220xWaitXtalCal_End
(
    ptmddNT220xObject_t   pObj,       /* I: Instance object */
    UInt32                  timeOut,    /* I: timeout */
    UInt32                  waitStep    /* I: wait step */
)
{     
    tmErrorCode_t   err  = TM_OK;
    UInt32          counter = timeOut/waitStep; /* Wait max timeOut/waitStepms */
    UInt8           uMSM_XtalCal_End = 0;

    while(err == TM_OK && (--counter)>0)
    {
        err = ddNT220xGetMSM_XtalCal_End(pObj, &uMSM_XtalCal_End);

        if(uMSM_XtalCal_End == 1)
        {
            /* MSM_XtalCal_End triggered => Exit */
            break;
        }

        ddNT220xWait(pObj, waitStep);
    }

    if(counter == 0)
    {
        err = ddNT220x_ERR_NOT_READY;
    }

    return err;
}

/*============================================================================*/
/* FUNCTION:    ddNT220xWrite                                               */
/*                                                                            */
/* DESCRIPTION: Write in NT220x hardware                                    */
/*                                                                            */
/* RETURN:      TM_OK if no error                                             */
/*                                                                            */
/* NOTES:                                                                     */
/*                                                                            */
/*============================================================================*/
tmErrorCode_t 
ddNT220xWrite
(
    ptmddNT220xObject_t   pObj,           /* I: Driver object */
    UInt8                   uSubAddress,    /* I: sub address */
    UInt8                   uNbData         /* I: nb of data */
)
{
    tmErrorCode_t   err = TM_OK;
    UInt8*          pI2CMap = Null;

    /* pI2CMap initialization */
    pI2CMap = &(pObj->I2CMap.uBx00.ID_byte_1);

    err = POBJ_SRVFUNC_SIO.Write (pObj->tUnitW, 1, &uSubAddress, uNbData, &(pI2CMap[uSubAddress]));

    /* return value */
    return err;
}

/*============================================================================*/
/* FUNCTION:    ddNT220xRead                                                */
/*                                                                            */
/* DESCRIPTION: Read in NT220x hardware                                     */
/*                                                                            */
/* RETURN:      TM_OK if no error                                             */
/*                                                                            */
/* NOTES:                                                                     */
/*                                                                            */
/*============================================================================*/
tmErrorCode_t 
ddNT220xRead
(
    ptmddNT220xObject_t   pObj,           /* I: Driver object */
    UInt8                   uSubAddress,    /* I: sub address */
    UInt8                   uNbData         /* I: nb of data */
)
{
    tmErrorCode_t   err = TM_OK;
    UInt8*          pI2CMap = Null;

    /* pRegister initialization */
    pI2CMap = &(pObj->I2CMap.uBx00.ID_byte_1) + uSubAddress;

    /* Read data from the Tuner */
    err = POBJ_SRVFUNC_SIO.Read(pObj->tUnitW, 1, &uSubAddress, uNbData, pI2CMap);

    /* return value */
    return err;
}

/*============================================================================*/
/* FUNCTION:    ddNT220xWait                                                */
/*                                                                            */
/* DESCRIPTION: Wait for the requested time                                   */
/*                                                                            */
/* RETURN:      TM_OK if no error                                             */
/*                                                                            */
/* NOTES:                                                                     */
/*                                                                            */
/*============================================================================*/
tmErrorCode_t 
ddNT220xWait
(
    ptmddNT220xObject_t   pObj,   /* I: Driver object */
    UInt32                  Time    /*  I: time to wait for */
)
{
    tmErrorCode_t   err  = TM_OK;

    /* wait Time ms */
    err = POBJ_SRVFUNC_STIME.Wait (pObj->tUnit, Time);

    /* Return value */
    return err;
}

/*============================================================================*/
/* FUNCTION:    ddNT220xMutexAcquire                                        */
/*                                                                            */
/* DESCRIPTION: Acquire driver mutex                                          */
/*                                                                            */
/* RETURN:      TM_OK if no error                                             */
/*                                                                            */
/* NOTES:                                                                     */
/*                                                                            */
/*============================================================================*/
tmErrorCode_t
ddNT220xMutexAcquire
(
    ptmddNT220xObject_t   pObj,
    UInt32                  timeOut
)
{
    tmErrorCode_t   err = TM_OK;

    if(pObj->sMutex.Acquire != Null && pObj->pMutex != Null)
    {
        err = pObj->sMutex.Acquire(pObj->pMutex, timeOut);
    }

    return err;
}

/*============================================================================*/
/* FUNCTION:    ddNT220xMutexRelease                                        */
/*                                                                            */
/* DESCRIPTION: Release driver mutex                                          */
/*                                                                            */
/* RETURN:      TM_OK if no error                                             */
/*                                                                            */
/* NOTES:                                                                     */
/*                                                                            */
/*============================================================================*/
tmErrorCode_t
ddNT220xMutexRelease
(
    ptmddNT220xObject_t   pObj
)
{
    tmErrorCode_t   err = TM_OK;

    if(pObj->sMutex.Release != Null && pObj->pMutex != Null)
    {
        err = pObj->sMutex.Release(pObj->pMutex);
    }

    return err;
}
/*============================================================================*/
/* FUNCTION:    tmNT220xAGC1_change                                           */
/*                                                                            */
/* DESCRIPTION: adapt AGC1_gain from latest call  ( simulate AGC1 gain free ) */
/*                                                                            */
/* RETURN:      TM_OK if no error                                             */
/*                                                                            */
/* NOTES:                                                                     */
/*                                                                            */
/*============================================================================*/
tmErrorCode_t
tmddNT220xAGC1_Adapt
(
    tmUnitSelect_t  tUnit   /* I: Unit number */
)
{
    ptmddNT220xObject_t   pObj = Null;
	tmErrorCode_t   err  = TM_OK;
    UInt8 counter, vAGC1min, vAGC1_max_step;
	Int16 TotUp , TotDo ;
	UInt8 NbStepsDone = 0;

    /* Get Instance Object */
    err = ddNT220xGetInstance(tUnit, &pObj);
    tmASSERTExT(err, TM_OK, (DEBUGLVL_ERROR, "ddNT220xGetInstance(0x%08X) failed.", tUnit));

	if (pObj->I2CMap.uBx0C.bF.AGC1_6_15dB == 0)
	{
		vAGC1min = 0; // -12 dB
		vAGC1_max_step = 10;  // -12 +15 dB
	}
	else
	{
		vAGC1min = 6; // 6 dB
		vAGC1_max_step = 4;  // 6 -> 15 dB
	}

	while(err == TM_OK && NbStepsDone < vAGC1_max_step)  // limit to min - max steps 10
	{
		counter = 0; TotUp = 0; TotDo = 0; NbStepsDone = NbStepsDone + 1;
		while(err == TM_OK && (counter++)<40)
		{
			err = ddNT220xRead(pObj, 0x31, 1);  /* read UP , DO AGC1 */
			tmASSERTExT(err, TM_OK, (DEBUGLVL_ERROR, "ddNT220xRead(0x%08X) failed.", tUnit));
			TotDo = TotDo + ( pObj->I2CMap.uBx31.bF.Do_AGC1 ? 14 : -1 );
			TotUp = TotUp + ( pObj->I2CMap.uBx31.bF.Up_AGC1 ? 1 : -4 );
			err = ddNT220xWait(pObj, 1);
		}
		if ( TotUp >= 15 && pObj->I2CMap.uBx24.bF.AGC1_Gain != 9 ) 
		{
			pObj->I2CMap.uBx24.bF.AGC1_Gain = pObj->I2CMap.uBx24.bF.AGC1_Gain + 1;
   		    err = ddNT220xWrite(pObj, 0x24, 1);
			tmASSERTExT(err, TM_OK, (DEBUGLVL_ERROR, "ddNT220xWrite(0x%08X) failed.", tUnit));

		}
		else if ( TotDo >= 10 && pObj->I2CMap.uBx24.bF.AGC1_Gain != vAGC1min  ) 
		{
			pObj->I2CMap.uBx24.bF.AGC1_Gain = pObj->I2CMap.uBx24.bF.AGC1_Gain - 1;
   		    err = ddNT220xWrite(pObj, 0x24, 1);
			tmASSERTExT(err, TM_OK, (DEBUGLVL_ERROR, "ddNT220xWrite(0x%08X) failed.", tUnit));
		}
		else
		{
			NbStepsDone = vAGC1_max_step;
		}
	}
    return err;
}
