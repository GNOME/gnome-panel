/* mico-parse.h - argp wrappers for MICO setup.
   Written by Tom Tromey <tromey@cygnus.com>.  */

#ifndef __MICO_PARSE_H__
#define __MICO_PARSE_H__

#ifdef __cplusplus
/* Call this after arguments are parsed.  It will initialize CORBA and
   fill in the return parameters.  */
extern void panel_initialize_corba (CORBA::ORB_ptr *orb,
				    CORBA::BOA_ptr *boa);
#endif

BEGIN_GNOME_DECLS

/* Call this to register MICO's command-line arguments.  This is
   callable from C, but you'll have to declare it elsewhere.  */
void panel_corba_register_arguments (void);

END_GNOME_DECLS

#endif /* __MICO_PARSE_H__ */
