#include <string.h>
#include "intern.h"

twx_status_t ZLX_CALL htxt_handler (twx_win_t * win, unsigned int evt, 
                                    twx_event_info_t * ei);

twx_win_class_t htxt_wcls =
{
    htxt_handler,
    htxt_finish,
    sizeof(htxt_win_t),
    "txt/htxt"
};

/* htxt_finish **************************************************************/
void ZLX_CALL htxt_finish (twx_win_t * win)
{
    htxt_win_t * hw = (htxt_win_t *) win;
    size_t i;

    hbs_mutex_destroy(hw->mutex);
    for (i = 0; i < hw->rsn; ++i)
        if (hw->rsa[i]) hbs_free(hw->rta[i], hw->rsa[i]);
    if (hw->rsn) hbs_free(hw->rsa, hw->rsn * sizeof(size_t));
    if (hw->rtn) hbs_free(hw->rta, hw->rtn * sizeof(uint8_t *));
}

/* htxt_handler *************************************************************/
twx_status_t ZLX_CALL htxt_handler (twx_win_t * win, unsigned int evt, 
                                    twx_event_info_t * ei)
{
    htxt_win_t * hw = (htxt_win_t *) win;
    size_t n;
    twx_status_t ts = TWX_OK;
    unsigned int cs;

    switch (evt)
    {
    case TWX_DRAW:
        if (!(win->flags & TWX_WF_UPDATE)) 
        {
            HBS_DM("skip drawing $s@$i as the update flag is clear!", 
                   win->wcls->name, win->id);
            break;
        }
        win->flags &= ~TWX_WF_UPDATE;
        hbs_mutex_lock(hw->mutex);
        n = hw->n;
        HBS_DM("rect $i+$i+$ix$i $es...", win->scr_col, win->scr_row, win->width, n, hw->rta[0]);
        if (win->height < n) n = win->height;
        O(acx1_rect((uint8_t const * const *) hw->rta,
                    win->scr_row, win->scr_col, n, win->width, hw->attr_a));
        for (;n < win->height; ++n)
        {
            O(acx1_write_pos(win->scr_row + n, win->scr_col));
            O(acx1_attr(hw->attr_a[0].bg, hw->attr_a[0].fg, 
                        hw->attr_a[0].mode));
            O(acx1_fill(' ', win->width));
        }
        hbs_mutex_unlock(hw->mutex);
        HBS_DM("finished drawing $s@$i", win->wcls->name, win->id);
        break;
    default:
        ts = twx_default_handler(win, evt, ei);
    }

    return ts;
}

/* twx_htxt_win_create ******************************************************/
TWX_API twx_status_t ZLX_CALL twx_htxt_win_create
(
    twx_t * twx,
    twx_win_t * * win_ptr,
    acx1_attr_t * attr_a,
    size_t attr_n,
    char const * htxt
)
{
    htxt_win_t * hw;
    hbs_status_t hs;

    L("allocating...");
    hw = win_alloc(twx, &htxt_wcls);
    L("hw=%p", hw);
    if (!hw) return TWX_NO_MEM;
    hs = hbs_mutex_create(&hw->mutex);
    L("mutex=%p", hw->mutex);
    if (hs) { hbs_free(hw, htxt_wcls.size); return TWX_NO_MEM; }
    *win_ptr = &hw->base;
    hw->attr_a = attr_a;
    hw->attr_n = attr_n;
    L("setting content...");
    twx_htxt_win_set_content(&hw->base, htxt);
    L("set content done");
    return TWX_OK;
}

/* twx_htxt_win_set_content *************************************************/
TWX_API twx_status_t ZLX_CALL twx_htxt_win_set_content
(
    twx_win_t * win,
    void const * htxt
)
{
    htxt_win_t * hw = (htxt_win_t *) win;
    size_t i, n, nn, l;
    uint8_t const * p;
    uint8_t const * q;
    uint8_t const * e;
    uint8_t * * rta;
    size_t * rsa;
    twx_status_t ts;

    hbs_mutex_lock(hw->mutex);
    p = htxt;
    for (e = p; *e; e += 1 + (*e == '\a'));
    //e = zlx_u8a_scan(p, 0);

    L("e - p = %ld\n", (long) (e - p));
    do
    {
        while (p != e && e[-1] == '\n') --e;
        n = 0;
        for (n = 0; p < e; ++n, ++p) {
            p = zlx_u8a_search(p, e, '\n');
        }
        L("n=%u\n", (int) n);

        ts = TWX_NO_MEM;
        if (hw->rtn < n) {
            nn = (n + 3) & ~(size_t) 3;
            rta = hbs_realloc(hw->rta,
                              hw->rtn * sizeof(uint8_t *),
                              nn * sizeof(uint8_t *));
            if (!rta) break;
            for (i = hw->rtn; i < nn; ++i) rta[i] = NULL;
            hw->rta = rta;
            hw->rtn = nn;
        }
        if (hw->rsn < n) {
            nn = (n + 3) & ~(size_t) 3;
            rsa = hbs_realloc(hw->rsa,
                              hw->rsn * sizeof(size_t),
                              nn * sizeof(size_t));
            if (!rsa) break;
            for (i = hw->rsn; i < nn; ++i) rsa[i] = 0;
            hw->rsa = rsa;
            hw->rsn = nn;
        }
        L("rtn=%u, rsn=%u\n", (int) hw->rtn, (int) hw->rsn);

        for (p = htxt, i = 0; i < n && p < e; ++i, p = q + 1)
        {
            q = zlx_u8a_search(p, e, '\n');
            l = q - p + 1;
            if (hw->rsa[i] < l)
            {
                l = (l + 15) & ~(size_t) 15;
                if (hw->rsa[i]) hbs_free(hw->rta[i], hw->rsa[i]);
                hw->rta[i] = hbs_alloc(l, "twx.htxt.row");
                if (!hw->rta[i]) { hw->rsa[i] = 0; break; }
                hw->rsa[i] = l;
            }
            memcpy(hw->rta[i], p, q - p);
            hw->rta[i][q - p] = 0;
        }
        hw->n = i;
        if (i < n) break;
        ts = TWX_OK;
    }
    while (0);

    hbs_mutex_unlock(hw->mutex);
    return ts;
}

