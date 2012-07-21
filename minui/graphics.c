/*
 * Copyright (C) 2007-2011 The Android Open Source Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <stdio.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <sys/types.h>
#include <linux/fb.h>
#include <linux/kd.h>
#include <pixelflinger/pixelflinger.h>
#include <math.h>

#if defined(PIXELS_BGRA)
# define PIXEL_FORMAT GGL_PIXEL_FORMAT_BGRA_8888
# define PIXEL_SIZE   4
#elif defined(PIXELS_RGBA)
# define PIXEL_FORMAT GGL_PIXEL_FORMAT_RGBA_8888
# define PIXEL_SIZE   4
#elif defined(PIXELS_RGBX)
# define PIXEL_FORMAT GGL_PIXEL_FORMAT_RGBX_8888
# define PIXEL_SIZE   4
#elif defined(PIXELS_BGR_16BPP)
# define PIXEL_FORMAT GGL_PIXEL_FORMAT_RGB_565
# define PIXEL_SIZE   2
# define COLORS_REVERSED
#else
# define PIXEL_FORMAT GGL_PIXEL_FORMAT_RGB_565
# define PIXEL_SIZE   2
#endif

#define NUM_BUFFERS 2

#include "minui.h"
#include "font_10x18.h"
#include "roboto_15x24.h"

#include "../common.h"


static struct UiFont FONTS[3];
static int selectedFont = FONT_HEAD;
static GGLContext *gr_context = 0;
static GGLSurface gr_framebuffer[NUM_BUFFERS];
static GGLSurface gr_mem_surface;
static unsigned gr_active_fb = 0;
static unsigned double_buffering = 0;

static void * gr_fontmem = NULL;
static void * gr_bigfontmem = NULL;
static int gr_fb_fd = -1;
static int gr_vt_fd = -1;

static int gr_vt_mode = -1;

static struct fb_var_screeninfo vi;
static struct fb_fix_screeninfo fi;

static unsigned mmap_len = 0;

static void gr_fb_clear(GGLSurface *fb) {
    if (fb && fb->data) {
        memset(fb->data, 0, vi.yres * vi.xres * PIXEL_SIZE);
    }
}

#ifndef DEFAULT_PAGE_SIZE
#  define DEFAULT_PAGE_SIZE 4096
#endif

static int get_framebuffer(GGLSurface *fb)
{
    int fd;
    void *bits;

    memset(&vi, 0, sizeof(vi));
    memset(&fi, 0, sizeof(fi));

    // init to prevent free of random address
    fb->data = NULL;

    fd = open("/dev/graphics/fb0", O_RDWR);
    if (fd < 0) {
        perror("cannot open fb0");
        return -1;
    }

    if (ioctl(fd, FBIOGET_VSCREENINFO, &vi) < 0) {
        perror("failed to get fb0 info");
        close(fd);
        return -1;
    }

    vi.bits_per_pixel = PIXEL_SIZE * 8;
    if (PIXEL_FORMAT == GGL_PIXEL_FORMAT_RGBA_8888
     || PIXEL_FORMAT == GGL_PIXEL_FORMAT_RGBX_8888) {
      vi.red.offset     = 0;
      vi.red.length     = 8;
      vi.green.offset   = 8;
      vi.green.length   = 8;
      vi.blue.offset    = 16;
      vi.blue.length    = 8;
      vi.transp.offset  = 24;
      vi.transp.length  = 8; //RGBX use 0xFF on Alpha
    } else if (PIXEL_FORMAT == GGL_PIXEL_FORMAT_BGRA_8888) {
      // defy cm7 config
      vi.blue.offset    = 0;
      vi.blue.length    = 8;
      vi.green.offset   = 8;
      vi.green.length   = 8;
      vi.red.offset     = 16;
      vi.red.length     = 8;
      vi.transp.offset  = 24;
      vi.transp.length  = 8;
    } else {
#ifdef COLORS_REVERSED
      // BGR565 16-bits
      vi.blue.offset    = 0;
      vi.blue.length    = 5;
      vi.green.offset   = 5;
      vi.green.length   = 6;
      vi.red.offset     = 11;
      vi.red.length     = 5;
#else
      // RGB565 16-bits
      vi.red.offset     = 0;
      vi.red.length     = 5;
      vi.green.offset   = 5;
      vi.green.length   = 6;
      vi.blue.offset    = 11;
      vi.blue.length    = 5;
#endif
      vi.transp.offset  = 0;
      vi.transp.length  = 0;
    }
    if (ioctl(fd, FBIOPUT_VSCREENINFO, &vi) < 0) {
        perror("failed to put fb0 info");
        close(fd);
        return -1;
    }

    if (ioctl(fd, FBIOGET_FSCREENINFO, &fi) < 0) {
        perror("failed to get fb0 info");
        close(fd);
        return -1;
    }

    /* adjust to be page-aligned */
    unsigned adjust = fi.smem_len % DEFAULT_PAGE_SIZE;
    mmap_len = fi.smem_len;
    if (adjust) {
        mmap_len += (DEFAULT_PAGE_SIZE - adjust);
    }

    bits = mmap(0, mmap_len, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
    if (bits == MAP_FAILED) {
        perror("failed to mmap framebuffer");
        close(fd);
        return -1;
    }

    fb->version = sizeof(*fb);
    fb->width = vi.xres;
    fb->height = vi.yres;
    fb->stride = fi.line_length/PIXEL_SIZE;
    fb->data = bits;
    fb->format = PIXEL_FORMAT;
    gr_fb_clear(fb);

    fb++;

    /* check if we can use double buffering */
    if (vi.yres * fi.line_length * NUM_BUFFERS > fi.smem_len)
        return fd;

    double_buffering = 1;

    fb->version = sizeof(*fb);
    fb->width = vi.xres;
    fb->height = vi.yres;
    fb->stride = fi.line_length/PIXEL_SIZE;
    fb->data = (void*) (((unsigned) bits) + vi.yres * fi.line_length);
    fb->format = PIXEL_FORMAT;
    gr_fb_clear(fb);

    return fd;
}

static int release_framebuffer(GGLSurface *fb) {
    int ret;
    void *bits;

    bits = fb->data;
    if (bits == NULL)
        return -2;

    close(gr_fb_fd);
    gr_fb_fd = -1;

    if (mmap_len == 0)
        return -1;

    ret = munmap(bits, mmap_len);

    if (ret < 0)
       led_alert("red", 1);

    return ret;
}

int gr_fb_test(void)
{

    release_framebuffer(gr_framebuffer);

    gr_fb_fd = get_framebuffer(gr_framebuffer);
    if (gr_fb_fd < 0) {
        led_alert("red", 1);
        gr_exit();
        return -1;
    }

    return 0;
}

static void get_memory_surface(GGLSurface* ms) {
    ms->version = sizeof(GGLSurface);
    ms->width = vi.xres;
    ms->height = vi.yres;
    //ms->stride = vi.xres;
    ms->stride = fi.line_length/PIXEL_SIZE;
    ms->data = malloc(vi.yres * fi.line_length);
    ms->format = PIXEL_FORMAT;
}

static void set_active_framebuffer(unsigned n)
{
    if (n >= NUM_BUFFERS || !double_buffering) return;
    vi.yres_virtual = vi.yres * NUM_BUFFERS;
    vi.yoffset = n * vi.yres;
    vi.bits_per_pixel = PIXEL_SIZE * 8;
    if (ioctl(gr_fb_fd, FBIOPUT_VSCREENINFO, &vi) < 0) {
        perror("active fb swap failed");
    }
}

// on bootmenu exit, set this final config
static void set_final_framebuffer(void)
{
    vi.bits_per_pixel = 32;
    vi.yres_virtual = vi.yres;
    vi.xres_virtual = vi.xres;
    vi.yoffset = 0;
    vi.vmode = FB_VMODE_NONINTERLACED;
    vi.hsync_len = vi.vsync_len = 0;

    if (ioctl(gr_fb_fd, FBIOPUT_VSCREENINFO, &vi) < 0) {
        perror("final fb swap failed");
    }
}

void gr_flip(void)
{
    GGLContext *gl = gr_context;

    /* swap front and back buffers */
    if (double_buffering)
        gr_active_fb = (gr_active_fb + 1) % NUM_BUFFERS;

    /* copy data from the in-memory surface to the buffer we're about
     * to make active. */
    memcpy(gr_framebuffer[gr_active_fb].data, gr_mem_surface.data,
           vi.yres * fi.line_length);

    /* inform the display driver */
    set_active_framebuffer(gr_active_fb);
}

void gr_color(unsigned char r, unsigned char g, unsigned char b, unsigned char a)
{
    GGLContext *gl = gr_context;
    GGLint color[4];
    color[0] = ((r << 8) | r) + 1;
    color[1] = ((g << 8) | g) + 1;
    color[2] = ((b << 8) | b) + 1;
    color[3] = ((a << 8) | a) + 1;
#ifdef COLORS_REVERSED
    color[0] = ((b << 8) | b) + 1;
    color[2] = ((r << 8) | r) + 1;
#endif
    gl->color4xv(gl, color);
}

void gr_set_uicolor(struct UiColor c) {
    gr_color(c.r, c.g, c.b, c.a);
}
struct UiColor gr_make_uicolor(unsigned char r, unsigned char g, unsigned char b, unsigned char a) {
    struct UiColor c = {r, g, b, a};
    return c;
}

int gr_measure(const char *s)
{
    return FONTS[selectedFont].gr_font->cwidth * strlen(s);
}

void gr_font_size(int *x, int *y)
{
    if (FONTS[selectedFont].gr_font != NULL) {
        *x = FONTS[selectedFont].gr_font->cwidth;
        *y = FONTS[selectedFont].gr_font->cheight;
    }
}

int gr_text(int x, int y, const char *s)
{
    return gr_text_cut(x,y,s,-1,-1,-1,-1);
}

int gr_text_cut(int _x, int _y, const char *s, int minx, int maxx, int miny, int maxy) {
    GGLContext *gl = gr_context;
    GRFont *font = FONTS[selectedFont].gr_font;
    unsigned off;
    _y -= font->ascent;

    gl->bindTexture(gl, &font->texture);
    gl->texEnvi(gl, GGL_TEXTURE_ENV, GGL_TEXTURE_ENV_MODE, GGL_REPLACE);
    gl->texGeni(gl, GGL_S, GGL_TEXTURE_GEN_MODE, GGL_ONE_TO_ONE);
    gl->texGeni(gl, GGL_T, GGL_TEXTURE_GEN_MODE, GGL_ONE_TO_ONE);
    gl->enable(gl, GGL_TEXTURE_2D);

    while((off = *s++)) {
      off -= 32;
      if (off < 96) {
        int tex_diff_y = 0;
        int tex_diff_x = 0;
        int x = _x;
        int y = _y;
        int dwidth=font->cwidth;
        int dheight=font->cheight;

        if(minx>=0 && x<minx) {
          tex_diff_x=minx-x;
          dwidth-=minx-x;
          x=minx;
        }

        if(miny>=0 && y<miny) {
          tex_diff_y=miny-y;
          dheight-=miny-y;
          y=miny;
        }

        gl->texCoord2i(gl, (off * font->cwidth) - x+tex_diff_x, 0 - y+tex_diff_y);

        // maxx
        if(maxx>=0 && (x+(int)font->cwidth)>maxx) {
          dwidth-=((x+(int)font->cwidth)-maxx);
          if(dwidth<0) dwidth=0;
        }

        // maxy
        if(maxy>=0 && (y+(int)font->cheight)>maxy) {
          dheight-=((y+(int)font->cheight)-maxy);
          if(dheight<0) dheight=0;
        }

        gl->recti(gl, x, y, x + dwidth, y + dheight);
      }
      _x += font->cwidth;
    }

    return _x;
}

void gr_fill(int x, int y, int w, int h)
{
    GGLContext *gl = gr_context;
    gl->disable(gl, GGL_TEXTURE_2D);
    gl->recti(gl, x, y, w, h);
}

void gr_drawLine(int ax, int ay, int bx, int by, int width)
{
    GGLContext *gl = gr_context;
    gl->disable(gl, GGL_TEXTURE_2D);

    int v0[] = {ax*16,ay*16};
    int v1[] = {bx*16,by*16};
    gl->linex(gl, v0, v1, width*16);
}

void gr_drawRect(int ax, int ay, int bx, int by, int width)
{
    gr_drawLine(ax, ay, bx, ay, width); //top
    gr_drawLine(bx-abs(width/2)-((width % 2)?1:0), ay, bx-abs(width/2)-((width % 2)?1:0), by, width); //right
    gr_drawLine(bx, by-abs(width/2), ax, by-abs(width/2), width); //bottom
    gr_drawLine(ax+abs(width/2), by, ax+abs(width/2), ay, width); //left
}

void gr_blit(gr_surface source, int sx, int sy, int w, int h, int dx, int dy) {
    if (gr_context == NULL) {
        return;
    }
    GGLContext *gl = gr_context;

    gl->bindTexture(gl, (GGLSurface*) source);
    gl->texEnvi(gl, GGL_TEXTURE_ENV, GGL_TEXTURE_ENV_MODE, GGL_REPLACE);
    gl->texGeni(gl, GGL_S, GGL_TEXTURE_GEN_MODE, GGL_ONE_TO_ONE);
    gl->texGeni(gl, GGL_T, GGL_TEXTURE_GEN_MODE, GGL_ONE_TO_ONE);
    gl->enable(gl, GGL_TEXTURE_2D);
    gl->texCoord2i(gl, sx - dx, sy - dy);
    gl->recti(gl, dx, dy, dx + w, dy + h);
}

unsigned int gr_get_width(gr_surface surface) {
    if (surface == NULL) {
        return 0;
    }
    return ((GGLSurface*) surface)->width;
}

unsigned int gr_get_height(gr_surface surface) {
    if (surface == NULL) {
        return 0;
    }
    return ((GGLSurface*) surface)->height;
}

static struct UiFont gr_init_font(struct CFont *font_p)
{
    struct UiFont uifont;
    GGLSurface *ftex;
    unsigned char *bits, *rle;
    unsigned char *in, data;
    int i;

    uifont.gr_font = calloc(sizeof(*uifont.gr_font), 1);
    uifont.cfont = font_p;
    ftex = &uifont.gr_font->texture;

    bits = malloc(font_p->width * font_p->height);
    uifont.gr_fontmem = (void *) bits;

    ftex->version = sizeof(*ftex);
    ftex->width = font_p->width;
    ftex->height = font_p->height;
    ftex->stride = font_p->width;
    ftex->data = (void*) bits;
    ftex->format = GGL_PIXEL_FORMAT_A_8;

    in = font_p->rundata;
    while((data = *in++)) {
        memset(bits, (data & 0x80) ? 255 : 0, data & 0x7f);
        bits += (data & 0x7f);
    }

    uifont.gr_font->cwidth = font_p->cwidth;
    uifont.gr_font->cheight = font_p->cheight;
    uifont.gr_font->ascent = font_p->cheight - 2;

    return uifont;
}

static void gr_init_fonts(void)
{
    FONTS[FONT_HEAD] = gr_init_font(&bigfont);
    FONTS[FONT_ITEM] = gr_init_font(&bigfont);
    FONTS[FONT_LOGS] = gr_init_font(&font);
}

static void gr_free_font(struct UiFont *uifont)
{
    // free font allocs
    if (uifont->gr_fontmem) free(uifont->gr_fontmem);
    uifont->gr_fontmem = NULL;

    if (uifont->gr_font) free(uifont->gr_font);
    uifont->gr_font = NULL;
}

static void gr_free_fonts(void)
{
    gr_free_font(&FONTS[FONT_HEAD]);
    gr_free_font(&FONTS[FONT_ITEM]);
    gr_free_font(&FONTS[FONT_LOGS]);
}

int gr_init(void)
{
    gglInit(&gr_context);
    GGLContext *gl = gr_context;

    gr_mem_surface.data = NULL;

    gr_init_fonts();
    gr_vt_fd = open("/dev/tty0", O_RDWR | O_SYNC);
    if (gr_vt_fd < 0) {
        gr_vt_fd = open("/dev/tty", O_RDWR | O_SYNC);
    }
    if (gr_vt_fd < 0) {
        // This is non-fatal; post-Cupcake kernels don't have tty0.
        perror("can't open /dev/tty");
    } else {
        ioctl(gr_vt_fd, KDGETMODE, &gr_vt_mode);
        if (ioctl(gr_vt_fd, KDSETMODE, (void*) KD_GRAPHICS)) {
            // However, if we do open tty0, we expect the ioctl to work.
            perror("failed KDSETMODE to KD_GRAPHICS on tty");
            //gr_exit();
            //return -1;
        }
    }

    gr_fb_fd = get_framebuffer(gr_framebuffer);
    if (gr_fb_fd < 0) {
        perror("unable to get framebuffer");
        gr_exit();
        return -1;
    }

    get_memory_surface(&gr_mem_surface);

    fprintf(stderr, "framebuffer: fd %d (%d x %d)\n",
            gr_fb_fd, gr_framebuffer[0].width, gr_framebuffer[0].height);

    /* start with 0 as front (displayed) and 1 as back (drawing) */
    gr_active_fb = 0;
    set_active_framebuffer(0);
    gl->colorBuffer(gl, &gr_mem_surface);

    gl->activeTexture(gl, 0);
    gl->enable(gl, GGL_BLEND);
    gl->blendFunc(gl, GGL_SRC_ALPHA, GGL_ONE_MINUS_SRC_ALPHA);

    gr_fb_blank(true);
    gr_fb_blank(false);

    return 0;
}

void gr_exit(void)
{
    // restore original vt mode (text or graphic)
    if (gr_vt_mode != -1)
        ioctl(gr_vt_fd, KDSETMODE, &gr_vt_mode);

    // close tty
    if (gr_vt_fd != -1) {
        close(gr_vt_fd);
        gr_vt_fd = -1;
    }

    // black screen before resolution change by bootanimation.
    // both are required to prevent some weird bunnies display
    gr_fb_clear(&gr_framebuffer[0]);
    gr_fb_clear(&gr_framebuffer[1]);

    set_final_framebuffer();

    // free memory buffer
    if (gr_mem_surface.version == sizeof(GGLSurface) && gr_mem_surface.data) {
        free(gr_mem_surface.data);
        gr_mem_surface.data = NULL;
    }

    gr_free_fonts();

    // un-mmap
    release_framebuffer(gr_framebuffer);

    gglUninit(gr_context);
}

int gr_fb_width(void)
{
    return gr_framebuffer[0].width;
}

int gr_fb_height(void)
{
    return gr_framebuffer[0].height;
}

gr_pixel *gr_fb_data(void)
{
    return (unsigned short *) gr_mem_surface.data;
}

void gr_fb_blank(bool blank)
{
    int ret;

    ret = ioctl(gr_fb_fd, FBIOBLANK, blank ? FB_BLANK_POWERDOWN : FB_BLANK_UNBLANK);
    if (ret < 0)
        perror("ioctl(): blank");
}

void gr_setfont(int i) {
    selectedFont = i;
}

int gr_getfont_cwidth() {
    return FONTS[selectedFont].gr_font->cwidth;
}

int gr_getfont_cheight() {
    return FONTS[selectedFont].gr_font->cheight;
}

int gr_getfont_cheightfix() {
    return FONTS[selectedFont].cfont->cheightfix;
}
