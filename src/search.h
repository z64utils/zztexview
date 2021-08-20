/*
 * search.h <z64.me>
 *
 * texture searching functions
 * pattern matching magic is used for locating color-indexed images
 *
 */

struct valNum
{
	int  val;
	int  num;
	int  most;
};

int valNum_descend(const void *aa, const void *bb);

int valNum_ascend(const void *aa, const void *bb);

int valNum_ascend_val(const void *aa, const void *bb);

unsigned char *
match5551_color(
	  unsigned char *hay, unsigned int hay_sz
	, unsigned char *ndl, unsigned int ndl_sz
	, int            stride
	, unsigned char *Lmat
);

unsigned char *
match5551_color_abit(
	  unsigned char *hay, unsigned int hay_sz
	, unsigned char *ndl, unsigned int ndl_sz
	, int            stride
	, unsigned char *Lmat
);

void *
matchBlock_stride(
	  unsigned char *hay, unsigned int hay_sz
	, unsigned char *ndl, unsigned int ndl_sz
	, int            stride
	, unsigned char *Lmat
);

unsigned char *
match44_color(
	  unsigned char *hay, unsigned int hay_sz
	, unsigned char *ndl, unsigned int ndl_sz
	, int            stride
	, unsigned char *Lmat
);

unsigned char *
match44_color_abits(
	  unsigned char *hay, unsigned int hay_sz
	, unsigned char *ndl, unsigned int ndl_sz
	, int            stride
	, unsigned char *Lmat
);

unsigned char *
match31_color(
	  unsigned char *hay, unsigned int hay_sz
	, unsigned char *ndl, unsigned int ndl_sz
	, int            stride
	, unsigned char *Lmat
);

unsigned char *
match31_color_abit(
	  unsigned char *hay, unsigned int hay_sz
	, unsigned char *ndl, unsigned int ndl_sz
	, int            stride
	, unsigned char *Lmat
);

unsigned char *
match88_color(
	  unsigned char *hay, unsigned int hay_sz
	, unsigned char *ndl, unsigned int ndl_sz
	, int            stride
	, unsigned char *Lmat
);

unsigned char *
match88_alpha(
	  unsigned char *hay, unsigned int hay_sz
	, unsigned char *ndl, unsigned int ndl_sz
	, int            stride
	, unsigned char *Lmat
);

unsigned char *
match88_color_abits(
	  unsigned char *hay, unsigned int hay_sz
	, unsigned char *ndl, unsigned int ndl_sz
	, int            stride
	, unsigned char *Lmat
);

unsigned char *
match8888_color(
	  unsigned char *hay, unsigned int hay_sz
	, unsigned char *ndl, unsigned int ndl_sz
	, int            stride
	, unsigned char *Lmat
);

unsigned char *
match8(
	  unsigned char *hay, unsigned int hay_sz
	, unsigned char *ndl, unsigned int ndl_sz
	, unsigned char  ndl_delim
	, int            stride
	, unsigned char *Lmat
);

/* convert 4bpp data to 8bpp data */
void
match_cvt_4to8(unsigned char *dst, unsigned char *src, unsigned bytes);

/* match 8bit data inside 4bit haystack */
unsigned char *
match4_8(
	  unsigned char *hay, unsigned int hay_sz, unsigned char *hay_buf
	, unsigned char *ndl, unsigned int ndl_sz
	, unsigned char  ndl_delim
	, int            stride
	, unsigned char *Lmat
);

unsigned char *
match4(
	  unsigned char *hay, unsigned int hay_sz, unsigned char *hay_buf
	, unsigned char *ndl, unsigned int ndl_sz, unsigned char *ndl_buf
	, unsigned char  ndl_delim
	, int            stride
	, unsigned char *Lmat
);

