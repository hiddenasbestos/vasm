/* output_bin.c binary output driver for vasm */
/* (c) in 2002-2009,2013,2015,2017 by Volker Barthelmann and Frank Wille */

#include "vasm.h"

#ifdef OUTBIN
static char *copyright="vasm binary output module 1.8a (c) 2002-2009,2013,2015,2017 Volker Barthelmann";

#define BINFMT_RAW      0
#define BINFMT_CBMPRG   1   /* Commodore VIC-20/C-64 PRG format */
#define BINFMT_ZXTAP    2   /* ZX Spectrum .TAP format */
static int binfmt = BINFMT_RAW;

static uint8_t g_zxtap_name[ 10 ];

static uint8_t g_gap_value = 0xFF;

static int g_rom_mode = 0;
static int g_print_map = 0;
static uint64_t g_length = 0;

static void debug_section_overlap( section* sec1, section* sec2 )
{
    utaddr size1;
    utaddr size2;

	size1 = (utaddr)(sec1->pc) - (utaddr)(sec1->org);
	size2 = (utaddr)(sec2->pc) - (utaddr)(sec2->org);

	printf( "\nerror: sections overlap:\n" );

	printf( " %08llX - %08llX %16s (%llu)", sec1->out_pos,sec1->out_pos + size1-1,sec1->name,ULLTADDR(size1) );

	if ( sec1->out_pos != sec1->org )
	{
		if ( size1 == 1 ) {
			printf("\t<-- local: %08llX",
				ULLTADDR(sec1->org)
			);
		} else {
			printf("\t<-- local: %08llX - %08llX",
				ULLTADDR(sec1->org),ULLTADDR(sec1->org)+size1-1
			);
		}
	}

	putchar('\n');

	printf( " %08llX - %08llX %16s (%llu)", sec2->out_pos,sec2->out_pos + size2-1,sec2->name,ULLTADDR(size2) );

	if ( sec2->out_pos != sec2->org )
	{
		if ( size2 == 1 ) {
			printf("\t<-- local: %08llX",
				ULLTADDR(sec2->org)
			);
		} else {
			printf("\t<-- local: %08llX - %08llX",
				ULLTADDR(sec2->org),ULLTADDR(sec2->org)+size2-1
			);
		}
	}

	putchar('\n');
}

static int out_pos_cmp( const void * _sec1,const void * _sec2 )
{
	section* sec1;
	section* sec2;

    utaddr size1;
    utaddr size2;

	sec1 = (*(section **)_sec1);
	sec2 = (*(section **)_sec2);

	size1 = (utaddr)(sec1->pc) - (utaddr)(sec1->org);
	size2 = (utaddr)(sec2->pc) - (utaddr)(sec2->org);

	if ( ( sec1->out_pos + size1 ) > ( sec2->out_pos + size2 ) )
		return 1;
	if ( ( sec1->out_pos + size1 ) < ( sec2->out_pos + size2 ) )
		return -1;

	debug_section_overlap( sec1, sec2 );

	output_error(0);
	return 0;
}

static int is_section_writeable( section *sec )
{
  char *a = sec->attr;
  while (*a) {
    if (*a++ == 'w') {
      return 1;
	}
  }
  return 0;
}

static int use_section(section *sec)
{
  unsigned long long size;
  size = sec->pc - sec->org;
  if (size<=0) {
    return 0;
  }
  if ( g_rom_mode && is_section_writeable(sec) ) {
    return 0;
  }
  return 1;
}

static void write_output(FILE *f,section *sec,symbol *sym)
{
  section *s,*s2,**seclist,**slp;
  atom *p;
  size_t nsecs;
  taddr pc,pc0,npc,i,j;
  utaddr size;
  uint64_t wpc, st1,st2,ed1,ed2;

  if (!sec)
    return;

  for (; sym; sym=sym->next) {
    if (sym->type == IMPORT)
      output_error(6,sym->name);  /* undefined symbol */
  }

  /* count valid sections */
  for (s=sec,nsecs=0; s!=NULL; s=s->next) {
	if (use_section(s)) {
	  nsecs++;
	}
  }

  /* make an array of section pointers, sorted by their output address */
  seclist = (section **)mymalloc(nsecs * sizeof(section *));
  for (s=sec,slp=seclist; s!=NULL; s=s->next) {
	if (use_section(s)) {
      *slp++ = s;
	}
  }
  if (nsecs > 1)
    qsort(seclist,nsecs,sizeof(section *),out_pos_cmp);

  /* check for overlapping sections */
  for (i=0; i<nsecs; ++i) {
    s = seclist[i];
	st1 = s->out_pos;
	ed1 = st1 + ((utaddr)(s->pc) - (utaddr)(s->org));
    for (j=i+1; j<nsecs; ++j) {
      s2 = seclist[j];
      st2 = s2->out_pos;
	  ed2 = st2 + ((utaddr)(s2->pc) - (utaddr)(s2->org));
	  if ( ( ( st2 >= st1 ) && ( st2 < ed1 ) ) ||
           ( ( st1 >= st2 ) && ( st1 < ed2 ) ) )
	  {
		debug_section_overlap( s, s2 );
	    output_error(0);
	  }
    }
  }

  if (binfmt == BINFMT_CBMPRG) {
    /* Commodore 6502 PRG header:
     * 00: LSB of load address
     * 01: MSB of load address
     */
    fw8(f,sec->org&0xff);
    fw8(f,(sec->org>>8)&0xff);
  }

	/* map */
	{
		wpc = 0;

		if ( g_print_map ) printf("\n ==== output section map ============\n\n");
		for ( i = 0; i < nsecs; ++i )
		{
			s = seclist[i];
			size = (utaddr)(s->pc) - (utaddr)(s->org);

			if ( wpc < s->out_pos )
			{
  				utaddr gap_size;
  				gap_size = (s->out_pos-1) - wpc;

				if ( g_print_map ) printf(" %08llX - %08llX %16s (%llu)\t\t<-- fill: 0x%02X\n",
					wpc,s->out_pos-1,"---",gap_size,g_gap_value);
			}

			if ( g_print_map )
			{
				if ( size == 1 ) {
					printf(" %08llX            %16s",
						s->out_pos,
						s->name);
				} else if ( ULLTADDR(size) > 0 ) {
					printf(" %08llX - %08llX %16s (%llu)",
						s->out_pos,s->out_pos + size-1,
						s->name,ULLTADDR(size));
				}

				if ( ULLTADDR(s->out_pos) != ULLTADDR(s->org) )
				{
					if ( size == 1 )
					{
						printf("\t\t<-- local: %08llX",
							ULLTADDR(s->org)
						);
					}
					else
					{
						printf("\t<-- local: %08llX - %08llX",
							ULLTADDR(s->org),ULLTADDR(s->org)+size-1
						);
					}
				}

				if ( ULLTADDR(size) > 0 )
					putchar('\n');
			}

			wpc = s->out_pos + ULLTADDR(size);
		}

		if ( wpc < g_length && g_length > 0 )
		{
			utaddr gap_size;
			gap_size = (g_length-1) - wpc;

			if ( g_print_map )
			{
				printf(" %08llX - %08llX %16s (%llu)\t\t<-- fill: 0x%02X\n",
					wpc,g_length-1,"---",gap_size,g_gap_value);
			}

			wpc = g_length;
		}

		if ( g_print_map )
		{
			printf( "\n  total: %12llu bytes.", wpc );
			printf("\n ====================================\n\n");
		}
	}

	if ( binfmt == BINFMT_ZXTAP )
	{
		uint64_t wpc2;
		uint8_t zxtap_header[ 21 ];
		int count;
		char c;
		uint8_t checkbit;

		printf("\nWriting ZX Spectrum .TAP header (org = %d; length = %d)\n\n", sec->org, wpc );

		/* ZX Spectrum TAP header */
		zxtap_header[ 0 ] = 19;
		zxtap_header[ 1 ] = 0;
		zxtap_header[ 2 ] = 0;
		zxtap_header[ 3 ] = 3;

		/* NEXT 10 BYTES ARE FILENAME */
		for(count=0;count<10;count++)
		{
			c = g_zxtap_name[ count ];
			zxtap_header[ 4 + count ] = ( c < 32 ) ? 32 : c;
		}

		/* CODE DATA */
		zxtap_header[ 14 ] = wpc&0xff;
		zxtap_header[ 15 ] = (wpc>>8)&0xff;
		zxtap_header[ 16 ] = sec->org&0xff;
		zxtap_header[ 17 ] = (sec->org>>8)&0xff;
		zxtap_header[ 18 ] = 0;
		zxtap_header[ 19 ] = 0x80;

		/* CHECK BIT */
		for(count=2;count<20;count++)
			checkbit=(checkbit ^ zxtap_header[count]);
		zxtap_header[ 20 ]=checkbit;

		for(count=0;count<21;++count)
			fw8(f,zxtap_header[count]);

		/* DATA HEADER */
		wpc2 = wpc + 2; // data length + 2 checksums
		zxtap_header[ 0 ] = wpc2&0xff;
		zxtap_header[ 1 ] = (wpc2>>8)&0xff;
		zxtap_header[ 2 ] = 255;
		for(count=0;count<3;++count)
			fw8(f,zxtap_header[count]);
	}

  uint8_t checkbit = 255;

  /* flush sections */
  pc = 0; wpc = 0;
  for (slp=seclist; nsecs>0; nsecs--) {
    s = *slp++;
    if (s->out_pos > wpc) {
      /* fill gap between sections */
      for (; wpc < s->out_pos; ++wpc)
      {
        fw8(f,g_gap_value);
        checkbit=(checkbit ^ g_gap_value);
	  }
    }

    for (p=s->first; p; p=p->next) {
	  pc0 = pc;
      npc = fwpcalign(f,p,s,pc);
      if (p->type == DATA) {
        for (i=0; i<p->content.db->size; i++)
        {
		  uint8_t data = (unsigned char)p->content.db->data[i];
          fw8(f,data);
          checkbit=(checkbit ^ data);
	  	}
      }
      else if (p->type == SPACE) {
        fwsblock(f,p->content.sb);
      }
	  size = atom_size(p,s,npc);
      pc = npc + size;
	  wpc += ULLTADDR(pc - pc0);
    }
  }
  free(seclist);

  /* pad to given length? */
  for (; wpc<g_length; ++wpc) {
	  fw8(f,g_gap_value);
	  checkbit=(checkbit ^ g_gap_value);
  }

	if ( binfmt == BINFMT_ZXTAP )
	{
		fw8(f,checkbit);
	}
}


static int output_args(char *p)
{
  if (!strcmp(p,"-cbm-prg")) {
    binfmt = BINFMT_CBMPRG;
    return 1;
  }
  else if (!strncmp(p,"-zx-tap=",8)) {
    binfmt = BINFMT_ZXTAP;
    strncpy( g_zxtap_name, p+8, 10 );
    return 1;
  }
  else if (!strcmp(p,"-rom")) {
    g_rom_mode = 1;
    return 1;
  }
  else if (!strcmp(p,"-map")) {
    g_print_map = 1;
    return 1;
  }
  else if (!strncmp(p,"-fill=",6)) {
    g_gap_value = atol(p+6);
    return 1;
  }
  else if (!strncmp(p,"-len=",5)) {
    g_length = atol(p+5);
    return 1;
  }
  return 0;
}


int init_output_bin(char **cp,void (**wo)(FILE *,section *,symbol *),int (**oa)(char *))
{
  *cp = copyright;
  *wo = write_output;
  *oa = output_args;
  return 1;
}

#else

int init_output_bin(char **cp,void (**wo)(FILE *,section *,symbol *),int (**oa)(char *))
{
  return 0;
}
#endif
