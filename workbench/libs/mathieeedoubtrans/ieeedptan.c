/*
    Copyright (C) 1995-2008, The AROS Development Team. All rights reserved.
*/

#include "mathieeedoubtrans_intern.h"

/*****************************************************************************

    NAME */

        AROS_LHDOUBLE1(double, IEEEDPTan,

/*  SYNOPSIS */
        AROS_LHA2(double, y, D0, D1),

/*  LOCATION */
        struct MathIeeeDoubTransBase *, MathIeeeDoubTransBase, 8, MathIeeeDoubTrans)

/*  FUNCTION
        Calculate the tangens of the given IEEE double precision number
        where y represents an angle in radians.

    INPUTS

    RESULT
        result - IEEE double precision floating point number

    BUGS

    INTERNALS

*****************************************************************************/
{
    AROS_LIBFUNC_INIT
    QUAD sn, cs, Res;
    
    cs = IEEEDPCos(y);
    sn = IEEEDPSin(y);
    if (is_eqC(cs, 0, 0))
    {
        Set_Value64C(Res, IEEEDPPInfty_Hi, IEEEDPPInfty_Lo);
        return Res;
    }
    else
    {
        return IEEEDPDiv(sn, cs);
    }

    AROS_LIBFUNC_EXIT
}

