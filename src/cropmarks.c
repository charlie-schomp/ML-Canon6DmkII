// included in zebra.c
// always use cropmarkS in plural

static void bvram_mirror_clear();
static void clrscr_mirror();

static void cropmark_cache_update_signature();
static int cropmark_cache_is_valid();
static void default_movie_cropmarks();
static void black_bars();

void cropmark_clear_cache();

static struct bmp_file_t * cropmarks = 0;

/* 1 = recorded area (mlv_lite's own rectangle, shaded outside), 2 = bitmap
 * file. Deliberately NOT 0/1: ML treats a 0..1 entry as a boolean and greys
 * every child of the submenu while it reads 0 - so with "Recorded area"
 * selected the framing guide, PLAY-mode and photo-mode options all looked
 * disabled. min=1 keeps it a plain choice. crop_enabled is what the rest of
 * this file has always tested; it now derives from the mode. */
static CONFIG_INT( "crop.mode", crop_mode, 1);      /* 0 off, 1 recorded area, 2 bitmap */
/* Bitmap cropmarks are compiled out on this body for now: the submenu that
 * hosts the picker was unstable and nothing in the workflow needs a static
 * file. crop_mode is 0 = off, 1 = recorded area. Mode 2 is never reachable. */
#define crop_enabled (0)
static CONFIG_INT( "crop.index", crop_index, 0); // index of crop file
static CONFIG_INT( "crop.movieonly", cropmark_movieonly, 0);
static CONFIG_INT("crop.playback", cropmarks_play, 0);

/* Framing guide drawn INSIDE the recorded area: record full-width 16:9, frame
 * for 4:3 or 2.39, keep the rest as reframing room in post. Ratios are laid
 * out in screen space, which on this body is visually correct for the LV. */
static CONFIG_INT("crop.guide", crop_guide, 0);
static const int crop_guide_num[] = { 0, 4, 1, 185, 2, 239, 9, 4 };
static const int crop_guide_den[] = { 1, 3, 1, 100, 1, 100, 16, 5 };
#ifdef FEATURE_CROPMARKS     /* menu only; unused (and -Werror) without cropmarks */
static const char * crop_guide_names[] =
    { "OFF", "4:3", "1:1", "1.85:1", "2:1", "2.39:1", "9:16", "4:5" };
#endif

/* Fit the guide inside (x1,y1)-(x2,y2). Returns 0 if no guide. */
static int crop_guide_rect(int x1, int y1, int x2, int y2,
                           int * gx1, int * gy1, int * gx2, int * gy2)
{
    if (!crop_guide || crop_guide >= (int)COUNT(crop_guide_num)) return 0;
    int num = crop_guide_num[crop_guide], den = crop_guide_den[crop_guide];
    int rw = x2 - x1, rh = y2 - y1;
    if (rw <= 0 || rh <= 0) return 0;
    int gw, gh;
    if (rw * den > rh * num) { gh = rh; gw = rh * num / den; }   /* recorded is wider */
    else                     { gw = rw; gh = rw * den / num; }   /* recorded is taller */
    *gx1 = x1 + (rw - gw) / 2;  *gx2 = *gx1 + gw;
    *gy1 = y1 + (rh - gh) / 2;  *gy2 = *gy1 + gh;
    return 1;
}

static int cropmark_cache_dirty = 1;
static int crop_dirty = 0; // redraw cropmarks after some time (unit: 0.1s)

static int cropmarks_x = -1;
static int cropmarks_y = -1;

void crop_set_dirty(int value)
{
    crop_dirty = MAX(crop_dirty, value);
}

static int is_valid_cropmark_filename(char* filename)
{
    int n = strlen(filename);
    if ((n > 4) && (streq(filename + n - 4, ".BMP") || streq(filename + n - 4, ".bmp")) && (filename[0] != '.') && (filename[0] != '_'))
        return 1;
    return 0;
}

#define MAX_CROP_NAME_LEN 15
#define MAX_CROPMARKS 9
static int num_cropmarks = 0;
static int cropmarks_initialized = 0;
static char cropmark_names[MAX_CROPMARKS][MAX_CROP_NAME_LEN];

// Cropmark sorting code contributed by Nathan Rosenquist
static void sort_cropmarks()
{
    int i = 0;
    int j = 0;
    
    char aux[MAX_CROP_NAME_LEN];
    
    for (i=0; i<num_cropmarks; i++)
    {
        for (j=i+1; j<num_cropmarks; j++)
        {
            if (strcmp(cropmark_names[i], cropmark_names[j]) > 0)
            {
                strcpy(aux, cropmark_names[i]);
                strcpy(cropmark_names[i], cropmark_names[j]);
                strcpy(cropmark_names[j], aux);
            }
        }
    }
}

static void find_cropmarks()
{
    struct fio_file file;
    struct fio_dirent * dirent = FIO_FindFirstEx( "ML/CROPMKS/", &file );
    if( IS_ERROR(dirent) )
    {
        NotifyBox(2000, "ML/CROPMKS dir missing\n"
                        "Please copy all ML files!" );
        return;
    }
    int k = 0;
    do {
        if (file.mode & ATTR_DIRECTORY) continue; // is a directory
        if (is_valid_cropmark_filename(file.name))
        {
            if (k >= MAX_CROPMARKS)
            {
                NotifyBox(2000, "TOO MANY CROPMARKS (max=%d)", MAX_CROPMARKS);
                break;
            }
            snprintf(cropmark_names[k], MAX_CROP_NAME_LEN, "%s", file.name);
            k++;
        }
    } while( FIO_FindNextEx( dirent, &file ) == 0);
    FIO_FindClose(dirent);
    num_cropmarks = k;
    sort_cropmarks();
    cropmarks_initialized = 1;
}

static __attribute__((unused)) void reload_cropmark()
{
    int i = crop_index;
    static int old_i = -1;
    if (i == old_i) return; 
    old_i = i;
    //~ bmp_printf(FONT_LARGE, 0, 100, "reload crop: %d", i);

    if (cropmarks)
    {
        void* old_crop = cropmarks;
        cropmarks = 0;
        free(old_crop);
    }

    cropmark_clear_cache();
    
    if (!num_cropmarks) return;
    i = COERCE(i, 0, num_cropmarks-1);
    char bmpname[100];
    snprintf(bmpname, sizeof(bmpname), "ML/CROPMKS/%s", cropmark_names[i]);
    cropmarks = bmp_load(bmpname,1);
    if (!cropmarks) bmp_printf(FONT_LARGE, 0, 50, "LOAD ERROR %d:%s   ", i, bmpname);
}

static void
__attribute__((unused)) crop_toggle( void* priv, int sign )
{
    if (num_cropmarks <= 0) { crop_index = 0; return; }
    crop_index = MOD(crop_index + sign, num_cropmarks);
    //~ reload_cropmark(crop_index);
    crop_set_dirty(10);
}

static MENU_UPDATE_FUNC(crop_display)
{
    if (crop_enabled)
    {
        if (num_cropmarks <= 0)
        {
            MENU_SET_VALUE("Bitmap: none on card");
            MENU_SET_WARNING(MENU_WARN_NOT_WORKING, "No .BMP files in ML/cropmks - nothing will be drawn.");
        }
        else
        {
            int index = COERCE(crop_index, 0, num_cropmarks-1);
            MENU_SET_VALUE(cropmark_names[index]);
        }
    }
    else if (crop_mode == 0)
    {
        MENU_SET_VALUE("OFF");
    }
    else
    {
        /* OFF is not nothing. With no bitmap selected, ML draws the "default"
         * cropmarks: the rectangle mlv_lite reports as its actual recorded
         * area, via set_movie_cropmarks(), shaded outside. Selecting a bitmap
         * REPLACES that with a static file that knows nothing about the
         * resolution setting. Upstream's label of OFF for the honest one has
         * misled people; say what it is. */
        MENU_SET_VALUE(lv && is_movie_mode() ? "Recorded area" : "Recorded area (movie)");
    }
    if (cropmark_movieonly && !is_movie_mode())
        MENU_SET_WARNING(MENU_WARN_NOT_WORKING, "Cropmarks are configured only for movie mode");
}

/* The guide lives in the cropmark cache; notice a change from the draw task
 * itself (called at the top of cropmark_draw), never from a menu callback. */
static void crop_guide_check_changed()
{
    static int last = -1;
    if (last != crop_guide) { last = crop_guide; cropmark_cache_dirty = 1; }
}

static __attribute__((unused)) MENU_UPDATE_FUNC(crop_display_submenu)
{
    /* With no bitmaps on the card, COERCE(index, 0, -1) yields -1 and this
     * read cropmark_names[-1]: the [FF,FF,FF,FF] entry and a crash on opening
     * the submenu. This build ships no bitmaps, so that is the normal case. */
    if (num_cropmarks <= 0)
    {
        crop_index = 0;
        MENU_SET_NAME("Bitmap");
        MENU_SET_VALUE("none on card");
        MENU_SET_WARNING(MENU_WARN_NOT_WORKING, "Put .BMP files in ML/cropmks to use this.");
        return;
    }
    int index = crop_index;
    index = COERCE(index, 0, num_cropmarks-1);
    crop_index = index;
    MENU_SET_NAME(
        "Bitmap (%d/%d)",
         index+1, num_cropmarks
    );
    MENU_SET_VALUE(
        "%s",
         cropmark_names[index]
    );

    if (info->can_custom_draw)
    {
        int h = 170;
        int w = h * 720 / 480;
        //~ int xc = 360 - w/2;
        int xc = 400;
        int yc = info->y + font_large.height * 3 + 10;
        BMP_LOCK( reload_cropmark(); )
        bmp_fill(0, xc, yc, w, h);
        BMP_LOCK( bmp_draw_scaled_ex(cropmarks, xc, yc, w, h, 0); )
        bmp_draw_rect(COLOR_WHITE, xc, yc, w, h);
    }
    
    MENU_SET_ICON(MNI_DICE, (num_cropmarks<<16) + index);
}

#ifdef FEATURE_CROPMARKS
static struct menu_entry cropmarks_menu[] = {
    {
        .name = "Cropmarks",
        .priv = &crop_mode,
        .update    = crop_display,
        .max = 1,
        .choices = CHOICES("OFF", "Recorded area"),
        .help  = "Recorded area: shades everything outside what mlv_lite records.",
        .depends_on = DEP_GLOBAL_DRAW,
        .submenu_width = 710,
        .submenu_height = 250,
        .children =  (struct menu_entry[]) {
            {
                .name = "Framing guide",
                .priv = &crop_guide,
                .max = 7,
                .choices = crop_guide_names,
                .help  = "Aspect frame inside the recorded area, for reframing in post.",
                .help2 = "Record 16:9, frame for 4:3 or 2.39 - the rest stays as margin.",
            },
            {
                .name = "Show in photo mode",
                .priv = &cropmark_movieonly, 
                .max = 1,
                .choices = (const char *[]) {"ON", "OFF"},
                .help = "Cropmarks are mostly used in movie mode.",
            },
            {
                .name = "Show in PLAY mode",
                .priv = &cropmarks_play, 
                .max = 1,
                .help = "You may also have cropmarks in Playback mode.",
            },
            MENU_EOL
        },
    },
};
#endif

static void cropmark_draw_from_cache()
{
    uint8_t* B = bmp_vram();
    uint8_t* M = get_bvram_mirror();
    get_yuv422_vram();
    ASSERT(B);
    ASSERT(M);
    
    for (int i = os.y0; i < os.y_max; i++)
    {
        for (int j = os.x0; j < os.x_max; j++)
        {
            uint8_t p = B[BM(j,i)];
            uint8_t m = M[BM(j,i)];
            if (!(m & 0x80)) continue;
            if (p != 0 && p != 0x14 && p != 0x3 && p != m) continue;
            B[BM(j,i)] = m & ~0x80;
        }
    }
}

void cropmark_clear_cache()
{
#ifdef FEATURE_CROPMARKS
    BMP_LOCK(
        /* clear focus peaking, zebras, but leave the old cropmark */
        clrscr_mirror();
        
        /* default_movie_cropmarks needs the old cropmark info, for incremental redrawing */
        /* e.g. if the cropmark shifts a little, it won't redraw the entire thing */
        /* so don't delete it */
        //~ bvram_mirror_clear();
        
        /* fill bvram_mirror with new cropmarks */
        /* note: cropmark pixels are identified in mirror by 0x80 */
        /* this flag does not get transferred to real screen; it's only for mirror */
        default_movie_cropmarks();
    )
#endif
}

static int cropmarks_last_drawn_x = -1;
static int cropmarks_last_drawn_y = -1;

static void cropmarks_clear_last_frame()
{
    if (cropmarks_last_drawn_x != -1 && cropmarks_last_drawn_y != -1)
    {
        int x1 = (cropmarks_last_drawn_x >> 16);
        int x2 = (cropmarks_last_drawn_x & 0xFFFF);
        int y1 = (cropmarks_last_drawn_y >> 16);
        int y2 = (cropmarks_last_drawn_y & 0xFFFF);
        bmp_draw_rect(0, x1, y1, x2-x1, y2-y1);
        bmp_draw_rect(0, x1-1, y1-1, x2-x1+2, y2-y1+2);
        //~ draw_line(x1,y1,x2,y2,0);
        //~ draw_line(x1,y2,x2,y1,0);
        cropmarks_last_drawn_x = -1;
        cropmarks_last_drawn_y = -1;
    }
}

/* only for bitmap cropmarks: if there are any overriden cropmarks, show where they are */
static void cropmarks_draw_frame()
{
    if (cropmarks_x != -1 && cropmarks_y != -1)
    {
        int x1 = (cropmarks_x >> 16);
        int x2 = (cropmarks_x & 0xFFFF);
        int y1 = (cropmarks_y >> 16);
        int y2 = (cropmarks_y & 0xFFFF);
        bmp_draw_rect(COLOR_GRAY(70), x1, y1, x2-x1, y2-y1);
        bmp_draw_rect(COLOR_BLACK, x1-1, y1-1, x2-x1+2, y2-y1+2);
        //~ draw_line(x1,y1,x2,y2,COLOR_RED);
        //~ draw_line(x1,y2,x2,y1,COLOR_RED);
        cropmarks_last_drawn_x = cropmarks_x;
        cropmarks_last_drawn_y = cropmarks_y;
    }
}

static int cropmarks_last_x = -1;
static int cropmarks_last_y = -1;

static int cropmarks_frame_changed()    /* changed since last call to cropmarks_frame_save */
{
    return (cropmarks_last_x != cropmarks_x) || (cropmarks_last_y != cropmarks_y);
}

static void cropmarks_frame_save()
{
    cropmarks_last_x = cropmarks_x;
    cropmarks_last_y = cropmarks_y;
}

static int should_use_default_cropmarks()
{
    return
        (crop_mode == 1) ||
        (crop_enabled && cropmark_movieonly && !is_movie_mode() && lv);
}


static void 
cropmark_draw()
{
    if (!get_global_draw()) return;

    if (crop_mode == 0)
    {
        /* Genuinely off. Upstream had no such state - its OFF still drew the
         * default movie bars. Drop any cached cropmark once so the next
         * redraw wipes it, then draw nothing. */
        if (cropmark_cache_dirty || crop_dirty)
        {
            bvram_mirror_clear();
            cropmark_cache_dirty = 0;
            crop_dirty = 0;
        }
        return;
    }

    crop_guide_check_changed();
    get_yuv422_vram(); // just to refresh VRAM params
    clear_lv_afframe_if_dirty();

    #ifdef FEATURE_GHOST_IMAGE
    if (transparent_overlay && !transparent_overlay_hidden && !PLAY_MODE)
    {
        show_overlay();
        zoom_overlay_dirty = 1;
        cropmark_cache_dirty = 1;
    }
    #endif
    crop_dirty = 0;

    if (cropmarks_frame_changed())
    {
        cropmarks_clear_last_frame();
    }

    // this is very fast
    if (cropmark_cache_is_valid())
    {
        clrscr_mirror();
        cropmark_draw_from_cache();
        //~ bmp_printf(FONT_MED, 50, 50, "crop cached");
        //~ info_led_blink(5, 10, 10);
        goto end;
    }

    if (should_use_default_cropmarks())
    {
        // Cropmarks disabled (or not shown in this mode)
        // Generate and draw default cropmarks
        cropmark_cache_update_signature();
        cropmark_clear_cache();
        cropmark_draw_from_cache();
        //~ info_led_blink(5,50,50);
        goto end;
    }
    
    if (cropmarks) 
    {
        // Cropmarks enabled, but cache is not valid
        if (!lv) msleep(500); // let the bitmap buffer settle, otherwise ML may see black image and not draw anything (or draw half of cropmark)
        clrscr_mirror(); // clean any remaining zebras / peaking
        cropmark_cache_update_signature();
        bvram_mirror_clear();

        if (hdmi_code >= 5 && is_pure_play_movie_mode())
        {   // exception: cropmarks will have some parts of them outside the screen
            bmp_draw_scaled_ex(cropmarks, BMP_W_MINUS+1, BMP_H_MINUS - 50, 960, 640, bvram_mirror);
        }
        else
            bmp_draw_scaled_ex(cropmarks, os.x0, os.y0, os.x_ex, os.y_ex, bvram_mirror);
        //~ info_led_blink(5,50,50);
        //~ bmp_printf(FONT_MED, 50, 50, "crop regen");
        goto end;
    }

end:

    if (!should_use_default_cropmarks())
    {
        /* bitmap cropmarks and overriden default cropmarks? show a simple rectangle */
        cropmarks_draw_frame();
    }
    cropmarks_frame_save();
    cropmark_cache_dirty = 0;
    zoom_overlay_dirty = 1;
    crop_dirty = 0;
}

static int cropmark_cache_sig = 0;

static int cropmark_cache_get_signature()
{
    get_yuv422_vram(); // update VRAM params if needed
    int sig = 
        (
            should_use_default_cropmarks() 
                ? (cropmarks_x * 1601 + cropmarks_y * 481)          /* default cropmarks: they get burned in the bvram mirror */
                : (crop_index * 13579 + crop_enabled * 14567)       /* bitmap cropmarks: only the bitmap gets burned in the bvram mirror */
        )
        + (hdmi_code + EXT_MONITOR_RCA) * 315 +                     /* force redraw when changing display type (LCD, HDMI, SD) */
        os.x0*811 + os.y0*467 + os.x_ex*571 + os.y_ex*487 +         /* force redraw when bitmap parameters changed */
        (is_movie_mode() ? 113 : 0) + video_mode_resolution * 8765; /* force redraw when video resolution changed, or when switching between video and photo mode */
    return sig;
}

static void cropmark_cache_update_signature()
{
    cropmark_cache_sig = cropmark_cache_get_signature();
}

static int cropmark_cache_is_valid()
{
    if (cropmark_cache_dirty) return 0; // some other ML task asked for redraw
    if (hdmi_code >= 5 && PLAY_MODE) return 0; // unusual geometry - better force full redraw every time
    
    int sig = cropmark_cache_get_signature(); // video mode changed => needs redraw
    if (cropmark_cache_sig != sig) return 0;
    
    return 1; // everything looks alright
}

static void
cropmark_redraw()
{
    if (!cropmarks_initialized) return;
    if (gui_menu_shown()) return; 
    if (!zebra_should_run() && !PLAY_OR_QR_MODE) return;
    if (digic_zoom_overlay_enabled()) return;
    BMP_LOCK(
        cropmark_draw(); 
    )
}

#if 0
void draw_cropmark_area()
{
    get_yuv422_vram();
    bmp_draw_rect(COLOR_BLUE, os.x0, os.y0, os.x_ex, os.y_ex);
    draw_line(os.x0, os.y0, os.x_max, os.y_max, COLOR_BLUE);
    draw_line(os.x0, os.y_max, os.x_max, os.y0, COLOR_BLUE);
    
    bmp_draw_rect(COLOR_RED, HD2BM_X(0), HD2BM_Y(0), HD2BM_DX(vram_hd.width), HD2BM_DY(vram_hd.height));
    draw_line(HD2BM_X(0), HD2BM_Y(0), HD2BM_X(vram_hd.width), HD2BM_Y(vram_hd.height), COLOR_RED);
    draw_line(HD2BM_X(0), HD2BM_Y(vram_hd.height), HD2BM_X(vram_hd.width), HD2BM_Y(0), COLOR_RED);
}

void show_apsc_crop_factor()
{
    int x_ex_crop = os.x_ex * 10/16;
    int y_ex_crop = os.y_ex * 10/16;
    int x_off = (os.x_ex - x_ex_crop)/2;
    int y_off = (os.y_ex - y_ex_crop)/2;
    bmp_draw_rect(COLOR_WHITE, os.x0 + x_off, os.y0 + y_off, x_ex_crop, y_ex_crop);
    bmp_draw_rect(COLOR_BLACK, os.x0 + x_off + 1, os.y0 + y_off + 1, x_ex_crop - 2, y_ex_crop - 2);
}
#endif

static void black_bars()
{
    if (!get_global_draw()) return;
    if (!is_movie_mode()) return;
    if (video_mode_resolution > 1) return; // these are only for 16:9
    int i,j;
    uint8_t * const bvram = bmp_vram();
    get_yuv422_vram();
    ASSERT(bvram);
    for (i = os.y0; i < MIN(os.y_max+1, BMP_H_PLUS); i++)
    {
        if (i < os.y0 + os.off_169 || i > os.y_max - os.off_169)
        {
            int newcolor = (i < os.y0 + os.off_169 - 2 || i > os.y_max - os.off_169 + 2) ? COLOR_BLACK : COLOR_BG;
            for (j = os.x0; j < os.x_max; j++)
            {
                if (bvram[BM(j,i)] == COLOR_BG)
                    bvram[BM(j,i)] = newcolor;
            }
        }
    }
}

static void FAST default_movie_cropmarks()
{
    if (!get_global_draw()) return;
    if (!lv) return;
    if (!is_movie_mode())
    {
        /* no default cropmarks in photo mode */
        bvram_mirror_clear();
        return;
    }
    
    int x,y;
    uint8_t * const bvram = bmp_vram();
    if (!bvram) return;
    uint8_t * const bvram_mirror = get_bvram_mirror();
    get_yuv422_vram();
    if (!bvram_mirror) return;

    int crop_x = cropmarks_x;
    int crop_y = cropmarks_y;
    
    if(crop_x == -1 || crop_y == -1)
    {
        if(video_mode_resolution > 1) // 4:3
        {
            crop_x = ((os.x0 + os.off_43) << 16) | (os.x_max - os.off_43);
            crop_y = (os.y0 << 16) | os.y_max;
        }
        else
        {
            crop_x = (os.x0 << 16) | os.x_max;
            crop_y = ((os.y0 + os.off_169) << 16) | (os.y_max - os.off_169);
        }
    }
    
    /*
        doesn't work with HDMI, please fix that, I don't have it
    */
    int gx1 = 0, gy1 = 0, gx2 = 0, gy2 = 0;
    int have_guide = crop_guide_rect(crop_x >> 16, crop_y >> 16,
                                     crop_x & 0xFFFF, crop_y & 0xFFFF,
                                     &gx1, &gy1, &gx2, &gy2);

    for (y = os.y0; y < MIN(os.y_max+1, BMP_H_PLUS); y++)
    {
        int x1 = (crop_x >> 16);
        int x2 = (crop_x & 0xFFFF);
        int y1 = (crop_y >> 16);
        int y2 = (crop_y & 0xFFFF);

        bool draw = (y < y1 || y > y2);
        
        for (x = os.x0; x < os.x_max; x++)
        {
            int newcolor = 0;
            int newcolor_with_flag = 0;
            if(draw || (x < x1 || x > x2))
            {
                /* default cropmarks are black */
                /* border pixels should be brighter, to be visible in dark */
                int border = (x > x1-2 && x < x2+2 && y > y1-2 && y < y2+2);
                newcolor = border ? COLOR_GRAY(70) : COLOR_BLACK;
            }
            else if (have_guide &&
                     x >= gx1 && x <= gx2 && y >= gy1 && y <= gy2 &&
                     (x <= gx1 + 1 || x >= gx2 - 1 || y <= gy1 + 1 || y >= gy2 - 1))
            {
                /* framing guide: a 2px frame inside the recorded area, drawn
                 * into the same cache as the cropmark so it survives the
                 * zebra/peaking passes and costs nothing per frame */
                newcolor = COLOR_WHITE;
            }
            
            if (newcolor)
            {
                /* prevent zebras from drawing here */
                newcolor_with_flag = newcolor | 0x80;
            }

            int idx = BM(x,y);
            if (newcolor_with_flag != bvram_mirror[idx])
            {
                /* anything changed? */
                
                if (newcolor != bvram[idx] && (bvram_mirror[idx] & 0x80) && ((bvram_mirror[idx] & ~0x80) == bvram[idx]))
                {
                    /* pixel from the old cropmark? clear from the main screen too */
                    /* otherwise, cropmark_draw_from_cache will ignore those */
                    /* note: we could clear the entire screen, but redrawing everything looks ugly; incremental redraws look better */
                    bvram[idx] = 0;
                }
                
                bvram_mirror[idx] = newcolor_with_flag;
            }
        }
    }
}

void set_movie_cropmarks(int x, int y, int w, int h)
{
    x = COERCE(x, os.x0+1, os.x_max-1);
    y = COERCE(y, os.y0+2, os.y_max-2);
    w = COERCE(w, 0, os.x_max-2 - x);
    h = COERCE(h, 0, os.y_max-4 - y);
    cropmarks_x = (x << 16) | (x + w);
    cropmarks_y = (y << 16) | (y + h);
}

void reset_movie_cropmarks()
{
    cropmarks_x = cropmarks_y = -1;
}

static void cropmark_step()
{
    /* cropmark frame changed and nobody asked for update? force an update now (once per second) */
    /* food for thought: could this replace the entire crop_set_dirty thingie? */
    static int aux = 0;
    if (!crop_dirty && should_run_polling_action(RECORDING ? 500 : 100, &aux) && cropmarks_frame_changed())
    {
        crop_dirty = 1;
    }

    if (crop_dirty && lv && zebra_should_run())
    {
        crop_dirty--;
        
        //~ bmp_printf(FONT_MED, 50, 100, "crop: cache=%d dirty=%d ", cropmark_cache_is_valid(), crop_dirty);

        // if cropmarks are disabled, we will still draw default cropmarks (fast)
        if (should_use_default_cropmarks()) 
            crop_dirty = MIN(crop_dirty, 2);
        
        // if cropmarks are cached, we can redraw them fast
        if (cropmark_cache_is_valid() && !should_draw_zoom_overlay() && !get_halfshutter_pressed())
            crop_dirty = MIN(crop_dirty, 2);
            
        if (crop_dirty == 0)
            cropmark_redraw();
    }
}
