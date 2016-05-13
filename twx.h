#ifndef _TWX_H
#define _TWX_H

#include <zlx.h>
#include <hbs.h>

#if TWX_STATIC
#define TWX_API
#elif TWX_DYNAMIC
#define TWX_API ZLX_LIB_EXPORT
#else
#define TWX_API ZLX_LIB_IMPORT
#endif

#define TWX_ITXT_ATTR_PFX 0
#define TWX_ITXT_ATTR_TXT 1
#define TWX_ITXT_ATTR_MARK 2 // marker to the left or right of the input area
// when there is text that doesn't fit on the display
#define TWX_ITXT_ATTR_COUNT 3

typedef enum twx_status_enum twx_status_t;
typedef struct twx_s twx_t;
typedef struct twx_win_class_s twx_win_class_t;
typedef struct twx_win_s twx_win_t;

enum twx_event_enum
{
    TWX_NOP = 0,
    TWX_GEOM,
    TWX_INVALIDATE,
    TWX_DRAW,
    TWX_FOCUS,
    TWX_UNFOCUS,
    TWX_KEY,
    TWX_ITXT_ENTERED,
    TWX_ITXT_CANCELLED,
};

typedef union twx_event_info_u twx_event_info_t;
union twx_event_info_u
{
    struct
    {
        unsigned int scr_row;
        unsigned int scr_col;
        unsigned int height;
        unsigned int width;
    } geom;
    uint32_t km;
    unsigned int id;
};

struct twx_win_class_s
{
    twx_status_t (ZLX_CALL * handler) (twx_win_t * win, unsigned int evt, 
                                       twx_event_info_t * ei);
    void (ZLX_CALL * finish) (twx_win_t * win);
    size_t size;
    char const * name;
};

#define TWX_WF_UPDATE   (1 << 0)

struct twx_win_s
{
    twx_win_class_t * wcls;
    twx_t * twx;
    unsigned int scr_row, scr_col, height, width;
    unsigned int flags;
    unsigned int id;
};

enum twx_status_enum
{
    TWX_OK = 0,
    TWX_NO_MEM,
    TWX_BAD_STRING,
    TWX_THREAD_CREATE_FAILED,
    TWX_MAIN_COND_INIT_FAILED,
    TWX_CONSOLE_INIT_ERROR,
    TWX_CONSOLE_INPUT_ERROR,
    TWX_CONSOLE_OUTPUT_ERROR,
    TWX_BUG,
};

extern TWX_API char const * const twx_lib_name;

/* twx_event_name ***********************************************************/
/**
 *  Gives a static string with the event name.
 */
TWX_API char * ZLX_CALL twx_event_name (unsigned int evt);

/* twx_create ***************************************************************/
/**
 *  Creates the text windowing interface instanca.
 */
TWX_API twx_status_t ZLX_CALL twx_create
(
    twx_t * * twx_ptr
);

/* twx_destroy **************************************************************/
/**
 *  Destroys the instance.
 */
TWX_API void ZLX_CALL twx_destroy
(
    twx_t * twx
);

/* twx_run ******************************************************************/
/**
 *  Runs the UI loop until the user program calls twx_shutdown().
 */
TWX_API twx_status_t ZLX_CALL twx_run
(
    twx_t * twx
);

/* twx_shutdown *************************************************************/
/**
 *  Triggers the shutdown of the text windowing interface.
 *  Calling this function prompts the twx_run() function to exit gracefully
 *  at the earliest opportunity with the given exit code.
 */
TWX_API void ZLX_CALL twx_shutdown
(
    twx_t * twx,
    twx_status_t exit_status
);

/* twx_refresh **************************************************************/
/**
 *  Places the request onto the ui thread to do a screen refresh.
 */
TWX_API void ZLX_CALL twx_refresh
(
    twx_t * twx
);

/* twx_win_draw *************************************************************/
/**
 *  Call the window class to do the actual drawing.
 */
ZLX_INLINE twx_status_t ZLX_CALL twx_win_draw
(
    twx_win_t * win
)
{
    return win->wcls->handler(win, TWX_DRAW, NULL);
}

/* twx_win_destroy **********************************************************/
/**
 *  Destroys window.
 */
TWX_API void ZLX_CALL twx_win_destroy
(
    twx_win_t * win
);

/* twx_set_root *************************************************************/
/**
 *  Sets the root window.
 */
TWX_API void ZLX_CALL twx_set_root
(
    twx_win_t * win
);

/* twx_win_focus ************************************************************/
/**
 *  Sets the input window.
 *  Calls the current focused window with TWX_UNFOCUS, then calls this
 *  window with TWX_FOCUS.
 */
TWX_API twx_status_t ZLX_CALL twx_win_focus
(
    twx_win_t * win
);

/* twx_post_win_focus *******************************************************/
/**
 *   Posts the request for switching the focus to the given window.
 *   This function should be called when setting the focus window before
 *   twx_run(), or while twx_run() is running on  different thread.
 */
TWX_API void ZLX_CALL twx_post_win_focus
(
    twx_win_t * win
);

/* twx_set_geom *************************************************************/
/**
 *  Requests the window to update itself to specified geometry.
 */
TWX_API twx_status_t ZLX_CALL twx_win_geom
(
    twx_win_t * win,
    unsigned int scr_row,
    unsigned int scr_col,
    unsigned int height,
    unsigned int width
);

/* twx_win_invalidate *******************************************************/
/**
 *  Requests the window to invalidate the given region so that a subsequent
 *  delivery of TWX_DRAW will cause that region to be refreshed
 */
TWX_API twx_status_t ZLX_CALL twx_win_invalidate
(
    twx_win_t * win,
    unsigned int scr_row,
    unsigned int scr_col,
    unsigned int height,
    unsigned int width
);

/* twx_win_refresh **********************************************************/
ZLX_INLINE twx_status_t twx_win_refresh
(
    twx_win_t * win
)
{
    return twx_win_invalidate(win, win->scr_row, win->scr_col, 
                              win->height, win->width);
}

/* twx_win_write_geom *******************************************************/
/**
 *  Updates the geometry fields from the given window.
 *  This function is suitable for a very basic window class geom callback.
 */
TWX_API twx_status_t ZLX_CALL twx_win_write_geom
(
    twx_win_t * win,
    unsigned int scr_row,
    unsigned int scr_col,
    unsigned int height,
    unsigned int width
);

TWX_API void ZLX_CALL twx_nop_finish (twx_win_t * win);

TWX_API twx_status_t ZLX_CALL twx_default_handler 
(
    twx_win_t * win,
    unsigned int evt, 
    twx_event_info_t * ei
);

/* twx_blank_win_create *****************************************************/
TWX_API twx_status_t ZLX_CALL twx_blank_win_create
(
    twx_t * twx,
    twx_win_t * * win_ptr,
    acx1_attr_t * attr,
    uint32_t ch
);

/* twx_htxt_win_create ******************************************************/
/**
 * Creates a hypertext window.
 */
TWX_API twx_status_t ZLX_CALL twx_htxt_win_create
(
    twx_t * twx,
    twx_win_t * * win_ptr,
    acx1_attr_t * attr_a,
    size_t attr_n,
    char const * htxt
);

/* twx_htxt_win_set_content *************************************************/
/**
 *  Sets the content of the hypertext window.
 *  The format of the text is UTF8 with no control chars other than '\n'
 *  (signifying new line) and '\a' which is followed by a single byte
 *  signifying the index of the attribute to be selected for displaying the
 *  text that follows.
 *  The text starts to be displayed with attribute 0 and each line is padded
 *  to the right with that same attribute.
 */
TWX_API twx_status_t ZLX_CALL twx_htxt_win_set_content
(
    twx_win_t * win,
    void const * htxt
);

/* twx_itxt_win_create ******************************************************/
/**
 *  Creates an input text window.
 */
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
);

#endif

