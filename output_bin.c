/* output_bin.c binary output driver for vasm */
/* (c) in 2002-2009,2013,2015,2017 by Volker Barthelmann and Frank Wille */

#include "vasm.h"

#ifdef OUTBIN
static char *copyright="vasm binary output module 1.8a (c) 2002-2009,2013,2015,2017 Volker Barthelmann";

#define BINFMT_RAW      0
#define BINFMT_CBMPRG   1   /* Commodore VIC-20/C-64 PRG format */
static int binfmt = BINFMT_RAW;

static int g_rom_mode = 0;
static int g_print_map = 0;
static uint64_t g_length = 0;

static int orgcmp( const void * _sec1,const void * _sec2 )
{
	section* sec1;
	section* sec2;
	
	sec1 = (*(section **)_sec1);
	sec2 = (*(section **)_sec2);
	
	if ( ( sec1->out_pos + sec1->org ) > ( sec2->out_pos + sec2->org ) )
		return 1;
	if ( ( sec1->out_pos + sec1->org ) < ( sec2->out_pos + sec2->org ) )
		return -1;
	
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
  
  /* make an array of section pointers, sorted by their start address */
  seclist = (section **)mymalloc(nsecs * sizeof(section *));
  for (s=sec,slp=seclist; s!=NULL; s=s->next) {
	if (use_section(s)) {
      *slp++ = s;
	}
  }
  if (nsecs > 1)
    qsort(seclist,nsecs,sizeof(section *),orgcmp);

  /* we don't support overlapping sections */
  for (i=0; i<nsecs; ++i) {
    s = seclist[i];
	st1 = s->out_pos;
	ed1 = st1 + ( s->pc - s->org );
    for (j=i+1; j<nsecs; ++j) {
      s2 = seclist[j];
      st2 = s2->out_pos;
	  ed2 = st2 + ( s2->pc - s2->org );
	  if ( ( ( st2 >= st1 ) && ( st2 < ed1 ) ) ||
           ( ( st1 >= st2 ) && ( st1 < ed2 ) ) )
	  {
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
	if ( g_print_map )
	{
		wpc = 0;
		
		printf("\n ==== output section map ============\n\n");
		for ( i = 0; i < nsecs; ++i )
		{
			s = seclist[i];
			size = (utaddr)(s->pc) - (utaddr)(s->org);
			
			if ( wpc < s->out_pos ) {
				printf(" %08llX - %08llX %16s\n", 
					wpc,s->out_pos-1,"---");
			}
			
			if ( size == 1 ) {
				printf(" %08llX            %16s",
					s->out_pos,
					s->name);
			} else {
				printf(" %08llX - %08llX %16s (%llu)",
					s->out_pos,s->out_pos + size-1,
					s->name,ULLTADDR(size));
			}

			if ( s->out_pos != s->org )
			{
				if ( size == 1 ) {
					printf("\t<-- local: %08llX",
						ULLTADDR(s->org)
					);
				} else {
					printf("\t<-- local: %08llX - %08llX",
						ULLTADDR(s->org),ULLTADDR(s->org)+size-1
					);
				}
			}
			
			putchar('\n');
			
			wpc = s->out_pos + ULLTADDR(size);
		}
		
		if ( wpc < g_length && g_length > 0 ) {
			printf(" %08llX - %08llX %16s\n", 
				wpc,g_length-1,"---");
			wpc = g_length;
		}			
		
		printf( "\n  length: %12llu bytes.", wpc );
		printf("\n ====================================\n\n");
	}

  /* flush sections */
  pc = 0; wpc = 0;
  for (slp=seclist; nsecs>0; nsecs--) {
    s = *slp++;
    if (s->out_pos > wpc) {
      /* fill gap between sections with zeros */
      for (; wpc < s->out_pos; ++wpc)
        fw8(f,0);
    }

    for (p=s->first; p; p=p->next) {
	  pc0 = pc;
      npc = fwpcalign(f,p,s,pc);
      if (p->type == DATA) {
        for (i=0; i<p->content.db->size; i++)
          fw8(f,(unsigned char)p->content.db->data[i]);
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
	  fw8(f,0);
  }
}


static int output_args(char *p)
{
  if (!strcmp(p,"-cbm-prg")) {
    binfmt = BINFMT_CBMPRG;
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
