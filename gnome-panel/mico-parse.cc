/* mico-parse.cc - argp wrappers for MICO setup.
   Written by Tom Tromey <tromey@cygnus.com>.  */

#include <config.h>

#include <iostream.h>
#include <mico/gtkmico.h>
#include <mico/naming.h>
#include <gnome.h>
#include "mico-glue.h"
#include "gnome-panel.h"
#include "panel.h"
#include "mico-parse.h"

static error_t parse_mico_arg (int key, char *arg, struct argp_state *state);

/* This describes all the options we know that MICO uses.  We just
   lump the BOA and ORB options together.  It probably doesn't
   matter.  This code is generally useful and should probably be put
   into a Gnome library that is used whenever MICO is used.

   Note that keys must start at -1 and decrease by 1.  The key number
   is used to look up the argument name.

   FIXME: should these really all be hidden?  */
static struct argp_option our_mico_options[] =
{
  { "ORBNoIIOPServer", -1, NULL, OPTION_HIDDEN, NULL, -1 },
  { "ORBNoIIOPProxy", -2, NULL, OPTION_HIDDEN, NULL, -1 },
  { "ORBIIOPAddr", -3, "ADDR", OPTION_HIDDEN, NULL, -1 },
  { "ORBId", -4, "ID", OPTION_HIDDEN, NULL, -1 },
  { "ORBImplRepoIOR", -5, "IOR", OPTION_HIDDEN, NULL, -1 },
  { "ORBImplRepoAddr", -6, "ADDR", OPTION_HIDDEN, NULL, -1 },
  { "ORBIfaceRepoIOR", -7, "IOR", OPTION_HIDDEN, NULL, -1 },
  { "ORBIfaceRepoAddr", -8, "ADDR", OPTION_HIDDEN, NULL, -1 },
  { "ORBNamingIOR", -9, "IOR", OPTION_HIDDEN, NULL, -1 },
  { "ORBNamingAddr", -10, "ADDR", OPTION_HIDDEN, NULL, -1 },
  { "ORBDebugLevel", -11, "LEVEL", OPTION_HIDDEN, NULL, -1 },
  { "ORBBindingAddr", -12, "ADDR", OPTION_HIDDEN, NULL, -1 },

  { "OARemoteIOR", -13, "IOR", OPTION_HIDDEN, NULL, -1 },
  { "OARemoteAddr", -14, "ADDR", OPTION_HIDDEN, NULL, -1 },
  { "OARestoreIOR", -15, "IOR", OPTION_HIDDEN, NULL, -1 },
  { "OAImplName", -16, "NAME", OPTION_HIDDEN, NULL, -1 },
  { "OAId", -17, "ID", OPTION_HIDDEN, NULL, -1 },

  { NULL, 0, NULL, 0, NULL, 0 }
};

/* This describes how we parse MICO arguments.  */
static struct argp our_mico_parser =
{
  our_mico_options,
  parse_mico_arg,
  NULL,
  NULL,
  NULL,
  NULL,
  PACKAGE
};

/* These are used to store arguments we are sending to MICO.  */
static int our_mico_argc;
static char **our_mico_argv;

static error_t
parse_mico_arg (int key, char *arg, struct argp_state *state)
{
  if (key < 0)
    {
      /* We defined this argument.  Handle it by pushing the flag and
	 possibly the argument onto our saved argument vector.  */
      /* NOTE: this requires the Gnome-modified MICO.  The stock MICO
	 uses "-" and not "--" arguments.  */
      our_mico_argv[our_mico_argc++]
	= g_strconcat ("--", our_mico_options[- key - 1].name, NULL);
      if (arg)
	our_mico_argv[our_mico_argc++] = strdup (arg);
    }
  else if (key == ARGP_KEY_INIT)
    {
      /* Allocate enough space.  */
      our_mico_argv = (char **) malloc ((state->argc + 1) * sizeof (char *));
      our_mico_argc = 0;
      our_mico_argv[our_mico_argc++] = strdup (state->argv[0]);
    }
  else
    return ARGP_ERR_UNKNOWN;

  return 0;
}

void
panel_corba_register_arguments (void)
{
  gnome_parse_register_arguments (&our_mico_parser);
}

void
panel_initialize_corba (CORBA::ORB_ptr *orb, CORBA::BOA_ptr *boa)
{
  int i;

  our_mico_argv[our_mico_argc] = NULL;
  *orb = CORBA::ORB_init (our_mico_argc, our_mico_argv,
			  "mico-local-orb");
  *boa = (*orb)->BOA_init (our_mico_argc, our_mico_argv,
			   "mico-local-boa");

  for (i = 0; i < our_mico_argc; ++i)
    free (our_mico_argv[i]);
  free (our_mico_argv);
}
