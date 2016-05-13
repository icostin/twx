#ifndef TWX_INTERN_H
#define TWX_INTERN_H

#include <zlx.h>
#include <hbs.h>
#include <acx1.h>
#include "twx.h"

#define TWX_KEY_RING_POWER 8

typedef struct blank_win_s blank_win_t;
typedef struct htxt_win_s htxt_win_t;
typedef struct itxt_win_s itxt_win_t;

enum twx_state_enum
{
    TWX_INIT_FAILED = 0,
    TWX_INITED,
    TWX_WAITING,
    TWX_PROCESSING,
    TWX_OUTPUTTING
};

#define TWX_INITED_CONSOLE (1 << 0)
#define TWX_INITED_MUTEX (1 << 1)
#define TWX_INITED_COND (1 << 2)
#define TWX_INITED_INPUT (1 << 3)
#define TWX_INITED_KEY_RING (1 << 4)

struct twx_s
{
    zlx_mutex_t * main_mutex;
    zlx_tid_t input_thread;
    zlx_cond_t * main_cond;
    twx_win_t * root_win;
    twx_win_t * focus_win;
    twx_win_t * new_focus_win;
    uint32_t * key_ring;
    twx_status_t exit_status;
    unsigned int krb, kre, krm;
    unsigned int height, width;
    unsigned int seed;
    uint8_t state;
    volatile uint8_t shutdown;
    uint8_t screen_resized;
    uint8_t draw_mode;
    uint8_t init_state;
};

struct blank_win_s
{
    twx_win_t base;
    acx1_attr_t * attr;
    uint32_t ch;
};

struct htxt_win_s
{
    twx_win_t base;
    zlx_mutex_t * mutex;
    uint8_t * * rta; // row text array
    size_t * rsa; // row len array - how many bytes are allocated for each row
    acx1_attr_t * attr_a;
    size_t attr_n;
    size_t rtn; // number of rows in row text array
    size_t rsn; // number of rows in row size array
    size_t n; // number of used rows
};

struct itxt_win_s
{
    twx_win_t base;
    zlx_mutex_t * mutex;
    acx1_attr_t * attr_a;
    uint8_t * pfx;
    uint8_t * text;
    size_t attr_n;
    size_t pfx_size; // allocated size
    size_t pfx_width;
    size_t text_n;
    size_t text_size;
    size_t view_ofs; // offset in text where to start displaying
    size_t cursor_ofs; // offset in text where the cursor is pointing to
    size_t cursor_col; // column where the cursor is (relative to view text)
    size_t view_width;
    size_t view_size;
    twx_win_t * ntf_win;
    unsigned int ntf_id;
};

#if _DEBUG
#include <stdlib.h>
#include <stdio.h>
#define L(...) ((void) fprintf(stderr, "%s:%u:%s(): ", __FILE__, __LINE__, __FUNCTION__), (void) fprintf(stderr, __VA_ARGS__), (void) fprintf(stderr, "\n"), (void) fflush(stderr))
#define A(_cond) ((_cond)) ? (void) 0 : (L("*** ASSERT FAILED *** %s", #_cond), abort())
#else
#define L(...) ((void) 0)
#define A(_cond) ((void) 0)
#endif

#if _DEBUG >= 2
#define L2(...) L(__VA_ARGS__)
#else
#define L2(...) ((void) 0)
#endif

#define O(_x) \
    if ((cs = (_x))) { \
        L("%s failed: %u", #_x, cs); \
        if (!ts) ts = TWX_CONSOLE_OUTPUT_ERROR; \
        break; \
    } else (void) 0

twx_status_t ZLX_CALL htxt_draw (twx_win_t * win, unsigned int mode);
void ZLX_CALL htxt_finish (twx_win_t * win);

twx_status_t ZLX_CALL blank_draw (twx_win_t * win, unsigned int mode);

void * win_alloc (twx_t * twx, twx_win_class_t * wcls);


#endif /* TWX_INTERN_H */

