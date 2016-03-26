#include "intern.h"

/* blank_handler ************************************************************/
twx_status_t ZLX_CALL blank_handler (twx_win_t * win, unsigned int evt, 
                                     twx_event_info_t * ei);

twx_win_class_t blank_wcls =
{
    blank_handler,
    twx_nop_finish,
    sizeof(blank_win_t),
    "twx/blank"
};

/* blank_handler ************************************************************/
twx_status_t ZLX_CALL blank_handler (twx_win_t * win, unsigned int evt, 
                                     twx_event_info_t * ei)
{
    blank_win_t * bw = (blank_win_t *) win;
    unsigned int i, cs;
    twx_status_t ts = TWX_OK;

    (void) ei;
    switch (evt)
    {
    case TWX_DRAW:
        if (!(win->flags & TWX_WF_UPDATE)) break;
        win->flags &= ~TWX_WF_UPDATE;
        L("drawing a blank here %u+%u+%ux%u:'%c'...",
          win->scr_col, win->scr_row, win->width, win->height, bw->ch);
        O(acx1_attr(bw->attr->bg, bw->attr->fg, bw->attr->mode));
        for (i = 0; i < win->height; ++i) 
        {
            O(acx1_write_pos(win->scr_row + i, win->scr_col));
            O(acx1_fill(bw->ch, win->width));
        }
        bw->ch++;
        break;
    default:
        ts = twx_default_handler(win, evt, ei);
    }

    return ts;
}

/* twx_blank_win_create *****************************************************/
TWX_API twx_status_t ZLX_CALL twx_blank_win_create
(
    twx_t * twx,
    twx_win_t * * win_ptr,
    acx1_attr_t * attr,
    uint32_t ch
)
{
    blank_win_t * bw;

    L("creating blank window...");
    bw = win_alloc(twx, &blank_wcls);
    if (!bw) return TWX_NO_MEM;
    *win_ptr = &bw->base;
    bw->attr = attr;
    bw->ch = ch;
    return TWX_OK;
}

