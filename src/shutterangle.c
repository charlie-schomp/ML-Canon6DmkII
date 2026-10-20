/**\file
 * Shutter angle lock (6D Mark II): keep the shutter at a chosen angle, so
 * motion blur stays the same when the frame rate changes - 1080p at 23.976
 * and crop mode at 29.97, or a Canon movie size change.
 *
 * ML cannot change the frame rate or the sensor timing on this body, but it
 * can ask Canon for a shutter speed (PROP_SHUTTER, allowed for writing in
 * the 6D2 property whitelist), exactly as turning the dial does.  The lock
 * picks the speed Canon offers in movie mode (1/3 stops) closest to the
 * angle: 180 deg is 1/50 at 23.976 (172.6 deg) and 1/60 at 29.97 (179.8 deg).
 *
 * It acts only outside takes, with the ML menu closed, and only once the
 * frame rate has read the same twice in a row (the view switches modes
 * around a crop-mode change).  Turning the shutter dial with the lock on is
 * undone the same way.  If Canon refuses - an exposure mode that is not
 * Manual - it stops asking until something changes.
 */

#include "dryos.h"
#include "config.h"
#include "menu.h"
#include "bmp.h"
#include "lens.h"
#include "fps.h"
#include "property.h"
#include "propvalues.h"
#include "math.h"

#ifdef FEATURE_SHUTTER_ANGLE_LOCK

static CONFIG_INT("shutter.angle.lock", angle_lock, 0);    /* index into lock_angle_x10 */

static const int lock_angle_x10[] = { 0, 900, 1200, 1440, 1800, 2700, 3600 };
#define ANGLE_CHOICES CHOICES("OFF", "90" SYM_DEGREE, "120" SYM_DEGREE, "144" SYM_DEGREE, \
                              "180" SYM_DEGREE, "270" SYM_DEGREE, "360" SYM_DEGREE)

/* Canon's nominal reciprocal for a movie-mode shutter code (1/50 -> 50), or 0 */
static int nominal_reciprocal(int raw)
{
    if (raw < 70 || raw - 15 >= SHUTTER_ARRAY_LEN) return 0;
    return values_shutter[raw - 15];
}

/* refusal memory: what we asked for, at which frame rate, from which shutter */
static int refused_target = 0, refused_fps = 0, refused_from = 0;

/* the angle a shutter code gives at this frame rate, x10, from Canon's
 * nominal speed (1/50 is 1/50 s, though its APEX code says 1/49.4) */
static int code_angle_x10(int raw, int fps_x1000)
{
    int recip = nominal_reciprocal(raw);
    if (!recip || fps_x1000 <= 0) return 0;
    return (int) ((3600LL * fps_x1000 / 1000 + recip / 2) / recip);
}

/* the 1/3-stop movie shutter code whose angle is closest, or 0 */
static int angle_target_raw(int angle_x10, int fps_x1000)
{
    if (angle_x10 <= 0 || fps_x1000 <= 0) return 0;
    float t = (angle_x10 / 3600.0f) * (1000.0f / fps_x1000);    /* seconds */
    int exact = (int) (56.0f - log2f(t) * 8.0f + 0.5f);         /* raw shutter units */
    int best = 0, best_err = 1 << 30;
    for (int c = exact - 4; c <= exact + 4; c++)
    {
        if (c < 70 || c > FASTEST_SHUTTER_SPEED_RAW) continue;
        int r = c % 8;
        if (r != 0 && r != 3 && r != 5) continue;               /* Canon's 1/3-stop codes */
        int err = ABS(code_angle_x10(c, fps_x1000) - angle_x10);
        if (err < best_err) { best_err = err; best = c; }
    }
    return best;
}

static int angle_lock_active()
{
    return angle_lock && lv && !LV_PAUSED && is_movie_mode() && !gui_menu_shown() && NOT_RECORDING;
}

static void angle_lock_task()
{
    int pending = 0, pending_n = 0;
    TASK_LOOP
    {
        msleep(500);
        if (!angle_lock_active()) { pending_n = 0; continue; }

        int fps = fps_get_current_x1000();
        int target = angle_target_raw(lock_angle_x10[angle_lock], fps);
        int now = lens_info.raw_shutter;
        if (!target || now == target) { pending_n = 0; continue; }

        /* Canon said no to exactly this; wait until something changes */
        if (target == refused_target && fps == refused_fps && now == refused_from) continue;

        /* the same target on two polls in a row */
        if (target != pending) { pending = target; pending_n = 1; continue; }
        if (++pending_n < 2) continue;
        pending_n = 0;

        int t = target;
        prop_request_change_wait(PROP_SHUTTER, &t, 4, 500);
        if (lens_info.raw_shutter != target)
        {
            refused_target = target;
            refused_fps = fps;
            refused_from = lens_info.raw_shutter;
        }
        else
        {
            refused_target = 0;
        }
    }
}

TASK_CREATE("angle_lock_task", angle_lock_task, 0, 0x1e, 0x1000);

static MENU_UPDATE_FUNC(angle_lock_update)
{
    if (!angle_lock) return;
    int fps = fps_get_current_x1000();
    int target = angle_target_raw(lock_angle_x10[angle_lock], fps);
    if (!is_movie_mode())
    {
        MENU_SET_WARNING(MENU_WARN_NOT_WORKING, "Movie mode only.");
        return;
    }
    if (!target) return;

    int real_x10 = code_angle_x10(target, fps);
    MENU_SET_RINFO(SYM_1_SLASH "%d", nominal_reciprocal(target));
    MENU_SET_HELP("At %d.%03d fps: " SYM_1_SLASH "%d = %d.%d" SYM_DEGREE ", the nearest speed Canon offers.",
                  fps / 1000, fps % 1000, nominal_reciprocal(target), real_x10 / 10, real_x10 % 10);
    if (refused_target == target && lens_info.raw_shutter != target)
        MENU_SET_WARNING(MENU_WARN_ADVICE, "Canon refused " SYM_1_SLASH "%d - set movie exposure to Manual.", nominal_reciprocal(target));
}

static struct menu_entry angle_lock_menu[] = {
    {
        .name    = "Shutter angle lock",
        .priv    = &angle_lock,
        .max     = COUNT(lock_angle_x10) - 1,
        .choices = ANGLE_CHOICES,
        .update  = angle_lock_update,
        .help    = "Keeps the shutter at this angle when the frame rate changes",
        .help2   = "(1080p 23.976 / crop 29.97). Movie Manual exposure; not during takes.",
    },
};

static void angle_lock_init()
{
    menu_add("Expo", angle_lock_menu, COUNT(angle_lock_menu));
}

INIT_FUNC(__FILE__, angle_lock_init);

#endif // FEATURE_SHUTTER_ANGLE_LOCK
