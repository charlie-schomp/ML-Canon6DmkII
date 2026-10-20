/** \file
 * Dialog test code
 * Based on http://code.google.com/p/400plus/source/browse/trunk/menu_developer.c
 */
#include "dryos.h"
#include "bmp.h"
#include "tasks.h"
#include "debug.h"
#include "menu.h"
#include "property.h"
#include "config.h"
#include "gui.h"
#include "lens.h"
#include "propvalues.h"

static struct semaphore * notify_box_show_sem = 0;
static struct semaphore * notify_box_main_sem = 0;

static int notify_box_timeout = 0;
static int notify_box_stop_request = 0;
static int notify_box_dirty = 0;
static char notify_box_msg[100];
static char notify_box_msg_tmp[100];

#ifdef FEATURE_VRAM_RGBA
/* On RGBA bodies redraw() does not clear ML's layer while recording (a full
 * clear mid-take killed takes), so a message shown during a take used to stay
 * on screen for the rest of it.  While recording, erase just the rectangle
 * the message was drawn in - a small fill is safe mid-take. */
static int notify_box_drawn_w = 0, notify_box_drawn_lines = 0;

static void notify_box_erase()
{
    if (NOT_RECORDING || !notify_box_drawn_w)
    {
        redraw();
    }
    else
    {
        bmp_fill(COLOR_EMPTY, 50, 50, MIN(notify_box_drawn_w + 8, 720 - 50),
                 notify_box_drawn_lines * fontspec_height(FONT_LARGE));
    }
    notify_box_drawn_w = 0;
}

static void notify_box_draw()
{
    int w = bmp_printf(FONT_LARGE, 50, 50, notify_box_msg);
    int lines = 1;
    for (const char * c = notify_box_msg; *c; c++)
        if (*c == '\n') lines++;
    notify_box_drawn_w = MAX(notify_box_drawn_w, w);
    notify_box_drawn_lines = MAX(notify_box_drawn_lines, lines);
}
#else
static void notify_box_erase() { redraw(); }
static void notify_box_draw() { bmp_printf(FONT_LARGE, 50, 50, notify_box_msg); }
#endif

static void NotifyBox_task(void* priv)
{
    TASK_LOOP
    {
        // wait until some other task asks for a notification
        int err = take_semaphore(notify_box_show_sem, 500);
        if (err)
            continue;
        
        if (!notify_box_timeout) 
            continue;
        
        // show notification for a while, then redraw to erase it
        notify_box_stop_request = 0;
        notify_box_dirty = 0;
        for ( ; notify_box_timeout > 0 ; notify_box_timeout -= 50)
        {
            if (notify_box_dirty)
            {
                /* clear old message */
                notify_box_erase();
                notify_box_dirty = 0;
            }
            notify_box_draw();
            msleep(50);
            if (notify_box_stop_request)
                break;
        }
        notify_box_timeout = 0;
        notify_box_erase();
    }
}

TASK_CREATE( "notifybox_task", NotifyBox_task, 0, 0x1b, 0x1000 );

void NotifyBoxHide()
{
    notify_box_stop_request = 1;
}

void NotifyBox(int timeout, char* fmt, ...) 
{
    // make sure this is thread safe
    take_semaphore(notify_box_main_sem, 0);
    
    va_list ap;
    va_start( ap, fmt );
    vsnprintf( notify_box_msg_tmp, sizeof(notify_box_msg_tmp)-1, fmt, ap );
    va_end( ap );
    
    if (notify_box_timeout && streq(notify_box_msg_tmp, notify_box_msg)) 
    {
        // same message: do not redraw, just re-set the timeout
        notify_box_timeout = MAX(timeout, 100);
        goto end;
    }

    // new message
    qprint("[NotifyBox] "); qprint(notify_box_msg_tmp); qprint("\n");
    memcpy(notify_box_msg, notify_box_msg_tmp, sizeof(notify_box_msg));
    notify_box_msg[sizeof(notify_box_msg)-1] = '\0';
    notify_box_timeout = MAX(timeout, 100);
    if (notify_box_timeout)
        notify_box_dirty = 1; // ask for a redraw, message changed

    give_semaphore(notify_box_show_sem); // request displaying the notification box

end:
    give_semaphore(notify_box_main_sem); // done, other call can be made now
}

static void dlg_init()
{
    notify_box_show_sem = create_named_semaphore("nbox_show_sem", SEM_CREATE_LOCKED);
    notify_box_main_sem = create_named_semaphore("nbox_done_sem", SEM_CREATE_UNLOCKED);
}

INIT_FUNC(__FILE__, dlg_init);
