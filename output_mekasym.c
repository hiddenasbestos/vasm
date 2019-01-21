/* output_vasm.c mekasym format output driver for vasm */
/* (c) in 2019 by David Walters */

#include "vasm.h"

#ifdef OUTMEKASYM
static char *copyright="vasm mekasym output module 0.1 (c) 2019 David Walters";

static int sym_valid(symbol *symp)
{
	if(*symp->name==' ')
		return 0;  /* ignore internal/temporary symbols */
	if(symp->flags & VASMINTERN)
		return 0;  /* ignore vasm-internal symbols */
	return 1;
}

static void write_output(FILE *f,section *sec,symbol *sym)
{
	int nsyms,nsecs;
	section *secp;
	symbol *symp,*first,*last;
	taddr size,data,nrelocs;
	
	/* assign section index, make section symbols */
	for(nsecs=1,secp=sec,first=sym,last=NULL;secp;secp=secp->next)
	{
		secp->idx=nsecs++;  /* symp->idx will become equal to secp->idx */
		symp=mymalloc(sizeof(*symp));
		symp->name=secp->name;
		symp->type=LABSYM;
		symp->flags=TYPE_SECTION;
		symp->sec=secp;
		symp->pc=0;
		symp->expr=symp->size=NULL;
		symp->align=0;
		symp->next=sym;
		
		if(last!=NULL)
			last->next=symp;
		else
			first=symp;
		
		last=symp;
	}
	
	/* assign symbol index to all symbols */
	for(nsyms=1,symp=first;symp;symp=symp->next)
	{
		if(sym_valid(symp))
			symp->idx=nsyms++;
		else
			symp->idx=0;  /* use section-symbol, when needed */
	}
	
	for(symp=first;symp;symp=symp->next)
	{
		taddr val;
		
		if (symp->flags&TYPE_SECTION)
			continue;

		if (symp->type != TYPE_OBJECT)
			continue;

		if(!sym_valid(symp))
			continue;
			
		val = get_sym_value(symp);
		
		fprintf( f, "%04x %s\n", val & 0xFFFF, symp->name ); /* NO$GMB/WLA format */
	}
}

static int output_args(char *p)
{
	return 0;
}

int init_output_mekasym(char **cp,void (**wo)(FILE *,section *,symbol *),int (**oa)(char *))
{
	*cp=copyright;
	*wo=write_output;
	*oa=output_args;
	return 1;
}

#else

int init_output_mekasym(char **cp,void (**wo)(FILE *,section *,symbol *),int (**oa)(char *))
{
	return 0;
}

#endif
