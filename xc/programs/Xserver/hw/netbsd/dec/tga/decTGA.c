/* $NetBSD: decTGA.c,v 1.1 2001/09/18 20:02:55 ad Exp $ */

/* $XConsortium: sunCfb.c,v 1.15.1.2 95/01/12 18:54:42 kaleb Exp $ */
/* $XFree86: xc/programs/Xserver/hw/sun/sunCfb.c,v 3.2 1995/02/12 02:36:22 dawes Exp $ */

/*
Copyright (c) 1990  X Consortium

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in
all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL THE
X CONSORTIUM BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN
AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.

Except as contained in this notice, the name of the X Consortium shall not be
used in advertising or otherwise to promote the sale, use or other dealings
in this Software without prior written authorization from the X Consortium.
 */

/************************************************************
Copyright 1987 by Sun Microsystems, Inc. Mountain View, CA.

                    All Rights Reserved

Permission  to  use,  copy,  modify,  and  distribute   this
software  and  its documentation for any purpose and without
fee is hereby granted, provided that the above copyright no-
tice  appear  in all copies and that both that copyright no-
tice and this permission notice appear in  supporting  docu-
mentation,  and  that the names of Sun or X Consortium
not be used in advertising or publicity pertaining to 
distribution  of  the software  without specific prior 
written permission. Sun and X Consortium make no 
representations about the suitability of this software for 
any purpose. It is provided "as is" without any express or 
implied warranty.

SUN DISCLAIMS ALL WARRANTIES WITH REGARD TO  THIS  SOFTWARE,
INCLUDING ALL IMPLIED WARRANTIES OF MERCHANTABILITY AND FIT-
NESS FOR A PARTICULAR PURPOSE. IN NO EVENT SHALL SUN BE  LI-
ABLE  FOR  ANY SPECIAL, INDIRECT OR CONSEQUENTIAL DAMAGES OR
ANY DAMAGES WHATSOEVER RESULTING FROM LOSS OF USE,  DATA  OR
PROFITS,  WHETHER  IN  AN  ACTION OF CONTRACT, NEGLIGENCE OR
OTHER TORTIOUS ACTION, ARISING OUT OF OR IN CONNECTION  WITH
THE USE OR PERFORMANCE OF THIS SOFTWARE.

********************************************************/

/*
 * Copyright (c) 1987 by the Regents of the University of California
 * Copyright (c) 1987 by Adam de Boor, UC Berkeley
 *
 * Permission to use, copy, modify, and distribute this
 * software and its documentation for any purpose and without
 * fee is hereby granted, provided that the above copyright
 * notice appear in all copies.  The University of California
 * makes no representations about the suitability of this
 * software for any purpose.  It is provided "as is" without
 * express or implied warranty.
 */

/****************************************************************/
/* Modified from  sunCG4C.c for X11R3 by Tom Jarmolowski	*/
/****************************************************************/

/* 
 * Copyright 1991, 1992, 1993 Kaleb S. Keithley
 *
 * Permission to use, copy, modify, and distribute this
 * software and its documentation for any purpose and without
 * fee is hereby granted, provided that the above copyright
 * notice appear in all copies.  Kaleb S. Keithley makes no 
 * representations about the suitability of this software for 
 * any purpose.  It is provided "as is" without express or 
 * implied warranty.
 */

#include "dec.h"
#include "cfb.h"
#include "tga.h"

/* XXX */
#include <stdio.h>

Bool decTGAInit (screen, pScreen, argc, argv)
    int	    	  screen;    	/* what screen am I going to be */
    ScreenPtr	  pScreen;  	/* The Screen to initialize */
    int	    	  argc;	    	/* The number of the Server's arguments. */
    char    	  **argv;   	/* The arguments themselves. Don't change! */
{
	unsigned char *fb = fb = decFbs[screen].fb;
	unsigned char *fbr;
	int fb_off, realwidth, rowsize;
	volatile tga_reg_t *tgaregs;

	decFbs[screen].EnterLeave = (void (*)())NoopDDA;

    	if (!decScreenAllocate(pScreen))
		return FALSE;
	if (!fb) {
		if ((fb = decMemoryMap ((size_t)decFbs[screen].size,
		    0, decFbs[screen].fd)) == NULL)
			return FALSE;
	        decFbs[screen].fb = fb;
	}

	/*
	 * Frame buffer RAM always starts at core space size / 2.
	 * TGA Registers always start at offset 1M into core space.
	 *
	 * The actual offset of the displayed screen may vary, because
	 * of cursor ram location.  Also, pixel-width of the frame buffer
	 * may not be the same as the displayed width.  To figure these
	 * things out, we have to look at the TGA registers.
	 */ 
	tgaregs = (tga_reg_t *)(fb + (1 * 1024 * 1024));
	fb_off = (int)decFbs[screen].size / 2;
	fbr = fb + ( 1 * 1024 * 1024 );
	decFbs[screen].tgaregs0 = (tga_reg_t *)(fbr + 0 * 64 * 1024);
	decFbs[screen].tgaregs1 = (tga_reg_t *)(fbr + 1 * 64 * 1024);
	decFbs[screen].tgaregs2 = (tga_reg_t *)(fbr + 2 * 64 * 1024);
	decFbs[screen].tgaregs3 = (tga_reg_t *)(fbr + 3 * 64 * 1024);

	/* Find out real pixel width of the display. */
        switch (tgaregs[TGA_REG_VHCR] & 0x1ff) {            /* XXX */
        case 0:
                realwidth = 8192;
                break;

        case 1:
                realwidth = 8196;
                break;

        default:
                realwidth = (tgaregs[TGA_REG_VHCR] & 0x1ff) * 4; /* XXX */
                break;
        }

	/*
	 * Find out how big one 'row' of video is, so that we can tell
	 * where the displayed frame buffer actually starts.
	 */
	rowsize = 2 * 1024;			/* 2k for 128K VRAMs, 8BPP */
	if (decFbs[screen].depth == 32)
		rowsize *= 4;			/* increase by 4x for 32BPP */
	if ((tgaregs[TGA_REG_GDER] & 0x200) == 0) /* 256K VRAMs */
		rowsize *= 2;			/* increase by 2x */

	/*
	 * Finally, calcluate real displayed frame buffer offset.
	 */
#if 0	/* VVBR is write only, but we always write it as 1. */
	fb_off += rowsize * (tgaregs[TGA_REG_VVBR] & 0x1ff);
#else
	fb_off += rowsize * 1;
#endif

	if (!decTgaScreenInit(pScreen, fb + fb_off,
	    decFbs[screen].width,
	    decFbs[screen].height,
	    monitorResolution, monitorResolution,
	    realwidth,
	    decFbs[screen].depth)) {
fprintf(stderr, "decTgaScreenInit failed\n");
            return FALSE;
	}

	decColormapScreenInit(pScreen);
	if (!decScreenInit(pScreen)) {
fprintf(stderr, "decScreenInit failed\n");
		return FALSE;
	}
	(void) decSaveScreen(pScreen, SCREEN_SAVER_OFF);
	return cfbCreateDefColormap(pScreen);
}

Bool
decTgaSetupScreen(pScreen, pbits, xsize, ysize, dpix, dpiy, width, bpp)
    register ScreenPtr pScreen;
    pointer pbits;		/* pointer to screen bitmap */
    int xsize, ysize;		/* in pixels */
    int dpix, dpiy;		/* dots per inch */
    int width;			/* pixel width of frame buffer */
    int	bpp;			/* bits per pixel of root */
{
    switch (bpp) {
    case 32:
	if (!cfb32SetupScreen(pScreen, pbits, xsize, ysize, dpix, dpiy,
	  width))
	    return FALSE;
	return cfbSetVisualTypes(24, 1 << TrueColor, 8);
    case 8:
	if (!cfbSetupScreen(pScreen, pbits, xsize, ysize, dpix, dpiy,
	  width))
	    return FALSE;
	if (decAccelerate) {
	    pScreen->CopyWindow = decTgaCopyWindow;
	    pScreen->CreateGC = decTgaCreateGC;
	}
	return TRUE;
    default:
	fprintf(stderr, "decTgaSetupScreen:  unsupported bpp = %d\n", bpp);
	return FALSE;
    }
}

Bool
decTgaFinishScreenInit(pScreen, pbits, xsize, ysize, dpix, dpiy, width, bpp)
    register ScreenPtr pScreen;
    pointer pbits;		/* pointer to screen bitmap */
    int xsize, ysize;		/* in pixels */
    int dpix, dpiy;		/* dots per inch */
    int width;			/* pixel width of frame buffer */
    int bpp;			/* bits per pixel of root */
{
    Bool retval;

    switch (bpp) {
    case 32:
	retval = cfb32FinishScreenInit(pScreen, pbits, xsize, ysize,
		    dpix, dpiy, width);

	/* XXXNJW cfb doesn't provide a way to tweak these, so cheat. */
	pScreen->visuals[0].redMask = 0xff0000;
	pScreen->visuals[0].greenMask = 0xff00;
	pScreen->visuals[0].blueMask = 0xff;

	pScreen->visuals[0].offsetRed = 16;
	pScreen->visuals[0].offsetGreen = 8;
	pScreen->visuals[0].offsetBlue = 0;

	break;
    case 8:
	retval = cfbFinishScreenInit(pScreen, pbits, xsize, ysize,
		    dpix, dpiy, width);
	break;
    default:
	retval = FALSE;
    }

    return retval;
}

Bool
decTgaScreenInit(pScreen, pbits, xsize, ysize, dpix, dpiy, width, bpp)
    register ScreenPtr pScreen;
    pointer pbits;		/* pointer to screen bitmap */
    int xsize, ysize;		/* in pixels */
    int dpix, dpiy;		/* dots per inch */
    int width;			/* pixel width of frame buffer */
    int bpp;			/* bits per pixel of root */
{
    if (!decTgaSetupScreen(pScreen, pbits, xsize, ysize, dpix,
	dpiy, width, bpp))
	    return FALSE;
    return decTgaFinishScreenInit(pScreen, pbits, xsize, ysize, dpix,
	  dpiy, width, bpp);
}
