#ifndef MENU_FENTRY_H
#define MENU_FENTRY_H

#include <sys/stat.h>
#include <unistd.h>
#include <libgnome/gnome-desktop-item.h>

G_BEGIN_DECLS

typedef struct _MenuFinfo MenuFinfo;
typedef struct _FileRec FileRec;
typedef struct _DirRec DirRec;
typedef struct _MFile MFile;

struct _MenuFinfo {
	char *menudir;
	gboolean launcher_add;
	char *dir_name;
	gboolean fake_menu;
	FileRec *fr;
};

struct _MFile {
	char *name;
	gboolean verified;
	time_t mtime;
	gboolean is_dir;
};

enum {
	FILE_REC_FILE, /*.desktop file record*/
	FILE_REC_DIR,  /*directory*/
	FILE_REC_EXTRA, /*just check the mtime*/
	FILE_REC_SEP
};

struct _FileRec {
	int type;
	char *name;
	char *comment;
	char *fullname;
	char *name_collate_key;
	char *icon;
	char *exec;
	char *tryexec_path;
	DirRec *parent;
	time_t mtime;
	time_t last_stat;
};

struct _DirRec {
	FileRec frec;
	time_t ditemmtime;
	time_t ditemlast_stat;
	GSList *tryexecs;
	GSList *recs; /*records for directories*/
	GSList *mfl;  /*records of menus using this record*/
};

GSList * get_mfiles_from_menudir (const char *menudir, gboolean *sorted);

FileRec * fr_read_dir(DirRec *dr, const char *muri, time_t mtime, int sublevels);
FileRec * fr_replace(FileRec *fr);
FileRec * fr_check_and_reread(FileRec *fr);
FileRec * fr_get_dir(const char *mdir);

int fr_compare (FileRec *fra, FileRec *frb);

void free_mfile (MFile *mfile);
void free_mfile_list (GSList *list);

G_END_DECLS

#endif
