/**\file
 * Clip info (6D Mark II): a slate for every take, RAW or not.
 *
 * mlv_lite writes it into the clip as an INFO block, read by MLV App and MLV
 * Drive. Canon's MP4 has no room for it, so a take recorded only as H.264 adds
 * a line to SLATE.CSV beside the clips instead; the columns are Resolve's, for
 * its Import Metadata. The H.264 proxy of a RAW take gets no line: the MLV has
 * the slate.
 *
 * Take counts up after every recorded clip and goes back to 1 when the scene or
 * shot changes.
 */

#include "dryos.h"
#include "config.h"
#include "menu.h"
#include "bmp.h"
#include "property.h"
#include "propvalues.h"
#include "fileprefix.h"

#ifdef FEATURE_CLIP_INFO

static CONFIG_INT("clip.info", clip_info_on, 0);
static CONFIG_INT("clip.camera", clip_camera, 0);      /* A..H */
static CONFIG_INT("clip.reel", clip_reel, 1);
static CONFIG_INT("clip.scene", clip_scene, 1);
static CONFIG_INT("clip.shot", clip_shot, 0);          /* 0 = none, 1..26 = A..Z */
static CONFIG_INT("clip.take", clip_take, 1);

static char clip_camera_name(void) { return 'A' + COERCE(clip_camera, 0, 7); }
static char clip_shot_name(void)   { return clip_shot > 0 ? 'A' + COERCE(clip_shot, 1, 26) - 1 : 0; }

/* "camera=A reel=001 scene=12 shot=B take=3" (key=value, space separated).
 * Returns its length, 0 if Clip info is off. */
int clip_info_text(char * text, int size)
{
    if (!clip_info_on) return 0;

    /* snprintf here has no %c */
    char cam[2] = { clip_camera_name(), 0 };
    char shot[2] = { clip_shot_name(), 0 };
    int n = snprintf(text, size, "camera=%s reel=%03d scene=%d", cam, clip_reel, clip_scene);
    if (shot[0]) n += snprintf(text + n, size - n, " shot=%s", shot);
    n += snprintf(text + n, size - n, " take=%d", clip_take);
    return n;
}

static void clip_info_next_take(void)
{
    if (clip_info_on) clip_take = MIN(clip_take + 1, 999);
}

/* mlv_lite brackets each RAW take with these; the H.264 proxy starts inside */
static volatile int raw_take = 0;

void clip_info_raw_take_start(void)
{
    raw_take = 1;
}

void clip_info_raw_take_end(int recorded)
{
    raw_take = 0;
    if (recorded) clip_info_next_take();
}

static void clip_info_log_task(void * unused)
{
    char cam[2] = { clip_camera_name(), 0 };
    char shot[2] = { clip_shot_name(), 0 };
    char line[128];
    char name[FIO_MAX_PATH_LENGTH];
    snprintf(name, sizeof(name), "%s/SLATE.CSV", get_dcim_dir());
    int new_file = !is_file(name);
    int n = 0;
    if (new_file) n = snprintf(line, sizeof(line), "File Name,Scene,Shot,Take,Camera #,Roll Card #\r\n");
    /* lens.c's movie log reads the same number at the same moment */
    n += snprintf(line + n, sizeof(line) - n, "%s%04d.MP4,%d,%s,%d,%s,%s%03d\r\n",
                  get_file_prefix(), get_shooting_card()->file_number, clip_scene, shot, clip_take, cam, cam, clip_reel);

    FILE * f = FIO_CreateFileOrAppend(name);
    if (f)
    {
        FIO_WriteFile(f, line, n);
        FIO_CloseFile(f);
    }
    clip_info_next_take();
}

PROP_HANDLER(PROP_MVR_REC_START)
{
    static int started = 0;
    static int proxy = 0;
    int state = buf[0];

    if (state && !started) proxy = raw_take;
    if (!state && started && !proxy && clip_info_on)
        task_create("clip_info_log", 0x1e, 0x1000, clip_info_log_task, 0);
    started = state;
}

static MENU_UPDATE_FUNC(clip_info_update)
{
    if (!clip_info_on) return;
    char cam[2] = { clip_camera_name(), 0 };
    char shot[2] = { clip_shot_name(), 0 };
    MENU_SET_VALUE("%s%03d  %d%s-%d", cam, clip_reel, clip_scene, shot, clip_take);
}

/* a new scene or shot starts again at take 1 */
static MENU_SELECT_FUNC(clip_scene_select)
{
    menu_numeric_toggle(&clip_scene, delta, 1, 999);
    clip_take = 1;
}

static MENU_SELECT_FUNC(clip_shot_select)
{
    menu_numeric_toggle(&clip_shot, delta, 0, 26);
    clip_take = 1;
}

static struct menu_entry clip_info_menu[] = {
    {
        .name = "Clip info",
        .priv = &clip_info_on,
        .max = 1,
        .update = clip_info_update,
        .help  = "Slate for every take: Q sets camera, reel, scene, shot and take.",
        .help2 = "In the MLV for RAW takes; SLATE.CSV beside the clips for MP4-only takes.",
        .submenu_width = 700,
        .children = (struct menu_entry[]) {
            {
                .name = "Camera",
                .priv = &clip_camera,
                .max = 7,
                .choices = CHOICES("A", "B", "C", "D", "E", "F", "G", "H"),
            },
            {
                .name = "Reel",
                .priv = &clip_reel,
                .min = 1, .max = 999,
                .unit = UNIT_DEC,
            },
            {
                .name = "Scene",
                .priv = &clip_scene,
                .min = 1, .max = 999,
                .unit = UNIT_DEC,
                .select = clip_scene_select,
                .help = "Changing it starts again at take 1.",
            },
            {
                .name = "Shot",
                .priv = &clip_shot,
                .max = 26,
                .choices = CHOICES("-", "A", "B", "C", "D", "E", "F", "G", "H", "I", "J", "K", "L", "M",
                                   "N", "O", "P", "Q", "R", "S", "T", "U", "V", "W", "X", "Y", "Z"),
                .select = clip_shot_select,
                .help = "Changing it starts again at take 1.",
            },
            {
                .name = "Take",
                .priv = &clip_take,
                .min = 1, .max = 999,
                .unit = UNIT_DEC,
                .help = "The next clip's take. Counts up by itself after each clip.",
            },
            MENU_EOL
        },
    },
};

static void clip_info_init()
{
    menu_add("Movie", clip_info_menu, COUNT(clip_info_menu));
}

INIT_FUNC(__FILE__, clip_info_init);

#endif // FEATURE_CLIP_INFO
