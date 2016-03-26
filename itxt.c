#include <string.h>
#include "intern.h"

typedef unsigned int (ZLX_CALL * char_grp_func_t) (uint32_t ucp);

static size_t text_fwd (uint8_t const * p, uint8_t const * q, 
                        unsigned int * width);
static size_t text_bwd (uint8_t const * p, uint8_t const * q,
                        unsigned int * width);

static unsigned int ZLX_CALL char_normal_group (uint32_t ucp);

void ZLX_CALL itxt_finish (twx_win_t * win);
twx_status_t ZLX_CALL itxt_handler (twx_win_t * win, unsigned int evt,
                                    twx_event_info_t * ei);

/* fit_cursor ***************************************************************/
/**
 *  Changes view_ofs such that cursor_ofs points in the visible part of the
 *  input line then recomputes cursor_col.
 */
static void ZLX_CALL fit_cursor (itxt_win_t * itw);

twx_win_class_t itxt_wcls =
{
    itxt_handler,
    itxt_finish,
    sizeof(itxt_win_t),
    "txt/itxt"
};

/* char_normal_group ********************************************************/
static unsigned int ZLX_CALL char_normal_group (uint32_t ucp)
{
    if (ucp == ' ') return 0;
    if (ucp < 0x20) return 1;
    if (ucp >= 'A' && ucp <= 'Z') return 2;
    if (ucp >= 'a' && ucp <= 'z') return 2;
    if (ucp >= '0' && ucp <= '9') return 2;
    if (ucp == '_') return 2;
    return 3;
}

/* text_fwd_group ***********************************************************/
static uint8_t * text_fwd_group (uint8_t const * p, uint8_t const * q,
                                 char_grp_func_t cgf)
{
    ptrdiff_t d;
    uint32_t ucp;
    unsigned int g, ng;
    if (p == q) return (uint8_t *) p;
    d = zlx_utf8_to_ucp(p, q, 0, &ucp);
    A(d >= 0); (void) d;
    p += text_fwd(p, q, NULL);
    g = cgf(ucp);
    while (p != q)
    {
        d = zlx_utf8_to_ucp(p, q, 0, &ucp);
        A(d >= 0); (void) d;
        ng = cgf(ucp);
        if (ng != g)
        {
            if (g && !ng) g = 0; // skip whitespace after whatever group
            else break;
        }
        p += text_fwd(p, q, NULL);
    }
    return (uint8_t *) p;
}

/* text_bwd_group ***********************************************************/
static uint8_t * text_bwd_group (uint8_t const * p, uint8_t const * q,
                                 char_grp_func_t cgf)
{
    ptrdiff_t d;
    uint32_t ucp;
    unsigned int g, ng;
    uint8_t const * r;
    if (p == q) return (uint8_t *) p;

    r = q - text_bwd(p, q, NULL);
    d = zlx_utf8_to_ucp(r, q, 0, &ucp);
    A(d >= 0); (void) d;
    g = cgf(ucp);
    q = r;
    while (p != q)
    {
        r = q - text_bwd(p, q, NULL);
        d = zlx_utf8_to_ucp(r, q, 0, &ucp);
        A(d >= 0); (void) d;
        ng = cgf(ucp);
        if (ng != g)
        {
            if (!g) g = ng;
            else break;
        }
        q = r;
    }
    return (uint8_t *) q;
}

/* itxt_finish **************************************************************/
void ZLX_CALL itxt_finish (twx_win_t * win)
{
    itxt_win_t * itw = (itxt_win_t *) win;
    if (itw->text_size) hbs_free(itw->text, itw->text_size);
    if (itw->pfx_size) hbs_free(itw->pfx, itw->pfx_size);
}

/* itxt_handler *************************************************************/
twx_status_t ZLX_CALL itxt_handler (twx_win_t * win, unsigned int evt,
                                    twx_event_info_t * ei)
{
    itxt_win_t * itw = (itxt_win_t *) win;
    uint8_t * p;
    twx_event_info_t e;
    unsigned int cs, l;
    twx_status_t ts = TWX_OK;
    uint32_t km, ucp;
    size_t i, tw, tb;

    switch (evt)
    {
    case TWX_DRAW:
        if (!(win->flags & TWX_WF_UPDATE)) break;
        win->flags &= ~TWX_WF_UPDATE;
        O(acx1_write_pos(win->scr_row, win->scr_col));
        if (itw->pfx_width + 5 > win->width)
        {
            O(acx1_attr(itw->attr_a[TWX_ITXT_ATTR_MARK].bg,
                        itw->attr_a[TWX_ITXT_ATTR_MARK].fg,
                        itw->attr_a[TWX_ITXT_ATTR_MARK].mode));
            O(acx1_fill('-', win->width));
            break;
        }

        if (itw->pfx_size)
        {
            O(acx1_attr(itw->attr_a[TWX_ITXT_ATTR_PFX].bg,
                        itw->attr_a[TWX_ITXT_ATTR_PFX].fg,
                        itw->attr_a[TWX_ITXT_ATTR_PFX].mode));
            O(acx1_write(itw->pfx, itw->pfx_size));
        }
        tw = itw->pfx_width;

        if (itw->view_ofs)
        {
            O(acx1_attr(itw->attr_a[TWX_ITXT_ATTR_MARK].bg,
                        itw->attr_a[TWX_ITXT_ATTR_MARK].fg,
                        itw->attr_a[TWX_ITXT_ATTR_MARK].mode));
            O(acx1_fill('<', 1));
            tw++;
        }

        O(acx1_attr(itw->attr_a[TWX_ITXT_ATTR_TXT].bg,
                    itw->attr_a[TWX_ITXT_ATTR_TXT].fg,
                    itw->attr_a[TWX_ITXT_ATTR_TXT].mode));

        O(acx1_write(itw->text + itw->view_ofs, itw->view_size));
        tw += itw->view_width;
        if (itw->view_ofs + itw->view_size < itw->text_n)
        {
            O(acx1_attr(itw->attr_a[TWX_ITXT_ATTR_MARK].bg,
                        itw->attr_a[TWX_ITXT_ATTR_MARK].fg,
                        itw->attr_a[TWX_ITXT_ATTR_MARK].mode));
            O(acx1_fill('>', 1));
            tw++;
        }

        if (tw < win->width)
        {
            O(acx1_fill(' ', win->width - tw));
        }

        O(acx1_set_cursor_pos(win->scr_row,
                              win->scr_col + itw->pfx_size 
                              + (itw->view_ofs != 0) + itw->cursor_col));
        break;

    case TWX_KEY:
        km = ei->km;
        switch (km)
        {
            /* exits */
        case ACX1_ENTER:
            e.id = itw->ntf_id;
            ts = itw->ntf_win->wcls->handler(itw->ntf_win, 
                                             TWX_ITXT_ENTERED, &e);
            break;

        case ACX1_ESC:
            e.id = itw->ntf_id;
            ts = itw->ntf_win->wcls->handler(itw->ntf_win, 
                                             TWX_ITXT_CANCELLED, &e);
            break;

            /* movement */
        case ACX1_HOME:
        case ACX1_CTRL | 'A':
            itw->view_ofs = 0;
            itw->cursor_ofs = 0;
            //itw->cursor_col = 0;
            fit_cursor(itw);
            twx_win_refresh(win);
            break;

        case ACX1_END:
        case ACX1_CTRL | 'E':
            itw->cursor_ofs = itw->text_n;
            //itw->cursor_col = tw;
            fit_cursor(itw);
            twx_win_refresh(win);
            break;

        case ACX1_LEFT:
        case ACX1_CTRL | 'B':
            tb = text_bwd(itw->text, itw->text + itw->cursor_ofs, NULL);
            if (!tb) break;
            itw->cursor_ofs -= tb;
            fit_cursor(itw);
            twx_win_refresh(win);
            break;

        case ACX1_RIGHT:
        case ACX1_CTRL | 'F':
            tb = text_fwd(itw->text + itw->cursor_ofs, 
                          itw->text + itw->text_n, NULL);
            if (!tb) break;
            itw->cursor_ofs += tb;
            fit_cursor(itw);
            twx_win_refresh(win);
            break;

        case ACX1_CTRL | ACX1_LEFT:
        case ACX1_ALT | 'b':
            if (!itw->cursor_ofs) break;
            p = text_bwd_group(itw->text, itw->text + itw->cursor_ofs,
                               char_normal_group);
            itw->cursor_ofs = p - itw->text;
            fit_cursor(itw);
            twx_win_refresh(win);
            break;

        case ACX1_CTRL | ACX1_RIGHT:
        case ACX1_ALT | 'f':
            if (itw->cursor_ofs == itw->text_n) break;
            p = text_fwd_group(itw->text + itw->cursor_ofs,
                               itw->text + itw->text_n,
                               char_normal_group);
            itw->cursor_ofs = p - itw->text;
            fit_cursor(itw);
            twx_win_refresh(win);
            break;

            /* edit */
        case ACX1_BACKSPACE:
        case ACX1_CTRL | 'H':
            if (!itw->cursor_ofs) break;
            for (i = itw->cursor_ofs - 1;
                 i && (itw->text[i] & 0xC0) == 0x80;
                 --i);
            memmove(itw->text + i,
                    itw->text + itw->cursor_ofs,
                    itw->text_n - itw->cursor_ofs);
            itw->text_n -= itw->cursor_ofs - i;
            itw->cursor_ofs = i;
            fit_cursor(itw);
            twx_win_refresh(win);
            break;

        case ACX1_DEL:
        case ACX1_CTRL | 'D':
            if (itw->cursor_ofs == itw->text_n) break;
            tb = zlx_utf8_to_ucp(itw->text + itw->cursor_ofs, 
                                 itw->text + itw->text_n, 0, &ucp);
            i = itw->cursor_ofs + tb;
            memmove(itw->text + itw->cursor_ofs, 
                    itw->text + i, 
                    itw->text_n - i);
            itw->text_n -= tb;
            fit_cursor(itw);
            twx_win_refresh(win);
            break;

        case ACX1_CTRL | 'K':
            if (itw->cursor_ofs == itw->text_n) break;
            itw->text_n = itw->cursor_ofs;
            fit_cursor(itw);
            twx_win_refresh(win);
            break;

        case ACX1_CTRL | 'U':
            if (!itw->cursor_ofs) break;
            memmove(itw->text, itw->text + itw->cursor_ofs, 
                    itw->text_n - itw->cursor_ofs);
            itw->text_n -= itw->cursor_ofs;
            itw->cursor_ofs = 0;
            fit_cursor(itw);
            twx_win_refresh(win);
            break;

        case ACX1_CTRL | ACX1_BACKSPACE:
        case ACX1_CTRL | 'W':
            if (!itw->cursor_ofs) break;
            p = text_bwd_group(itw->text, itw->text + itw->cursor_ofs,
                               char_normal_group);
            memmove(p, itw->text + itw->cursor_ofs, 
                    itw->text_n - itw->cursor_ofs);
            itw->text_n -= itw->text + itw->cursor_ofs - p;
            itw->cursor_ofs = p - itw->text;
            fit_cursor(itw);
            twx_win_refresh(win);
            break;

        case ACX1_CTRL | ACX1_DEL:
        case ACX1_ALT | 'd':
            if (itw->cursor_ofs == itw->text_n) break;
            p = text_fwd_group(itw->text + itw->cursor_ofs,
                               itw->text + itw->text_n,
                               char_normal_group);
            memmove(itw->text + itw->cursor_ofs, p, 
                    itw->text + itw->text_n - p);
            itw->text_n -= p - itw->text - itw->cursor_ofs;
            fit_cursor(itw);
            twx_win_refresh(win);
            break;

        default:
            if (km < 0x20 || km >= 0x110000 || acx1_term_char_width(km) < 0)
                break;
            l = zlx_ucp_to_utf8_len(km);
            p = zlx_u8a_insert(&itw->text, &itw->text_n, &itw->text_size, 
                               itw->cursor_ofs, l, hbs_ma);
            if (!p) return TWX_NO_MEM;
            zlxi_ucp_to_utf8(km, p);
            itw->cursor_ofs += l;
            //itw->cursor_col += acx1_term_char_width(km);
            fit_cursor(itw);
            twx_win_refresh(win);
            //twx_refresh(win->twx);
        }
        break;

    case TWX_FOCUS:
        O(acx1_set_cursor_mode(1));
        break;

    case TWX_UNFOCUS:
        O(acx1_set_cursor_mode(0));
        break;

    default:
        ts = twx_default_handler(win, evt, ei);
        break;
    }
    return ts;
}

/* twx_itxt_win_create ******************************************************/
TWX_API twx_status_t ZLX_CALL twx_itxt_win_create
(
    twx_t * twx,
    twx_win_t * * win_ptr,
    acx1_attr_t * attr_a,
    size_t attr_n,
    char const * pfx,
    char const * init_text,
    twx_win_t * ntf_win,
    unsigned int ntf_id
)
{
    itxt_win_t * itw;
    size_t pb, pc, pw;
    size_t tb, tc, tw;

    if (pfx)
    {
        if (acx1_utf8_str_measure(acx1_term_char_width_wctx, NULL,
                                  pfx, strlen(pfx), SIZE_MAX, SIZE_MAX,
                                  &pb, &pc, &pw) < 0)
            return TWX_BAD_STRING;
    }

    if (init_text)
    {
        if (acx1_utf8_str_measure(acx1_term_char_width_wctx, NULL,
                                  init_text, strlen(init_text),
                                  SIZE_MAX, SIZE_MAX, &tb, &tc, &tw) < 0)
            return TWX_BAD_STRING;
    }

    itw = win_alloc(twx, &itxt_wcls);
    if (!itw) return TWX_NO_MEM;
    *win_ptr = &itw->base;
    itw->attr_a = attr_a;
    itw->attr_n = attr_n;
    if (pfx)
    {
        itw->pfx_size = pb;
        itw->pfx = hbs_alloc(pb, "twx.itxt.pfx");
        if (!itw->pfx)
        {
            hbs_free(itw, itxt_wcls.size);
            return TWX_NO_MEM;
        }
        memcpy(itw->pfx, pfx, pb);
        itw->pfx_width = pw;
    }

    itw->text_n = tb;
    itw->text_size = (itw->text_n + 16) & ~(size_t) 15;
    itw->text = hbs_alloc(itw->text_size, "twx.itxt.text");
    if (!itw->text)
    {
        if (itw->pfx_size) hbs_free(itw->pfx, itw->pfx_size);
        hbs_free(itw, itxt_wcls.size);
        return TWX_NO_MEM;
    }
    memcpy(itw->text, init_text, itw->text_n);
    itw->cursor_ofs = tb;
    fit_cursor(itw);
    //itw->cursor_col = tw;
    itw->ntf_win = ntf_win;
    itw->ntf_id = ntf_id;

    return TWX_OK;
}

/* text_fwd *****************************************************************/
/**
 *  skips 1 char and any subsequent zero-width chars that follow.
 */
static size_t text_fwd (uint8_t const * p, uint8_t const * q,
                        unsigned int * width)
{
    uint8_t const * r;
    uint32_t ucp;
    int l, cw;

    if (p == q) { if (width) *width = 0; return 0; }
    l = zlx_utf8_to_ucp(p, q, 0, &ucp);
    A(l > 0);
    cw = acx1_term_char_width(ucp);
    if (cw < 0) cw = 1;
    if (width) *width = cw;
    r = p + l;
    /* now skip all 0-width chars that modify the skipped one */
    while (r < q)
    {
        l = zlx_utf8_to_ucp(r, q, 0, &ucp);
        A(l > 0);
        cw = acx1_term_char_width(ucp);
        if (cw) break;
        r += l;
    }
    return r - p;
}

/* text_bwd *****************************************************************/
/**
 *  Goes back from q until p or until a non-zero-width char is encountered
 */
static size_t text_bwd (uint8_t const * p, uint8_t const * q,
                        unsigned int * width)
{
    uint8_t const * r = q;
    uint32_t ucp;
    int l, cw = 0;

    if (p == q) { if (width) *width = 0; return 0; }
    while (r != p)
    {
        --r;
        if ((*r & 0xC0) == 0x80) continue;
        l = zlx_utf8_to_ucp(r, q, 0, &ucp);
        A(r + l == q);
        (void) l;
        cw = acx1_term_char_width(ucp);
        if (cw) break;
    }
    if (width) *width = 1 + (cw == 2);

    return q - r;
}



/* fit_cursor ***************************************************************/
static void ZLX_CALL fit_cursor (itxt_win_t * itw)
{
    size_t co = itw->cursor_ofs;
    size_t vo = itw->view_ofs;
    size_t w = itw->base.width - itw->pfx_width;
    size_t tb, tc, tw, tn, vso, wl;
    int m;
    int mwr, cw, cl;
    unsigned int sw;
    uint32_t ucp;
    uint8_t const * t;
    uint8_t const * e;

    tn = itw->text_n;
    t = itw->text;
    e = t + tn;

    L("at entry: vo=0x%X, co=0x%X, w=0x%X", (int) vo, (int) co, (int) w);
    if (vo >= co)
    {
        vo = co;
        itw->cursor_col = 0;
        if (vo)
        {
            vso = vo - text_bwd(t, t + vo, &sw);
            L("vso=0x%X, sw=%d", (int) vso, sw);
            if (vso == 0 && sw == 1) { vo = 0; itw->cursor_col = 1; }
        }
    }
    else // vo < co
    {
        // calculate minimum width that needs to be present to the right of
        // cursor, including the char at cursor
        if (co == tn) mwr = 1;
        else
        {
            cl = (int) zlx_utf8_to_ucp(t + co, e, 0, &ucp);
            A(cl > 0);
            mwr = acx1_term_char_width(ucp);
            A(mwr >= 0);
            mwr += (co + cl < tn);
        }

        // compute width from vo to co
        wl = (vo != 0);
        m = acx1_utf8_str_measure(acx1_term_char_width_wctx, NULL, t + vo, 
                                  co - vo, SIZE_MAX, SIZE_MAX, &tb, &tc, &tw);
        L("m=%d, tb=0x%X, tc=0x%X, tw=0x%X", m, (int) tb, (int) tc, (int) tw);
        A(m == 0);
        wl += tw;
        while (wl + mwr > w)
        {
            if (!vo) wl++;
            vo += text_fwd(t + vo, e, &sw);
            wl -= sw;
            L("vo=0x%X, wl=0x%X", (int) vo, (int) wl);
        }
        itw->cursor_col = wl - (vo != 0);
    }
    m = acx1_utf8_str_measure(acx1_term_char_width_wctx, NULL,
                              t + vo, tn - vo, SIZE_MAX, w - 1 - (vo != 0),
                              &tb, &tc, &tw);
    L("vo=0x%X, tn-vo=0x%X => m=%d, tb=0x%X, tc=0x%X, tw=0x%X", 
      (int) vo, (int) (tn - vo), m, (int) tb, (int) tc, (int) tw);
    A(m == 0 || m == 2);
    if (m == 2)
    {
        A(tb < tn - vo);
        cl = (int) zlx_utf8_to_ucp(t + vo + tb, e, 0, &ucp);
        A(cl > 0);
        if (vo + tb + cl == tn)
        {
            cw = acx1_term_char_width(ucp);
            A(cw >= 0);
            if (cw == 1) { tb += cl; tw += cw; }
        }
    }
    itw->view_ofs = vo;
    itw->view_size = tb;
    itw->view_width = tw;
    L("at exit: vo=0x%X, vs=0x%X, vw=0x%X, co=0x%X, cc=0x%X",
      (int) vo, (int) itw->view_size, (int) itw->view_width,
      (int) itw->cursor_ofs, (int) itw->cursor_col);
}

