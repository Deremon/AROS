/*
    Copyright (C) 1995-2017, The AROS Development Team. All rights reserved.
*/

#include "mathieeedoubbas_intern.h"

/*****************************************************************************

    NAME */

        AROS_LHDOUBLE1(double, IEEEDPFloor,

/*  SYNOPSIS */
        AROS_LHA2(double, y, D0, D1),

/*  LOCATION */
        struct MathIeeeDoubBasBase *, MathIeeeDoubBasBase, 15, MathIeeeDoubBas)

/*  FUNCTION
        Calculates the floor value of an IEEE double precision number.

    INPUTS
        y - IEEE double precision floating point number.

    RESULT
        x - floor of y.

        Flags:
          zero     : result is zero
          negative : result is negative
          overflow : 0

    NOTES

    EXAMPLE

    BUGS

    SEE ALSO
        IEEEDPCeil(), IEEEDPFix()

    INTERNALS

*****************************************************************************/
{
    AROS_LIBFUNC_INIT
    
    QUAD Mask;
    QUAD y_tmp,y2;
    int shift;
    QUAD * Qy = (QUAD *)&y;
    
    if (is_eqC(*Qy,0,0)) return *Qy;
    
    Set_Value64(y_tmp, *Qy);
    AND64QC(y_tmp, IEEEDPExponent_Mask_Hi, IEEEDPExponent_Mask_Lo );
    
    if (is_lessC(y_tmp, one_Hi, one_Lo ))
    {
        if (is_lessSC(*Qy,0,0))
        {
            SetSR(Negative_Bit, Zero_Bit | Negative_Bit | Overflow_Bit);
            Set_Value64C
            (
                *Qy,
                one_Hi | IEEEDPSign_Mask_Hi,
                one_Lo | IEEEDPSign_Mask_Lo
            );
            return y;
        }
        else
        {
            SetSR(Zero_Bit, Zero_Bit | Negative_Bit | Overflow_Bit);
            Set_Value64C(*Qy, 0,0);
            return y;
        }
    }
    
    /* |fnum| >= 1 */
    Set_Value64C(Mask, 0x80000000, 0x00000000);
    SHRU32(shift, y_tmp, 52);
    SHRS64(Mask, Mask, (shift-0x3ff+11));  /* leave the () there! StormC 1.1 needs 'em! */
    
    /* y is negative */
    if (is_leqSC(*Qy, 0x0, 0x0))
    {
        QUAD Mask2;
        NOT64(Mask2, Mask);
        Set_Value64(y2,*Qy);
        AND64Q(y2, Mask2);
        if (is_neqC(y2,0x0,0x0))
        {
            QUAD minusone;
            double * Dminusone = (double *)&minusone;
            Set_Value64C
            (
                minusone,
                one_Hi | IEEEDPSign_Mask_Hi,
                one_Lo | IEEEDPSign_Mask_Lo
            );
            *Qy = IEEEDPAdd(*Qy, *Dminusone);
            Set_Value64C(Mask, 0x80000000, 0x00000000);
            SHRU32(shift, y_tmp, 52);
            SHRS64(Mask, Mask, (shift-0x3ff+11)); /* leave the () there! StormC 1.1 needs 'em! */
        }
    }
    
    if (is_lessSC(*Qy,0x0,0x0))
    {
        SetSR(Negative_Bit, Zero_Bit | Negative_Bit | Overflow_Bit);
    }
    
    AND64Q(*Qy, Mask);
    
    return y;

    AROS_LIBFUNC_EXIT
}
