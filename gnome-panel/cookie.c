#include <config.h>
#include <gnome.h>
#include <stdlib.h>
#include <stdio.h>
#include <sys/stat.h>

char *cookie_file, *cookie_dir;

static void
check_dir_permissions (char *s)
{
	struct stat sb;
	
	if (stat (s, &sb) == -1){
		printf ("Can not stat %s\n", s);
		exit (1);
	}
	if (!S_ISDIR (sb.st_mode)){
		printf ("%s is not a directory\n", s);
		exit (1);
	}
	if (chmod (s, 0700) != 0){
		printf ("%s can not set permissions to %s\n", s);
		exit (1);
	}
}

static void
create_cookie ()
{
	FILE *f;
	int i;
	
	f = fopen (cookie_file, "w");
	if (f == NULL){
		printf ("Could not create cookie file %s\n", cookie_file);
		exit (1);
	}
	
	srandom (time (NULL));
	for (i = 0; i < 80; i++){
		int v = (random () % 50) + 'A';

		fputc (v, f);
	}
	fclose (f);
}

char *
compute_cookie ()
{
	FILE *f;
	static char buffer [90];
	
	f = fopen (cookie_file, "w");
	if (f == NULL){
		printf ("Could not open cookie file\n", cookie_file);
		exit (1);
	}
	fgets (buffer, 80, f);
	fclose (f);
	buffer [80] = 0;
	return buffer;
}

void
poor_security_cookie_init (void)
{
	char buffer [40];
	
	srandom (time (NULL));
	snprintf (buffer,40, "%d-%d", getpid (), (int)random());
	cookie_dir = g_copy_strings ("/tmp/.panel-", buffer, NULL);
	mkdir (cookie_dir, 0700);
	check_dir_permissions (cookie_dir);

	cookie_file = g_concat_dir_and_file (cookie_dir, "cookie");
}

