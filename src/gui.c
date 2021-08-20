/*
 * gui.c <z64.me>
 *
 * zztexview's GUI resides within
 *
 */

#define WOW_GUI_IMPLEMENTATION
#define WOW_GUI_USE_PTHREAD 1
#include <wow.h>
#include <wow_gui.h>
#include <wow_clipboard.h>

#include <stdio.h>
#include <stdarg.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <ctype.h> /* tolower */
#include <errno.h>

#include "common.h"
#include "n64texconv.h"

/*
 *
 * revision history
 *
 *
 * v1.0.2
 * - Better `edge` results with `i` (intensity) formats
 * - In exported images, alpha channel is user's selected 'blending' mode
 * - `zztexview` is now open source
 *
 * v1.0.1
 * - Bug fix: could not edit value/text boxes after clicking drop-downs
 * - Bug fix: "Scale" drop-down text now wraps properly when scrolling on it
 * - Bug report credits: AriaHiro64, Zeldaboy14
 *
 * v1.0.0
 * - Initial release
 *
 */

/*
 *
 * checklist, brainstorming, bug tracking
 *
 */
/* FIXME
 * i got a "Floating point exception" error exactly once when using  *
 * frame_delay = 100 and haven't been able to reproduce the error... *
 * UPDATE: i also got it when initiating a right-click-drag on a     *
 *         texture preview (this was caused by casting btw) */
/* DONE texture search finds only partial match on textures that share
 *      a palette (convert bunny textures as ci8 then do a search for
 *      the eye) (zzconvert only?) (SOLUTION: watermark required
 *      'pal_colors' not 'pal_max'!) */
/* TODO allow 'custom' viewing window to exceed 256x256 max; when the
 *      user does this, allow them to right-click-drag inside the
 *      preview to pan around inside it; perhaps a scissor icon will
 *      be present, and provide a helpful tooltip on hover */
/* DONE manual broken on win32 (turns out was doing a sprintf into a
 *      buffer only strlen(x)+1, did a +16 to be safe) (this was in
 *      table of contents generation btw */
/* DONE allow dropdowns to wrap around when scrolling on them */
/* DONE Palette Override, and watermarking those palettes as well */
/* DONE fix endianness problems within n64texconv */
/* DONE built-in guide */
/* DONE guide table of contents */
/* DONE white/black alpha mode + watermark = heart icon broken
 *      (it turns out it wasn't the watermark; texture_to_n64 needed
         to use < instead of <= for the 'new nearest color' search)
        (actually, went with (diffRGB <= nearestRGB), where diffRGB
         is derived from (diffR + diffG + diffB) / 3.0) */
/* DONE display loaded file and palette name (non-ascii chars = '?') */
/* DONE png imports should use acfunc settings from texconv pane */
/* DONE corner toggle for eof data toggle between 00 and FF */
/* TODO XIO fatal IO error linux (minifb X11) */
/* DONE ci8 conversion watermarks: z64.me and z64me */
/* DONE drop png onto custom w/h image box */
/* DONE FEATURE: Texture Convert */
/* DONE FEATURE: Texture Search */
/* DONE "custom" width/height should match converted texture */
/* DONE icon win32 */
/* DONE drop file on "close external palette" button then click close,
 *      it closes then opens the next opened file */
/* DONE texture search ia-4 MM trees broken */
/* DONE PERFORMANCE: texture search format select pane */
/* DONE PERFORMANCE: texture search needs threading
 * aka progress bar and prevent "not responding" on slow searches */
/* DONE drop-file needs mouse move to activate */
/* DONE acfunc white */
/* DONE ACCURACY & PERFORMANCE:                            *
 *      texture search should omit 1bit alpha formats      *
 *      when dealing with images containing detailed alpha */
/* DONE i(intensity) formats need alpha=0xFF on PNG export */
/* DONE texture search must support searching external palette */
/* DONE texture search allow user to browse partial ci matches */
/* DONE external palette allow drag & drop */
/* DONE texture search: factor 1bit alpha into search */
/* DONE PERFORMANCE texture search: use memmem when contains no alpha */
/* DONE export png "mouth.png" writes "mouth.png.png" on linux *
 *      the solution? had to change strcasecmp to !strcasecmp  */
/* DONE windows fread/fwrite 64mb rom Z:/ directory file read error why *
 *      create wowGui_fread/fwrite, which will fall back to a buffered_ *
 *      equivalent if it fails, which will read/write chunks at a time  *
 *      instead of the entirety of the file                             */
/* TODO PERFORMANCE: step framerate down when window is inactive */
/* DONE 60hz framerate regulation */
/* DONE error with holding backspace */
/* DONE disable window scrolling! */
/* DONE some inputs are ignored at random (keyboard keys, clicks, etc) (only on Linux... no, Win32 as well) */
/* DONE external palette file separate [Save As...] dialog */
/* DONE PERFORMANCE: reconvert raw / palette_raw only when file or offsets changes */
/* DONE scrolling in a dropdown should step through its items */
/* DONE scrolling in a value box should adjust its value */




/*
 *
 * globals and misc parameters
 *
 */

/* type of data to fill eof with */
static int eof_mode = 0;
#define ENDFILEPAD (256 * 256 * 4)

static int g_png_in_is_pal = 0;
static void *pbuf = 0;

#define STATUSBAR_H (20) /* the bar at the bottom showing filenames */
#define WINW 640
#define WINH (596+STATUSBAR_H)

#define BPAD 4     /* image border padding */
#define BP(X) ((X)+BPAD)

static struct
{
	struct wowGui_rect
		xy_scroll
		, xy_64x64
		, xy_32x64
		, xy_16x32
		, xy_16x16
		, xy_64x32
		, xy_32x32
		, xy_8x16
		, xy_8x8
		, xy_custom
		, xy_palette
	;
	struct
	{
		struct prop
		{
			int row_size;
		}
			xy_scroll
			, xy_64x64
			, xy_32x64
			, xy_16x32
			, xy_16x16
			, xy_64x32
			, xy_32x32
			, xy_8x16
			, xy_8x8
			, xy_custom
			, xy_palette
		;
	} im;
	unsigned char  *buf;
	unsigned int    buf_sz;
	int             scale;
	char           *scale_str;
	int             custom_w;
	int             custom_h;
	int             custom_scale;
	unsigned int    offset;
	unsigned int    pal_ofs;
	int             pal_colors;
	int             pal_sq;
	int             NOblend;
	int             has_exp; /* has expanded 4bit to 8bit */
	int             palOR;   /* palette override */
	
	char           *statusbar; /* status bar override */
	char           *statusFile;  /* filename in status bar */
	
	/* external palette */
	struct {
		unsigned char  *buf;
		unsigned int    buf_sz;
		unsigned int    ofs;
	} Epal;
} g_zztv = {
	.xy_scroll  = {0, 0, 128 + BPAD*2, 508 + BPAD*2}
	, .xy_64x64 = {0, 0, BP(64) * 2, BP(64) * 2}
	, .xy_32x64 = {0, 0, BP(32) * 2, BP(64) * 2}
	, .xy_16x32 = {0, 0, BP(16) * 2, BP(32) * 2}
	, .xy_16x16 = {0, 0, BP(16) * 2, BP(16) * 2}
	, .xy_64x32 = {0, 0, BP(64) * 2, BP(32) * 2}
	, .xy_32x32 = {0, 0, BP(32) * 2, BP(32) * 2}
	, .xy_8x16  = {0, 0, BP( 8) * 2, BP(16) * 2}
	, .xy_8x8   = {0, 0, BP( 8) * 2, BP( 8) * 2}
	, .xy_palette = {0, 0, BP(64) * 2, BP(64) * 2}
	, .im = {
		.xy_scroll  = { .row_size = 128 }
		, .xy_64x64 = { .row_size = 64  }
		, .xy_32x64 = { .row_size = 32  }
		, .xy_16x32 = { .row_size = 16  }
		, .xy_16x16 = { .row_size = 16  }
		, .xy_64x32 = { .row_size = 64  }
		, .xy_32x32 = { .row_size = 32  }
		, .xy_8x16  = { .row_size = 8   }
		, .xy_8x8   = { .row_size = 8   }
		, .xy_palette = { .row_size = 8 }
	}
	, .statusbar = "no file loaded"
	, .pal_sq = 16
	, .custom_w = 128
	, .custom_h = 128
	, .custom_scale = 1
	, .scale = 1
	, .NOblend = 0
	, .pal_colors = 256
};
				
/* data list */
struct list
{
	void          *data;
	void          *next;
	char          *fn;
	int            w;
	int            h;
	unsigned int   sz;
};

#include "search.h"

/* threading */
static void *(*g_threadfunc)(void *) = 0;
struct wowGui_rect g_progress_bar_loc = {0};
							
unsigned char *is_unique(unsigned char *b, int want)
{
	int i;
	int k;
	/* test 0 through 5/6 */
	for (i=0; i < want; ++i)
	{
		/* check 5/6 bytes against themselves */
		for (k = i+1; k < want; ++k)
		{
			if (b[i] == b[k])
				return 0;
		}
	}
	return b;
}

static
void *
threadfunc_test(void *progress_float)
{
	float *progress = progress_float;
	
	*progress = 0;
	while (*progress < 1.0f)
	{
#ifdef _WIN32
		Sleep(1000);
		*progress += 0.1f;
#else
		for (unsigned long i = 0; i < 100000; ++i)
			;
		*progress += 0.00001f;
#endif
	}
	
	return 0;
}
/* end threading */


static int reconv_texture = 1;
static int reconv_palette = 1;

static void *calloc_safe(size_t nmemb, size_t size);
static void *malloc_safe(size_t size);
static void *realloc_safe(void *ptr, size_t size);

static unsigned char *g_pix_src = 0;
static unsigned char *g_pal_src = 0;
static int g_codec_idx = 0;
static char *g_codec_text = 0;
static int acfunc = N64TEXCONV_ACGEN_EDGEXPAND;
static int g_free_results = 0;

static void *popup_mem = 0;
static int popup_w = 0;
static int popup_h = 0;
static void *popup_dst = 0;

#define INIT_POPUP(MEM, DST, W, H) { \
	popup.rect.x = (wowGui.mouse.x - 16); \
	popup.rect.y = (wowGui.mouse.y - 16); \
	popup.rect.w = 8 * 16; \
	popup.rect.h = 20 * 5; \
	wowGui_mouse_click(wowGui.mouse.x, wowGui.mouse.y, 0); \
	popup_mem = MEM; \
	popup_dst = DST; \
	popup_w = W; \
	popup_h = H; \
}

/* returns non-zero if all bytes in array match */
static
int
all_same_byte(unsigned char *data, int sz)
{
	while (--sz)
	{
		if (data[0] != data[1])
			return 0;
		++data;
	}
	return 1;
}

const char *fn_only(const char *path)
{
	const char *slash = strrchr(path, '/');
	if (!slash)
		slash = strrchr(path, '\\');
	if (!slash)
		slash = path;
	else
		++slash;
	return slash;
}


/* watermark ci texture data*/
static void watermark(void *pal, unsigned pal_bytes, unsigned pal_max, struct list *rme)
{
	/* ensure palette size ends with 0 or 8 */
	if (pal_bytes & 7)
		pal_bytes += 8 - (pal_bytes & 7);
	
	if ((pal_bytes/2) >= 'Z')
	{
		//fprintf(stderr, "attempt watermark\n");
		
		struct list *item;
		int max_unique = 0;
		unsigned char *unique = 0;
		
		/* step through every texture; we want one with *
		 * a pattern of 5 unique bytes in a row "Z64ME" *
		 * or 6 for "z64.me"                            */
		for (item = rme; item; item = item->next)
		{
			unsigned char *b;
			unsigned char *s = item->data;
			int want = 5; /* enough characters to fit 'z64me' */
			
			
			struct valNum vn[256] = {0};
			/* set index value of each */
			for (int k = 0; k < pal_max; ++k)
			{
				vn[k].val = k;
				vn[k].num = 0;
			}
			
			/* count number of each index */
			for (b = s; b - s < item->sz; ++b)
				vn[*b].num += 1;
			
			/* sort them */
			qsort(vn, 256, sizeof(vn[0]), valNum_descend);
			
			/* derive "most" */
			for (int k = 0; k < pal_max; ++k)
				vn[k].most = k;
			
			/* sort back */
			qsort(vn, 256, sizeof(vn[0]), valNum_ascend_val);
			
			/* cut off before end just to be safe */
			for (b = s; b - s < (item->sz - (want+2)); ++b)
			{
				/* found a good or better match */
				unsigned char *x = is_unique(b, want);
				
				/* skip if in top 8 most common */
				if (x && vn[*x].most > 8 && (x == b || *(b-1) != *b) && b[want] != b[want-1])
				{
				//	fprintf(stderr, "%d isn't many\n", vn[*x].most);
					max_unique = want;
					unique = x;
					/* found a perfect match */
					if (max_unique == want)
						break;
				}
			}
			
			/* found a perfect match */
			if (max_unique == want)
				break;
		}
		
		/* an acceptable match was found */
		if (unique)
		{
			//fprintf(stderr, "watermark successful\n");
			unsigned char lut[256];
			int i;
			
			/* the lut begins with standard order */
			for (i = 0; i < 256; ++i)
				lut[i] = i;
			
			/* determine how to rearrange indices */
			int lower_ok = 0;
			for (i = 0; i < max_unique; ++i)
			{
				int nidx;
				if (i == 0) nidx = 'Z';
				if (i == 1) nidx = '6';
				if (i == 2) nidx = '4';
				if (max_unique == 5)
				{
					if (i == 3) nidx = 'M';
					if (i == 4) nidx = 'E';
				}
				else /* 6 */
				{
					if (i == 3) nidx = '.';
					if (i == 4) nidx = 'M';
					if (i == 5) nidx = 'E';
				}
				
				/* use lowercase if available */
				/* applies to the whole string */
				if (i == 0 && tolower(nidx)*2 < pal_bytes)
					lower_ok = 1;
				if (lower_ok)
					nidx = tolower(nidx);
				int old = unique[i];
				lut[unique[i]] = nidx;
				lut[nidx] = old;
			}
			
			/* now rearrange every texture */
			for (item = rme; item; item = item->next)
			{
				unsigned int i;
				unsigned char *b = item->data;
				for (i = 0; i < item->sz; ++i)
					b[i] = lut[b[i]];
			}
			
			/* now rearrange palette colors */
			unsigned short *b = pal;
			static unsigned short *Nb = 0;
			if (!Nb)
				Nb = malloc_safe(512 * sizeof(*Nb));
			for (i = 0; i < pal_bytes/2; ++i)
				Nb[i] = b[lut[i]];
			memcpy(b, Nb, pal_bytes);
		}
	}
}

static
void
dief(char *fmt, ...)
{
	char message[1024];
	va_list ap;
	va_start (ap, fmt);
	vsnprintf(message, sizeof(message), fmt, ap);
	wowGui_popup(
		WOWGUI_POPUP_ICON_ERR
		, WOWGUI_POPUP_OK
		, WOWGUI_POPUP_OK
		, "Error"
		, message
	);
	va_end(ap);
	fprintf(stderr, "%s\n", message);
	exit(EXIT_FAILURE);
}

static
void
complain(const char *message)
{
	wowGui_popup(
		WOWGUI_POPUP_ICON_ERR
		, WOWGUI_POPUP_OK
		, WOWGUI_POPUP_OK
		, "Error"
		, message
	);
	fprintf(stderr, "%s\n", message);
}

static
void
die(const char *message)
{
	complain(message);
	exit(EXIT_FAILURE);
}

/* safe calloc */
static
void *
calloc_safe(size_t nmemb, size_t size)
{
	void *result;
	
	result = calloc(nmemb, size);
	
	if (!result)
		die("memory error");
	
	return result;
}

/* safe malloc */
static
void *
malloc_safe(size_t size)
{
	void *result;
	
	result = malloc(size);
	
	if (!result)
		die("memory error");
	
	return result;
}

/* safe realloc */
static
void *
realloc_safe(void *ptr, size_t size)
{
	void *result;
	
	result = realloc(ptr, size);
	
	if (!result)
		die("memory error");
	
	return result;
}

static
void *
strdup_safe(char *str)
{
	char *ok = malloc_safe(strlen(str)+1);
	strcpy(ok, str);
	return ok;
}

/* remove characters in middle of string */
static
void
rmchrs(char *str, int chrs)
{
	memmove(str, str + chrs, strlen(str+chrs)+1);
}

static
void
strip_utf8(char *str)
{
	char *s = str;
	for (s = str; *s; ++s) {
		if (0xf0 == (0xf8 & *s)) {
			// 4-byte utf8 code point (began with 0b11110xxx)
			rmchrs(s, 3);
		} else if (0xe0 == (0xf0 & *s)) {
			// 3-byte utf8 code point (began with 0b1110xxxx)
			rmchrs(s, 2);
		} else if (0xc0 == (0xe0 & *s)) {
			// 2-byte utf8 code point (began with 0b110xxxxx)
			rmchrs(s, 1);
		} else { // if (0x00 == (0x80 & *s)) {
			// 1-byte ascii (began with 0b0xxxxxxx)
			continue;
		}
		*s = '?';
	}
}

/* convert full path to only filename */
static
char *
fnonly(char *fn)
{
	char *ss = strrchr(fn, '/');
	if (!ss) ss = strrchr(fn, '\\');
	if (!ss) return fn;
	return ss + 1;
}

static
void *
strdup_safe_sanitize(char *str)
{
	str = strdup_safe(fnonly(str));
	strip_utf8(str);
	return str;
}

static
int
png_fail(void *png)
{
	if (!png)
	{
		/* warning message that importing image was a failure */
		wowGui_popup(
			WOWGUI_POPUP_ICON_WARN
			, WOWGUI_POPUP_OK
			, WOWGUI_POPUP_OK
			, "Failed to load image"
			, stbi_failure_reason()
		);
		return 1;
	}
	return 0;
}

static
void *
file_load_FILE(FILE *fp, unsigned int *sz, unsigned int exp)
{
#define complain(X) { (complain)(X); return 0; }
	unsigned char *data;
	int padding = ENDFILEPAD;
	
	if (!fp)
		complain("unable to open file for reading");
	
	fseek(fp, 0, SEEK_END);
	*sz = ftell(fp);
	if (!*sz)
		complain("size of file == 0");
	
	/* the +0x4000 is for padding at end */
	if (!exp)
		exp = 1;
	else
		exp = 3;
	data = malloc_safe((*sz * exp) + padding);
	
	/* read file */
	fseek(fp, 0, SEEK_SET);
	if (fread(data, 1, *sz, fp) != *sz)
		complain("file read error: try moving it");
	
	/* clear end-padding */
	memset(data + *sz, -eof_mode, padding);
	
	/* everything worked */
	return data;
#undef complain
}

static
void *
file_load(const void *fn, unsigned int *sz, unsigned int exp)
{
#define complain(X) { (complain)(X); return 0; }
	FILE *fp = fopen(fn, "rb");
	void *data;
	if (!fp)
		complain("unable to open file for reading");
	
	data = file_load_FILE(fp, sz, exp);
	if (fp)
		fclose(fp);
	return data;
#undef complain
}

static
void
file_save(char *fn, void *data, unsigned int sz)
{
#define complain(X) { (complain)(X); return; }
	FILE *fp;
	
	/* update filename in status bar if it should be */
	if (!g_zztv.statusFile)
		g_zztv.statusFile = strdup_safe_sanitize(fn);
	
	if (!sz)
		complain("file size == 0");
	
	fp = fopen(fn, "wb");
	if (!fp)
		complain("unable to open file for writing");
	
	/* read file */
	if (fwrite(data, 1, sz, fp) != sz)
		complain("write error: try writing elsewhere");
	
	/* everything worked */
	fclose(fp);
#undef complain
}

static
void *
file_load_gui(char *ext, char *path, unsigned int *sz, unsigned int exp)
{
	void *rv;
	nfdchar_t *ofn = 0;
	nfdresult_t result = NFD_OpenDialog(ext, path, &ofn);
	*sz = 0;
	
	if (result != NFD_OKAY)
	{
		return 0;
	}
	
	/* update filename in status bar if it should be */
	if (!g_zztv.statusFile)
		g_zztv.statusFile = strdup_safe_sanitize(ofn);
	
	rv = file_load(ofn, sz, exp);
	//fprintf(stderr, "%s\n", ofn);
	free(ofn);
	
	return rv;
}

static
inline
uint32_t
png_color(uint32_t color)
{
#if (COLORCORRECT == COLORCORRECT_NONE)
	return color;
#else
	return CHANNEL_A(color) << 24 | CHANNEL_B(color) << 16 | CHANNEL_G(color) << 8 | CHANNEL_R(color);
#endif
}

static
void
png_color_array(void *_pix, unsigned long num)
{
	uint32_t *pix = _pix;
	while (num--)
	{
		*pix = png_color(*pix);
		 ++pix;
	}
}

static
inline
uint32_t
rgba_color(uint32_t color)
{
#if (COLORCORRECT == COLORCORRECT_NONE)
	return color;
#else
	return CHANNEL_R(color) << 24 | CHANNEL_G(color) << 16 | CHANNEL_B(color) << 8 | CHANNEL_A(color);
#endif
}

static
void
rgba_color_array(void *_pix, unsigned long num)
{
	uint32_t *pix = _pix;
	while (num--)
	{
		*pix = rgba_color(*pix);
		 ++pix;
	}
}

static
inline
uint32_t
rgba32_to_abgr32(uint32_t color)
{
	return
		  ( color        & 0xFF) << 24   /* r -> a */
		| ((color >>  8) & 0xFF) << 16   /* g -> b */
		| ((color >> 16) & 0xFF) << 8    /* b -> g */
		| ((color >> 24) & 0xFF)         /* a -> r */
	;
}

static
void
rgba32_to_abgr32_array(void *_pix, unsigned long num)
{
	uint32_t *pix = _pix;
	while (num--)
	{
		*pix = rgba32_to_abgr32(*pix);
		 ++pix;
	}
}

static
inline
uint32_t
abgr32_to_rgba32(uint32_t color)
{
	return
		  ( color        & 0xFF) << 24   /* a -> r */
		| ((color >>  8) & 0xFF) << 16   /* b -> g */
		| ((color >> 16) & 0xFF) << 8    /* g -> b */
		| ((color >> 24) & 0xFF)         /* r -> a */
	;
}

static
void
abgr32_to_rgba32_array(void *_pix, unsigned long num)
{
	uint32_t *pix = _pix;
	while (num--)
	{
		*pix = abgr32_to_rgba32(*pix);
		 ++pix;
	}
}

static
inline
uint32_t
rgba32_to_internal(uint32_t color)
{
	return
		  ( color        & 0xFF) << SHIFT_A
		| ((color >>  8) & 0xFF) << SHIFT_B
		| ((color >> 16) & 0xFF) << SHIFT_G
		| ((color >> 24) & 0xFF) << SHIFT_R
	;
}

static
void
rgba32_to_internal_array(void *_pix, unsigned long num)
{
	uint32_t *pix = _pix;
	while (num--)
	{
		*pix = rgba32_to_internal(*pix);
		 ++pix;
	}
}

static
void
rgba32_to_internal_array_8(void *_pix, unsigned long num)
{
	unsigned char *pix = _pix;
	while (num--)
	{
		unsigned char np[4] = { pix[2], pix[1], pix[0], pix[3] };
		pix[0] = np[0];
		pix[1] = np[1];
		pix[2] = np[2];
		pix[3] = np[3];
		pix += 4;
	}
}

static
void
file_save_gui(void *data, unsigned int sz)
{
	nfdchar_t *ofn = 0;
	nfdresult_t result = NFD_SaveDialog(0, 0/*TODO*/, &ofn);
	
	if (result != NFD_OKAY)
	{
		return;
	}
	
	file_save(ofn, data, sz);
	free(ofn);
}

static
void *
load_png_gui(int *w, int *h)
{
	void *rv;
	nfdchar_t *ofn = 0;
	int n;
	nfdresult_t result = NFD_OpenDialog("png", 0/*TODO path*/, &ofn);
	
	if (result != NFD_OKAY)
	{
		return 0;
	}
	
	rv = stbi_load((void*)ofn, w, h, &n, 4);
	free(ofn);
	
	if (png_fail(rv))
		return 0;
	
	return rv;
}

struct codec
{
	char *name;
	enum n64texconv_fmt     fmt;
	enum n64texconv_bpp     bpp;
	enum wowGui_blit_blend  blend;
	int pixel_sz;
	int search_enabled;
};

struct xy
{
	int x;
	int y;
};

//const static int lbw = 128; /* left bar width in pixels */
static struct codec codec[] = {
	{ .name = "rgba16", .fmt = N64TEXCONV_RGBA, .bpp = N64TEXCONV_16, .pixel_sz = 2, .blend = WOWGUI_BLIT_ALPHATEST }
	, { .name = "rgba32", .fmt = N64TEXCONV_RGBA, .bpp = N64TEXCONV_32, .pixel_sz = 4, .blend = WOWGUI_BLIT_ALPHABLEND }
	//, { .name = "yuvXXX", .fmt = N64TEXCONV_RGBA, .bpp = N64TEXCONV_16 /* TODO yuv? */ }
	, { .name = "ci4", .fmt = N64TEXCONV_CI, .bpp = N64TEXCONV_4, .pixel_sz = 0, .blend = WOWGUI_BLIT_ALPHATEST }
	, { .name = "ci8", .fmt = N64TEXCONV_CI, .bpp = N64TEXCONV_8, .pixel_sz = 1, .blend = WOWGUI_BLIT_ALPHATEST }
	, { .name = "ia4", .fmt = N64TEXCONV_IA, .bpp = N64TEXCONV_4, .pixel_sz = 0, .blend = WOWGUI_BLIT_ALPHATEST }
	, { .name = "ia8", .fmt = N64TEXCONV_IA, .bpp = N64TEXCONV_8, .pixel_sz = 1, .blend = WOWGUI_BLIT_ALPHABLEND }
	, { .name = "ia16", .fmt = N64TEXCONV_IA, .bpp = N64TEXCONV_16, .pixel_sz = 2, .blend = WOWGUI_BLIT_ALPHABLEND }
	, { .name = "i4", .fmt = N64TEXCONV_I, .bpp = N64TEXCONV_4, .pixel_sz = 0, .blend = WOWGUI_BLIT_ALPHABLEND }
	, { .name = "i8", .fmt = N64TEXCONV_I, .bpp = N64TEXCONV_8, .pixel_sz = 1, .blend = WOWGUI_BLIT_ALPHABLEND }
	//, { .name = "1bit", .fmt = N64TEXCONV_1BIT, .bpp = 0, .pixel_sz = 1, .blend = WOWGUI_BLIT_ALPHATEST }
};

#define  SET_XY(R)                                         \
	g_zztv.R.x = wowGui.scissor.x + wowGui.win->cursor.x - BPAD;                      \
	g_zztv.R.y = wowGui.scissor.y + wowGui.win->cursor.y + wowGui.advance.y - BPAD;   \
	if (wowGui_generic_clickable_rect(&g_zztv.R) & WOWGUI_CLICKTYPE_CLICK)

#define RDRAG(R, SCALE, OFS, OFS_MAX)                 \
{                                                     \
   int scale = SCALE;                                 \
   unsigned int *Mofs = OFS;                          \
   int ofs_max = OFS_MAX;                             \
   if (!scale)                                        \
   	scale = g_zztv.scale + 1;                       \
   if (!Mofs) Mofs = &g_zztv.offset;                  \
   if (!ofs_max) ofs_max = g_zztv.buf_sz;             \
	int scrolled = wowGui_scrolled();                  \
	unsigned int OMofs = *Mofs;                        \
	int is_pal = &g_zztv.R == &g_zztv.xy_palette;      \
	if (scrolled)                                      \
	{                                                  \
		scrolled = -scrolled;                           \
		int increment = codec[g_codec_idx].pixel_sz;    \
		if (is_pal)                                     \
			increment = 2; /* ci palette */              \
		if (!increment) /* 4bpp */                      \
			increment = 1;                               \
		if (scrolled < 0 && increment > *Mofs)          \
			*Mofs = 0;                                   \
		else                                            \
			*Mofs += scrolled * increment;               \
	}                                                  \
	static unsigned int rcofs = 0;                     \
	int rcdrag = wowGui_rightdrag_y();                 \
	if (rcdrag)                                        \
	{                                                  \
		int row_size = g_zztv.im.R.row_size;            \
		if (is_pal)                                     \
			row_size = g_zztv.pal_sq*2; /* ci palette */ \
		else if (codec[g_codec_idx].pixel_sz)           \
			row_size *= codec[g_codec_idx].pixel_sz;     \
		else /* 4bpp */                                 \
			row_size /= 2;                               \
		int diff = (rcdrag / (scale)) * row_size;       \
		*Mofs = rcofs - diff;                           \
		if (*Mofs > ofs_max)                            \
		{                                               \
			if (rcofs < diff && diff > 0)                \
				*Mofs = 0;                                \
			else                                         \
				*Mofs = ofs_max;                          \
		}                                               \
	}                                                  \
	else                                               \
		rcofs = *Mofs;                                  \
	if (OMofs != *Mofs)                                \
	{                                                  \
		if (is_pal)                                     \
			reconv_palette |= 1;                         \
		reconv_texture |= 1; /* texture must reconv  */ \
		                     /* on either ofs change */ \
	}                                                  \
}

static
void
export_png_gui(void *mem, int w, int h)
{
	nfdchar_t *ofn = 0;
	nfdresult_t result = NFD_SaveDialog("png", 0/*TODO*/, &ofn);
	
	if (result != NFD_OKAY)
	{
		return;
	}
	
	if (wowGui_force_extension(&ofn, "png"))
		die("memory error");
	
	png_color_array(mem, w * h);
	
	/* alpha channel = 0xFF if user has blending turned off */
	if (g_zztv.NOblend)
	{
		unsigned char *b = mem;
		unsigned i;
		for (i = 0; i < w * h; ++i, b+= 4)
			b[3] = 0xFF;
	}
	stbi_write_png(ofn, w, h, 4, mem, 0);
	reconv_texture = 1;
	reconv_palette = 1;
	//fprintf(stderr, "%s\n", ofn);
	free(ofn);
}

static
void
use_buffer(void *nbuf, unsigned int nbuf_sz)
{
	if (nbuf)
	{
		/* free this if it's already loaded */
		if (g_zztv.buf)
			free(g_zztv.buf);
		
		g_zztv.buf = nbuf;
		g_zztv.buf_sz = nbuf_sz;
	
		/* if offsets exceed size, clear */
		if (g_zztv.offset > g_zztv.buf_sz)
			g_zztv.offset = 0;
		if (g_zztv.pal_ofs > g_zztv.buf_sz)
			g_zztv.pal_ofs = 0;
		
		/* say reconvert texture and palette */
		reconv_texture = 1;
		reconv_palette = 1;
		g_free_results = 1;
		
		/* freshly loaded file, so has not expanded 4bit to 8bit */
		g_zztv.has_exp = 0;
	}
}

static
void
use_filename(const char *fn)
{
	if (!fn)
		return;
	
	void *nbuf = 0;
	unsigned int nbuf_sz;
	
	nbuf = file_load(fn, &nbuf_sz, 1);
	
	use_buffer(nbuf, nbuf_sz);
}

static
void
use_FILE(FILE *fp)
{
	if (!fp)
		return;
	
	void *nbuf = 0;
	unsigned int nbuf_sz;
	
	nbuf = file_load_FILE(fp, &nbuf_sz, 1);
	
	use_buffer(nbuf, nbuf_sz);
}

/* user clicked "Open" button, or left pane */
static
void
button_open(void)
{
	void *nbuf = 0;
	unsigned int nbuf_sz;
	
	nbuf = file_load_gui(0, 0, &nbuf_sz, 1);
	
	use_buffer(nbuf, nbuf_sz);
}

/* user dropped file onto "Open" button or left pane */
static
void
dropfile_open(void)
{
#if 0 /* testing: list dropped files */
	char *n;
	for (n = wowGui_dropped_file_name(); n; n = wowGui_dropped_file_name())
	{
		fprintf(stderr, "dropped '%s'\n", n);
	}
	exit(0);
#endif
	char *dfn = wowGui_dropped_file_name();
	
	/* update filename in status bar if it should be */
	if (g_zztv.statusFile)
		free(g_zztv.statusFile);
	g_zztv.statusFile = strdup_safe_sanitize(dfn);
	
	use_filename(dfn);
}

static
void
png_proc(void *png, int w, int h, unsigned char *dst, int dst_w, int dst_h)
{
	/* no png loaded */
	if (!png)
		return;
	
	/* if dimensions match */
	if (w == dst_w && h == dst_h)
	{
		unsigned int sz;
		
		/* if palette, treat as list of colors */
		if (g_png_in_is_pal)
		{
			n64texconv_to_n64(
				png /* in-place conversion */
				, png
				, 0
				, 0
				, N64TEXCONV_RGBA
				, N64TEXCONV_16
				, g_zztv.pal_colors
				, 1
				, &sz
			);
		}
		
		/* is otherwise a texture */
		else
		{
			unsigned int pal_bytes;
			
			/* non-ci textures use acfunc */
			//TODO any reason we shouldn't do this when palette override?
			if (codec[g_codec_idx].fmt != N64TEXCONV_CI)
			{
				n64texconv_acgen(
					png
					, w
					, h
					, acfunc
					, 0
					, calloc_safe
					, realloc_safe
					, free
					, codec[g_codec_idx].fmt
				);
			}
			
			/* palette override */
			if (codec[g_codec_idx].fmt == N64TEXCONV_CI && g_zztv.palOR)
			{
				/* generate palette from image */
				n64texconv_palette_ify(
					png
					, pbuf
					, w
					, h
					, g_zztv.pal_colors
					, 0
					, calloc_safe
					, realloc_safe
					, free
				);
				
				/* convert palette colors (rgba8888 -> rgba5551) */
				n64texconv_to_n64(
					g_pal_src /* in-place conversion */
					, pbuf
					, 0
					, 0
					, N64TEXCONV_RGBA
					, N64TEXCONV_16
					, g_zztv.pal_colors
					, 1
					, &pal_bytes
				);
			}
			
			n64texconv_to_n64(
				png /* in-place conversion */
				, png
				, g_pal_src
				, g_zztv.pal_colors
				, codec[g_codec_idx].fmt
				, codec[g_codec_idx].bpp
				, w
				, h
				, &sz
			);
			
			/* palette override: watermarking */
			if (codec[g_codec_idx].fmt == N64TEXCONV_CI && g_zztv.palOR)
			{
				struct list *rme = 0;
				
				/* link data into list */
				struct list *item = malloc_safe(sizeof(*item));
				item->next = rme;
				item->data = png;
				item->fn   = 0;
				item->w    = w;
				item->h    = h;
				item->sz   = sz;
				rme = item;
				
				watermark(g_pal_src, pal_bytes, g_zztv.pal_colors, rme);
				
				free(rme);
			}
		}
		
		/* debugging: test to_n64 conversion match */
		//if (memcmp(dst, png, sz))
		//	dief("reconverted data doesn't match\n");
		
		/* copy converted data back into n64 file */
		memcpy(dst, png, sz);
	}
	
	else
	{
		/* warning: dimensions don't match */
		const char fmt[] = "PNG dimensions (%dx%d) don't match destination dimensions (%dx%d)";
		char *errstr = malloc_safe(sizeof(fmt) * 2);
		//dief("dimensions don't match\n");
		snprintf(errstr, sizeof(fmt) * 2, fmt, w, h, dst_w, dst_h);
		wowGui_popup(
			WOWGUI_POPUP_ICON_WARN
			, WOWGUI_POPUP_OK
			, WOWGUI_POPUP_OK
			, "Error"
			, errstr
		);
		free(errstr);
	}
	
	/* free generated image */
	stbi_image_free(png);
	
	/* reconvert texture/palette */
	reconv_texture = 1;
	reconv_palette = 1;
}

static
void
dropfile_png(void *dst, int dst_w, int dst_h)
{
	int rv =
	wowGui_popup(
		WOWGUI_POPUP_ICON_QUESTION
		, WOWGUI_POPUP_YES | WOWGUI_POPUP_CANCEL
		, WOWGUI_POPUP_CANCEL
		, "Are you sure?"
		, "Are you sure you want to overwrite this data with a PNG?"
	);
	
	if (rv & WOWGUI_POPUP_CANCEL)
		return;
	
	void *png;
	int w;
	int h;
	int n;
	png = stbi_load(wowGui_dropped_file_name(), &w, &h, &n, 4);
	
	if (png_fail(png))
		return;
	
	png_proc(png, w, h, dst, dst_w, dst_h);
}

#include "threadfunc_search.h"

int wow_main(argc, argv)
{
	wow_main_args(argc, argv);
	int show_manual = 0;
	unsigned char *raw = malloc_safe(256 * 256 * 4);
	unsigned char *pLut = malloc_safe(256);
	unsigned char *palette_raw = malloc_safe(16 * 16 * 4);
	FILE *notes = 0;
	int i;
	
	if (argc > 1)
	{
		g_zztv.buf = file_load(argv[1], &g_zztv.buf_sz, 1);
	}
	
	wowGui_bind_init(PROGNAME, WINW, WINH);
	
	wow_windowicon(1);
	
	for (i = 0; i < 256; ++i)
		pLut[i] = i;
	
	/* palette color buffer */
	pbuf = malloc_safe(256 * 4);
	
	while (1)
	{
		/* wowGui_frame() must be called before you do any input */
		wowGui_frame();
		
		/* events */
		wowGui_bind_events();
		
		if (wowGui_bind_should_redraw())
		{
		wowGui_viewport(0, 0, WINW, WINH);
		wowGui_padding(8, 8);
		
		static struct wowGui_window win = {
			.rect = {
				.x = 0
				, .y = 0
				, .w = 432
				, .h = WINH
			}
			, .color = 0x301818FF
			, .not_scrollable = 1
			, .scroll_valuebox = 1
		};
		#define rpane_x 432
		#define rpane_y 0
		#define rpane_w (WINW - 432)
		#define rpane_h (WINH / 3)
		#define palpane_h 300
		#define convpane_h ((WINH - palpane_h) / 2)
		static struct wowGui_window palpane = {
			.rect = {
				.x = rpane_x
				, .y = 0
				, .w = rpane_w
				, .h = palpane_h
			}
			, .color = 0x271313FF
			, .not_scrollable = 1
			, .scroll_valuebox = 1
		};
		static struct wowGui_window convpane = {
			.rect = {
				.x = rpane_x
				, .y = palpane_h + 0
				, .w = rpane_w
				, .h = convpane_h
			}
			, .color = 0x371e1eFF
			, .not_scrollable = 1
			, .scroll_valuebox = 1
		};
		static struct wowGui_window searchpane = {
			.rect = {
				.x = rpane_x
				, .y = palpane_h + convpane_h
				, .w = rpane_w
				, .h = convpane_h
			}
			, .color = 0x271313FF
			, .not_scrollable = 1
			, .scroll_valuebox = 1
		};
		static struct wowGui_window popup = {
			.color = 0x000000FF
			, .not_scrollable = 1
			, .scroll_valuebox = 1
		};
		static struct wowGui_window statusbar = {
			.rect = {
				.x = 0
				, .y = WINH - STATUSBAR_H
				, .w = WINW
				, .h = STATUSBAR_H
			}
			, .color = 0x1c0e0eff
			, .not_scrollable = 1
		};
		int ppw = palpane.rect.w - 8 * 3;
		if (popup.rect.w)
		{
			if (wowGui_window(&popup))
			{
				int close = 0;
				wowGui_row_height(20);
				wowGui_padding(8, 8);
				wowGui_column_width(popup.rect.w - 8 * 2);
				wowGui_columns(1);
				if (wowGui_button("Export PNG"))
				{
					export_png_gui(popup_mem, popup_w, popup_h);
					close = 1;
				}
				if (wowGui_button("Import PNG"))
				{
					close = 1;
					
					int w;
					int h;
					void *png = load_png_gui(&w, &h);
					
					png_proc(png, w, h, popup_dst, popup_w, popup_h);
				}
				if (
					wowGui_button("Cancel")
					|| ( wowGui.mouse.click.up.frames && !point_in_rect(
							wowGui.mouse.click.x
							, wowGui.mouse.click.y
							, &popup.rect
						)
					)
				)
				{
					close = 1;
				}
				
				if (close)
				{
					popup.rect.w = 0;
					g_png_in_is_pal = 0;
				}
				wowGui_window_end();
			}
		} /* end popup */
		else if (show_manual)
		{
			static struct wowGui_window win = {
				.rect = {
					.x = 0
					, .y = 0
					, .w = WINW
					, .h = WINH
				}
				, .color = 0x301818FF
				, .not_scrollable = 0
				, .scroll_valuebox = 1
			};
			static struct wowGui_window back = {
				.rect = {
					.x = 0
					, .y = 0
					, .w = 8 * (strlen("[Back]")-1) + 8 * 2
					, .h = 16 + 8
				}
				, .color = 0x301818FF
				, .not_scrollable = 1
				, .scroll_valuebox = 1
			};
			static struct wowGui_window back_to_top = {
				.rect = {
					.x = 0
					, .y = 0
					, .w = 8 * (strlen("[Back to top]")-1) + 8 * 2
					, .h = 16 + 8
				}
				, .color = 0x301818FF
				, .not_scrollable = 1
				, .scroll_valuebox = 1
			};
			back_to_top.rect.x = WINW - back_to_top.rect.w - 8;
			wowGui_padding(24, 8);
			wowGui_columns(1);
			char *manual =
				#include "manual.h"
			;
			
			struct chapter {
				char  *name;
				void  *next;
				int    scroll;
			};
			
			static struct chapter *toc = 0;
			if (!toc)
			{
				wowGui.scrollSpeed.y *= 2; /* double scroll speed */
				char *ss = manual;
				int line = 0;
				while (1)
				{
					ss = strchr(ss, '\n');
					if (!ss)
						break;
					
					/* first character on line is alphanumeric */
					ss++;
					if (isalnum(*ss))
					{
						static int ct = 0;
						
						struct chapter *chap = calloc_safe(1, sizeof(*chap));
						if (!toc)
							toc = chap;
						else
						{
							struct chapter *end;
							for (end = toc; end->next; end = end->next)
								;
							end->next = chap;
						}
						
						char *endl = strchr(ss, '\n');
						int len = (endl) ? endl - ss : strlen(ss);
						//fprintf(stderr, "chapter '%.*s'\n", len, ss);
						
						chap->name = malloc_safe(len + 16);
						sprintf(chap->name, "[%d] %.*s", ct++, len, ss);
						chap->scroll = line * -16/*TODO hardcoded font height*/;
					}
					
					line += 1;
				}
			}
			
			if (wowGui_window(&win))
			{
				wowGui_label(manual);
				
				/* table of contents */
				wowGui_column_width(256+8);
				wowGui_padding(24, 8);
				wowGui_seek_top();
				wowGui_row();
				wowGui_row();
				wowGui_row();
				wowGui_row();
				
				wowGui_padding(24+3*8, 8);
				wowGui_label("TABLE OF CONTENTS");
				wowGui_row();
				struct chapter *chap;
				for (chap = toc; chap; chap = chap->next)
					if (wowGui_clickable(chap->name))
						win.scroll.y = chap->scroll;
				
				wowGui_window_end();
			}
			wowGui_padding(8, 8);
			if (wowGui_window(&back))
			{
				if (wowGui_button("Back"))
					show_manual = 0;
				wowGui_window_end();
			}
			
			if (wowGui_window(&back_to_top))
			{
				if (wowGui_button("Back to Top"))
					win.scroll.y = 0;
				wowGui_window_end();
			}
		} /* end manual */
		else {
		if (wowGui_window(&win))
		{
			wowGui_row_height(20/*12*/);
			wowGui_row();
			
		/* skip first row, b/c it gets drawn last */
			
			/* configure complex drawing */
			wowGui_columns(10);
			wowGui_column_width(8 * 8);
			wowGui_padding(16, 16);
			
		/* first row */
			wowGui_row();
			wowGui_column_width(128);
			wowGui_row_height(24);
			SET_XY(xy_scroll)
			{
				button_open();
			}
			else if (wowGui_dropped_file_last())
			{
				dropfile_open();
			}
			RDRAG(xy_scroll, 1, 0, 0)
			if (wowGui_hex_range(&g_zztv.offset, 0x0, g_zztv.buf_sz, 0))
				reconv_texture |= 1;
			
			SET_XY(xy_64x64)
			{
				INIT_POPUP(raw, g_pix_src, 64, 64)
			}
			else if (wowGui_dropped_file_last())
			{
				dropfile_png(g_pix_src, 64, 64);
			}
			RDRAG(xy_64x64, 0, 0, 0)
			wowGui_label("64x64");
			wowGui_column_width(32 * 2);
			SET_XY(xy_32x64)
			{
				INIT_POPUP(raw, g_pix_src, 32, 64)
			}
			else if (wowGui_dropped_file_last())
			{
				dropfile_png(g_pix_src, 32, 64);
			}
			RDRAG(xy_32x64, 0, 0, 0)
			wowGui_label("32x64");
			
			
			SET_XY(xy_16x32)
			{
				INIT_POPUP(raw, g_pix_src, 16, 32)
			}
			else if (wowGui_dropped_file_last())
			{
				dropfile_png(g_pix_src, 16, 32);
			}
			RDRAG(xy_16x32, 0, 0, 0)
			wowGui_label("16x32");
			
			
		/* row for 16x16 */
			wowGui_row();
			wowGui_row_height(20);
			wowGui_row();
			wowGui_row();
			wowGui_row();
			wowGui_row_height(16);
			wowGui_row();
			wowGui_row_height(20);
			wowGui_column_width(128);
			wowGui_label("");
			wowGui_label("");
			wowGui_column_width(32 * 2);
			wowGui_label("");
			SET_XY(xy_16x16)
			{
				INIT_POPUP(raw, g_pix_src, 16, 16)
			}
			else if (wowGui_dropped_file_last())
			{
				dropfile_png(g_pix_src, 16, 16);
			}
			RDRAG(xy_16x16, 0, 0, 0)
			wowGui_label("16x16");
			
			
		/* third row */
			wowGui_row();
			wowGui_row();
			wowGui_row();
			wowGui_column_width(128);
			wowGui_label("");
			
			SET_XY(xy_64x32)
			{
				INIT_POPUP(raw, g_pix_src, 64, 32)
			}
			else if (wowGui_dropped_file_last())
			{
				dropfile_png(g_pix_src, 64, 32);
			}
			RDRAG(xy_64x32, 0, 0, 0)
			wowGui_label("64x32");
			
			wowGui_column_width(32 * 2);
			SET_XY(xy_32x32)
			{
				INIT_POPUP(raw, g_pix_src, 32, 32)
			}
			else if (wowGui_dropped_file_last())
			{
				dropfile_png(g_pix_src, 32, 32);
			}
			RDRAG(xy_32x32, 0, 0, 0)
			wowGui_label("32x32");
			
			SET_XY(xy_8x16)
			{
				INIT_POPUP(raw, g_pix_src, 8, 16)
			}
			else if (wowGui_dropped_file_last())
			{
				dropfile_png(g_pix_src, 8, 16);
			}
			wowGui_label("8x16");
			RDRAG(xy_8x16, 0, 0, 0)
			
			
		/* row for 8x8 */
			wowGui_row();
			wowGui_row();
			wowGui_row_height(12);
			wowGui_row();
			wowGui_column_width(128);
			wowGui_label("");
			wowGui_label("");
			wowGui_column_width(32 * 2);
			wowGui_label("");
			wowGui_row_height(16);
			SET_XY(xy_8x8)
			{
				INIT_POPUP(raw, g_pix_src, 8, 8)
			}
			else if (wowGui_dropped_file_last())
			{
				dropfile_png(g_pix_src, 8, 8);
			}
			RDRAG(xy_8x8, 0, 0, 0)
			wowGui_label("8x8");
			wowGui_row_height(20);
			
			
		/* row for custom */
			wowGui_row();
			wowGui_row();
			wowGui_row_height(24);
			wowGui_column_width(128);
			wowGui_label("");
			wowGui_column_width(8 * 6);
			
			g_zztv.xy_custom.x = wowGui.win->cursor.x - BPAD;
			g_zztv.xy_custom.y = wowGui.win->cursor.y + wowGui.advance.y - BPAD;
			g_zztv.xy_custom.w = BP(g_zztv.custom_w) * 2;
			g_zztv.xy_custom.h = BP(g_zztv.custom_h) * 2;
			if (g_zztv.custom_h > 128 || g_zztv.custom_w > 128)
			{
				g_zztv.xy_custom.w = BP(g_zztv.custom_w) + BPAD;
				g_zztv.xy_custom.h = BP(g_zztv.custom_h) + BPAD;
			}
			if (wowGui_generic_clickable_rect(&g_zztv.xy_custom) & WOWGUI_CLICKTYPE_CLICK)
			{
				INIT_POPUP(raw, g_pix_src, g_zztv.custom_w, g_zztv.custom_h)
			}
			else if (wowGui_dropped_file_last())
			{
				dropfile_png(g_pix_src, g_zztv.custom_w, g_zztv.custom_h);
			}
			RDRAG(xy_custom, g_zztv.custom_scale, 0, 0)
			
			wowGui_label("Custom");
			wowGui_int_range(&g_zztv.custom_w, 1, 256, 1);
			wowGui_column_width(8 * 2);
			wowGui_label("x");
			wowGui_column_width(8 * 6);
			wowGui_int_range(&g_zztv.custom_h, 1, 256, 1);
			wowGui_row_height(20);
			if (g_zztv.custom_w > 256)
				g_zztv.custom_w = 256;
			if (g_zztv.custom_h > 256)
				g_zztv.custom_h = 256;
			g_zztv.im.xy_custom.row_size = g_zztv.custom_w;
		
		/* display texture */
			if (g_zztv.buf)
			{
				unsigned char *pal_buf = g_zztv.buf;
				unsigned int pal_ofs = g_zztv.pal_ofs;
				
				if (g_zztv.Epal.buf)
				{
					pal_buf = g_zztv.Epal.buf;
					pal_ofs = g_zztv.Epal.ofs;
					if (pal_ofs > g_zztv.Epal.buf_sz)
						pal_ofs = g_zztv.Epal.buf_sz;
				}
				
				//fprintf(stderr, "offset = %08X\n", g_zztv.offset);
				g_pix_src = g_zztv.buf + g_zztv.offset;
				g_pal_src = pal_buf + pal_ofs;
				if (reconv_texture)
				{
					//fprintf(stderr, "dst = %p, src = (%p, %p (%08X))\n", raw, g_pix_src, g_pal_src, pal_ofs);
					n64texconv_to_rgba8888(
						raw
						, g_pix_src /*pix*/
						, g_pal_src /*pal*/
						, codec[g_codec_idx].fmt/*2*//*ci*/
						, codec[g_codec_idx].bpp/*1*//*8*/
						, 256
						, 256
					);
					//unsigned char *r8 = raw;
					//fprintf(stderr, "conv %02X%02X%02X%02X -> ", r8[0], r8[1], r8[2], r8[3]);
					rgba32_to_internal_array_8(raw, 256 * 256);
					//fprintf(stderr, "%02X%02X%02X%02X\n", r8[0], r8[1], r8[2], r8[3]);
				}
				reconv_texture = 0;
				
				struct wowGui_rect *r = &g_zztv.xy_scroll;
				
				for (int i=0; i < 10; ++i, ++r)
				{
					int w = r->w / 2 - BPAD;
					int h = r->h / 2 - BPAD;
					int im_w = w;
					int im_h = h;
					int scale = g_zztv.scale + 1;
					
					if (r == &g_zztv.xy_custom)
					{
						im_w = g_zztv.custom_w;
						im_h = g_zztv.custom_h;
						if (im_w > 128 || im_h > 128)
						{
							w = 0;
							h = 0;
						}
					}
					
					if (r == &g_zztv.xy_scroll)
					{
						im_w = 128;
						im_h = 508;
						scale = 1;
						w = 0;
						h = 0;
					}
					
					if (im_w > 128 || im_h > 128)
					{
						scale = 1;
					}
					
					if (r == &g_zztv.xy_custom)
						g_zztv.custom_scale = scale;
					
					if (scale == 1)
					{
						wowGui_bind_blit_raw(
							raw
							, r->x + (w) / 2 + BPAD
							, r->y + (h) / 2 + BPAD
							, im_w
							, im_h
							, codec[g_codec_idx].blend * !g_zztv.NOblend
						);
					}
					else
					{
						wowGui_bind_blit_raw_scaled(
							raw
							, r->x + BPAD
							, r->y + BPAD
							, im_w
							, im_h
							, codec[g_codec_idx].blend * !g_zztv.NOblend
							, 2
						);
					}
				}
			}
			
		/* display top row controls */
			wowGui_padding(8, 8);
			wowGui_seek_top();
			wowGui_columns(10);
			
			wowGui_column_width(8 * sizeof(PROG_NAME_VER_ATTRIB));
			wowGui_italic(2);
			wowGui_label(PROG_NAME_VER_ATTRIB);
			wowGui_italic(0);
			
			wowGui_column_width(8 * sizeof("[blend=off][v]"));
			//wowGui_label("[blend=off][v]");
			int Oblend = g_zztv.NOblend;
			static char *blend_text = 0;
			if (wowGui_dropdown(&blend_text, &g_zztv.NOblend, 16 * 8, 16 * 3))
			{
				wowGui_dropdown_item("blend=on ");
				wowGui_dropdown_item("blend=off");
				wowGui_dropdown_end();
			}
			reconv_texture |= Oblend != g_zztv.NOblend;
			
			
			/* performance test: window refresh rate customization */
		#if 0
			static int hzmagic = 5; /* 10hz is default */
			int Ohz = hzmagic;
			static char *hzText = 0;
			if (wowGui_dropdown(&hzText, &hzmagic, 16 * 4, 16 * 9))
			{
				wowGui_dropdown_item("60hz");
				wowGui_dropdown_item("50hz");
				wowGui_dropdown_item("40hz");
				wowGui_dropdown_item("30hz");
				wowGui_dropdown_item("20hz");
				wowGui_dropdown_item("10hz");
				wowGui_dropdown_end();
			}
			if (hzmagic != Ohz)
			{
				/* customizable hz */
				wowGui_bind_set_fps(60 - hzmagic * 10);
			}
			wowGui_label("[help]");
		#else
			wowGui_column_width(8);
			wowGui_label("");
		#endif
			wowGui_column_width(8 * (sizeof("[manual]")-1));
			if (wowGui_button("manual"))
				show_manual = 1;
			
			
			wowGui_row();
			wowGui_column_width(8 * 6);
			if (wowGui_button("Open"))
			{
				if (g_zztv.statusFile)
					free(g_zztv.statusFile);
				g_zztv.statusFile = 0;
				button_open();
			}
			else if (wowGui_dropped_file_last())
			{
				dropfile_open();
			}
			//wowGui_button("Insert");
			wowGui_column_width(8 * 12);
			if (wowGui_button("Save As..."))
			{
				if (!g_zztv.buf)
					wowGui_popup(
						WOWGUI_POPUP_ICON_WARN
						, WOWGUI_POPUP_OK
						, WOWGUI_POPUP_OK
						, "Error"
						, "No file has been opened, so we can't save one"
					);
				else
				{
					if (g_zztv.statusFile)
						free(g_zztv.statusFile);
					g_zztv.statusFile = 0;
					file_save_gui(g_zztv.buf, g_zztv.buf_sz);
				}
			}
			//wowGui_column_width(8 * 4);
			//wowGui_label("  | ");
			wowGui_column_width(8 * 6);
			wowGui_label("Codec:");
			wowGui_column_width(8 * 11);
			
			int Ocodec_idx = g_codec_idx;
			if (wowGui_dropdown(&g_codec_text, &g_codec_idx, 16 * 8, 16 * 16))
			{
				int i;
				for (i=0 ; i < sizeof(codec) / sizeof(codec[0]); ++i)
				{
					wowGui_dropdown_item(codec[i].name);
				}
				wowGui_dropdown_end();
			}
			reconv_texture |= g_codec_idx != Ocodec_idx;
			
			wowGui_column_width(8 * 6);
			wowGui_label("Scale:");
			static char *scale_text = 0;
			/* r1 fix: switch to wowGui_dropdown b/c scroll roll-over */
			if (
				wowGui_dropdown(
					&scale_text
					, &g_zztv.scale
					, 6 * 8
					, 4 * 16
				)
			)
			{
				wowGui_dropdown_item("1");
				wowGui_dropdown_item("2");
				wowGui_dropdown_end();
			}
			
			wowGui_window_end();
		}
		if (wowGui_window(&palpane))
		{
			unsigned char  *buf;
			unsigned int   *ofs;
			unsigned int    buf_sz;
			if (g_zztv.Epal.buf)
			{
				buf = g_zztv.Epal.buf;
				buf_sz = g_zztv.Epal.buf_sz;
				ofs = &g_zztv.Epal.ofs;
			}
			else
			{
				buf = g_zztv.buf;
				buf_sz = g_zztv.buf_sz;
				ofs = &g_zztv.pal_ofs;
			}
			if (*ofs > buf_sz)
				*ofs = buf_sz;
			
			wowGui_columns(1);
			wowGui_column_width(ppw);
			wowGui_label("CI Palette");
			
			wowGui_columns(2);
			wowGui_column_width(ppw / 4);
			wowGui_label("Offset");
			wowGui_column_width(ppw / 2);
			if (wowGui_hex_range(ofs, 0x0, buf_sz, 2))
			{
				reconv_texture |= 1;
				reconv_palette |= 1;
			}
			wowGui_column_width(ppw / 4);
			wowGui_label("Colors");
			wowGui_column_width(ppw / 2);
			wowGui_padding(16, 0);
			wowGui_row_height(4);
			if (wowGui_int_range(&g_zztv.pal_colors, 0, 256, 1))
				reconv_palette |= 1;
			wowGui_row_height(20);
			
			/* palette buffer */
			if (buf)
			{
				unsigned char *raw = palette_raw;
				
				if (reconv_palette)
				{
					/* clear to 0 first */
					memset(raw, 0, 16 * 16 * 4);
					
					n64texconv_to_rgba8888(
						raw
						, pLut
						, buf + *ofs /*pal*/
						, N64TEXCONV_CI
						, N64TEXCONV_8
						, g_zztv.pal_colors
						, 1
					);
					rgba32_to_internal_array_8(raw, g_zztv.pal_colors * 1);
				}
				reconv_palette = 0;
				
				int im_w = 16;
				int im_h = 16;
				if (g_zztv.pal_colors <= 16)
					im_w = im_h = 4;
				g_zztv.pal_sq = im_w;
				
				SET_XY(xy_palette)
				{
					INIT_POPUP(raw, g_pal_src, im_w, im_h)
					g_png_in_is_pal = 1;
				}
				else if (wowGui_dropped_file_last())
				{
					g_png_in_is_pal = 1;
					dropfile_png(g_pal_src, im_w, im_h);
				}
				RDRAG(xy_palette, 128 / im_w, ofs, buf_sz)
				
				struct wowGui_rect *r = &g_zztv.xy_palette;
				wowGui_bind_blit_raw_scaled(
					raw
					, r->x + BPAD
					, r->y + BPAD
					, im_w
					, im_h
					, WOWGUI_BLIT_ALPHATEST
					, 128 / im_w
				);
			}
			wowGui_padding(8, 8);
			
			wowGui_column_width(ppw);
			wowGui_columns(1);
			wowGui_row();
			wowGui_row();
			wowGui_row();
			wowGui_row();
			wowGui_row();
			wowGui_row();
			wowGui_row();
			wowGui_row();
			
			
			/* external palette has been loaded */
			if (g_zztv.Epal.buf)
			{
				if (wowGui_button("Close External Palette"))
				{
					/* prevents automatically re-opening extra drops */
					wowGui_dropped_file_flush();
					
					free(g_zztv.Epal.buf);
					g_zztv.Epal.buf = 0;
					reconv_palette = reconv_texture = 1;
				}
				if (wowGui_button("Save Extern File As..."))
				{
					file_save_gui(g_zztv.Epal.buf, g_zztv.Epal.buf_sz);
				}
			}
			else if (buf)
			{
				if (wowGui_button("Load External Palette"))
				{
					/* load external file here */;				
					g_zztv.Epal.buf = file_load_gui(0, 0, &g_zztv.Epal.buf_sz, 0);
					reconv_palette = reconv_texture = 1;
				}
				else if (wowGui_dropped_file_last())
				{
					g_zztv.Epal.buf =
					file_load(
						wowGui_dropped_file_name()
						, &g_zztv.Epal.buf_sz
						, 0
					);
					reconv_palette = reconv_texture = 1;
				}
				#if 0
				if (wowGui_button("progress bar test"))
				{
					g_threadfunc = threadfunc_test;
					g_progress_bar_loc = wowGui.last_clickable_rect;
				}
				#endif
			}
			if (g_zztv.Epal.buf || buf)
			{
				wowGui_checkbox("Palette Override", &g_zztv.palOR);
				wowGui_label(" (generates new palette");
				wowGui_label("  when importing PNGs)");
			}
			
			wowGui_window_end();
		}
		
		/* texture convert pane */
		if (wowGui_window(&convpane))
		{
			/* XXX hard-coded name for testing*/
			char *hcname = 0;//"arrows-icon.png";
			
			/* header */
			wowGui_column_width(ppw);
			wowGui_columns(1);
			wowGui_label("Texture Convert");
			
			/* codec select */
			wowGui_columns(2);
			wowGui_column_width(8 * 6);
			wowGui_label("Codec:");
			wowGui_column_width(8 * 11);
			static char *codec_text = 0;
			static int codec_idx = 0;
			if (wowGui_dropdown(&codec_text, &codec_idx, 16 * 8, 16 * 12))
			{
				int i;
				for (i=0 ; i < sizeof(codec) / sizeof(codec[0]); ++i)
				{
					wowGui_dropdown_item(codec[i].name);
				}
				wowGui_dropdown_end();
			}
			static int acfunc_colors = 4; /* max alpha colors */
			
			/* alpha color generation function drop-down selection */
			wowGui_columns(2);
			wowGui_column_width(ppw / 2);
			wowGui_label("AlphaColor:");
			static char *acfunc_text = 0;
			if (wowGui_dropdown(&acfunc_text, &acfunc, 16 * 8, 20 * 5))
			{
				int i;
				for (i=0 ; i < N64TEXCONV_ACGEN_MAX; ++i)
				{
					char *str;
					switch (i)
					{
						case N64TEXCONV_ACGEN_EDGEXPAND:
							str = "edge";
							break;
						case N64TEXCONV_ACGEN_AVERAGE:
							str = "average";
							break;
						case N64TEXCONV_ACGEN_BLACK:
							str = "black";
							break;
						case N64TEXCONV_ACGEN_WHITE:
							str = "white";
							break;
						case N64TEXCONV_ACGEN_USER:
							str = "image";
							break;
						default:
							str = 0;
							break;
					}
					wowGui_dropdown_item(str);
				}
				wowGui_dropdown_end();
			}
			
			/* for ci conversion modes, can set max alpha colors */
			if (codec[codec_idx].fmt == N64TEXCONV_CI || codec[g_codec_idx].fmt == N64TEXCONV_CI)
			{
				wowGui_column_width(ppw / 2 + 8 * 2);
				wowGui_label("AlphaColorMax:");
				
				int mx = 16;
				if (codec[codec_idx].bpp == N64TEXCONV_4)
					mx = 4;
				
				wowGui_column_width(ppw / 2 - 8 * 2);
				wowGui_int_range(&acfunc_colors, 1, mx, 1);
			}
			
			/* png select/drop box */
			/*wowGui_columns(2);
			wowGui_column_width(ppw / 6);
			wowGui_label("PNG:");
			wowGui_column_width(ppw - 8 * 4);*/
			nfdpathset_t nfdpaths = {0};
			nfdpathset_t *paths = 0;
			int drops = 0;
			
			/* files selected via nfd */
			wowGui_columns(1);
			wowGui_column_width(ppw);
			if (wowGui_button("New File from PNGs"))
			{
				nfdresult_t r;
				r = NFD_OpenDialogMultiple("png", 0/*TODO*/, &nfdpaths);
				if (r == NFD_OKAY)
					paths = &nfdpaths;
			}
			/* files dropped in */
			else if (wowGui_dropped_file_last())
			{
				drops = 1;
			}
			
			wowGui_column_width(ppw+8*2);
			if (notes && wowGui_button("Copy Notes to Clipboard"))
			{
				if (wowClipboard_set_FILE(notes))
					die("fatal clipboard error");
			}
			
			/* files have been dropped, or opened via nfd */
			if (drops || paths || hcname)
			{
				void *name = 0;
				void *pal = 0;
				int path_idx = 0;
				int pal_colors = 0;
				const char *errstr = 0;
				int total_invisible = 0;
				int pal_max = 0;
				FILE *fp = 0;
				unsigned int pal_override = 0;
				unsigned int ofs_override = -1;
				int w_OR = 0;
				int h_OR = 0;
				int ofs_matches = 0;
				if (notes)
					fclose(notes);
				notes = tmpfile();
				if (!notes)
					complain("failed to open temporary log file");
				
				struct n64texconv_palctx *pctx = 0;
				
				struct list *rme = 0;
				
				if (hcname)
				{
					codec_idx = 3;
					name = hcname;
				}
				
				/* must generate palette */
				if (codec[codec_idx].fmt == N64TEXCONV_CI)
				{
					/* derive max colors */
					if (codec[codec_idx].bpp == N64TEXCONV_4)
						pal_max = 16;
					else// if (codec[codec_idx].bpp == N64TEXCONV_8)
						pal_max = 256;
					
					/* where colors will be stored */
					pal = pbuf;
					
					/* allocate palette context */
					pctx = n64texconv_palette_new(
						pal_max
						, pal
						, calloc_safe
						, realloc_safe
						, free
					);
				}
				
				/* for every file in list */
				while (1)
				{
					if (hcname) name = hcname; else
					if (drops)
						name = wowGui_dropped_file_name();
					
					else
					{
						/* end of list */
						if (path_idx >= NFD_PathSet_GetCount(paths))
							break;
						
						name = NFD_PathSet_GetPath(paths, path_idx);
						path_idx += 1;
					}
					
					/* last item */
					if (!name)
						break;
					
					int n;
					int w;
					int h;
					void *png = 0;
					png = stbi_load(name, &w, &h, &n, 4);
					
					/* error checking */
					if (png_fail(png))
					{
						/* clean up file list on failure */
						if (drops)
							wowGui_dropped_file_flush();
						
						/* XXX may set later; png_fail already complains */
						errstr = 0;
						goto L_conv_cleanup;
					}
					
					/* link data into list */
					struct list *item = malloc_safe(sizeof(*item));
					item->next = rme;
					item->data = png;
					item->fn   = name;
					item->w    = w;
					item->h    = h;
					rme = item;
					
					/* add png to n64texconv palette queue */
					if (pctx)
					{
						int num_invisible;
						
						/* simplify colors first */
						/* rgba8888 -> rgba5551 -> rgba8888 */
						n64texconv_to_n64_and_back(
							png /* in-place conversion */
							, 0
							, 0
							, N64TEXCONV_RGBA
							, N64TEXCONV_16
							, w
							, h
						);
						
						/* generate alpha pixel colors */
						num_invisible =
						n64texconv_acgen(
							png
							, w
							, h
							, acfunc
							, acfunc_colors
							, calloc_safe
							, realloc_safe
							, free
							, codec[codec_idx].fmt
						);
						
						#if 0 /* just a test */
						rgba32_to_abgr32_array(png, w * h);
						stbi_write_png("edge-detect.png", w, h, 4, png, 0);
						exit(0);
						#endif
						
						/* white/black means only one invisible pixel    *
						 * color is shared across all converted textures */
						if (num_invisible &&
							(
								acfunc == N64TEXCONV_ACGEN_BLACK
								|| acfunc == N64TEXCONV_ACGEN_WHITE
							)
						)
							total_invisible = 1;
						/* otherwise, each has unique invisible pixel(s) */
						else
							total_invisible += num_invisible;
						
						if (num_invisible < 0 || total_invisible >= (pal_max/2))
						{
							errstr =
							"ran out of alpha colors "
							"(change alpha color mode or use fewer textures)";
							goto L_conv_cleanup;
						}
						
						/* make all the xlu pixels have slightly different color */
						//color_array_xlu(png, w * h);
						
						/* add image to queue */
						n64texconv_palette_queue(pctx, png, w, h, 0);
					}
					
					/* do acgen */
					else
					{
						n64texconv_acgen(
							png
							, w
							, h
							, acfunc
							, 0
							, calloc_safe
							, realloc_safe
							, free
							, codec[codec_idx].fmt
						);
					}
				}
				
				/* quantize images */
				if (pctx)
				{
					n64texconv_palette_alpha(pctx, total_invisible);
					pal_colors = n64texconv_palette_exec(pctx);
				}
				
				/* textures generated successfully */
				if (rme)
				{
					fp = tmpfile();
					unsigned char zeroes[8] = {0};
					unsigned int pal_bytes = 0;
					
					if (!fp)
					{
						errstr = "failed to open temporary output file";
						goto L_conv_cleanup;
					}
					
					/* a palette is involved */
					if (pal)
					{
						/* convert palette colors (rgba8888 -> rgba5551) */
						n64texconv_to_n64(
							pal /* in-place conversion */
							, pal
							, 0
							, 0
							, N64TEXCONV_RGBA
							, N64TEXCONV_16
							, pal_colors
							, 1
							, &pal_bytes
						);
					}
					
					/* convert every texture */
					for (struct list *item = rme; item; item = item->next)
						n64texconv_to_n64(
							item->data /* in-place conversion */
							, item->data
							, pal
							, pal_colors
							, codec[codec_idx].fmt
							, codec[codec_idx].bpp
							, item->w
							, item->h
							, &item->sz
						);
					
					/* watermark ci texture data*/
					watermark(pal, pal_bytes, pal_max, rme);
						
					/* write file */
					if (fp)
					{
						/* palette goes first */
						if (pal)
						{
							pal_override = 0;
							
							/* write palette to file */
							fwrite(pal, 1, pal_bytes, fp);
							
							/* notes generation */
							if (notes)
								fprintf(
									notes
									, "%08X : palette"ENDLINE
									, pal_override
								);
						}
						
						/* write each texture */
						struct list *item;
						for (item = rme; item; item = item->next)
						{
							unsigned int ofs = ftell(fp);
							
							/* offset override */
							if (ofs_override & 3)
							{
								ofs_override = ofs;
								w_OR = item->w;
								h_OR = item->h;
							}
							if (g_zztv.offset == ofs)
								ofs_matches = 1;
							
							/* ensure texture offset is aligned */
							if (ofs & 7)
								fwrite(zeroes, 1, 8 - (ofs & 7), fp);
							ofs = ftell(fp);
							
							/* write data to file */
							fwrite(item->data, 1, item->sz, fp);
							
							/* do something with ofs, notes generation? */
							if (notes)
								fprintf(
									notes
									, "%08X : %s"ENDLINE
									, ofs
									, fn_only(item->fn)
								);
						}
					}
				}
				
				L_conv_cleanup: do{}while(0);
				
				/* free n64texconv palette context */
				if (pctx)
					n64texconv_palette_free(pctx);
				
				/* free texture list */
				if (rme)
				{
					/* step through every texture */
					void *next;
					while (rme)
					{
						next = rme->next;
						
						/* free allocated texture/image data */
						stbi_image_free(rme->data);
						free(rme);
						
						/* go to next in list */
						rme = next;
					}
				}
				
				/* nfd paths cleanup */
				if (paths)
					NFD_PathSet_Free(paths);
				
				if (errstr)
				{
					if (notes)
					{
						fclose(notes);
						notes = 0;
					}
					complain(errstr);
				}
				
				/* load the file that was just converted */
				else if (fp)
				{
					/* load this file */
					use_FILE(fp);
					g_zztv.statusbar = "unsaved new file";
					if (g_zztv.statusFile)
						free(g_zztv.statusFile);
					g_zztv.statusFile = 0;
					
					/* free external palette if there is one */
					if (g_zztv.Epal.buf)
					{
						free(g_zztv.Epal.buf);
						memset(&g_zztv.Epal, 0, sizeof(g_zztv.Epal));
					}
					
					/* ensure palette is at 0 if ci texture */
					if (codec[codec_idx].fmt == N64TEXCONV_CI)
					{
						g_zztv.pal_ofs = pal_override;
					}
					
					/* override codec */
					g_codec_idx = codec_idx;
					g_codec_text = 0;
					if (!(ofs_override&3))
					{
						g_zztv.custom_w = w_OR;
						g_zztv.custom_h = h_OR;
					}
					if (!ofs_matches)
						g_zztv.offset = ofs_override;
				}
					
				if (fp)
					fclose(fp);
			}
			
			wowGui_window_end();
		}
		if (wowGui_window(&searchpane))
		{
			int w = 0;
			int h = 0;
			void *png = 0;
			struct search_results *use_results = 0;
			
			/* no file loaded yet */
			if (!g_zztv.buf)
				goto L_textureSearchWindowEnd;
			
			/* header */
			wowGui_column_width(ppw);
			wowGui_columns(1);
			wowGui_label("Texture Search");
			
#if 1
			/* codec select */
			wowGui_columns(2);
			wowGui_column_width(8 * 6);
			wowGui_label("Codec:");
			wowGui_column_width(ppw);
			if (wowGui_dropdown(&search_codec_text, &search_codec_idx, 16 * 8, 16 * 12))
			{
				wowGui_dropdown_item("auto");
				wowGui_dropdown_item("all (slow!)");
				int i;
				for (i=0 ; i < sizeof(codec) / sizeof(codec[0]); ++i)
				{
					wowGui_dropdown_item(codec[i].name);
				}
				wowGui_dropdown_end();
			}
			
			/* mode select */
			wowGui_columns(2);
			wowGui_column_width(8 * 6);
			wowGui_label("Mode:");
			wowGui_column_width(ppw);
			if (wowGui_dropdown(&searchMode_text, &searchMode_idx, 16 * 12, 16 * 4))
			{
				wowGui_dropdown_item("1st match");
				wowGui_dropdown_item("find all (slow!)");
				wowGui_dropdown_end();
			}
#endif
			
			/* png select/drop box */
			wowGui_columns(1);
			wowGui_column_width(ppw);
			if (wowGui_button("Drop or Open PNG here"))
			{
				png = load_png_gui(&w, &h);
			}
			else if (wowGui_dropped_file_last())
			{
				int n;
				png = stbi_load(wowGui_dropped_file_name(), &w, &h, &n, 4);
				
				/* error checking */
				png_fail(png);
				
				g_free_results = 1;
			}
			
			/* search results are in */
			if (threadfunc_search_results)
			{
				wowGui_column_width(ppw);
				
				static char *ddtext = 0;
				static int ddidx = 0;
				int Oddidx = ddidx;
				
				if (wowGui_dropdown(&ddtext, &ddidx, 16 * 12, 16 * 3))
				{
					struct search_results *item = threadfunc_search_results;
					int i = 0;
					for (item = threadfunc_search_results; item; item = item->next)
					{
						static char ofs[64];
						static char ofs2[64];
						char partial[16] = {0};
						if (item->is_partial)
							strcpy(partial, "-P");
						sprintf(
							ofs2
							, "%d  %08X (%s%s)"
							, i
							, item->ofs
							, codec[item->codec].name
							, partial
						);
						wowGui_dropdown_item(ofs2);
						if (i == ddidx)
						{
							strcpy(ofs, ofs2);
							ddtext = ofs;
						}
						i++;
					}
					wowGui_dropdown_end();
				}
				
				/* dropdown item changed */
				if (ddidx != Oddidx)
				{
					int c = 0;
					struct search_results *item = threadfunc_search_results;
					for (item = threadfunc_search_results; item; item = item->next)
					{
						if (c == ddidx)
							break;
						c += 1;
					}
					
					if (item)
						use_results = item;
				}
				
				/* create tmpfile, propagate, load, close */
				if (wowGui_button("Results -> Clipboard"))
				{
					FILE *log = tmpfile();
					if (!log)
						complain("could not create temporary file");
					else
					{
						struct search_results *item = threadfunc_search_results;
						int display_pal = 0;
						const char *ptext = "palette";
						
						for (item = threadfunc_search_results; item; item = item->next)
							if (codec[item->codec].fmt == N64TEXCONV_CI)
							{
								if (item->Epal)
									ptext = "palette (external)";
								display_pal = 1;
							}
						
						if (display_pal)
						{
							fprintf(
								log
								, "%-6s  %-8s  %-8s"ENDLINE
								, "format"
								, "offset"
								, ptext
							);
						}
						else
							fprintf(
								log
								, "%-6s  %-8s"ENDLINE
								, "format"
								, "offset"
							);
						
						for (item = threadfunc_search_results; item; item = item->next)
						{
							if (codec[item->codec].fmt == N64TEXCONV_CI)
							{
								if (item->is_partial)
								{
									char partial[16];
									strcpy(partial, codec[item->codec].name);
									strcat(partial, "-P");
									fprintf(
										log
										, "%-6s  %08X  ????????"ENDLINE
										, partial
										, item->ofs
									);
								}
								else
									fprintf(
										log
										, "%-6s  %08X  %08X"ENDLINE
										, codec[item->codec].name
										, item->ofs
										, item->pal
									);
							}
							else
								fprintf(
									log
									, "%-6s  %08X"ENDLINE
									, codec[item->codec].name
									, item->ofs
								);
						}
						if (wowClipboard_set_FILE(log))
							die("fatal clipboard error");
						fclose(log);
					}
				}
				
				if (g_free_results)
				{
					void *next;
					while (threadfunc_search_results)
					{
						next = threadfunc_search_results->next;
						
						/* if override gets freed, invalidate pointer */
						if (threadfunc_search_results == use_results)
							use_results = 0;
						
						free(threadfunc_search_results);
						threadfunc_search_results = next;
					}
					ddidx = 0;
					ddtext = 0;
				}
			}
			g_free_results = 0;
			
			/* convert png to n64 format if loaded successfully */
			if (png)
			{
				search_png = png;
				search_png_w = w;
				search_png_h = h;
				
				//fprintf(stderr, "grayscale = %d\n", grayscale);
				//fprintf(stderr, "colors = %d\n", colors);
				
				/* now we hand things over to threadfunc; don't worry, *
				 * it handles cleanup as well                          */
				g_threadfunc = threadfunc_search;
				g_progress_bar_loc = wowGui.last_clickable_rect;
			}
			
			/* override with search results */
			if (use_results)
				search_use_results(use_results, w, h);
			
L_textureSearchWindowEnd:
			wowGui_window_end();
		}
		if (wowGui_window(&statusbar))
		{
			statusbar.scroll.y = -6;
			wowGui_columns(2);
			wowGui_column_width(statusbar.rect.w - 8 * 8);
			
			/* status bar override */
			if (g_zztv.statusFile)
			{
				wowGui_label(g_zztv.statusFile);
			}
			else if (g_zztv.statusbar)
			{
				wowGui_label(g_zztv.statusbar);
			}
			char *toggle[] = { "EOF=00", "EOF=FF" };
			if (wowGui_clickable(toggle[eof_mode]))
			{
				reconv_texture = 1;
				reconv_palette = 1;
				eof_mode = !eof_mode;
				if (g_zztv.buf)
				{
					/* clear end-padding */
					memset(g_zztv.buf + g_zztv.buf_sz, -eof_mode, ENDFILEPAD);
				}
				
				if (g_zztv.Epal.buf)
				{
					/* clear end-padding */
					memset(g_zztv.Epal.buf + g_zztv.Epal.buf_sz, -eof_mode, ENDFILEPAD);
				}
			}
			wowGui_window_end();
		}
		} /* !popup */
		
		}
			
		/* redraw the UI if any reconversion conditions are met */
		if (reconv_palette || reconv_texture)
			wowGui_redrawIncrement();
		
		wowGui_frame_end(wowGui_bind_ms());
		
		/* display */
		wowGui_bind_result();
		
		if (g_threadfunc)
		{
			wowGui_wait_func(
				g_threadfunc
				, die
				, wowGui_bind_events
				, wowGui_bind_result
				, wowGui_bind_ms
				, &g_progress_bar_loc
			);
			g_threadfunc = 0;
		}
		
		/* loop exit condition */
		if (wowGui_bind_endmainloop())
			break;
	}
	
	wowGui_bind_quit();
	if (g_zztv.buf)
		free(g_zztv.buf);
	if (notes)
		fclose(notes);
	free(raw);
	free(palette_raw);
	free(pLut);
	free(pbuf);
	
	return 0;
}

