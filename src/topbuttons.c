/**\file
 * Top-plate buttons as overlay shortcuts in movie LiveView (6D Mark II).
 *
 * In movie LiveView Canon has little use for the AF, ISO, DRIVE and metering
 * buttons, so they can toggle ML overlays, the way cinema cameras use
 * assignable buttons.
 *
 * None of these buttons has a code of its own.  Each press arrives as the
 * generic "button pressed" event (GMT_GUICMD_PRESS_BUTTON_SOMETHING), and
 * only Canon's reaction tells them apart: a top-LCD update
 * (GMT_OLC_INFO_CHANGED) a few ms later, whose arg says what Canon did -
 *   AF       0x16 / 0x11  (AF operation selection opens / closes)
 *   ISO      0x1e         (ISO selection)
 *   DRIVE    9, obj bit 26 set  ("not available in movie mode" warning)
 *   metering and the AF-point button: the same as DRIVE, so all three
 *            share one setting
 * Mapped from d7probe event logs on 1.1.1.  The d7probe MPU log shows why
 * the three cannot be told apart: the MPU never sends which button it was,
 * only Canon's decision, and in movie mode that is the same "not available"
 * warning for all three, byte for byte.
 *
 * The light button sends nothing after the generic event - and neither does
 * the rear dial around SET, so the light button is not offered (v87 tried it:
 * turning that dial toggled the light button's overlay).
 *
 * An update counts as a press only within PRESS_WINDOW_MS of the generic
 * event; Canon sends the same updates on its own when its screens time out.
 * Canon has already acted by then, so nothing is blocked: its screen for the
 * button still flashes up briefly, and ISO stays adjustable with the dial.
 */

#include "dryos.h"
#include "config.h"
#include "menu.h"
#include "bmp.h"
#include "gui.h"
#include "propvalues.h"
#include "notify_box.h"
#include "timer.h"

#ifdef FEATURE_TOP_BUTTON_SHORTCUTS

#define PRESS_WINDOW_MS 60

/* defaults: AF toggles focus peaking, DRIVE/metering false colour, ISO nothing
 * (ISO keeps Canon's ISO screen) - indices into action_vars below */
static CONFIG_INT("topbtn.af",    topbtn_af,    2);
static CONFIG_INT("topbtn.iso",   topbtn_iso,   0);
static CONFIG_INT("topbtn.drive", topbtn_drive, 4);

/* action index -> (name shown, config variable toggled, value when turned on
 * with nothing remembered) */
static const char * const action_names[] = {
    "OFF", "All overlays", "Focus peaking", "Zebras", "False color",
    "Waveform", "Histogram", "Vectorscope", "Magic Zoom", "Frame guides",
};
static const char * const action_vars[] = {
    0, "global.draw", "focus.peaking", "zebra.draw", "falsecolor.draw",
    "waveform.draw", "hist.draw", "vectorscope.draw", "zoom.overlay", "crop.mode",
};
static const int action_default_on[] = { 0, 3, 1, 1, 1, 1, 1, 1, 1, 1 };

/* the value each overlay had when a button turned it off, so turning it back
 * on restores the user's mode (e.g. cropmarks "Recorded area" vs bitmap) */
static int saved_value[COUNT(action_vars)];

#define TOPBTN_CHOICES CHOICES("OFF", "All overlays", "Focus peaking", "Zebras", "False color", \
                               "Waveform", "Histogram", "Vectorscope", "Magic Zoom", "Frame guides")

static void topbtn_toggle(int action)
{
    if (action <= 0 || action >= (int) COUNT(action_vars)) return;
    const char * var = action_vars[action];
    int v = get_config_var(var);
    if (v)
    {
        saved_value[action] = v;
        set_config_var(var, 0);
    }
    else
    {
        set_config_var(var, saved_value[action] ? saved_value[action] : action_default_on[action]);
    }
    NotifyBox(1000, "%s: %s", action_names[action], get_config_var(var) ? "ON" : "OFF");
    redraw();
}

static int last_press_ms = -1000;

int handle_top_buttons(struct event * event)
{
    if (event->param == GMT_GUICMD_PRESS_BUTTON_SOMETHING)
    {
        last_press_ms = get_ms_clock();
        return 1;
    }

    if (event->param != GMT_OLC_INFO_CHANGED) return 1;
    if (get_ms_clock() - last_press_ms > PRESS_WINDOW_MS) return 1;   /* Canon on its own */
    last_press_ms = -1000;                                             /* one press, one action */

    if (!lv || !is_movie_mode() || gui_menu_shown()) return 1;

    uint32_t * obj = (uint32_t *) event->obj;
    int action = 0;
    if (event->arg == 0x16 || event->arg == 0x11)
        action = topbtn_af;
    else if (event->arg == 0x1e)
        action = topbtn_iso;
    else if (event->arg == 9 && obj && (obj[0] & 0x4000000))
        action = topbtn_drive;

    if (action) topbtn_toggle(action);
    return 1;   /* never blocks: Canon has already handled the button */
}

static struct menu_entry topbtn_menus[] = {
    {
        .name = "Top buttons (movie)",
        .select = menu_open_submenu,
        .help  = "Use the AF, ISO and DRIVE/metering/AF-point buttons to toggle",
        .help2 = "overlays in movie LiveView. Canon's own screen still flashes up.",
        .submenu_width = 700,
        .children = (struct menu_entry[]) {
            {
                .name = "AF button",
                .priv = &topbtn_af,
                .max = COUNT(action_vars) - 1,
                .choices = TOPBTN_CHOICES,
                .help = "What the AF button toggles in movie LiveView.",
            },
            {
                .name = "ISO button",
                .priv = &topbtn_iso,
                .max = COUNT(action_vars) - 1,
                .choices = TOPBTN_CHOICES,
                .help  = "What the ISO button toggles in movie LiveView.",
                .help2 = "Canon still opens ISO: the top dial changes ISO afterwards.",
            },
            {
                .name = "DRIVE / metering / AF-pt",
                .priv = &topbtn_drive,
                .max = COUNT(action_vars) - 1,
                .choices = TOPBTN_CHOICES,
                .help  = "What DRIVE, metering or the AF-point button toggles.",
                .help2 = "One setting for all three: the camera reports them identically.",
            },
            MENU_EOL
        },
    },
};

static void topbtn_init()
{
    menu_add("Prefs", topbtn_menus, COUNT(topbtn_menus));
}

INIT_FUNC(__FILE__, topbtn_init);

#endif // FEATURE_TOP_BUTTON_SHORTCUTS
