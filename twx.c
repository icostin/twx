#include <string.h>
#include "intern.h"

TWX_API char const * const twx_lib_name = "twx"
#if TWX_STATIC
    "-static"
#else
    "-dynamic"
#endif
    "-" TWX_CONFIG
    "-" TWX_TARGET
    "-" TWX_COMPILER
    ;

/* twx_event_name ***********************************************************/
TWX_API char * ZLX_CALL twx_event_name (unsigned int evt)
{
    switch (evt)
    {
#define X(_x) case _x: return #_x
        X(TWX_NOP);
        X(TWX_GEOM);
        X(TWX_INVALIDATE);
        X(TWX_DRAW);
        X(TWX_FOCUS);
        X(TWX_UNFOCUS);
        X(TWX_KEY);
        X(TWX_ITXT_ENTERED);
        X(TWX_ITXT_CANCELLED);
#undef X
    }
    return "<twx-unknown-evt>";
}

/* twx_shutdown *************************************************************/
TWX_API void ZLX_CALL twx_shutdown
(
    twx_t * twx,
    twx_status_t exit_status
)
{
    hbs_mutex_lock(twx->main_mutex);
    twx->exit_status = exit_status;
    twx->shutdown = 1;
    hbs_mutex_unlock(twx->main_mutex);
    hbs_cond_signal(twx->main_cond);
}

/* input_processor **********************************************************/
static uint8_t ZLX_CALL input_processor (void * arg)
{
    twx_t * twx = arg;
    unsigned int cs;
    acx1_event_t e;

    while (!twx->shutdown)
    {
        L("waiting for event...");
        cs = acx1_read_event(&e);
        L("got event %u", e.type);
        if (cs)
        {
            L("acx1_read_event() error %u", cs);
            twx_shutdown(twx, TWX_CONSOLE_INPUT_ERROR);
            break;
        }
        switch (e.type)
        {
        case ACX1_RESIZE:
            L("signalling resize to %ux%u", e.size.w, e.size.h);
            hbs_mutex_lock(twx->main_mutex);
            twx->screen_resized = 1;
            twx->height = e.size.h;
            twx->width = e.size.w;
            hbs_mutex_unlock(twx->main_mutex);
            hbs_cond_signal(twx->main_cond);
            break;
        case ACX1_KEY:
            {
                unsigned int sig;
                L("signalling key 0x%X", e.km);
                hbs_mutex_lock(twx->main_mutex);
                if (((twx->kre + 1) & twx->krm) != twx->krb)
                {
                    twx->key_ring[twx->kre++] = e.km;
                    twx->kre &= twx->krm;
                    sig = 1;
                }
                else sig = 0;
                hbs_mutex_unlock(twx->main_mutex);
                if (sig) hbs_cond_signal(twx->main_cond);
            }
            break;
        case ACX1_FINISH:
            break;
        default:
            L("unhandled event %u", e.type);
            twx_shutdown(twx, TWX_CONSOLE_INPUT_ERROR);
        }
    }
    return 0;
}

/* twx_create ***************************************************************/
TWX_API twx_status_t ZLX_CALL twx_create
(
    twx_t * * twx_ptr
)
{
    twx_t * twx;
    unsigned int cs;
    zlx_mth_status_t ths;
    twx_status_t ts = TWX_BUG;
    uint16_t h, w;

    hbs_init();
    twx = hbs_alloc(sizeof(twx_t), "twx");
    L("twx=%p", twx);
    if (!twx) return TWX_NO_MEM;

    do
    {
        memset(twx, 0, sizeof(*twx));
        twx->krm = (1 << TWX_KEY_RING_POWER) - 1;
        L("krm=%u", (int) twx->krm);
        twx->key_ring = hbs_alloc(sizeof(uint32_t) * (twx->krm + 1), 
                                  "twx.key_ring");
        if (!twx->key_ring)
        {
            L("grr");
            ts = TWX_NO_MEM;
            break;
        }

        twx->main_mutex = hbs_mutex_create("twx.mutex.main");
        if (!twx->main_mutex)
        {
            L("ouch");
            ts = TWX_NO_MEM;
            break;
        }
        twx->init_state |= TWX_INITED_MUTEX;

        twx->main_cond = hbs_cond_create(&ths, "twx.cond.main");
        if (!twx->main_cond)
        {
            L("ouch: %u", ths);
            ts = ths == ZLX_MTH_NO_MEM ? TWX_NO_MEM : TWX_MAIN_COND_INIT_FAILED;
            break;
        }
        twx->init_state |= TWX_INITED_COND;

        cs = acx1_init();
        if (cs)
        {
            L("acx1_init() error %u", cs);
            ts = TWX_CONSOLE_INIT_ERROR;
            break;
        }
        twx->init_state |= TWX_INITED_CONSOLE;

        cs = acx1_get_screen_size(&h, &w);
        if (cs)
        {
            L("failed to get screen size (%u)", cs);
            ts = TWX_CONSOLE_INIT_ERROR;
            break;
        }
        twx->height = h;
        twx->width = w;
        twx->screen_resized = 1;

        ths = hbs_thread_create(&twx->input_thread, input_processor, twx);
        if (ths)
        {
            L("ouch: %u", ths);
            ts = TWX_THREAD_CREATE_FAILED;
            break;
        }

        twx->root_win = NULL;
        twx->focus_win = NULL;
        twx->state = TWX_INITED;
        twx->shutdown = 0;
        *twx_ptr = twx;
        ts = TWX_OK;

        twx->state = TWX_INITED;
    }
    while (0);

    if (twx->state == TWX_INIT_FAILED) twx_destroy(twx);

    L("return %u", ts);
    return ts;
}

/* twx_destroy **************************************************************/
TWX_API void ZLX_CALL twx_destroy
(
    twx_t * twx
)
{
    twx->shutdown = 1;
    if ((twx->init_state & TWX_INITED_COND))
        hbs_cond_destroy(twx->main_cond);
    if ((twx->init_state & TWX_INITED_MUTEX))
        hbs_mutex_destroy(twx->main_mutex);
    if ((twx->init_state & TWX_INITED_CONSOLE))
        acx1_finish();
    if ((twx->init_state & TWX_INITED_INPUT))
        hbs_thread_join(twx->input_thread, NULL);
    if ((twx->init_state & TWX_INITED_KEY_RING))
        hbs_free(twx->key_ring, sizeof(uint32_t) * (twx->krm + 1));
    hbs_free(twx, sizeof(twx_t));
}

/* twx_refresh **************************************************************/
TWX_API void ZLX_CALL twx_refresh
(
    twx_t * twx
)
{
    unsigned int sig;
    hbs_mutex_lock(twx->main_mutex);
    sig = (twx->draw_mode == 0);
    if (sig) twx->draw_mode = TWX_DRAW;
    hbs_mutex_unlock(twx->main_mutex);
    if (sig) hbs_cond_signal(twx->main_cond);
}

/* twx_set_root *************************************************************/
TWX_API void ZLX_CALL twx_set_root
(
    twx_win_t * win
)
{
    twx_t * twx = win->twx;
    hbs_mutex_lock(twx->main_mutex);
    twx->root_win = win;
    twx->screen_resized = 1;
    hbs_mutex_unlock(twx->main_mutex);
    hbs_cond_signal(twx->main_cond);
}

/* twx_post_win_focus *******************************************************/
TWX_API void ZLX_CALL twx_post_win_focus
(
    twx_win_t * win
)
{
    twx_t * twx = win->twx;
    L("posting focus to %s:%p", win->wcls->name, win);
    hbs_mutex_lock(twx->main_mutex);
    twx->new_focus_win = win;
    hbs_mutex_unlock(twx->main_mutex);
}

/* twx_win_focus ************************************************************/
TWX_API twx_status_t ZLX_CALL twx_win_focus
(
    twx_win_t * win
)
{
    twx_t * ZLX_RESTRICT twx = win->twx;
    twx_win_t * ZLX_RESTRICT owin = twx->focus_win;
    twx_status_t ts;

    if (owin)
    {
        L("unfocusing win=%s:%p...", owin->wcls->name, owin);
        ts = owin->wcls->handler(owin, TWX_UNFOCUS, NULL);
        if (ts) { L("ouch %u", ts); return ts; }
    }
    twx->new_focus_win = twx->focus_win = win;
    if (win)
    {
        L("focusing win=%s:%p...", win->wcls->name, win);
        ts = win->wcls->handler(win, TWX_FOCUS, NULL);
        if (ts) { L("ouch %u", ts); return ts; }
    }
    return TWX_OK;
}

/* twx_run ******************************************************************/
TWX_API twx_status_t ZLX_CALL twx_run
(
    twx_t * twx
)
{
    twx_event_info_t ei;
    twx_status_t ts = TWX_OK;
    unsigned int cs;

    A(twx->state == TWX_INITED);
    (void) twx;

    do
    {
        O(acx1_write_start());
        O(acx1_attr(0, 7, 0));
        O(acx1_clear());
        O(acx1_write_stop());
        O(acx1_set_cursor_mode(0));
        O(acx1_set_cursor_pos(1, 1));
        
        hbs_mutex_lock(twx->main_mutex);
        while (!twx->shutdown)
        {
            if (twx->screen_resized)
            {
                unsigned int h, w;
                twx_win_t * root;
                twx->screen_resized = 0;
                twx->draw_mode = TWX_DRAW;
                root = twx->root_win;
                if (root)
                {
                    h = twx->height;
                    w = twx->width;
                    hbs_mutex_unlock(twx->main_mutex);
                    L("calling geom for root=%p, h=%u, w=%u", root, h, w);
                    ts = twx_win_geom(root, 1, 1, h, w);
                    if (ts) break;
                    hbs_mutex_lock(twx->main_mutex);
                }
                continue;
            }
            if (twx->draw_mode)
            {
                twx_win_t * root;
                unsigned int mode;
                mode = twx->draw_mode;
                twx->draw_mode = 0;
                root = twx->root_win;
                if (root)
                {
                    hbs_mutex_unlock(twx->main_mutex);
                    L("calling draw for root=%p with mode=%u", root, mode);
                    O(acx1_write_start());
                    ts = root->wcls->handler(root, mode, NULL);
                    if (ts) break;
                    O(acx1_write_stop());
                    hbs_mutex_lock(twx->main_mutex);
                }
                continue;
            }
            if (twx->new_focus_win != twx->focus_win)
            {
                L("refocusing...");
                ts = twx_win_focus(twx->new_focus_win);
                if (ts) { L("ouch %u", ts); break; }
                A(twx->new_focus_win == twx->focus_win);
                continue;
            }

            if (twx->krb != twx->kre)
            {
                unsigned int km;
                twx_win_t * win;
                win = twx->focus_win;
                if (twx->key_ring[twx->krb] == (ACX1_ALT | '\\'))
                {
                    L("magic exit key");
                    hbs_mutex_unlock(twx->main_mutex);
                    twx_shutdown(twx, TWX_OK);
                    break;
                }

                if (win)
                {
                    km = twx->key_ring[twx->krb++];
                    twx->krb &= twx->krm;
                    hbs_mutex_unlock(twx->main_mutex);
                    L("sending key=0x%X to win=%p", km, win);
                    ei.km = km;
                    ts = win->wcls->handler(win, TWX_KEY, &ei);
                    hbs_mutex_lock(twx->main_mutex);
                }
                else
                {
                    L("no input win, consume all keys...");
                    twx->krb = twx->kre = 0;
                }
                continue;
            }
            hbs_cond_wait(twx->main_cond, twx->main_mutex);
        }
        twx->shutdown = 1;

        O(acx1_write_start());
        O(acx1_attr(0, 7, 0));
        O(acx1_clear());
        O(acx1_write_stop());
        O(acx1_set_cursor_mode(1));
        O(acx1_set_cursor_pos(1, 1));
    }
    while (0);

    return ts;
}

/* win_alloc ****************************************************************/
void * win_alloc (twx_t * twx, twx_win_class_t * wcls)
{
    twx_win_t * win;
    L("allocating 0x%X bytes for %s", (int) wcls->size, wcls->name);
    win = hbs_alloc(wcls->size, wcls->name);
    if (win)
    {
        memset(win, 0, wcls->size);
        win->wcls = wcls;
        win->twx = twx;
        win->id = twx->seed++;
    }
    return win;
}

/* twx_nop_draw *************************************************************/
TWX_API twx_status_t ZLX_CALL twx_nop_draw (twx_win_t * win, unsigned int mode)
{
    (void) win, (void) mode;
    return TWX_OK;
}

/* twx_nop_geom *************************************************************/
TWX_API twx_status_t ZLX_CALL twx_nop_geom (twx_win_t * win, 
                                            unsigned int sr, unsigned int sc, 
                                            unsigned int h, unsigned int w)
{
    (void) win, (void) sr, (void) sc, (void) h, (void) w;
    return TWX_OK;
}

/* twx_nop_key **************************************************************/
TWX_API twx_status_t ZLX_CALL twx_nop_key (twx_win_t * win, uint32_t km)
{
    (void) win, (void) km;
    return TWX_OK;
}

/* twx_nop_finish ***********************************************************/
TWX_API void ZLX_CALL twx_nop_finish (twx_win_t * win)
{
    (void) win;
}

/* twx_win_write_geom *******************************************************/
TWX_API twx_status_t ZLX_CALL twx_win_write_geom
(
    twx_win_t * win,
    unsigned int scr_row,
    unsigned int scr_col,
    unsigned int height,
    unsigned int width
)
{
    win->scr_row = scr_row;
    win->scr_col = scr_col;
    win->height = height;
    win->width = width;
    win->flags |= TWX_WF_UPDATE;
    return TWX_OK;
}

/* twx_win_geom *************************************************************/
TWX_API twx_status_t ZLX_CALL twx_win_geom
(
    twx_win_t * win,
    unsigned int scr_row,
    unsigned int scr_col,
    unsigned int height,
    unsigned int width
)
{
    twx_status_t ts;
    twx_event_info_t ei;

    L("win=%p, geom=%u+%u+%ux%u", win, scr_col, scr_row, width, height);
    ei.geom.scr_row = scr_row;
    ei.geom.scr_col = scr_col;
    ei.geom.height = height;
    ei.geom.width = width;
    do
    {
        ts = win->wcls->handler(win, TWX_GEOM, &ei);
        if (ts) break;
        ts = win->wcls->handler(win, TWX_INVALIDATE, &ei);
        if (ts) break;
        twx_refresh(win->twx);
    }
    while (0);

    return ts;
}

/* twx_win_invalidate *******************************************************/
TWX_API twx_status_t ZLX_CALL twx_win_invalidate
(
    twx_win_t * win,
    unsigned int scr_row,
    unsigned int scr_col,
    unsigned int height,
    unsigned int width
)
{
    twx_status_t ts;
    twx_event_info_t ei;

    L("win=%p, inv=%u+%u+%ux%u", win, scr_col, scr_row, width, height);
    ei.geom.scr_row = scr_row;
    ei.geom.scr_col = scr_col;
    ei.geom.height = height;
    ei.geom.width = width;
    do
    {
        ts = win->wcls->handler(win, TWX_INVALIDATE, &ei);
        if (ts) break;
        twx_refresh(win->twx);
    }
    while (0);

    return ts;
}

/* twx_win_destroy **********************************************************/
TWX_API void ZLX_CALL twx_win_destroy
(
    twx_win_t * win
)
{
    twx_win_class_t * wcls = win->wcls;
    HBS_DM("calling $s@$p.finish()...", wcls->name, win);
    wcls->finish(win);
    HBS_DM("freeing $s@$p...", wcls->name, win);
    hbs_free(win, wcls->size);
    HBS_DM("destroyed window $s@$p", wcls->name, win);
}

/* twx_default_handler ******************************************************/
TWX_API twx_status_t ZLX_CALL twx_default_handler 
(
    twx_win_t * win,
    unsigned int evt, 
    twx_event_info_t * ei
)
{
    switch (evt)
    {
    case TWX_GEOM:
        win->scr_row = ei->geom.scr_row;
        win->scr_col = ei->geom.scr_col;
        win->height = ei->geom.height;
        win->width = ei->geom.width;
        HBS_DM("storing new geometry $i+$i+$ix$i for $s@$i",
               win->scr_col, win->scr_row, win->width, win->height,
               win->wcls->name, win->id);
        break;

    case TWX_INVALIDATE:
        win->flags |= TWX_WF_UPDATE;
        HBS_DM("setting flag update for $s@$i", win->wcls->name, win->id);
        break;
    }
    return TWX_OK;
}

