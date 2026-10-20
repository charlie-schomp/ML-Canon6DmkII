
/** \file
 * Tweaks specific to movie mode
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
#include "math.h"
#include "shoot.h"
#include "zebra.h"
#include "fps.h"
#include "beep.h"
#include "lvinfo.h"
#include "powersave.h"
#include "patch.h"


#ifdef FEATURE_REC_NOTIFY

#ifdef CONFIG_BLUE_LED
#define RECNOTIFY_LED (rec_notify == 3)
#define RECNOTIFY_BEEP (rec_notify == 4)
#else
#define RECNOTIFY_LED 0
#define RECNOTIFY_BEEP (rec_notify == 3)
#endif

#ifdef FEATURE_REC_NOTIFY_BEEP
    #ifndef CONFIG_BEEP
    #error This requires CONFIG_BEEP
    #endif
#else
    #undef RECNOTIFY_BEEP
    #define RECNOTIFY_BEEP 0
#endif

#endif

#ifdef FEATURE_MOVIE_REC_KEY
#define ALLOW_MOVIE_START (movie_rec_key_action == 0 || movie_rec_key_action == 1)
#define ALLOW_MOVIE_STOP  (movie_rec_key_action == 0 || movie_rec_key_action == 2)
#endif

void update_lvae_for_autoiso_n_displaygain();

#ifdef FEATURE_FORCE_HDMI_VGA
CONFIG_INT("hdmi.force.vga", hdmi_force_vga, 0);

static int hdmi_code_array[8];

PROP_HANDLER(PROP_HDMI_CHANGE_CODE)
{
    ASSERT(len == 32);
    memcpy(hdmi_code_array, buf, 32);
}

static void ChangeHDMIOutputSizeToVGA()
{
    hdmi_code_array[0] = 2;
    prop_request_change(PROP_HDMI_CHANGE_CODE, hdmi_code_array, 32);
}

static void ChangeHDMIOutputSizeToFULLHD()
{
    hdmi_code_array[0] = 5;
    prop_request_change(PROP_HDMI_CHANGE_CODE, hdmi_code_array, 32);
} 
#endif

// WB workaround (not saved in movie mode)
//**********************************************************************
//~ CONFIG_INT( "white.balance.workaround", white_balance_workaround, 1);

static CONFIG_INT( "wb.kelvin", workaround_wb_kelvin, 6500);
static CONFIG_INT( "wbs.gm", workaround_wbs_gm, 100);
static CONFIG_INT( "wbs.ba", workaround_wbs_ba, 100);
static int kelvin_wb_dirty = 1;

#ifdef CONFIG_WB_WORKAROUND

void save_kelvin_wb()
{
    if (!lens_info.kelvin) return;
    workaround_wb_kelvin = lens_info.kelvin;
    workaround_wbs_gm = lens_info.wbs_gm + 100;
    workaround_wbs_ba = lens_info.wbs_ba + 100;
    //~ NotifyBox(1000, "Saved WB: %dK GM%d BA%d", workaround_wb_kelvin, workaround_wbs_gm, workaround_wbs_ba);
}

void restore_kelvin_wb()
{
    msleep(500); // to make sure mode switch is complete
    // sometimes Kelvin WB and WBShift are not remembered, usually in Movie mode 
    lens_set_kelvin_value_only(workaround_wb_kelvin);
    lens_set_wbs_gm(COERCE(((int)workaround_wbs_gm) - 100, -9, 9));
    lens_set_wbs_ba(COERCE(((int)workaround_wbs_ba) - 100, -9, 9));
    //~ NotifyBox(1000, "Restored WB: %dK GM%d BA%d", workaround_wb_kelvin, workaround_wbs_gm, workaround_wbs_ba); msleep(1000);
}

// called only in movie mode
void kelvin_wb_workaround_step()
{
    if (!kelvin_wb_dirty)
    {
        save_kelvin_wb();
    }
    else
    {
        restore_kelvin_wb();
        kelvin_wb_dirty = 0;
    }
}
#endif

static int ml_changing_shooting_mode = 0;
PROP_HANDLER(PROP_SHOOTING_MODE)
{
    kelvin_wb_dirty = 1;
    if (!ml_changing_shooting_mode) intervalometer_stop();
    #ifdef FEATURE_EXPO_OVERRIDE
    bv_auto_update();
    #endif
}

void set_shooting_mode(int m)
{
    if (shooting_mode == m)
    {
        /* nothing to do */
        return;
    }
    
#ifdef CONFIG_SEPARATE_BULB_MODE
    #define ALLOWED_MODE(x)  (x == SHOOTMODE_M || x == SHOOTMODE_TV || \
         x == SHOOTMODE_AV || x == SHOOTMODE_P || x == SHOOTMODE_BULB )
#else
    #define ALLOWED_MODE(x)  (x == SHOOTMODE_M || x == SHOOTMODE_TV || \
         x == SHOOTMODE_AV || x == SHOOTMODE_P )
#endif
    
    if (!ALLOWED_MODE(m) || !ALLOWED_MODE(shooting_mode))
    {
        /* only allow switching between M, Tv, Av, P and BULB */
        return;
    }
    
    #undef ALLOWED_MODE
    
    ml_changing_shooting_mode = 1;
    prop_request_change_wait(PROP_SHOOTING_MODE, &m, 4, 1000);
    msleep(500);    /* just in case, since mode switching is quite complex */
    ml_changing_shooting_mode = 0;
}

static CONFIG_INT("movie.restart", movie_restart,0);
static CONFIG_INT("movie.rec-key", movie_rec_key, 0);
static CONFIG_INT("movie.rec-key-action", movie_rec_key_action, 0);
static CONFIG_INT("movie.rec-key-long", movie_rec_key_long, 0);

#ifdef FEATURE_MOVIE_REC_KEY

static void movie_rec_halfshutter_step()
{
    if (!movie_rec_key) return;
    if (!is_movie_mode() || !liveview_display_idle() || gui_menu_shown()) return;

    if (HALFSHUTTER_PRESSED)
    {
        if (movie_rec_key_long)
        {
            // need to keep halfshutter pressed for one second
            for (int i = 0; i < 10; i++)
            {
                msleep(100);
                if (!HALFSHUTTER_PRESSED) break;
            }
            if (!HALFSHUTTER_PRESSED) return;
            info_led_on();
            NotifyBox(1000, "OK");
        }
        
        while (HALFSHUTTER_PRESSED) msleep(50);
        if (NOT_RECORDING && ALLOW_MOVIE_START) schedule_movie_start();
        else if(ALLOW_MOVIE_STOP) schedule_movie_end();
    }
}
#endif

// start with LV
//**********************************************************************

static CONFIG_INT( "enable-liveview",  enable_liveview,
    #ifdef CONFIG_5D2
    0
    #else
    1
    #endif
);

void force_liveview()
{
#ifdef CONFIG_LIVEVIEW
    extern int ml_started;
    while (!ml_started) msleep(50);

    msleep(50);
    if (lv) return;
    info_led_on();

    /* wait for some preconditions */
    while (sensor_cleaning) msleep(100);
    while (lens_info.job_state) msleep(100);
    while (get_halfshutter_pressed()) msleep(100);

    /* paused LV? */
    ResumeLiveView();
    while (get_halfshutter_pressed()) msleep(100);

    if (CURRENT_GUI_MODE)
    {
        /* we may be in some Canon menu opened from LiveView */
        SetGUIRequestMode(0);
        msleep(1000);
    }

    if (!lv)
    {
        /* we are probably in photo mode */
        fake_simple_button(BGMT_LV);
        msleep(1500);
    }

    info_led_off();

    /* make sure LiveView is up and running */
    #ifdef CONFIG_STATE_OBJECT_HOOKS
    wait_lv_frames(3);
    #endif
#endif
}

void close_liveview()
{
    if (lv_paused)
    {
        ResumeLiveView();
    }

    if (lv)
#ifdef CONFIG_EOSM
    {
        /* To shut off LiveView switch to the info screen */
        SetGUIRequestMode(21);
        msleep(1000);
    }
#else
    {
        if (!is_movie_mode()) {
            /* in photo mode, just exit LiveView by "pressing" the LiveView button */
            fake_simple_button(BGMT_LV);
        } else {
            #if defined(CONFIG_5D2) || defined(CONFIG_50D)
            /* on these cameras, pressing the LiveView button won't start recording */
            fake_simple_button(BGMT_LV);
            #else
            /* in movie mode, pressing LiveView would start recording,
             * so go to PLAY mode instead */
            enter_play_mode();
            #endif
        }
        msleep(1000);
    }
#endif
}

static CONFIG_INT("shutter.lock", shutter_lock, 0);
static CONFIG_INT("shutter.lock.value", shutter_lock_value, 0);

#ifdef FEATURE_SHUTTER_LOCK
static void
shutter_lock_print(
    void *          priv,
    int         x,
    int         y,
    int         selected
)
{
    bmp_printf(
        selected ? MENU_FONT_SEL : MENU_FONT,
        x, y,
        "Shutter Lock  : %s",
        shutter_lock ? "ON" : "OFF"
    );
}

static void shutter_lock_step()
{
    if (is_movie_mode()) // no effect in photo mode
    {
        int shutter = lens_info.raw_shutter;
        if (shutter_lock_value == 0) shutter_lock_value = shutter; // make sure it's some valid value
        if (!gui_menu_shown()) // lock shutter
        {
            if (shutter != shutter_lock_value) // i.e. revert it if changed
            {
                //~ lens_set_rawaperture(COERCE(lens_info.raw_aperture + shutter - shutter_lock_value, 16, 96));
                msleep(100);
                lens_set_rawshutter(shutter_lock_value);
                msleep(100);
            }
        }
        else
            shutter_lock_value = shutter; // accept change from ML menu
    }
}
#endif

#ifdef CONFIG_MOVIE_RECORDING_50D_SHUTTER_HACK

CONFIG_INT("shutter.btn.rec", shutter_btn_rec, 1);

void shutter_btn_rec_do(int rec)
{
    if (shutter_btn_rec == 1)
    {
        if (rec) gui_uilock(UILOCK_SHUTTER);
        else gui_uilock(UILOCK_NONE);
    }
    else if (shutter_btn_rec == 2)
    {
        if (rec) SW1(1,0);
        else SW1(0,0);
    }
}
#endif

int movie_was_stopped_by_set = 0;

static int wait_for_lv_err_msg(int wait) // 1 = msg appeared, 0 = did not appear
{
    extern thunk ErrCardForLVApp_handler;
    for(int i = 0; i <= wait/20; i++)
    {
        if((intptr_t)get_current_dialog_handler() == (intptr_t)&ErrCardForLVApp_handler)
        {
            return 1;
        }
        msleep(20);
    }
    return 0;
}

void movtweak_step()
{
    #ifdef FEATURE_MOVIE_REC_KEY
    movie_rec_halfshutter_step();
    #endif

    #ifdef FEATURE_MOVIE_RESTART
        static int recording_prev = 0;
        
        #if defined(CONFIG_5D2) || defined(CONFIG_50D) || defined(CONFIG_7D)
        if(!RECORDING_H264 && recording_prev && !movie_was_stopped_by_set) // see also gui.c
        #else
        if(!RECORDING_H264 && recording_prev && wait_for_lv_err_msg(0))
        #endif
        {
            if (movie_restart)
            {
                msleep(500);
                movie_start();
            }
        }
        recording_prev = RECORDING_H264;

        if(!RECORDING_H264)
        {
            movie_was_stopped_by_set = 0;
        }
    #endif
        
        if (is_movie_mode())
        {
            #ifdef CONFIG_WB_WORKAROUND
            kelvin_wb_workaround_step();
            #endif
            
            #ifdef FEATURE_SHUTTER_LOCK
            if (shutter_lock) shutter_lock_step();
            #endif
        }

        #ifdef FEATURE_FORCE_LIVEVIEW
        if ((enable_liveview && GUIMODE_MOVIE_PRESS_LV_TO_RESUME) ||
            (enable_liveview == 2 && GUIMODE_MOVIE_ENSURE_A_LENS_IS_ATTACHED))
        {
            msleep(200);
            // double-check
            if ((enable_liveview && GUIMODE_MOVIE_PRESS_LV_TO_RESUME) ||
                (enable_liveview == 2 && GUIMODE_MOVIE_ENSURE_A_LENS_IS_ATTACHED))
                force_liveview();
        }
        #endif

        //~ update_lvae_for_autoiso_n_displaygain();
        
        #ifdef FEATURE_FORCE_HDMI_VGA
        if (hdmi_force_vga && is_movie_mode() && (lv || PLAY_MODE) && !gui_menu_shown())
        {
            if (hdmi_code >= 5)
            {
                msleep(1000);
                gui_uilock(UILOCK_EVERYTHING);
                BMP_LOCK(
                    ChangeHDMIOutputSizeToVGA();
                    msleep(300);
                )
                msleep(2000);
                gui_uilock(UILOCK_NONE);
                msleep(5000);
            }
        }
        #endif
}

static CONFIG_INT("screen_layout.lcd", screen_layout_lcd, SCREENLAYOUT_3_2_or_4_3);
static CONFIG_INT("screen_layout.ext", screen_layout_ext, SCREENLAYOUT_16_10);
int* get_screen_layout_ptr() { return EXT_MONITOR_CONNECTED ? &screen_layout_ext : &screen_layout_lcd; }
int get_screen_layout() 
{
    int s = (int) *get_screen_layout_ptr();
    if (s == SCREENLAYOUT_4_3)
    {
        // the 4:3 layout is not suitable for 640x480, or when histograms are turned off
        // use the bottom 3:2 in those cases
        if (is_movie_mode() && video_mode_resolution > 1) return SCREENLAYOUT_UNDER_3_2;
        if (!histogram_or_small_waveform_enabled()) return SCREENLAYOUT_UNDER_3_2; 
        return s;
    }
    return s;
}

#ifdef FEATURE_SCREEN_LAYOUT

int screen_layout_menu_index;
MENU_UPDATE_FUNC(screen_layout_update)
{
    screen_layout_menu_index = *get_screen_layout_ptr();
}

void screen_layout_toggle(void* priv, int delta) 
{ 
    menu_numeric_toggle(get_screen_layout_ptr(), delta, 0, 4);
    screen_layout_menu_index = *get_screen_layout_ptr();
}
#endif

#ifdef FEATURE_MOVIE_RECORDING_50D

static MENU_UPDATE_FUNC(lv_movie_print)
{
    MENU_SET_VALUE(
        lv_movie_select != 2 ? "Disabled" :
        video_mode_resolution == 0 ? "1920x1080, 30fps" : 
        video_mode_resolution == 2 ? "640x480, 30fps" : "Invalid"
    );
    MENU_SET_ENABLED(lv_movie_select == 2);
}

void lv_movie_toggle(void* priv, int delta)
{
    int newvalue = lv_movie_select == 2 ? 1 : 2;
    GUI_SetLvMode(newvalue);
    if (newvalue == 2) GUI_SetMovieSize_b(1);
}

void lv_movie_size_toggle(void* priv, int delta)
{
    int s = video_mode_resolution ? 0 : 2;
    GUI_SetMovieSize_a(s);
}
#endif

#ifdef CONFIG_BLUE_LED
CONFIG_INT("rec.notify", rec_notify, 3);
#else
CONFIG_INT("rec.notify", rec_notify, 0);
#endif

#ifdef FEATURE_REC_NOTIFY

void rec_notify_continuous(int called_from_menu)
{
    if (!is_movie_mode()) return;

    if (RECNOTIFY_LED) // this is non-graphical notification, should also run when display is off
    {
        static int k = 0;
        k++;
        if (k % 10 == 0) // edled may take a while to process, don't try it often
        {
            if (RECORDING) info_led_on();
        }
    }

    if (!zebra_should_run()) return;
    if (gui_menu_shown() && !called_from_menu) return;

    get_yuv422_vram();

    static int prev = 0;
    
    if (rec_notify == 1)
    {
        if (NOT_RECORDING)
        {
            int xc = os.x0 + os.x_ex/2;
            int yc = os.y0 + os.y_ex/2;
            int rx = os.y_ex * 6/10;
            int ry = rx * 9/16; 
            bmp_printf(
                SHADOW_FONT(FONT(FONT_MED, COLOR_WHITE, COLOR_RED)) | FONT_ALIGN_CENTER | FONT_EXPAND(4), 
                xc, yc - ry - font_med.height,
                "NOT RECORDING"
            );
            bmp_draw_rect(COLOR_RED, xc - rx, yc - ry, rx * 2, ry * 2);
            bmp_draw_rect(COLOR_WHITE, xc - rx + 1, yc - ry + 1, rx * 2 - 2, ry * 2 - 2);
            draw_line(xc + rx, yc - ry, xc - rx, yc + ry, COLOR_RED);
            draw_line(xc + rx, yc - ry + 1, xc - rx, yc + ry + 1, COLOR_WHITE);
        }
    }
    else if (rec_notify == 2)
    {
            int screen_layout_menu_index = *get_screen_layout_ptr();
            int rec_indic_x = os.x_max;
            int rec_indic_y = get_ml_topbar_pos() + 32;
            if (screen_layout_menu_index > 2) rec_indic_y = rec_indic_y - 60; // bottom modes need shifting up
            if (RECORDING)
            {
                bmp_printf(
                FONT(FONT_MED_LARGE, COLOR_WHITE, COLOR_RED),
                rec_indic_x - 5 * font_med.width, // substracted some pixels to hide the red dot in top 3:2 and top 16:9
                rec_indic_y,
                "REC"
                );
            }
            else
                bmp_printf(
                FONT_MED_LARGE,
                rec_indic_x - 6 * font_med.width + 5, // align with ML bars
                rec_indic_y,
                "STBY"
                );       
    }
    
    if (prev != RECORDING_STATE) redraw();
    prev = RECORDING_STATE;
}

void rec_notify_trigger(int rec)
{
    if (RECNOTIFY_BEEP)
    {
        if (rec != 2) { unsafe_beep(); }
        if (!rec) { msleep(200); unsafe_beep(); }
    }

    if (RECNOTIFY_LED)
    {
        if (rec != 2) info_led_on();
        if (!rec) info_led_off();
    }

#ifndef CONFIG_50D
    if (rec == 1 && sound_recording_mode == 1 && !fps_should_record_wav())
        NotifyBox(1000, "Sound is disabled.");
#endif
}
#endif


#ifdef FEATURE_EXPO_OVERRIDE
/**
 * Exposure override mode
 * ======================
 * 
 * This mode bypasses exposure settings (ISO, Tv, Av) and uses some overriden values instead.
 * 
 * It's good for bypassing Canon limits:
 * 
 * - Manual video exposure controls in cameras without it (500D, 50D, 1100D)
 * - 1/25s in movie mode (24p/25p) -> 1/3 stops better in low light
 * - ISO 12800 is allowed in movie mode
 * - No restrictions on certain ISO values and shutter speeds (e.g. on 60D)
 * - Does not have the LiveView underexposure bug with manual lenses
 * - Previews DOF all the time in photo mode
 * 
 * Side effects:
 * 
 * - In photo mode, anything slower than 1/25 seconds will be underexposed in LiveView
 * - No exposure simulation; it's wysiwyg, like in movie mode
 * - You can't use display gain (night vision)
 *  
 * In menu:
 * 
 * - OFF: Canon default mode.
 * - ON: only values from Expo are used; Canon graphics may display other values.
 * 
 */

static struct semaphore * bv_sem = 0;

CONFIG_INT("bv.auto", bv_auto, 0);

static MENU_UPDATE_FUNC(bv_display)
{
    extern int zoom_auto_exposure;

    if (bv_auto == 1 && !CONTROL_BV) 
        MENU_SET_WARNING(MENU_WARN_NOT_WORKING,
            (zoom_auto_exposure && lv_dispsize > 1) ? "Temporarily disabled (auto exposure on zoom)." :
            LVAE_DISP_GAIN ? "Temporarily disabled (display gain active)." :
            "Temporarily disabled."
        );
}

CONFIG_INT("bv.iso", bv_iso, 88);
CONFIG_INT("bv.tv", bv_tv, 111);
CONFIG_INT("bv.av", bv_av, 48);

void bv_enable()
{
    if (CONTROL_BV) return; // already enabled, nothing to do
    
    //~ bmp_printf(FONT_LARGE, 50, 50, "EN     ");
    take_semaphore(bv_sem, 0);

    //~ bmp_printf(FONT_LARGE, 50, 50, "ENable ");
    call("lvae_setcontrolbv", 1);

    int auto_movie = (ae_mode_movie == 0) && is_movie_mode();

    if (auto_movie) // auto movie mode
    {
        bv_apply_tv(bv_tv);
        bv_apply_av(bv_av);
        bv_apply_iso(bv_iso);
    }
    else // manual movie mode or photo mode, try to sync with Canon values
    {
        bv_tv = lens_info.raw_shutter && ABS(lens_info.raw_shutter - bv_tv) > 4 ? lens_info.raw_shutter : bv_tv;
        bv_av = lens_info.raw_aperture ? lens_info.raw_aperture : bv_av;
        bv_iso = lens_info.raw_iso ? lens_info.raw_iso - (get_htp() ? 8 : 0) : bv_iso;
        bv_apply_tv(bv_tv);
        bv_apply_av(bv_av);
        bv_apply_iso(bv_iso);
    }
    
    CONTROL_BV_ZERO = 0;
    bv_update_lensinfo();

    give_semaphore(bv_sem);
}

void bv_disable()
{
    //~ bmp_printf(FONT_LARGE, 50, 50, "DIS    ");
    take_semaphore(bv_sem, 0);

    if (!CONTROL_BV) goto end;
    call("lvae_setcontrolbv", 0);
    CONTROL_BV_TV = CONTROL_BV_AV = CONTROL_BV_ISO = CONTROL_BV_ZERO = 0; // auto
    if (!lv) goto end;
    
    iso_auto_restore_hack();
    set_photo_digital_iso_gain_for_bv(1024);

    //~ bmp_printf(FONT_LARGE, 50, 50, "DISable");

end:
    give_semaphore(bv_sem);
}

void bv_toggle(void* priv, int delta)
{
    menu_numeric_toggle(&bv_auto, delta, 0, 1);
    if (bv_auto) bv_auto_update();
    else bv_disable();
}

PROP_HANDLER(PROP_LIVE_VIEW_VIEWTYPE)
{
    bv_auto_update();
}
#endif

//~ CONFIG_INT("lvae.iso.min", lvae_iso_min, 72);
//~ CONFIG_INT("lvae.iso.max", lvae_iso_max, 104);
//~ CONFIG_INT("lvae.iso.spd", lvae_iso_speed, 10);
//~ CONFIG_INT("lvae.disp.gain", lvae_disp_gain, 0);

CONFIG_INT("iso.smooth", smooth_iso, 0);
static CONFIG_INT("iso.smooth.spd", smooth_iso_speed, 2);

#ifdef FEATURE_GRADUAL_EXPOSURE

#ifndef CONFIG_FRAME_ISO_OVERRIDE
#error This requires CONFIG_FRAME_ISO_OVERRIDE. 
#endif

#ifndef FEATURE_EXPO_ISO_DIGIC
#error This requires FEATURE_EXPO_ISO_DIGIC. 
#endif

void smooth_iso_step()
{
    static int iso_acc = 0;
    if (!smooth_iso) 
    { 
        fps_expo_iso_step();
        iso_acc = 0; return; 
    }
    if (!is_movie_mode()) { iso_acc = 0; return; }
    if (!lv) { iso_acc = 0; return; }
    if (!lens_info.raw_iso) { iso_acc = 0; return; } // no auto iso
    
    static int prev_bv = (int)0xdeadbeef;
    #ifdef FRAME_BV
    int current_bv = FRAME_BV;
    #else
    int current_bv = -(FRAME_ISO & 0xFF);
    #endif
    int current_iso = FRAME_ISO & 0xFF;
    
    //~ static int k = 0; k++;
    
    static int frames_to_skip = 30;
    if (frames_to_skip) { frames_to_skip--; prev_bv = current_bv; return; }
    
    if (prev_bv != current_bv && prev_bv != (int)0xdeadbeef)
    {
        iso_acc -= (prev_bv - current_bv) * (1 << smooth_iso_speed);
        iso_acc = COERCE(iso_acc, -8 * 8 * (1 << smooth_iso_speed), 8 * 8 * (1 << smooth_iso_speed)); // don't correct more than 8 stops (overflow risk)
    }
    if (iso_acc)
    {
        float gf = 1024.0f * powf(2, iso_acc / (8.0f * (1 << smooth_iso_speed)));

        // it's not a good idea to use a digital ISO gain higher than +/- 0.5 EV (noise or pink highlights), 
        // so alter it via FRAME_ISO
        int digic_iso_gain_movie = get_digic_iso_gain_movie();
        #define G_ADJ ((int)roundf(digic_iso_gain_movie ? gf * digic_iso_gain_movie / 1024 : gf))
        int altered_iso = current_iso;
        while (G_ADJ > 861*2 && altered_iso < MAX_ANALOG_ISO) 
        {
            altered_iso += 8;
            gf /= 2;
        }
        while ((G_ADJ < 861 && altered_iso > 80) || (altered_iso > MAX_ANALOG_ISO))
        {
            altered_iso -= 8;
            gf *= 2;
        }
        
        #ifdef CONFIG_FRAME_SHUTTER_OVERRIDE
        // if ISO should go under 100, try a faster shutter speed
                
        int current_shutter = FRAME_SHUTTER_TIMER;
        int altered_shutter = current_shutter;

        while (G_ADJ < 861)
        {
            altered_shutter = MAX(2, altered_shutter / 2);
            gf *= 2;
        }

        if (altered_shutter != current_shutter)
        {
            FRAME_SHUTTER_TIMER = altered_shutter;
        }
        
        // fix imperfect sync
        // only do this when shutter speed is overriden by ML, not by user
        static int prev_shutter = 0;
        static int prev_altered_shutter = 0;
        if (prev_shutter && prev_altered_shutter && 
            prev_altered_shutter != altered_shutter && 
            prev_shutter == current_shutter
            )
        {
            gf = gf * altered_shutter / prev_altered_shutter;
        }
        prev_altered_shutter = altered_shutter;
        prev_shutter = current_shutter;
        
        #endif

        if (altered_iso != current_iso)
        {
            FRAME_ISO = altered_iso | (altered_iso << 8);
        }

        #if defined(CONFIG_5D2) || defined(CONFIG_550D) || defined(CONFIG_50D)
        // FRAME_ISO not synced perfectly, use digital gain to mask the flicker
        static int prev_altered_iso = 0;
        if (prev_altered_iso && prev_altered_iso != altered_iso)
            gf = gf * powf(2, (altered_iso - prev_altered_iso) / 8.0);
        prev_altered_iso = altered_iso;
        
        // also less than perfect sync when shutter speed is changed
        #ifdef FRAME_SHUTTER
        static int prev_tv = 0;
        int tv = FRAME_SHUTTER;
        if (prev_tv && prev_tv != tv)
        {
            gf = gf * powf(2, (prev_tv - tv) / 8.0);
        }
        prev_tv = tv;
        #endif
        #endif


        int g = (int)roundf(COERCE(gf, 1, 1<<20));
        if (g == 1024) g = 1025; // force override 

        set_movie_digital_iso_gain_for_gradual_expo(g);
        if (iso_acc > 0) iso_acc--; else iso_acc++;

    }
    else set_movie_digital_iso_gain_for_gradual_expo(1024);
    
    
    prev_bv = current_bv;
    
    // display a little progress bar

    int x0 = os.x0 + os.x_ex/2;
    int y0 = os.y_max - 2;
    int w = COERCE(iso_acc * 8 / (1 << smooth_iso_speed), -os.x_ex/2, os.x_ex/2);
    static int prev_w = 0;
    if (w || w != prev_w)
    {
        draw_line(x0, y0, x0 + w, y0, iso_acc > 0 ? COLOR_RED : COLOR_LIGHT_BLUE);
        draw_line(x0, y0 + 1, x0 + w, y0 + 1, iso_acc > 0 ? COLOR_RED : COLOR_LIGHT_BLUE);
        if (prev_w != w)
        {
            draw_line(x0 + w, y0, x0 + prev_w, y0, COLOR_BLACK);
            draw_line(x0 + w, y0 + 1, x0 + prev_w, y0 + 1, COLOR_BLACK);
        }
        for (int i = 64; i < ABS(w); i += 64) // mark full stops
        {
            int is = i * SGN(w);
            draw_line(x0 + is, y0, x0 + is, y0 + 1, COLOR_BLACK);
            draw_line(x0 + is + 1, y0, x0 + is + 1, y0 + 1, COLOR_BLACK);
        }
        
        prev_w = w;
    }

}
#endif

#ifdef FEATURE_OVERRIDE_MOVIE_30_MIN_LIMIT
static CONFIG_INT("movie.time_limit", mov_time_limit, 30 * 60);

static MENU_UPDATE_FUNC(print_mov_time_limit)
{
    if (mov_time_limit >= 60)
    {
        MENU_SET_VALUE("%d min", mov_time_limit / 60);
    }
    else
    {
        MENU_SET_VALUE("%d sec", mov_time_limit);
    }
    MENU_SET_WARNING(MENU_WARN_ADVICE, entry->help2);
}

static void change_mov_time_limit(void* priv, int delta)
{
    // Time is in held in seconds.  Defaults to 30m.
    // Allow upwards changes 10m at a time, max 90m (it is not determined what true max is).
    // Allow downwards changes 5m at a time until 5m, then 1m, then 10s,
    // 10s minimum.

    // Lets be extra sure to avoid negatives since we're MMU patching ROM code
    // with this value.
    if (mov_time_limit <= 0)
    {
        mov_time_limit = 10;
        return;
    }

    if (delta < 0)
    {
        if (mov_time_limit <= 10)
            mov_time_limit = 10;
        else if (mov_time_limit <= 60)
            mov_time_limit -= 10;
        else if (mov_time_limit <= 5 * 60)
            mov_time_limit -= 60;
        else if (mov_time_limit <= 30 * 60)
            mov_time_limit -= 5 * 60;
        else if (mov_time_limit <= 90 * 60)
            mov_time_limit -= 15 * 60;
        else if (mov_time_limit <= 180 * 60)
            mov_time_limit -= 30 * 60;
    }
    else if (delta > 0)
    {
        if (mov_time_limit >= 180 * 60)
            mov_time_limit = 180 * 60;
        else if (mov_time_limit >= 90 * 60)
            mov_time_limit += 30 * 60;
        else if (mov_time_limit >= 30 * 60)
            mov_time_limit += 15 * 60;
        else if (mov_time_limit >= 5 * 60)
            mov_time_limit += 5 * 60;
        else if (mov_time_limit >= 60)
            mov_time_limit += 60;
        else if (mov_time_limit >= 10)
            mov_time_limit += 10;
    }
    // We deliberately fall through if delta == 0,
    // to allow restoring saved settings without adjusting them.

    // We have our target max time. If user requests 30 min,
    // we revert to stock, which technically is 29m 59s.
    // Otherwise, we patch in the new limit.
    struct patch patches[2] = {
        {
            .addr = (uint8_t *)MVR_TIME_LIMIT_NORMAL_FPS,
            .old_value = 0x1b7358, // 29m59s
            .new_value = mov_time_limit * 1000,
            .size = 4,
            .description = "MOV time limit"
        },
        {
            .addr = (uint8_t *)MVR_TIME_LIMIT_HIGH_FPS,
            .old_value = 0x6d9e8, // 7m29s
            .new_value = mov_time_limit * 1000,
            .size = 4,
            .description = "MOV time limit, high FPS"
        }
    };
    unpatch_memory((uint32_t)patches[0].addr);
    unpatch_memory((uint32_t)patches[1].addr);
    if (mov_time_limit == 30 * 60)
    {
        return;
    }

    apply_patches(patches, 2);
    return;
}
#endif // FEATURE_OVERRIDE_MOVIE_30_MIN_LIMIT

void movtweak_task_init()
{
#ifdef FEATURE_FORCE_LIVEVIEW
    if (!lv && enable_liveview && is_movie_mode()
        && (GUIMODE_MOVIE_PRESS_LV_TO_RESUME || GUIMODE_MOVIE_ENSURE_A_LENS_IS_ATTACHED))
    {
        force_liveview();
    }
#endif

    extern int ml_started;
    while (!ml_started) msleep(100);

#ifdef FEATURE_OVERRIDE_MOVIE_30_MIN_LIMIT
    // This restores saved time limit from config file
    change_mov_time_limit(NULL, 0);
#endif

#ifdef FEATURE_EXPO_OVERRIDE
    bv_auto_update();
#endif
}


#ifdef FEATURE_WAV_AUDIO
/* Scratch audio via Canon's own WAV recorder.
 *
 * Cams with no ML audio backend (no ASIF stubs - true of every DIGIC 7 port)
 * cannot use mlv_snd, so the only documented way to get a sound track next to
 * raw video was mlv_lite's H.264 proxy, which costs several MB/s of write
 * bandwidth on a body that has none to spare.
 *
 * Canon's factory-adjustment eventprocs expose a complete WAV recorder that
 * needs no video at all. On 6D2 1.1.1:
 *
 *   FA_AudioRecStart  0xe008c0a0 -> ... -> 0xe064fc6e  FA_Audio_MakeWaveFileStart
 *   FA_AudioRecStop   0xe008c0ac -> ... -> 0xe064fd4a  FA_Audio_MakeWaveFileStop
 *
 * MakeWaveFileStart formats 'B:/Sample%d.wav' (or 'A:/...' depending on the
 * card check at 0xe065e9f4) and configures 0x2ee00 = 192000 bytes/sec, i.e.
 * 48 kHz 16-bit stereo -> about 0.19 MB/s, versus several MB/s for the proxy.
 *
 * Argument handling matters: FA_AudioRecStart does
 *     e008c0a0  ldrb r0, [r0]
 * so it takes a POINTER TO A BYTE, and that byte becomes %d in the filename.
 * Passing a plain integer would dereference it as an address. FA_AudioRecStop
 * takes no argument at all.
 *
 * EXPERIMENTAL: these are factory routines, reached by name rather than by a
 * verified stub, and this has not been confirmed on hardware yet.
 */

/* 0 OFF, 1 Auto with RAW, 2 Manual - the menu order.  Saved under a new name
 * since v80 swapped Auto and Manual, so an old saved value is not misread. */
#define WAV_SCRATCH_OFF    0
#define WAV_SCRATCH_AUTO   1
#define WAV_SCRATCH_MANUAL 2
#ifdef CONFIG_6D2
/* 6D2 default: the port's shooting setup (settings files store only values
 * that differ from the default, so a user's own choice still wins) */
static CONFIG_INT("wav.scratch.mode", wav_scratch_audio, WAV_SCRATCH_AUTO);
#else
static CONFIG_INT("wav.scratch.mode", wav_scratch_audio, WAV_SCRATCH_OFF);
#endif
static int wav_is_recording = 0;
static int wav_mic_now = 0;     /* 0 internal, 1 external: the take in progress */
#ifdef AUDIO_PEAK_REG
static CONFIG_INT("wav.meter", wav_meter, 1);          /* level meter: 0 off, 1 while the WAV records */
#else
static const int wav_meter = 0;
#endif
/* Take counter, persisted. It was a plain static that reset to 0 on every
 * boot - and Canon's recorder APPENDS to an existing Sample%d.wav rather than
 * replacing it. So each new session started by adding fresh audio onto the
 * end of whatever orphaned SampleN.wav an earlier session had left in the card
 * root, and the rename then filed the whole thing under today's clip: seen on
 * hardware as a 25s take with 414s of audio, the first six minutes of it from
 * days before. Persisting is belt; wav_scratch_free_number() is braces. */
static CONFIG_INT("wav.scratch.number", wav_file_number, 0);
static int wav_last_number = -1;         /* take Canon just closed, -1 if none */

void wav_scratch_stop(void);   /* defined below */

/* Input and gain for the scratch recorder.
 *
 * Canon's factory WAV recorder runs the audio driver in "factory mode", and in
 * that mode two things differ from normal movie recording (SoundDevice.c,
 * 1.1.1 0xe0646bce): the mic is NOT chosen by jack detection but by a factory
 * flag that defaults to the internal mic, and the input level comes from the
 * factory set-up rather than your Canon "Rec. level" - measured on a take as
 * peaks around -41 dBFS.
 *
 * Input: set through the factory eventproc Test_AudSetMicAmp (0 internal,
 * 1 external) before the recorder starts.  Auto follows the jack.
 *
 * Gain (calibrated against the v63 test takes): the level Canon takes is not
 * an abstract 0..63, it is an index into a 64-entry table per input
 * (AUDIO_GAIN_TABLE_*), whose entries go to the audio block's digital input
 * gain registers as a signed value in 0.375 dB steps (entry 0 = mute).  The
 * index steps are uneven (~0.875 dB to index 48, 0.75 above), so the menu
 * offers whole dB instead and looks up the nearest entry: stop 0 is mute,
 * stops 1..19 are -12 dB .. +42 dB in 3 dB steps.  Measured: internal mic
 * 20 -> 40 is +17.6 dB by the table, 17.3..18.3 dB on stationary noise.
 *
 * Canon's driver writes table[LEFT] to BOTH registers - the right level it
 * stores is never read - so ML writes the right channel itself.  That also
 * allows a safety channel: the same source a fixed number of dB lower, to
 * recover clipped peaks in post (6D2/tools/wav_dualgain.py).  Verified on
 * hardware: five takes with R 10-13 dB under L.
 *
 * All RAM/register state - nothing is written to Canon settings or factory
 * data (the factory mic-volume record, property 0x02050045, is deliberately
 * untouched).
 *
 * There is ONE mic select for both channels in the driver, so a different
 * source per channel (e.g. external on L, internal on R) is not possible;
 * per-channel control is the level. */
static CONFIG_INT("wav.input", wav_input, 0);          /* 0 auto, 1 internal, 2 external */
#ifdef SOUNDDEV_SET_VOLUME_IN
static CONFIG_INT("wav.gain.mode", wav_gain_mode, 0);  /* 0 factory default, 1 manual */
static CONFIG_INT("wav.gain.ldb", wav_gain_l, 11);     /* stop index, 11 = +18 dB */
static CONFIG_INT("wav.gain.rdb", wav_gain_r, 11);
static CONFIG_INT("wav.gain.rmode", wav_gain_rmode, 0); /* 0 same as left, 1 own, 2..5 safety -6/-12/-18/-24 */
#define WAV_GAIN_STOPS 20
#define WAV_STOP_DB(i) (-12 + ((i) - 1) * 3)           /* i >= 1 */
#define WAV_GAIN_CHOICES CHOICES("Mute", "-12 dB", "-9 dB", "-6 dB", "-3 dB", "0 dB", \
                                 "+3 dB", "+6 dB", "+9 dB", "+12 dB", "+15 dB", "+18 dB", \
                                 "+21 dB", "+24 dB", "+27 dB", "+30 dB", "+33 dB", "+36 dB", \
                                 "+39 dB", "+42 dB")
#endif

static uint8_t * wav_sounddev_state(void)
{
#ifdef SOUNDDEV_STATE_PTR_ADDR
    uint8_t * st = *(uint8_t **)(SOUNDDEV_STATE_PTR_ADDR + 8);
    /* heap object; refuse anything that does not look like one */
    if ((uint32_t)st < 0x1000 || (uint32_t)st >= 0x80000000) return 0;
    return st;
#else
    return 0;
#endif
}

static int wav_ext_mic_plugged(void)
{
    uint8_t * st = wav_sounddev_state();
    return st ? (st[0x28] != 0) : 0;
}

static int wav_mic_to_use(void)
{
    if (wav_input == 1) return 0;
    if (wav_input == 2) return 1;
    return wav_ext_mic_plugged();
}

#if defined(SOUNDDEV_SET_VOLUME_IN) && defined(AUDIO_GAIN_TABLE_INT)
#define WAV_GAIN_MUTE ((int32_t)0xffff0000)

/* the table Canon uses for this input: internal/external, attenuator on or off
 * (SoundDevice +0x30, fAttenuatorMode).  Sanity-checked before use - a wrong
 * address would otherwise put nonsense into the gain registers. */
static const int32_t * wav_gain_table(int ext)
{
    uint8_t * st = wav_sounddev_state();
    int att = st ? (*(uint32_t *)(st + 0x30) == 1) : 0;
    const int32_t * t = (const int32_t *)(ext ? (att ? AUDIO_GAIN_TABLE_EXT_ATT : AUDIO_GAIN_TABLE_EXT)
                                              : (att ? AUDIO_GAIN_TABLE_INT_ATT : AUDIO_GAIN_TABLE_INT));
    if (t[0] != WAV_GAIN_MUTE) return 0;
    if (!(t[1] < t[32] && t[32] < t[63])) return 0;
    if (t[1] < -128 || t[63] > 160) return 0;
    return t;
}

/* table entry closest to a requested gain in dB (entries are 0.375 dB steps) */
static int32_t wav_level_reg(const int32_t * t, int stop)
{
    stop = COERCE(stop, 0, WAV_GAIN_STOPS - 1);
    if (stop == 0) return WAV_GAIN_MUTE;
    int want = WAV_STOP_DB(stop) * 8 / 3;
    int best = 1, bestd = 1 << 30;
    for (int k = 1; k <= 63; k++)
    {
        int d = ABS(t[k] - want);
        if (d < bestd) { bestd = d; best = k; }
    }
    return t[best];
}

static int32_t wav_right_reg(const int32_t * t, int32_t lreg)
{
    static const int safety_steps[] = { 16, 32, 48, 64 };   /* 6/12/18/24 dB in 0.375 dB steps */
    if (wav_gain_rmode == 1) return wav_level_reg(t, wav_gain_r);
    if (wav_gain_rmode >= 2 && wav_gain_rmode <= 5)
    {
        if (lreg == WAV_GAIN_MUTE) return WAV_GAIN_MUTE;
        return MAX(lreg - safety_steps[wav_gain_rmode - 2], t[1]);
    }
    return lreg;
}

/* "+22.5 dB" / "Mute" for a register value */
static void wav_reg_str(char * buf, int len, int32_t reg)
{
    if (reg == WAV_GAIN_MUTE) { snprintf(buf, len, "Mute"); return; }
    int x10 = reg * 375 / 100;              /* dB x 10, 0.375 dB steps */
    snprintf(buf, len, "%s%d.%d dB", x10 < 0 ? "-" : "+", ABS(x10) / 10, ABS(x10) % 10);
}
#endif

static void wav_apply_gain(void)
{
#if defined(SOUNDDEV_SET_VOLUME_IN) && defined(AUDIO_GAIN_TABLE_INT)
    if (!wav_gain_mode) return;
    uint8_t * st = wav_sounddev_state();
    if (!st || !st[0x3e]) return;               /* only while the factory recorder owns the driver */
    if (!*(uint32_t *)(st + 0x44)) return;      /* input not running yet */

    /* Canon's setter first, so the driver's own idea of the level matches ours
     * (it also turns ALC off).  It writes table[L] to both channels; the two
     * register writes below then set the channels apart. */
    int idx = COERCE(wav_gain_l, 0, WAV_GAIN_STOPS - 1) * 63 / (WAV_GAIN_STOPS - 1);
    int (*set_volume_in)(int alc, int apply, int l, int r) = (void *) SOUNDDEV_SET_VOLUME_IN;
    set_volume_in(0, 1, idx, idx);

    const int32_t * t = wav_gain_table(wav_mic_to_use());
    if (!t) return;
    int32_t lreg = wav_level_reg(t, wav_gain_l);
    MEM(AUDIO_GAIN_REG_L) = lreg;
    MEM(AUDIO_GAIN_REG_R) = wav_right_reg(t, lreg);
#endif
}

/* Advance the counter past any Sample%d.wav already on the card, so Canon is
 * always asked to create a file rather than handed one to append to. Checks
 * both drives Canon might use. Bounded: the counter is a byte on Canon's side
 * (ldrb), so wrap at 256 and give up after one full lap. */
static void wav_scratch_free_number(void)
{
    char path[32];
    uint32_t sz;
    for (int tries = 0; tries < 256; tries++)
    {
        int n = wav_file_number & 0xFF;
        snprintf(path, sizeof(path), "B:/Sample%d.wav", n);
        int b = (FIO_GetFileSize(path, &sz) == 0);
        snprintf(path, sizeof(path), "A:/Sample%d.wav", n);
        int a = (FIO_GetFileSize(path, &sz) == 0);
        if (!a && !b) return;                       /* free - use it */
        wav_file_number = (wav_file_number + 1) & 0xFF;
    }
    /* every number taken; recording will append somewhere. Say so. */
    NotifyBox(3000, "WAV: 256 Sample files on card - clear the root");
}

void wav_scratch_start(void)
{
    if (wav_is_recording)
    {
        /* Our flag says a recording is open. mlv_lite now stops on every exit
         * path, so if this is set the likely truth is that Canon's recorder
         * failed or stopped on its own and our flag is stale - and the one
         * thing NOT to do is call FA_AudioRecStop on a recorder that is not
         * running: that is a SoundDevice assert (seen on hardware as
         * "ASSERT: FALSE at ./SoundDevice/SoundDevice.c:469"). An earlier
         * version did exactly that here. Skip this take's audio, say so, and
         * clear the flag so the next take starts clean. */
        NotifyBox(2000, "WAV busy - no audio this take");
        wav_is_recording = 0;
        wav_last_number = -1;
        return;
    }
    wav_scratch_free_number();

    /* FA_AudioRecStart (0xe008c0a0) does ldrb r0,[r0] on the vararg array, so
     * it wants the take number BY VALUE as vararg 1. v13 passed a pointer, so
     * Canon used the low byte of that address as the number and the file landed
     * under an unpredictable name. Canon writes B:/Sample%d.wav, falling back to
     * A:/Sample%d.wav - see 0xe064fc6e. */
    /* factory mode ignores the jack; pick the mic ourselves (see above).
     * Test_AudSetMicAmp reads vararg 1 by value (ldr r4,[r0]). */
    int mic = wav_mic_to_use();
    call("Test_AudSetMicAmp", mic);

    call("FA_AudioRecStart", (int) (wav_file_number & 0xFF));
    wav_is_recording = 1;

#ifdef SOUNDDEV_SET_VOLUME_IN
    /* the driver only forwards a level to the IC once input is active */
    if (wav_gain_mode)
    {
        uint8_t * st = wav_sounddev_state();
        for (int i = 0; i < 20 && st && !*(volatile uint32_t *)(st + 0x44); i++) msleep(10);
        wav_apply_gain();
    }
#endif
    /* with the level meter on, the meter itself says the WAV is running and
     * which mic; a message here would sit under it */
    wav_mic_now = mic;
    if (!wav_meter) NotifyBox(2000, "WAV rec (%s mic)", mic ? "ext" : "int");
}

void wav_scratch_stop(void)
{
    if (!wav_is_recording) return;
    call("FA_AudioRecStop");             /* no argument */
    wav_is_recording = 0;
    wav_last_number = wav_file_number & 0xFF;   /* the file Canon has just closed */
    NotifyBox(2000, "WAV stopped: Sample%d.wav", wav_file_number);
    wav_file_number = (wav_file_number + 1) & 0xFF;   /* next take gets its own file */
}

#ifdef AUDIO_PEAK_REG
/* ---- level meter --------------------------------------------------------
 *
 * L/R peak bars in dBFS with a 1.5 s peak hold and CLIP, while the scratch
 * WAV records.  AUDIO_PEAK_REG holds the current L/R peaks (what Canon's own
 * meter reads, 0xe04b576e); it is only meaningful while the recorder has the
 * input live, so the meter only runs then.
 *
 * Brought back from v64-v66 under the rule the v67 revert taught: on this
 * body ML draws into its own layer, and every draw makes the compositor push
 * that layer over whatever Canon is showing.  The v66 meter drew 20 times a
 * second gated only on `lv`, so it sat on top of Canon's screens and broke
 * every LiveView overlay.  This one draws only while liveview_display_idle()
 * (plain LiveView, no Canon dialog, no ML menu), ten times a second, and
 * clears its own small rectangle once when it has to go away.  No "always on"
 * mode: metering without a take needs the audio block started by hand
 * (FA_Aud_OnlyBlockStart) - a separate experiment, not part of this. */

static int wav_meter_db10[2] = { -999, -999 };      /* dBFS x 10, what the bars show */
static int wav_meter_hold10[2] = { -999, -999 };    /* peak hold, dBFS x 10 */
static int wav_clip_until = 0;

static int wav_peak_to_db10(int peak)
{
    if (peak <= 0) return -999;
    return (int)(200.0f * log10f(peak / 32767.0f) - 0.5f);
}

static uint8_t wav_db_color(int db10)
{
    if (db10 > -30)  return COLOR_RED;       /* above -3 dBFS */
    if (db10 > -120) return COLOR_YELLOW;    /* -12 .. -3 */
    return COLOR_GREEN1;
}

/* top left, clear of mlv_lite's recording indicator (x 500..720, y 40..118)
 * and its messages (y 70 and below) */
#define WAV_METER_X    24
#define WAV_METER_Y    48
#define WAV_METER_W    240     /* -48 .. 0 dBFS, 5 px per dB */
#define WAV_METER_H    6
#define WAV_METER_DB   48
#define WAV_TEXT_X     (WAV_METER_X + WAV_METER_W + 8)
#define WAV_TEXT_W     220     /* ends before x 500, mlv_lite's indicator */

static void wav_meter_draw(int with_text)
{
    for (int ch = 0; ch < 2; ch++)
    {
        int y = WAV_METER_Y + ch * (WAV_METER_H + 3);
        int db10 = wav_meter_db10[ch];
        int w = COERCE((db10 + WAV_METER_DB * 10) * WAV_METER_W / (WAV_METER_DB * 10), 0, WAV_METER_W);
        int hx = COERCE((wav_meter_hold10[ch] + WAV_METER_DB * 10) * WAV_METER_W / (WAV_METER_DB * 10), 0, WAV_METER_W - 2);
        bmp_fill(COLOR_BLACK, WAV_METER_X - 2, y - 1, WAV_METER_W + 4, WAV_METER_H + 2);
        if (w > 0) bmp_fill(wav_db_color(db10), WAV_METER_X, y, w, WAV_METER_H);
        if (wav_meter_hold10[ch] > -999) bmp_fill(COLOR_WHITE, WAV_METER_X + hx, y, 2, WAV_METER_H);
        /* ticks every 6 dB */
        for (int d = 6; d < WAV_METER_DB; d += 6)
            bmp_fill(COLOR_GRAY(50), WAV_METER_X + WAV_METER_W - d * WAV_METER_W / WAV_METER_DB, y + WAV_METER_H - 2, 1, 2);
    }

    if (!with_text) return;
    char l[8], r[8];
    int a = wav_meter_hold10[0], b = wav_meter_hold10[1];
    if (a <= -999) snprintf(l, sizeof(l), "-inf");
    else           snprintf(l, sizeof(l), "%s%d.%d", a < 0 ? "-" : "", ABS(a) / 10, ABS(a) % 10);
    if (b <= -999) snprintf(r, sizeof(r), "-inf");
    else           snprintf(r, sizeof(r), "%s%d.%d", b < 0 ? "-" : "", ABS(b) / 10, ABS(b) % 10);
    int clip = get_ms_clock() < wav_clip_until;
    /* spaces draw nothing on the RGBA layer, so clear the old text first */
    bmp_fill(COLOR_EMPTY, WAV_TEXT_X, WAV_METER_Y - 3, WAV_TEXT_W, 2 * (WAV_METER_H + 3) + 4);
    bmp_printf(FONT(FONT_SMALL, clip ? COLOR_RED : COLOR_WHITE, COLOR_BLACK),
               WAV_TEXT_X, WAV_METER_Y - 2, "L %s R %s %s%s", l, r,
               wav_mic_now ? "ext" : "int", clip ? " CLIP" : "");
}

static void wav_meter_clear(void)
{
    bmp_fill(COLOR_EMPTY, WAV_METER_X - 2, WAV_METER_Y - 3,
             WAV_TEXT_X + WAV_TEXT_W - (WAV_METER_X - 2), 2 * (WAV_METER_H + 3) + 4);
}

static void wav_meter_task(void * unused)
{
    int drawn = 0, tick = 0;
    int hold_t[2] = { 0, 0 };
    TASK_LOOP
    {
        if (!wav_meter || !wav_is_recording)
        {
            wav_meter_db10[0] = wav_meter_db10[1] = -999;
            wav_meter_hold10[0] = wav_meter_hold10[1] = -999;
            if (drawn) { BMP_LOCK( wav_meter_clear(); ) drawn = 0; }
            msleep(200);
            continue;
        }

        msleep(50);
        uint32_t v = MEM(AUDIO_PEAK_REG);
        int pk[2] = { ABS((int16_t)(v & 0xFFFF)), ABS((int16_t)(v >> 16)) };
        int now = get_ms_clock();
        for (int ch = 0; ch < 2; ch++)
        {
            int db10 = wav_peak_to_db10(pk[ch]);
            /* bar: instant attack, 20 dB/s release */
            wav_meter_db10[ch] = MAX(db10, wav_meter_db10[ch] - 10);
            /* hold the highest peak for 1.5 s */
            if (db10 >= wav_meter_hold10[ch] || now - hold_t[ch] > 1500)
            {
                wav_meter_hold10[ch] = db10;
                hold_t[ch] = now;
            }
            if (pk[ch] >= 32700) wav_clip_until = now + 1500;
        }

        /* draw at 10 Hz, and only over plain LiveView */
        if (++tick & 1) continue;
        int show = lv && liveview_display_idle() && get_global_draw() && !gui_menu_shown();
        if (show)
        {
            BMP_LOCK( wav_meter_draw(!drawn || (tick % 8) == 0); )
            drawn = 1;
        }
        else if (drawn)
        {
            BMP_LOCK( wav_meter_clear(); )
            drawn = 0;
        }
    }
}

TASK_CREATE("wav_meter_task", wav_meter_task, 0, 0x1d, 0x1000);
#endif // AUDIO_PEAK_REG

/* File the take Canon just wrote into <card>/DCIM/AUDIO/.
 *
 * Canon builds the path itself with a format string baked into ROM
 * ("B:/Sample%d.wav", falling back to "A:/Sample%d.wav" - see 0xe064fc6e), and
 * the eventproc takes only a take number, so there is no way to ask for a
 * different name up front. Renaming afterwards is the only lever we have.
 *
 * Named after the clip when there is one (M24-1720.MLV -> DCIM/AUDIO/
 * M24-1720.WAV); otherwise - a manual take, or a raw take that never wrote a
 * file - after the time, W<day>-<hhmm>.WAV.  DCIM/AUDIO is not a DCF folder
 * name (no three-digit prefix), so Canon neither shows nor numbers into it.
 *
 * Returns 1 if the file was moved. Failing is harmless: the audio is still on
 * the card under its original name. */
/* Find the take Canon just closed (B:/ first, then A:/) and wait until
 * Canon has really finished writing it - see below.  1 = ready in src. */
static int wav_last_take_ready(char * src, int len)
{
    uint32_t size = 0;
    snprintf(src, len, "B:/Sample%d.wav", wav_last_number);
    if (FIO_GetFileSize(src, &size) != 0)
    {
        snprintf(src, len, "A:/Sample%d.wav", wav_last_number);
        if (FIO_GetFileSize(src, &size) != 0)
        {
            NotifyBox(2000, "WAV: Sample%d.wav not found", wav_last_number);
            return 0;
        }
    }

    /* Wait for Canon to actually finish with it.
     *
     * FA_AudioRecStop returns before the recorder has flushed and closed the
     * file - on a long take it still has megabytes to write - and renaming a
     * file Canon holds open asserts inside Canon's FileIO (seen on hardware as
     * "ASSERT: 0 at ./FileIO/FileIO.c:119, raw_rec_task1"). A fixed sleep was
     * a guess; watching the size until it holds still is not. Three unchanged
     * readings 100ms apart, up to 10s (4s was not always enough after a
     * 7-minute take). If it never settles, leave the file where it is - a
     * mis-named WAV is recoverable, a crashed camera is not. */
    uint32_t last = 0;
    int stable = 0;
    for (int i = 0; i < 100; i++)
    {
        msleep(100);
        if (FIO_GetFileSize(src, &size) != 0) { stable = 0; continue; }
        if (size == last && size > 44) { if (++stable >= 3) break; }
        else stable = 0;
        last = size;
    }
    if (stable < 3)
    {
        NotifyBox(2000, "WAV still busy, kept as Sample%d.wav", wav_last_number);
        return 0;
    }
    return 1;
}

/* A raw take that stopped before it wrote a clip ("12-bit not live", raw
 * detect error) still started the recorder, and left a fraction of a second of
 * audio from its own setup.  Nothing to keep - remove it rather than file a
 * stub (v73 filed one as W30-1234.WAV). */
int wav_scratch_discard(void)
{
    if (wav_last_number < 0) return 0;
    char src[32];
    int ok = wav_last_take_ready(src, sizeof(src)) && FIO_RemoveFile(src) == 0;
    wav_last_number = -1;
    return ok;
}

int wav_scratch_rename(const char * mlv_path)
{
    if (wav_last_number < 0) return 0;

    char src[32];
    if (!wav_last_take_ready(src, sizeof(src)))
    {
        wav_last_number = -1;
        return 0;
    }

    /* Base name: the clip's, or the time. */
    char base[16];
    const char * name = mlv_path;
    for (const char * c = mlv_path; c && *c; c++)
        if (*c == '/') name = c + 1;
    int n = name ? strlen(name) : 0;
    if (n >= 5 && n - 4 < (int)sizeof(base) && name[n-4] == '.')
    {
        snprintf(base, sizeof(base), "%s", name);
        base[n-4] = 0;
    }
    else
    {
        struct tm now;
        LoadCalendarFromRTC(&now);
        snprintf(base, sizeof(base), "W%02d-%02d%02d", now.tm_mday, now.tm_hour, now.tm_min);
    }

    /* Canon's vsnprintf - which ML's snprintf calls - does NOT implement %c:
     * it pads the buffer with the character instead, which is how v64/v65
     * created a directory named "BBBBBBBB..." in the card root.  Every path
     * below is built without it. */
    char dir[24];
    snprintf(dir, sizeof(dir), "X:/DCIM/AUDIO");
    dir[0] = src[0];
    FIO_CreateDirectory(dir);                   /* no-op if it exists */

    /* Never write onto something that already exists: W24-1720.WAV, then
     * W241720A.WAV .. W241720Z.WAV (8.3 names, so the dash goes). */
    char dest[48];
    uint32_t dsz;
    snprintf(dest, sizeof(dest), "%s/%s.WAV", dir, base);
    for (int k = 0; k < 26 && FIO_GetFileSize(dest, &dsz) == 0; k++)
    {
        char alt[9];
        int j = 0;
        for (const char * c = base; *c && j < 7; c++)
            if (*c != '-') alt[j++] = *c;
        alt[j++] = 'A' + k;
        alt[j] = 0;
        snprintf(dest, sizeof(dest), "%s/%s.WAV", dir, alt);
    }
    if (FIO_GetFileSize(dest, &dsz) == 0)
    {
        NotifyBox(2000, "WAV target exists, kept as Sample%d.wav", wav_last_number);
        wav_last_number = -1;
        return 0;
    }

    int ok = (FIO_RenameFile(src, dest) == 0);
    if (ok)
        NotifyBox(2000, "WAV -> %s", dest + 3);
    else if (get_free_space_32k(get_shooting_card()) < 32)
        /* moving into DCIM/AUDIO needs a directory entry; a full card has no
         * room for one (seen after a take that filled the card) */
        NotifyBox(4000, "Card full: WAV kept as Sample%d.wav", wav_last_number);
    else
        NotifyBox(2000, "WAV kept as Sample%d.wav", wav_last_number);
    /* Deliberately NOT rewinding wav_file_number here. An earlier version
     * reused the number once the file had been renamed away, on the grounds
     * that the name was free again - but that asks Canon's recorder to create
     * the same path on every take, and take numbers are cheap. Always moving
     * forward keeps us off any state Canon may cache per file. */

    wav_last_number = -1;
    return ok;
}

/* Called by mlv_lite when raw recording starts/stops, so one button gets you
 * both. Returns whether the user asked for that (Mode = Auto with RAW). */
int wav_scratch_auto_enabled(void)
{
    return wav_scratch_audio == WAV_SCRATCH_AUTO;
}

/* Manual start/stop gets its own entry, and deliberately so. The menu routes
 * LEFT, RIGHT and SET all through .select, so a .select that swallows them
 * leaves no way to change the value - which is exactly how v13 trapped the user
 * in "Manual" with every button toggling the recorder instead of navigating.
 * The setting below now uses stock navigation and nothing else. */
/* The move waits for Canon to finish writing (up to 4 s), which the menu task
 * must not sit through - so a manual stop hands it to a short-lived task. */
static void wav_file_task(void * unused)
{
    wav_scratch_rename(NULL);
}

static MENU_SELECT_FUNC(wav_scratch_now)
{
    if (wav_is_recording)
    {
        wav_scratch_stop();
        task_create("wav_file_task", 0x1e, 0x1000, wav_file_task, 0);
    }
    else
    {
        wav_scratch_start();
    }
}

static MENU_UPDATE_FUNC(wav_scratch_now_update)
{
    MENU_SET_VALUE(wav_is_recording ? "Recording..." : "Idle");
    if (wav_is_recording)
    {
        /* raw reading of Canon's input level table (FA_AudGetInLevel ->
         * AudioLevel.c); uncalibrated, but it moves with the sound */
        int lvl = call("FA_AudGetInLevel");
        MENU_SET_RINFO("in %d", lvl);
        MENU_SET_WARNING(MENU_WARN_INFO, "Press SET again to stop and close the file.");
    }
}

static MENU_UPDATE_FUNC(wav_input_update)
{
    if (wav_input == 0)
        MENU_SET_RINFO(wav_ext_mic_plugged() ? "ext plugged" : "no ext mic");
    if (wav_input == 2 && !wav_ext_mic_plugged())
        MENU_SET_WARNING(MENU_WARN_ADVICE, "No external mic detected in the jack.");
    if (wav_is_recording)
        MENU_SET_WARNING(MENU_WARN_INFO, "Applies from the next take.");
}

#ifdef SOUNDDEV_SET_VOLUME_IN
static MENU_SELECT_FUNC(wav_gain_select)
{
    menu_numeric_toggle(priv, delta, 0, WAV_GAIN_STOPS - 1);
    if (wav_is_recording) wav_apply_gain();     /* live, so you can ride it */
}

static MENU_SELECT_FUNC(wav_gain_rmode_select)
{
    menu_numeric_toggle(priv, delta, 0, 5);
    if (wav_is_recording) wav_apply_gain();
}

static MENU_SELECT_FUNC(wav_gain_mode_select)
{
    menu_numeric_toggle(priv, delta, 0, 1);
    if (wav_is_recording) wav_apply_gain();
}

static MENU_UPDATE_FUNC(wav_gain_update)
{
  #ifdef AUDIO_GAIN_TABLE_INT
    /* Keep the right-hand text short: a long one runs into the entry name and
     * the two render on top of each other (that is what turned "Data format"
     * + "12-bit" into "bitormat" in mlv_lite). */
    if (wav_gain_mode)
    {
        const int32_t * t = wav_gain_table(wav_mic_to_use());
        if (t)
        {
            char g[16];
            int32_t lreg = wav_level_reg(t, wav_gain_l);
            wav_reg_str(g, sizeof(g), (entry->priv == &wav_gain_l) ? lreg : wav_right_reg(t, lreg));
            MENU_SET_RINFO("%s", g);
        }
        if (entry->priv == &wav_gain_r && wav_gain_rmode != 1)
            MENU_SET_WARNING(MENU_WARN_NOT_WORKING, "Set Right channel to 'Own level' to use this.");
        return;
    }
  #endif
    if (!wav_gain_mode)
        MENU_SET_WARNING(MENU_WARN_NOT_WORKING, "Gain is Factory default - set Gain to Manual.");
}
#endif

static MENU_UPDATE_FUNC(wav_scratch_main_update)
{
    static const char * modes[] = { "OFF", "Auto with RAW", "Manual" };
    if (wav_is_recording)
        MENU_SET_VALUE("REC");
    else
        MENU_SET_VALUE("%s", modes[COERCE(wav_scratch_audio, 0, 2)]);
}

static MENU_UPDATE_FUNC(wav_scratch_update)
{
    if (wav_is_recording)
    {
        MENU_SET_VALUE("REC -> Sample%d.wav", wav_file_number);
        MENU_SET_WARNING(MENU_WARN_INFO, "Recording. Press SET to stop.");
    }
    else if (wav_scratch_audio == WAV_SCRATCH_AUTO)
    {
        MENU_SET_VALUE("Auto (next: Sample%d)", wav_file_number);
        MENU_SET_HELP("Starts and stops with raw recording.");
    }
    else if (wav_scratch_audio == WAV_SCRATCH_MANUAL)
    {
        MENU_SET_VALUE("Manual (next: Sample%d)", wav_file_number);
        MENU_SET_HELP("Press SET to start recording.");
    }
}
#endif /* FEATURE_WAV_AUDIO */

static struct menu_entry mov_menus[] = {
    /* RAW video first: mlv_lite's entry fills this slot when the module loads
     * (the same placeholder mechanism upstream uses for Dual ISO, Silent
     * Picture and Cropmarks).  Hidden if no raw recorder is loaded. */
    MENU_PLACEHOLDER("RAW video"),
    #ifdef FEATURE_MOVIE_RECORDING_50D
    {
        .name       = "Movie Record",
        .priv       = &lv_movie_select,
        .select     = lv_movie_toggle,
        .select_Q   = lv_movie_size_toggle,
        .update     = lv_movie_print,
        .help       = "Enable movie recording on 50D :) ",
        .depends_on = DEP_LIVEVIEW,
    },
    #endif
    #ifdef CONFIG_MOVIE_RECORDING_50D_SHUTTER_HACK
    {
        .name = "Shutter Button",
        .priv = &shutter_btn_rec,
        .max  = 2,
        .choices = CHOICES("Leave unchanged", "Block during REC", "Hold during REC"),
        .help  = "Block shutter button while recording (avoids ERR99)",
        .help2 = "or hold it pressed halfway (enables image stabilization).",
        .depends_on = DEP_MOVIE_MODE,
    },
    #endif

    #ifdef FEATURE_MOVIE_REC_KEY
    {
        .name = "REC key",
        .priv = &movie_rec_key, 
        .max = 1,
        .icon_type = IT_BOOL,
        .choices = CHOICES("OFF", "HalfShutter"),
        .help = "Start recording by pressing shutter halfway. Wired remote.",
        .submenu_width = 700,
        .depends_on = DEP_MOVIE_MODE,
        .children =  (struct menu_entry[]) {
            {
                .name = "Require long press",
                .priv = &movie_rec_key_long,
                .max = 1,
                .icon_type = IT_BOOL,
                .choices = CHOICES("OFF", "ON (1s)"),
                .help = "If ON, you have to hold half-shutter pressed for 1 second.",
            },
            {
                .name = "Allowed actions",
                .priv = &movie_rec_key_action,
                .max = 2,
                .icon_type = IT_DICE,
                .choices = CHOICES("Start/Stop", "Start only", "Stop only"),
                .help = "Select actions for half-shutter.",
            },
            MENU_EOL,
        },
    },
    #endif
    #ifdef FEATURE_OVERRIDE_MOVIE_30_MIN_LIMIT
    {
        .name = "MOV/MP4 time limit",
        .priv = &mov_time_limit,
        .update = print_mov_time_limit,
        .select = change_mov_time_limit,
        .min = 1,
        .max = 180,
        .help = "Change 29:59 movie recording limit",
        .help2 = "Too long recording makes empty files on ExFAT, ok on FAT32",
    },
    #endif
    #ifdef FEATURE_GRADUAL_EXPOSURE
    {
        .name = "Gradual Exposure",
        .priv = &smooth_iso,
        .max = 1,
        .help   = "Use smooth exposure transitions, by compensating with ISO.",
        .help2  = "=> adjust ISO, shutter speed and aperture without large jumps.",
        .submenu_width = 700,
        .depends_on = DEP_MOVIE_MODE | DEP_MANUAL_ISO,
        .children =  (struct menu_entry[]) {
            {
                .name = "Ramping speed",
                .priv       = &smooth_iso_speed,
                .min = 1,
                .max = 7,
                .icon_type = IT_PERCENT,
                .choices = CHOICES("1 EV / 8 frames", "1 EV / 16 frames", "1 EV / 32 frames", "1 EV / 64 frames", "1 EV / 128 frames", "1 EV / 256 frames", "1 EV / 512 frames"),
                .help = "How fast the exposure transitions should be.",
            },
            MENU_EOL
        },
    },
    #endif
    #ifdef FEATURE_WAV_AUDIO
    /* All audio in one submenu, at the end of the Movie tab. */
    {
        .name       = "Scratch audio",
        .select     = menu_open_submenu,
        .update     = wav_scratch_main_update,
        .help       = "Record audio to a WAV via Canon's own recorder. ~0.19 MB/s.",
        .help2      = "Auto follows RAW recording and names the WAV after the clip.",
        .submenu_width = 700,
        .children =  (struct menu_entry[]) {
            {
                .name       = "Mode",
                .priv       = &wav_scratch_audio,
                .max        = 2,
                .choices    = CHOICES("OFF", "Auto with RAW", "Manual"),
                .help       = "Auto: starts and stops with RAW recording, WAV named after the clip.",
                .help2      = "Manual: use Record WAV now. Files go to DCIM/AUDIO.",
            },
            {
                .name       = "Record WAV now",
                .select     = wav_scratch_now,
                .update     = wav_scratch_now_update,
                .icon_type  = IT_ACTION,
                .help       = "Start/stop Canon's WAV recorder by hand, right now.",
                .help2      = "Filed as DCIM/AUDIO/W<day>-<hhmm>.WAV. Independent of RAW.",
            },
            {
                .name       = "Input",
                .priv       = &wav_input,
                .max        = 2,
                .update     = wav_input_update,
                .choices    = CHOICES("Auto (jack)", "Internal mic", "External mic"),
                .help       = "Mic for the WAV. Canon's recorder ignores the jack by itself,",
                .help2      = "so Auto checks it at each start. Both channels share one source.",
            },
            #ifdef AUDIO_PEAK_REG
            {
                .name       = "Level meter",
                .priv       = &wav_meter,
                .max        = 1,
                .choices    = CHOICES("OFF", "While recording"),
                .help       = "L/R peak bars in dBFS while the WAV records, top left of LiveView.",
                .help2      = "1.5 s peak hold, CLIP at full scale. Keep peaks below about -6.",
            },
            #endif
            #ifdef SOUNDDEV_SET_VOLUME_IN
            {
                .name       = "Gain",
                .priv       = &wav_gain_mode,
                .max        = 1,
                .select     = wav_gain_mode_select,
                .choices    = CHOICES("Factory default", "Manual"),
                .help       = "Factory default is quiet (peaks ~ -41 dBFS measured).",
                .help2      = "Manual sets the audio IC level per channel. RAM only.",
            },
            {
                .name       = "Left gain",
                .priv       = &wav_gain_l,
                .max        = WAV_GAIN_STOPS - 1,
                .choices    = WAV_GAIN_CHOICES,
                .select     = wav_gain_select,
                .update     = wav_gain_update,
                .help       = "Left input gain, in 3 dB stops. Changes apply live.",
                .help2      = "Mute silences the take. Keep peaks below about -6 dBFS.",
            },
            {
                .name       = "Right channel",
                .priv       = &wav_gain_rmode,
                .max        = 5,
                .select     = wav_gain_rmode_select,
                .update     = wav_gain_update,
                .choices    = CHOICES("Same as left", "Own level", "Safety -6 dB", "Safety -12 dB", "Safety -18 dB", "Safety -24 dB"),
                .help       = "Safety: R records the same source N dB lower - a backup for",
                .help2      = "clipped peaks (merge in post: 6D2/tools/wav_dualgain.py).",
            },
            {
                .name       = "Right gain",
                .priv       = &wav_gain_r,
                .max        = WAV_GAIN_STOPS - 1,
                .choices    = WAV_GAIN_CHOICES,
                .select     = wav_gain_select,
                .update     = wav_gain_update,
                .help       = "Right input gain, used when Right channel = Own level.",
            },
            #endif
            MENU_EOL,
        },
    },
    #endif
};

// Only create this menu if the cam has any feature that needs it
#if defined(FEATURE_NITRATE) || defined(FEATURE_MOVIE_RESTART) \
    || defined(FEATURE_REC_NOTIFY) || defined(FEATURE_FORCE_LIVEVIEW) \
    || defined(FEATURE_SHUTTER_LOCK) || defined(FEATURE_MOVIE_LOGGING)
static struct menu_entry movie_tweaks_menus[] = {
    {
        .name = "Movie Tweaks",
        .select = menu_open_submenu,
        .help = "Movie Restart, Movie Logging, REC/Standby Notify...",
        .depends_on = DEP_MOVIE_MODE,
        .submenu_width = 710,
        .children =  (struct menu_entry[]) {
                #ifdef FEATURE_MOVIE_RESTART
                {
                    .name = "Movie Restart",
                    .priv = &movie_restart,
                    .max        = 1,
                    .help = "Auto-restart movie recording, if it happens to stop.",
                    .depends_on = DEP_MOVIE_MODE,
                },
                #endif
                #ifdef FEATURE_REC_NOTIFY
                {
                    .name = "REC/STBY notify", 
                    .priv = &rec_notify, 
                    #if defined(CONFIG_BLUE_LED) && defined(FEATURE_REC_NOTIFY_BEEP)
                    .max = 4,
                    #elif defined(CONFIG_BLUE_LED) && !defined(FEATURE_REC_NOTIFY_BEEP)
                    .max = 3,
                    #elif !defined(CONFIG_BLUE_LED) && defined(FEATURE_REC_NOTIFY_BEEP)
                    .max = 3,
                    #else
                    .max = 2,
                    #endif
                    .choices = (const char *[]) {"OFF", "Red Crossout", "REC/STBY",
                            #ifdef CONFIG_BLUE_LED
                            "Blue LED",
                            #endif
                            "Beep, start/stop"
                        },
                    .icon_type = IT_DICE_OFF,
                    .help = "Custom REC/STANDBY notifications, visual or audible",
                    .depends_on = DEP_MOVIE_MODE,
                },
                #endif
                #ifdef FEATURE_FORCE_LIVEVIEW
                {
                    .name = "Force LiveView",
                    .priv = &enable_liveview,
                    .max = 2,
                    .choices = CHOICES("OFF", "Start & CPUlens", "Always"),
                    .icon_type = IT_DICE_OFF,
                    .help = "Always use LiveView (with manual lens or after lens swap).",
                    .depends_on = DEP_MOVIE_MODE,
                },
                #endif
                #ifdef FEATURE_SHUTTER_LOCK
                {
                    .name = "Shutter Lock",
                    .priv = &shutter_lock,
                    .max = 1,
                    .help   = "Lock shutter value in movie mode (change from Expo only).",
                    .help2  = "Tip: it prevents you from changing it by mistake.",
                    .depends_on = DEP_MOVIE_MODE,
                },
                #endif

                MENU_EOL
        }
    },
};

void movie_tweak_menu_init()
{
    menu_add( "Movie", movie_tweaks_menus, COUNT(movie_tweaks_menus) );
}
#else // end of any defines that want Movie Tweaks menu
// Here we don't want Movie Tweak menu to display,
// but we must do dummy init since this is used extern.
void movie_tweak_menu_init()
{
    ;
}
#endif

#ifdef FEATURE_EXPO_OVERRIDE
struct menu_entry expo_override_menus[] = {
    {
        .name = "Expo. Override",
        .priv = &bv_auto,
        .select     = bv_toggle,
        .update     = bv_display,
        .max = 1,
        .help       = "Low-level manual exposure controls (bypasses Canon limits).",
        .help2      = "Useful for long exposures, manual lenses, manual video ctl.",
        .depends_on = DEP_LIVEVIEW,
    },
};
#endif

static void movtweak_init()
{
    menu_add("Movie", mov_menus, COUNT(mov_menus));
    #ifdef FEATURE_EXPO_OVERRIDE
    bv_sem = create_named_semaphore("bv", SEM_CREATE_UNLOCKED);
    #endif
}

INIT_FUNC(__FILE__, movtweak_init);
