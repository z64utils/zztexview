/*
 * search.c <z64.me>
 *
 * texture searching functions
 * pattern matching magic is used for locating color-indexed images
 *
 */

#include "search.h"

#include <string.h> /* memcmp */

int valNum_descend(const void *aa, const void *bb)
{
	const struct valNum *a = aa;
	const struct valNum *b = bb;
	
	return (a->num > b->num) ? -1 : (a->num < b->num) ? 1 : 0;
}

int valNum_ascend(const void *aa, const void *bb)
{
	const struct valNum *a = aa;
	const struct valNum *b = bb;
	
	return (a->num > b->num) ? 1 : (a->num < b->num) ? -1 : 0;
}

int valNum_ascend_val(const void *aa, const void *bb)
{
	const struct valNum *a = aa;
	const struct valNum *b = bb;
	
	return (a->val > b->val) ? 1 : (a->val < b->val) ? -1 : 0;
}

unsigned char *
match5551_color(
	  unsigned char *hay, unsigned int hay_sz
	, unsigned char *ndl, unsigned int ndl_sz
	, int            stride
	, unsigned char *Lmat
)
{
	/* Lmat points to point within hay to start search, or 0 */
	if (Lmat)
	{
		hay_sz -= Lmat - hay;
		hay = Lmat;
	}
	
	while (ndl_sz <= hay_sz)
	{
		int i;
		for (i = 0; i < ndl_sz; i += 2)
		{
			/* skip invisible pixels */
			if (!(ndl[i+1]&1))
				continue;
			
			/* colors don't match */
			if (hay[i] != ndl[i] || hay[i+1] != ndl[i+1])
			{
			#if 0
				fprintf(
					stderr
					, "[%2x] %02X%02X != %02X%02X\n"
					, i / 2
					, hay[i], hay[i+1]
					, ndl[i], ndl[i+1]
				);
			#endif
				goto L_next;
			}
		}
		/* matches */
		if (i == ndl_sz)
			return hay;
L_next:
		hay += stride;
		hay_sz -= stride;
	}
	
	return 0;
}

/* ensures alpha bit matches; ensures colors match on visible pixels */
unsigned char *
match5551_color_abit(
	  unsigned char *hay, unsigned int hay_sz
	, unsigned char *ndl, unsigned int ndl_sz
	, int            stride
	, unsigned char *Lmat
)
{
	/* Lmat points to point within hay to start search, or 0 */
	if (Lmat)
	{
		hay_sz -= Lmat - hay;
		hay = Lmat;
	}
	
	while (ndl_sz <= hay_sz)
	{
		int i;
		for (i = 0; i < ndl_sz; i += 2)
		{
			/* skip invisible pixels */
			if (!(ndl[i+1]&1))
			{
				/* alpha bits don't match */
				if (hay[i+1]&1)
					goto L_next;
				
				continue;
			}
			
			/* colors don't match */
			if (hay[i] != ndl[i] || hay[i+1] != ndl[i+1])
				goto L_next;
		}
		/* matches */
		if (i == ndl_sz)
			return hay;
L_next:
		hay += stride;
		hay_sz -= stride;
	}
	
	return 0;
}

void *
matchBlock_stride(
	  unsigned char *hay, unsigned int hay_sz
	, unsigned char *ndl, unsigned int ndl_sz
	, int            stride
	, unsigned char *Lmat
)
{
	/* Lmat points to point within hay to start search, or 0 */
	if (Lmat)
	{
		hay_sz -= Lmat - hay;
		hay = Lmat;
	}
	
	while (ndl_sz <= hay_sz)
	{
		if (!memcmp(hay, ndl, ndl_sz))
			return hay;
		hay += stride;
		hay_sz -= stride;
	}
	
	return 0;
}

/* match color channel only */
unsigned char *
match44_color(
	  unsigned char *hay, unsigned int hay_sz
	, unsigned char *ndl, unsigned int ndl_sz
	, int            stride
	, unsigned char *Lmat
)
{
	/* Lmat points to point within hay to start search, or 0 */
	if (Lmat)
	{
		hay_sz -= Lmat - hay;
		hay = Lmat;
	}
	
	while (ndl_sz <= hay_sz)
	{
		int i;
		for (i = 0; i < ndl_sz; i += 1)
		{
			/* skip invisible pixels */
			if (!(ndl[i]&0xF))
				continue;
			
			/* colors don't match */
			if ((hay[i]&0xF0) != (ndl[i]&0xF0))
				goto L_next;
		}
		/* matches */
		if (i == ndl_sz)
			return hay;
L_next:
		hay += stride;
		hay_sz -= stride;
	}
	
	return 0;
}

/* match color channel and alpha channel */
unsigned char *
match44_color_abits(
	  unsigned char *hay, unsigned int hay_sz
	, unsigned char *ndl, unsigned int ndl_sz
	, int            stride
	, unsigned char *Lmat
)
{
	/* Lmat points to point within hay to start search, or 0 */
	if (Lmat)
	{
		hay_sz -= Lmat - hay;
		hay = Lmat;
	}
	
	while (ndl_sz <= hay_sz)
	{
		int i;
		for (i = 0; i < ndl_sz; i += 1)
		{
			/* alpha doesn't match */
			if ((hay[i]&0xF) != (ndl[i]&0xF))
				goto L_next;
			
			/* skip invisible pixels */
			if (!(ndl[i]&0xF))
				continue;
			
			/* colors don't match */
			if ((hay[i]&0xF0) != (ndl[i]&0xF0))
				goto L_next;
		}
		/* matches */
		if (i == ndl_sz)
			return hay;
L_next:
		hay += stride;
		hay_sz -= stride;
	}
	
	return 0;
}

/* test colors when alpha bit == 1, ensure alpha bit == 0 otherwise */
unsigned char *
match31_color_abit(
	  unsigned char *hay, unsigned int hay_sz
	, unsigned char *ndl, unsigned int ndl_sz
	, int            stride
	, unsigned char *Lmat
)
{
	/* Lmat points to point within hay to start search, or 0 */
	if (Lmat)
	{
		hay_sz -= Lmat - hay;
		hay = Lmat;
	}
	
	while (ndl_sz <= hay_sz)
	{
		int i;
		for (i = 0; i < ndl_sz; i += 1)
		{
#if 0 /* this precision loss breaks MM's grayscale trees */
	/* NOTE: this operates on 2 pixels at a time, *
	 *       so some precision is lost            */
			
			/* alpha doesn't match */
			if ((hay[i]&0x11) != (ndl[i]&0x11))
				goto L_next;
			
			/* skip invisible pixels */
			if (!(ndl[i]&0x11))
				continue;
			
			/* colors don't match */
			if ((hay[i]&0xEE) != (ndl[i]&0xEE))
				goto L_next;
#else
			/* alpha doesn't match */
			if ((hay[i]&0x11) != (ndl[i]&0x11))
				goto L_next;
			
			/* visible pixel, color doesn't match */
			if ((ndl[i]&0x10) && ((ndl[i]&0xE0) != (hay[i]&0xE0)))
				goto L_next;
			
			/* visible pixel, color doesn't match */
			if ((ndl[i]&0x1) && ((ndl[i]&0xE) != (hay[i]&0xE)))
				goto L_next;
#endif
		}
		/* matches */
		if (i == ndl_sz)
			return hay;
L_next:
		hay += stride;
		hay_sz -= stride;
	}
	
	return 0;
}

/* test on colors only */
unsigned char *
match31_color(
	  unsigned char *hay, unsigned int hay_sz
	, unsigned char *ndl, unsigned int ndl_sz
	, int            stride
	, unsigned char *Lmat
)
{
	/* Lmat points to point within hay to start search, or 0 */
	if (Lmat)
	{
		hay_sz -= Lmat - hay;
		hay = Lmat;
	}
	
	while (ndl_sz <= hay_sz)
	{
		int i;
		for (i = 0; i < ndl_sz; i += 1)
		{
			/* NOTE: this operates on 2 pixels at a time, *
			 *       so some precision is lost            */
			/* skip invisible pixels */
			if (!(ndl[i]&0x11))
				continue;
			
			/* colors don't match */
			if ((hay[i]&0xEE) != (ndl[i]&0xEE))
				goto L_next;
		}
		/* matches */
		if (i == ndl_sz)
			return hay;
L_next:
		hay += stride;
		hay_sz -= stride;
	}
	
	return 0;
}

unsigned char *
match88_color(
	  unsigned char *hay, unsigned int hay_sz
	, unsigned char *ndl, unsigned int ndl_sz
	, int            stride
	, unsigned char *Lmat
)
{
	/* Lmat points to point within hay to start search, or 0 */
	if (Lmat)
	{
		hay_sz -= Lmat - hay;
		hay = Lmat;
	}
	
	while (ndl_sz <= hay_sz)
	{
		int i;
		for (i = 0; i < ndl_sz; i += 2)
		{
			/* skip invisible pixels */
			if (!ndl[i+1])
				continue;
			
			/* colors don't match */
			if (hay[i] != ndl[i])
				goto L_next;
		}
		/* matches */
		if (i == ndl_sz)
			return hay;
L_next:
		hay += stride;
		hay_sz -= stride;
	}
	
	return 0;
}

/* matches color when abits == 0, otherwise matches abits */
unsigned char *
match88_color_abits(
	  unsigned char *hay, unsigned int hay_sz
	, unsigned char *ndl, unsigned int ndl_sz
	, int            stride
	, unsigned char *Lmat
)
{
	/* Lmat points to point within hay to start search, or 0 */
	if (Lmat)
	{
		hay_sz -= Lmat - hay;
		hay = Lmat;
	}
	
	while (ndl_sz <= hay_sz)
	{
		int i;
		for (i = 0; i < ndl_sz; i += 2)
		{
			/* alphas aren't within 2 units of each other */
			/*int dist = hay[i+1];
			dist -= ndl[i+1];
			if (dist < 0)
				dist *= -1;
			if (dist > 2)*/
			if (hay[i+1] != ndl[i+1])
				goto L_next;
			
			/* when alpha != 0, ensure colors match */
			if (ndl[i+1] && hay[i] != ndl[i])
				goto L_next;
		}
		/* matches */
		if (i == ndl_sz)
			return hay;
L_next:
		hay += stride;
		hay_sz -= stride;
	}
	
	return 0;
}

unsigned char *
match88_alpha(
	  unsigned char *hay, unsigned int hay_sz
	, unsigned char *ndl, unsigned int ndl_sz
	, int            stride
	, unsigned char *Lmat
)
{
	/* Lmat points to point within hay to start search, or 0 */
	if (Lmat)
	{
		hay_sz -= Lmat - hay;
		hay = Lmat;
	}
	
	while (ndl_sz <= hay_sz)
	{
		int i;
		for (i = 0; i < ndl_sz; i += 2)
		{
			/* alphas aren't within 2 units of each other */
			/*int dist = hay[i+1];
			dist -= ndl[i+1];
			if (dist < 0)
				dist *= -1;
			if (dist > 2)*/
			if (hay[i+1] != ndl[i+1])
				goto L_next;
		}
		/* matches */
		if (i == ndl_sz)
			return hay;
L_next:
		hay += stride;
		hay_sz -= stride;
	}
	
	return 0;
}

unsigned char *
match8888_color(
	  unsigned char *hay, unsigned int hay_sz
	, unsigned char *ndl, unsigned int ndl_sz
	, int            stride
	, unsigned char *Lmat
)
{
	/* Lmat points to point within hay to start search, or 0 */
	if (Lmat)
	{
		hay_sz -= Lmat - hay;
		hay = Lmat;
	}
	
	while (ndl_sz <= hay_sz)
	{
		int i;
		for (i = 0; i < ndl_sz; i += 4)
		{
			/* skip invisible pixels */
			if (!ndl[i+3])
				continue;
			
			/* colors don't match */
			if (hay[i] != ndl[i] || hay[i+1] != ndl[i+1] || hay[i+2] != ndl[i+2])
				goto L_next;
		}
		/* matches */
		if (i == ndl_sz)
			return hay;
L_next:
		hay += stride;
		hay_sz -= stride;
	}
	
	return 0;
}

unsigned char *
match8(
	  unsigned char *hay, unsigned int hay_sz
	, unsigned char *ndl, unsigned int ndl_sz
	, unsigned char  ndl_delim
	, int            stride
	, unsigned char *Lmat
)
{
	/* Lmat points to point within hay to start search, or 0 */
	if (Lmat)
	{
		hay_sz -= Lmat - hay;
		hay = Lmat;
	}
	while (ndl_sz <= hay_sz)
	{
		int i;
		int hay_delim = -1;
		for (i = 0; i < ndl_sz; ++i)
		{
			if (ndl[i] == ndl_delim)
			{
				/* first match */
				if (hay_delim < 0)
					hay_delim = hay[i];
				/* doesn't match */
				else if (hay[i] != hay_delim)
					goto L_next;
			}
			else if (hay[i] == hay_delim)
			{
				/* matches when it shouldn't */
				goto L_next;
			}
		}
		/* matches */
		if (i == ndl_sz)
			return hay;
L_next:
		hay += stride;
		hay_sz -= stride;
	}
	
	return 0;
}

/* convert 4bpp data to 8bpp data */
void
match_cvt_4to8(unsigned char *dst, unsigned char *src, unsigned bytes)
{
	int alt = 1;
	src += bytes;
	bytes *= 2;
	dst += bytes;
	
	while (bytes--)
	{
		src -= alt & 1;
		dst -= 1;
		
		if (!(alt&1))
			*dst = (*src)>>4;
		else
			*dst = (*src)&0xF;
		alt += 1;
	}
}

/* match 8bit data inside 4bit haystack */
unsigned char *
match4_8(
	  unsigned char *hay, unsigned int hay_sz, unsigned char *hay_buf
	, unsigned char *ndl, unsigned int ndl_sz
	, unsigned char  ndl_delim
	, int            stride
	, unsigned char *Lmat
)
{
	void *mat;
	unsigned sz4 = ndl_sz / 2;
	unsigned sz = ndl_sz;
	
	/* Lmat points to point within hay to start search, or 0 */
	if (Lmat)
	{
		hay_sz -= Lmat - hay;
		hay = Lmat;
	}
	
	while (ndl_sz <= hay_sz)
	{
		/* convert current hay from 4bpp to 8bpp */
		match_cvt_4to8(hay_buf, hay, sz4);
		
		/* does this block match pattern */
		if ((mat = match8(hay_buf, sz, ndl, sz, ndl_delim, 1, 0)))
			return mat;
		
		hay += stride;
		hay_sz -= stride;
	}
	
	return 0;
}

unsigned char *
match4(
	  unsigned char *hay, unsigned int hay_sz, unsigned char *hay_buf
	, unsigned char *ndl, unsigned int ndl_sz, unsigned char *ndl_buf
	, unsigned char  ndl_delim
	, int            stride
	, unsigned char *Lmat
)
{
	unsigned sz = ndl_sz * 2;
	
	/* convert ndl from 4bpp to 8bpp */
	match_cvt_4to8(ndl_buf, ndl, ndl_sz);
	
	return match4_8(
		hay, hay_sz, hay_buf
		, ndl_buf, sz
		, ndl_delim
		, stride
		, Lmat
	);
}

