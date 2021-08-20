/*
 * threadfunc_search.h <z64.me>
 *
 * spaghetti code wrapper for fast threaded texture searching
 *
 */

struct search_results
{
	void          *next;
	unsigned int   ofs;
	unsigned int   pal;
	int            codec;
	int            is_partial;  /* partial ci match */
	int            Epal;        /* uses external palette */
};
static struct search_results *threadfunc_search_results = 0;


static char *searchMode_text = 0;
static int searchMode_idx = 0;
static const int searchMode_idx_1st = 0;

static void *search_png = 0;
static int search_png_w = 0;
static int search_png_h = 0;

static char *search_codec_text = 0;
static int search_codec_idx = 0;

static
void
search_use_results(struct search_results *use_results, int w, int h)
{
	struct search_results *item = use_results;
	g_zztv.offset = item->ofs;
	g_zztv.pal_ofs = item->pal;
	g_zztv.Epal.ofs = item->pal;
	g_codec_idx = item->codec;
	g_codec_text = 0;
	reconv_palette = reconv_texture = 1;
	wowGui_redrawIncrement();
	
	/* set custom_w and custom_h to texture size */
	if (w)
		g_zztv.custom_w = w;
	if (h)
		g_zztv.custom_h = h;
	
	/* force 16 or 256 colors on ci4 and ci8 respectively */
	if (codec[g_codec_idx].fmt == N64TEXCONV_CI)
		g_zztv.pal_colors =
			(codec[g_codec_idx].bpp == N64TEXCONV_4)
			? 16
			: 256
		;
	
	if (codec[g_codec_idx].fmt == N64TEXCONV_CI)
		fprintf(
			stderr
			, "%-6s  %08X  %08X\n"
			, codec[g_codec_idx].name
			, item->ofs
			, item->pal
		);
	
	else
		fprintf(
			stderr
			, "%-6s  %08X\n"
			, codec[g_codec_idx].name
			, item->ofs
		);
}

static
void *
threadfunc_search(void *progress_float)
{
	float *progress = progress_float;
	*progress = 0;
	void *png = search_png;
	int w = search_png_w;
	int h = search_png_h;
	
	void *png_copy = malloc_safe(w * h * 4);
	int i;
	memcpy(png_copy, png, w * h * 4);
	
	struct search_results *use_results = 0;
				
	/* buf8 is just buf expanded from 4bpp to 8bpp */
	unsigned char *buf8 = g_zztv.buf + g_zztv.buf_sz + ENDFILEPAD;
	unsigned int   buf8sz = g_zztv.buf_sz * 2;
	
	/* has not expanded buffer from 4bit to 8bit yet */
	/* TODO do this only if user wants 4bit search */
	if (!g_zztv.has_exp)
	{
		unsigned char *src = g_zztv.buf;
		match_cvt_4to8(buf8, src, g_zztv.buf_sz);
		g_zztv.has_exp = 1;
	}
	
	int grayscale = 1;
	int multibit_alpha = 0;
	int colors = 0;
	int alphashades = 0;
	/* count up to 257 colors */
	{uint32_t *png32 = png;
	uint32_t color_arr[256];
	for (unsigned int i = 0; i < w * h; ++i, ++png32)
	{
		int k;
		for (k = 0; k < colors; ++k)
			if (color_arr[k] == *png32)
				break;
		if (k == colors)
		{
			++colors;
			if (k >= 256)
				break;
			color_arr[k] = *png32;
		}
	}}
	/* check grayscale */
	{unsigned char *png8 = png;
	for (unsigned int i = 0; i < w * h; i++, png8 += 4)
	{
		/* skip invisible pixels */
		if (png8[3] == 0)
			continue;
		
		/* rgb channels must all contain the same value */
		if (png8[0] != png8[1] || png8[0] != png8[2])
		{
			grayscale = 0;
			break;
		}
	}
	}
	/* count up to 257 alpha shades */
	if (grayscale)
	{
		unsigned char *png8 = png;
		unsigned char  shade[256];
		for (unsigned int i = 0; i < w * h; ++i, png8 += 4)
		{
			int k;
			for (k = 0; k < alphashades; ++k)
				if (shade[k] == png8[3])
					break;
			if (k == alphashades)
			{
				++alphashades;
				if (k >= 256)
					break;
				shade[k] = png8[3];
			}
		}
	}
	/* check multibit alpha */
	{unsigned char *png8 = png;
	for (unsigned int i = 0; i < w * h; i++, png8 += 4)
	{
		if (png8[3] > 0 && png8[3] < 0xFF)
		{
			multibit_alpha = 1;
			break;
		}
	}
	}
	
	/* enable or disable search on each codec */
	for (i = 0; i < (sizeof(codec)/sizeof(codec[0])); ++i)
	{
		/* auto */
		/* search only certain formats */
		if (search_codec_idx == 0)
		{
			/* every codec is searchable by default */
			codec[i].search_enabled = 1;
			
			/* skip I and IA if not grayscale */
			if (
				(codec[i].fmt == N64TEXCONV_I
				|| codec[i].fmt == N64TEXCONV_IA
				)
				&& !grayscale
			)
				codec[i].search_enabled = 0;
			
			/* skip CI, rgba-non32, IA4 if multibit alpha */
			if ((codec[i].fmt == N64TEXCONV_CI
				|| (codec[i].fmt == N64TEXCONV_RGBA
					&& codec[i].bpp != N64TEXCONV_32
					)
				|| (codec[i].fmt == N64TEXCONV_IA
					&& codec[i].bpp == N64TEXCONV_4
					)
				)
				&& multibit_alpha
			)
				codec[i].search_enabled = 0;
			
			/* skip IA8 if more than 16 shades of alpha */
			if (codec[i].fmt == N64TEXCONV_IA
				&& codec[i].bpp == N64TEXCONV_8
				&& alphashades > 16
			)
				codec[i].search_enabled = 0;
			
			/* skip CI if color limits are exceeded */
			if (codec[i].fmt == N64TEXCONV_CI)
			{
				int max_colors = 256;
				if (codec[i].bpp == N64TEXCONV_4)
					max_colors = 16;
				
				if (colors > max_colors)
					codec[i].search_enabled = 0;
			}
			
			/* skip CI and RGBA if grayscale */
			if ((codec[i].fmt == N64TEXCONV_CI
				|| codec[i].fmt == N64TEXCONV_RGBA
				) && grayscale
			)
				codec[i].search_enabled = 0;
		}
		
		/* all */
		else if (search_codec_idx == 1)
			codec[i].search_enabled = 1;
		
		/* one */
		else
			codec[i].search_enabled = (i == (search_codec_idx - 2));
	}
	
	/* by default, complain afterwards that texture is *
	 * a solid color; when a texture that is not solid *
	 * is detected, this is set == 1 so it doesn't     */
	int solid = 1;
	
	for (i = 0; i < (sizeof(codec)/sizeof(codec[0])); ++i)
	{
		float numprog = i;
		numprog /= (sizeof(codec)/sizeof(codec[0])) - 1;
		*progress = numprog;
		
		unsigned char *pal = 0;
		int pal_max = 0;
		unsigned int sz;
		int bpp = codec[i].bpp;
		unsigned char *buf = g_zztv.buf;
		unsigned int buf_sz = g_zztv.buf_sz;
		unsigned char *Lmat = 0; /* last match */
		int strd = 4; /* stride */
		
		/* use first result */
		if (use_results && searchMode_idx == searchMode_idx_1st)
			break;
		
		/* search is disabled for this one */
		if (!codec[i].search_enabled)
			continue;
		
		/* must be simplified BEFORE acgen */
		if (codec[i].fmt == N64TEXCONV_CI)
		{
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
		}
		
		/* make all invisible pixels black */
		int invis = n64texconv_acgen(
			png
			, w
			, h
			, N64TEXCONV_ACGEN_BLACK
			, 0
			, calloc_safe
			, realloc_safe
			, free
			, codec[i].fmt
		);
		
		/* must generate palette */
		if (codec[i].fmt == N64TEXCONV_CI)
		{
			struct n64texconv_palctx *pctx = 0;
			
			/* derive max colors */
			if (bpp == N64TEXCONV_4)
				pal_max = 16;
			else// if (bpp == N64TEXCONV_8)
				pal_max = 256;
			
			/* where colors will be stored */
			pal = pbuf;
			
			/* generate and apply palette to single texture */
			pctx = n64texconv_palette_new(
				pal_max
				, pal
				, calloc_safe
				, realloc_safe
				, free
			);
			memset(pal, 0, pal_max*2);
			n64texconv_palette_queue(pctx, png, w, h, 0);
			n64texconv_palette_alpha(pctx, invis);
			pal_max = n64texconv_palette_exec(pctx);
			n64texconv_palette_free(pctx);
			
			/* convert palette colors (rgba8888 -> rgba5551) */
			unsigned int bytes;
			//fprintf(stderr, "%d palette colors\n", pal_max);
		
			n64texconv_to_n64(
				pal /* in-place conversion */
				, pal
				, 0
				, 0
				, N64TEXCONV_RGBA
				, N64TEXCONV_16
				, pal_max
				, 1
				, &bytes
			);
		}
		
		/* convert to n64 texture format */
		n64texconv_to_n64(
			png /* in-place conversion */
			, png
			, pal
			, pal_max
			, codec[i].fmt
			, bpp
			, w
			, h
			, &sz
		);
		
		/* solid textures are skipped */
		if (all_same_byte(png, sz))
		{
			//fprintf(stderr, "solid using fmt %d-%d\n", codec[i].fmt, bpp);
			goto L_next_iter;
		}
		#if 0
		else
		{
			fprintf(stderr, "fmt %d-%d bytes:\n", codec[i].fmt, bpp);
			unsigned char *d = png;
			for (int i=0; i < sz; ++i)
				fprintf(stderr, "%02X \n", d[i]);
		}
		#endif
		
		/* this is used so it knows to not complain *
		 * about the texture being solid later      */
		solid = 0;
		
		/* search loaded file for data here */
		switch (codec[i].fmt)
		{
		case N64TEXCONV_CI: {
			struct valNum vn[256] = {0};
			unsigned char *ci = png;
			#if 0 /* slow */
			unsigned char *buf8 = ci + sz * 2; /* 4b->8b buf */
			#endif
			int k;
			
			/* set index value of each */
			for (k = 0; k < pal_max; ++k)
			{
				vn[k].val = k;
				vn[k].num = 0;
			}
			
			/* convert 4bit color index to 8bit */
			if (bpp == N64TEXCONV_4)
			{
				match_cvt_4to8(ci, ci, sz);
				sz *= 2;
			}
			
			/* count number of each index */
			for (k = 0; k < sz; ++k)
				vn[ci[k]].num += 1;
			
			/* sort them */
			qsort(vn, pal_max, sizeof(vn[0]), valNum_descend);
			
			/* loop through entire file, finding multiple */
			while (1)
			{
				/* use first result */
				if (use_results && searchMode_idx == searchMode_idx_1st)
					break;
				
//							{static int i;fprintf(stderr, "loop %d\n", i);}
				unsigned char *mat = 0;
				
				for (k = 0; k < pal_max; ++k)
				{
					int v = vn[k].val;
					int v2 = v * 2;
					int pcol = (pal[v2]<<8)|pal[v2+1];
					
					/* invisible pixels are skipped */
					if (!(pcol&1))
						continue;
					
					/* all colors found: possible match */
					if (!vn[k].num)
						break;
					
					//fprintf(stderr, " > %d colors\n", vn[k].num);
					
					/* first match should = populous color *
					 * (searches entire file)              */
					if (!mat)
					{
						if (bpp == N64TEXCONV_4)
						#if 0 /* slow */
							mat = match4_8(
								buf, buf_sz, buf8
								, ci, sz
								, v
								, strd
								, Lmat
							);
						#else
							mat = match8(
								buf8, buf8sz
								, ci, sz
								, v
								, strd
								, Lmat
							);
						#endif
						else
							mat = match8(
								buf, buf_sz
								, ci, sz
								, v
								, strd
								, Lmat
							);
						Lmat = mat;
						/* no match found, so give it up */
						if (!mat)
							break;
						
						/* matched solid block, skip it */
						if (all_same_byte(mat, sz))
							break;
						
						//fprintf(stderr, " > > mat at %08lX\n", mat - buf8);
					}
					/* all subsequent matches search only the *
					 * block found in the first match         */
					else
					{
						unsigned char *Nmat;
						unsigned char *hay = mat;
						
					#if 0
						/* use 4to8 buffer for subsequent matches */
						if (bpp == N64TEXCONV_4)
							hay = buf8;
					#endif
						
						/* ensure pattern matches active block */
						Nmat = match8(
							hay, sz
							, ci, sz
							, v
							, strd
							, Lmat
						);
						
						//fprintf(stderr, " > > Nmat %p - %p = %08lX\n", Nmat, hay, Nmat-hay);
						
						/* matches must point to same data */
						if (Nmat != hay)
						{
							mat = 0;
							break;
						}
					}
				}
				
				/* found matching indices */
				if (mat)
				{
					unsigned char *Pbuf;
					unsigned int Pbuf_sz;
					
					/* support external palette */
					if (g_zztv.Epal.buf)
					{
						Pbuf = g_zztv.Epal.buf;
						Pbuf_sz = g_zztv.Epal.buf_sz;
					}
					else
					{
						Pbuf = g_zztv.buf;
						Pbuf_sz = g_zztv.buf_sz;
					}
					
					/* rearrange palette color order */
					unsigned char *Pmat = 0;
					unsigned char Npal[256 * 2] = {0};
					int k;
					for (k = 0; k < sz; ++k)
					{
						int Oidx = ci[k];
						int Nidx = mat[k];
						
						Npal[Nidx*2+0] = pal[Oidx*2+0];
						Npal[Nidx*2+1] = pal[Oidx*2+1];
					}
					
				#if 0
					for (k = 0; k < 256; ++k)
						fprintf(
							stderr
							, "[%2x]: %04X %04X\n"
							, k
							, (Npal[k*2+0]<<8)|(Npal[k*2+1])
							, ( pal[k*2+0]<<8)|( pal[k*2+1])
						);
				#endif
					
					/* search palette in loaded file */
					/* NOTE: do not use match5551_color_abit here;
					 *       that would make it want the alpha bits
					 *       of unknown (possibly invisible) colors
					 *       to match; comparing color channel only
					 *       (except on invisibles) works best
					 */
					Pmat = match5551_color(
						Pbuf, Pbuf_sz
						, Npal, 256 * 2 /* necessary, due to diff *
							              * num colors in each pal */
						, 4
						, 0
					);
					
					#if 0
					if (!Pmat)
						fprintf(
							stderr
							, "partial ci match %08lX\n"
							, mat - buf
						);
					else
					{
					#endif
					unsigned char *hay;
					hay = (bpp == N64TEXCONV_4) ? buf8 : buf;
					/* allocate item, link into results list */
					struct search_results *item;
					item = calloc_safe(1, sizeof(*item));
					item->ofs = mat - hay;
					if (Pmat)
						item->pal = Pmat - Pbuf;
					else
						item->is_partial = 1;
					item->Epal = !!g_zztv.Epal.buf;
					item->codec = i;
					item->next = threadfunc_search_results;
					threadfunc_search_results = item;
					
					/* inside 4->8 expanded mem: divide by 2 */
					if (hay == buf8)
						item->ofs /= 2;
					
					/* first item in list: set offsets */
					if (!item->next)
						use_results = item;
					#if 0
					}
					#endif
				}
				
				/* no other possible matches */
				if (!Lmat)
					break;
				
				/* advance beyond found texture */
				Lmat += sz;
				
			} /* infinite loop through file */
		} break; /* ci */
		
		default: /* TODO YUV and stuff? */
		case N64TEXCONV_RGBA:
		{
			while (1)
			{
				/* use first result */
				if (use_results && searchMode_idx == searchMode_idx_1st)
					break;
				
				/* no invisible pixels: fast search */
				if (!invis)
					Lmat = matchBlock_stride(
						  buf, buf_sz
						, png, sz
						, 4
						, Lmat
					);
				
				/* rgba16 aka rgba5551 */
				else if (bpp == N64TEXCONV_16)
					Lmat = match5551_color_abit(
						  buf, buf_sz
						, png, sz
						, 4
						, Lmat
					);
				
				/* rgba32 aka rgba8888 */
				else
					Lmat = match8888_color(
						  buf, buf_sz
						, png, sz
						, 4
						, Lmat
					);
				
				/* no other possible matches */
				if (!Lmat)
					break;
						
				/* matched solid block, skip it */
				if (all_same_byte(Lmat, sz))
				{
					Lmat += sz;
					continue;
				}
				
				/* allocate item, link into results list */
				struct search_results *item;
				item = calloc_safe(1, sizeof(*item));
				item->ofs = Lmat - buf;
				item->pal = 0;
				item->codec = i;
				item->next = threadfunc_search_results;
				threadfunc_search_results = item;
				
				/* first item in list: set offsets */
				if (!item->next)
					use_results = item;
				
				/* advance beyond found texture */
				Lmat += sz;
			} /* infinite loop */
		} break; /* rgba */
		
		case N64TEXCONV_I:
		{
			while (1)
			{
				/* use first result */
				if (use_results && searchMode_idx == searchMode_idx_1st)
					break;
				
				/* i4 and i8 */
				Lmat = matchBlock_stride(
					  buf, buf_sz
					, png, sz
					, 4
					, Lmat
				);
				
				/* no other possible matches */
				if (!Lmat)
					break;
						
				/* matched solid block, skip it */
				if (all_same_byte(Lmat, sz))
				{
					Lmat += sz;
					continue;
				}
				
				/* allocate item, link into results list */
				struct search_results *item;
				item = calloc_safe(1, sizeof(*item));
				item->ofs = Lmat - buf;
				item->pal = 0;
				item->codec = i;
				item->next = threadfunc_search_results;
				threadfunc_search_results = item;
				
				/* first item in list: set offsets */
				if (!item->next)
					use_results = item;
				
				/* advance beyond found texture */
				Lmat += sz;
			} /* infinite loop */
		} break; /* i */
		
		case N64TEXCONV_IA:
		{
			while (1)
			{
				/* use first result */
				if (use_results && searchMode_idx == searchMode_idx_1st)
					break;
				
				/* no invisible pixels: fast search */
				if (!invis)
					Lmat = matchBlock_stride(
						  buf, buf_sz
						, png, sz
						, 4
						, Lmat
					);
				
				else
				{
					/* ia8 */
					switch (bpp)
					{
						case N64TEXCONV_4:
							Lmat = match31_color_abit(
								  buf, buf_sz
								, png, sz
								, 4
								, Lmat
							);
							break;
						
						case N64TEXCONV_8:
							Lmat = match44_color_abits(
								  buf, buf_sz
								, png, sz
								, 4
								, Lmat
							);
							break;
						
						//default:
						case N64TEXCONV_16:
							Lmat = match88_color_abits(
								  buf, buf_sz
								, png, sz
								, 4
								, Lmat
							);
							break;
					}
				}
				
				/* no other possible matches */
				if (!Lmat)
					break;
						
				/* matched solid block, skip it */
				if (all_same_byte(Lmat, sz))
				{
					Lmat += sz;
					continue;
				}
				
				/* allocate item, link into results list */
				struct search_results *item;
				item = calloc_safe(1, sizeof(*item));
				item->ofs = Lmat - buf;
				item->pal = 0;
				item->codec = i;
				item->next = threadfunc_search_results;
				threadfunc_search_results = item;
				
				/* first item in list: set offsets */
				if (!item->next)
					use_results = item;
				
				/* advance beyond found texture */
				Lmat += sz;
			} /* infinite loop */
		} break; /* ia */
		} /* end switch-case */
		L_next_iter:do{}while(0);
		/* copy png color data back for next iteration */
		if ((i+1) < (sizeof(codec)/sizeof(codec[0])))
			memcpy(png, png_copy, w * h * 4);
	} /* end codec loop */
	
	/* cleanup */
	stbi_image_free(png);
	free(png_copy);
	
	if (solid)
		complain("texture converts as solid color data, can't search that");
	
	else if (!use_results)
		complain("no matches found; try a different search codec");
	
	if (use_results)
		search_use_results(use_results, w, h);
	
	*progress = 1;
	
	return 0;
}

