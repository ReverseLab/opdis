/*!
 * \file opdis.c
 * \brief Disassembler front-end for libopcodes
 * \author thoughtgang.org
 */

#include <stdarg.h>
#include <stdlib.h>

#include <opdis/opdis.h>

/* ---------------------------------------------------------------------- */
/* Default callbacks */

static int default_handler( const opdis_insn_t * insn, void * arg ) {
	// add to internal list
	// if already present, return false
	// return true
	return 1;
}

static opdis_addr_t default_resolver( const opdis_insn_t * insn, void * arg ) {
	// resolve address if relative
	return OPDIS_INVALID_ADDR;
}

static void default_opdis_error( enum opdis_error_t error, const char * msg,
				 void * arg ) {
	char * str;

	switch (error) {
		case opdis_error_bounds:
			str = "memory bounds exceeded"; break;
		case opdis_error_invalid_insn:
			str = "invalid instruction"; break;
		case opdis_error_max_items:
			str = "max insn items exceeded"; break;
		case opdis_error_unknown:
		default:
			str = "unknown error"; break;
	}

	fprintf( stderr, "Error (%s): %s\n", str, msg );
}

/* ---------------------------------------------------------------------- */
/* Built-in decoders */

static int default_decoder( const opdis_insn_buf_t * in, opdis_insn_t * out,
		            const opdis_byte_t * start, opdis_off_t length ) {
	// fill out
	// handler must put bytes into buffer as well?
	return 0;
}

/* ---------------------------------------------------------------------- */
/* fprintf handler */

static int build_insn_fprintf( void * stream, const char * format, ... ) {
	char buf[OPDIS_MAX_ITEM_SIZE];
	int rv;
	/* hack to get around libopcodes' fprintf-only output */
	opdis_t o = (opdis_t) stream;

	va_list args;
	va_start (args, format);
	rv = vsnprintf( buf, OPDIS_MAX_ITEM_SIZE - 1, format, args );
	va_end (args);

	/* TODO: handle buf */
	// append to tmp
	// append to insn.ascii

	return rv;
}

/* this is used only to get the size of an insn: it discards all insn details */
static int null_fprintf( void * f, const char * str, ... ) {
	return 0;
}


/* ---------------------------------------------------------------------- */
/* OPDIS MGT */

opdis_t LIBCALL opdis_init( void ) {
	opdis_t o = (opdis_t) calloc( sizeof(opdis_info_t), 1 );
	
	if ( o ) {
		init_disassemble_info ( &o->config, o, build_insn_fprintf );
		opdis_set_defaults( o );
	}

	return o;
}

void LIBCALL opdis_term( opdis_t o ) {
	if ( o ) {
		free( o );
	}
}

opdis_t LIBCALL opdis_init_from_bfd( bfd * abfd ) {
	return NULL;
}

/* ---------------------------------------------------------------------- */
/* Configuration */
// NOTE: void disassembler_usage (FILE *);

void LIBCALL opdis_set_defaults( opdis_t o ) {
	opdis_set_handler( o, default_handler, NULL );
	opdis_set_resolver( o, default_resolver, NULL );
	opdis_set_error_reporter( o, default_opdis_error, NULL );

	opdis_set_x86_syntax( o, opdis_x86_syntax_intel );
}

void LIBCALL opdis_set_disassembler_options( opdis_t o, const char * options ) {
	if (! o ) {
		return;
	}
}

void LIBCALL opdis_set_x86_syntax( opdis_t o, enum opdis_x86_syntax_t syntax ) {
	disassembler_ftype fn = print_insn_i386_intel;
	OPDIS_DECODER d_fn = default_decoder; // intel

	if (! o ) {
		return;
	}

	if ( syntax == opdis_x86_syntax_att ) {
		fn = print_insn_i386_att;
		d_fn = default_decoder; // att
	}

	opdis_set_arch( o, bfd_arch_i386, fn );
	opdis_set_decoder( o, d_fn, NULL );
}

void LIBCALL opdis_set_arch( opdis_t o, enum bfd_architecture arch, 
			     disassembler_ftype fn ) {
	if (! o ) {
		return;
	}

	o->disassembler = fn;
	o->config.arch = arch;
	disassemble_init_for_target( &o->config );

	opdis_set_decoder( o, default_decoder, NULL );
}

void LIBCALL opdis_set_display( opdis_t o, OPDIS_DISPLAY fn, void * arg ) {
	if ( o && fn ) {
		o->display = fn;
		o->display_arg = arg;
	}
}

void LIBCALL opdis_set_handler( opdis_t o, OPDIS_HANDLER fn, void * arg ) {
	if ( o && fn ) {
		o->handler = fn;
		o->handler_arg = arg;
	}
}

void LIBCALL opdis_set_decoder( opdis_t o, OPDIS_DECODER fn, void * arg ) {
	if ( o && fn ) {
		o->decoder = fn;
		o->decoder_arg = arg;
	}
}

void LIBCALL opdis_set_resolver( opdis_t o, OPDIS_RESOLVER fn, void * arg ) {
	if ( o && fn ) {
		o->resolver = fn;
		o->resolver_arg = arg;
	}
}

void LIBCALL opdis_set_error_reporter( opdis_t o, OPDIS_ERROR fn, void * arg ) {
	if ( o && fn ) {
		o->error_reporter = fn;
		o->error_reporter_arg = arg;
	}
}

/* ---------------------------------------------------------------------- */
/* Disassemble instruction */

static int buffer_check( opdis_buf_t buf, opdis_off_t offset ) {
	if ( offset >= buf->len ) {
		char msg[64];
		snprintf( msg, 63, "Offset %d exceeds buffer length %d\n",
			 offset, buf->len );

		//opdis_error( o, opdis_error_bounds, buf );
		return 0;
	}

	return 1;
}

// size of single insn at address
unsigned int LIBCALL opdis_disasm_insn_size( opdis_t o, opdis_buf_t buf, 
					     opdis_off_t offset ){
	size_t size;
	fprintf_ftype fn = o->config.fprintf_func;
	o->config.fprintf_func = null_fprintf;

	if (! o || ! buf ||! buffer_check( buf, offset ) ) {
		return 0;
	}

	o->config.stream = buf->data;
	size = o->disassembler( offset, &o->config );
	
	o->config.fprintf_func = fn;
	return size;
}

// disasm single insn at address
unsigned int LIBCALL opdis_disasm_insn( opdis_t o, opdis_buf_t buf, 
					opdis_off_t offset, 
					opdis_insn_t * insn ) {
	size_t size;

	if (! o || ! buf ||! buffer_check( buf, offset ) ) {
		return 0;
	}


	o->config.stream = buf->data;
	size = o->disassembler( offset, &o->config );

	if (! size ) {
		char msg[64];
		snprintf( msg, 63, "Invalid insn at offset %d\n", offset );
		opdis_error( o, opdis_error_bounds, msg );
		return 0;
	}

	// o.decoder( o.raw, insn, &buf->data[offset], size );

	return size;
}

/* ---------------------------------------------------------------------- */
/* Disassembler algorithms */

int LIBCALL opdis_disasm_linear( opdis_t o, opdis_buf_t buf, opdis_off_t offset,
				 opdis_off_t length ) {
	//cont = o.handler( insn, o.handler_arg );
	return 0;
}

int LIBCALL opdis_disasm_cflow( opdis_t o, opdis_buf_t buf, 
				opdis_off_t offset ) {
	//cont = o.handler( insn, o.handler_arg );
	//addr = o.resolver(insn)
	return 0;
}

/* ---------------------------------------------------------------------- */
void LIBCALL opdis_error( opdis_t o, enum opdis_error_t error, 
			  const char * msg ) {
	if ( o ) o->error_reporter(error, msg, o->error_reporter_arg );
}


