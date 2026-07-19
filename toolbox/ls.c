#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <dirent.h>
#include <errno.h>

#include <sys/stat.h>
#include <unistd.h>
#include <time.h>

#include <pwd.h>
#include <grp.h>

#include <linux/kdev_t.h>
#include <limits.h>

#include "dynarray.h"
#include <getopt.h>

// bits for flags argument
#define LIST_LONG           (1 << 0)
#define LIST_ALL            (1 << 1)
#define LIST_RECURSIVE      (1 << 2)
#define LIST_DIRECTORIES    (1 << 3)
#define LIST_SIZE           (1 << 4)
#define LIST_LONG_NUMERIC   (1 << 5)
#define LIST_CLASSIFY       (1 << 6)
#define LIST_COLOR          (1 << 7)

#define LS_VERSION          1

// Colors!
#define COLOR_BLACK_FG      "\e[30m"
#define COLOR_BLACK_BG      "\e[40m"
#define COLOR_RED_FG        "\e[31m"
#define COLOR_RED_BG        "\e[41m"
#define COLOR_GREEN_FG      "\e[32m"
#define COLOR_GREEN_BG      "\e[42m"
#define COLOR_YELLOW_FG     "\e[33m"
#define COLOR_YELLOW_BG     "\e[43m"
#define COLOR_BLUE_FG       "\e[34m"
#define COLOR_BLUE_BG       "\e[44m"
#define COLOR_MAGENTA_FG    "\e[35m"
#define COLOR_MAGENTA_BG    "\e[45m"
#define COLOR_CYAN_FG       "\e[36m"
#define COLOR_CYAN_BG       "\e[46m"
#define COLOR_WHITE_FG      "\e[37m"
#define COLOR_WHITE_BG      "\e[47m"
#define COLOR_RESET         "\e[0m"

// fwd
static int listpath(const char *name, int flags);

static char mode2kind(unsigned mode)
{
    switch(mode & S_IFMT)
    {
        case S_IFSOCK:  return 's';
        case S_IFLNK:   return 'l';
        case S_IFREG:   return '-';
        case S_IFDIR:   return 'd';
        case S_IFBLK:   return 'b';
        case S_IFCHR:   return 'c';
        case S_IFIFO:   return 'p';
        default:        return '?';
    }
}

static void mode2str(unsigned mode, char *out)
{
    *out++ = mode2kind(mode);

    *out++ = (mode & 0400) ? 'r' : '-';
    *out++ = (mode & 0200) ? 'w' : '-';
    if(mode & 04000)
    {
        *out++ = (mode & 0100) ? 's' : 'S';
    }
    else
    {
        *out++ = (mode & 0100) ? 'x' : '-';
    }
    
    *out++ = (mode & 040) ? 'r' : '-';
    *out++ = (mode & 020) ? 'w' : '-';
    if(mode & 02000)
    {
        *out++ = (mode & 010) ? 's' : 'S';
    } 
    else
    {
        *out++ = (mode & 010) ? 'x' : '-';
    }

    *out++ = (mode & 04) ? 'r' : '-';
    *out++ = (mode & 02) ? 'w' : '-';

    if(mode & 01000)
    {
        *out++ = (mode & 01) ? 't' : 'T';
    } 
    else
    {
        *out++ = (mode & 01) ? 'x' : '-';
    }

    *out = 0;
}

static void user2str(unsigned uid, char *out)
{
    struct passwd *pw = getpwuid(uid);
    if (pw)
    {
        snprintf(out, 16, "%s", pw->pw_name);
    }
    else
    {
        snprintf(out, 16, "%d", uid);
    }
}

static void group2str(unsigned gid, char *out)
{
    struct group *gr = getgrgid(gid);
    if(gr)
    {
        strcpy(out, gr->gr_name);
    }
    else
    {
        sprintf(out, "%d", gid);
    }
}

static int show_total_size(const char *dirname, DIR *d, int flags)
{
    struct dirent *de;
    char tmp[1024];
    struct stat s;
    int sum = 0;

    /* run through the directory and sum up the file block sizes */
    while ((de = readdir(d)) != 0)
    {
        if (strcmp(de->d_name, ".") == 0 || strcmp(de->d_name, "..") == 0)
            continue;

        if (de->d_name[0] == '.' && (flags & LIST_ALL) == 0)
            continue;

        if (strcmp(dirname, "/") == 0)
            snprintf(tmp, sizeof(tmp), "/%s", de->d_name);
        else
            snprintf(tmp, sizeof(tmp), "%s/%s", dirname, de->d_name);

        if (lstat(tmp, &s) < 0)
        {
            fprintf(stderr, "stat failed on %s: %s\n", tmp, strerror(errno));
            rewinddir(d);
            return -1;
        }

        sum += s.st_blocks / 2;
    }

    printf("total %d\n", sum);
    rewinddir(d);
    return 0;
}

static int listfile_size(const char *path, const char *filename, int flags)
{
    struct stat s;

    if (lstat(path, &s) < 0)
    {
        fprintf(stderr, "lstat '%s' failed: %s\n", path, strerror(errno));
        return -1;
    }

    /* blocks are 512 bytes, we want output to be KB */
    if ((flags & LIST_SIZE) != 0)
    {
        printf("%lld ", s.st_blocks / 2);
    }

    if ((flags & LIST_CLASSIFY) != 0)
    {
        char filetype = mode2kind(s.st_mode);
        if (filetype != 'l')
        {
            printf("%c ", filetype);
        } 
        else
        {
            struct stat link_dest;
            if (!stat(path, &link_dest))
            {
                printf("l%c ", mode2kind(link_dest.st_mode));
            }
            else
            {
                fprintf(stderr, "stat '%s' failed: %s\n", path, strerror(errno));
                printf("l? ");
            }
        }
    }

    // Coloring!
    if (flags & LIST_COLOR)
    {
        if(S_ISBLK(s.st_mode) || S_ISCHR(s.st_mode))
            printf("%s", COLOR_YELLOW_FG);

        if (S_ISDIR(s.st_mode)) // directory
            printf("%s", COLOR_BLUE_FG);

        else if (S_ISLNK(s.st_mode)) // link
            printf("%s", COLOR_CYAN_FG);
            
        else if (s.st_mode & (S_IXUSR | S_IXGRP | S_IXOTH)) // execute
            printf("%s", COLOR_GREEN_FG);
    }

    printf("%s\n", filename);

    // Coloring! (again)
    // clearing changed color
    if(flags & LIST_COLOR)
        printf("%s", COLOR_RESET);

    return 0;
}

static int listfile_long(const char *path, int flags)
{
    struct stat s;

    char date[32];
    char mode[16];
    char user[16];
    char group[16];

    const char *name;

    /* name is anything after the final '/', or the whole path if none*/
    name = strrchr(path, '/');
    if(name == 0)
    {
        name = path;
    } 
    else
    {
        name++;
    }

    if(lstat(path, &s) < 0)
    {
        return -1;
    }

    mode2str(s.st_mode, mode);
    if (flags & LIST_LONG_NUMERIC)
    {
        sprintf(user, "%ld", s.st_uid);
        sprintf(group, "%ld", s.st_gid);
    }
    else
    {
        user2str(s.st_uid, user);
        group2str(s.st_gid, group);
    }

    strftime(date, 32, "%Y-%m-%d %H:%M", localtime((const time_t*)&s.st_mtime)); // TODO: fix 2Y38k bug
    date[31] = 0;

    // 12345678901234567890123456789012345678901234567890123456789012345678901234567890
    // MMMMMMMM UUUUUUUU GGGGGGGGG XXXXXXXX YYYY-MM-DD HH:MM NAME (->LINK)

    switch(s.st_mode & S_IFMT)
    {
        case S_IFBLK:
        case S_IFCHR:
        {
            if ((flags & LIST_LONG) != 0)
            {
                printf("%s %-8s %-8s %3d, %3d %s %s%s%s\n",
                        mode, user, group,
                        (int) MAJOR(s.st_rdev), (int) MINOR(s.st_rdev),
                        date, COLOR_YELLOW_FG, name, COLOR_RESET);
            }
            else
            {
                printf("%s %-8s %-8s %3d, %3d %s %s\n",
                        mode, user, group,
                        (int) MAJOR(s.st_rdev), (int) MINOR(s.st_rdev),
                        date, name);
            }
            break;
        }
        case S_IFREG: // exec
        {
            int exec = (s.st_mode & (S_IXUSR | S_IXGRP | S_IXOTH));

            if ((flags & LIST_LONG) != 0 && exec)
            {
                printf("%s %-8s %-8s %8lld %s %s%s%s\n",
                        mode, user, group, (long long)s.st_size, date, COLOR_GREEN_FG, name, COLOR_RESET);
            }
            else
            {
                printf("%s %-8s %-8s %8lld %s %s\n",
                        mode, user, group, (long long)s.st_size, date, name);
            }
            break;
        }
        case S_IFDIR: // directory
        {
            if ((flags & LIST_LONG) != 0)
            {
                printf("%s %-8s %-8s %8lld %s %s%s%s\n",
                        mode, user, group, (long long)s.st_size, date, COLOR_BLUE_FG, name, COLOR_RESET);
            }
            else
            {
                printf("%s %-8s %-8s %8lld %s %s\n",
                        mode, user, group, (long long)s.st_size, date, name);
            }
            break;
        }
        case S_IFLNK: // link
        {
            char linkto[256];
            int len;

            len = readlink(path, linkto, 256);
            if(len < 0)
                return -1;

            if(len > 255)
            {
                linkto[252] = '.';
                linkto[253] = '.';
                linkto[254] = '.';
                linkto[255] = 0;
            }
            else
            {
                linkto[len] = 0;
            }

            if ((flags & LIST_LONG) != 0)
            {
                printf("%s %-8s %-8s          %s %s%s%s -> %s\n",
                        mode, user, group, date, COLOR_CYAN_FG, name, COLOR_RESET, linkto);
            }
            else
            {
                printf("%s %-8s %-8s          %s %s -> %s\n",
                        mode, user, group, date, name, linkto);
            }

            break;
        }
        default:

        printf("%s %-8s %-8s          %s %s\n",
               mode, user, group, date, name);

    }

    return 0;
}

static int listfile(const char *dirname, const char *filename, int flags)
{
    // if there no flags; show all files
    if ((flags & (LIST_LONG | LIST_SIZE | LIST_CLASSIFY | LIST_COLOR)) == 0)
    {
        printf("%s\n", filename);
        return 0;
    }

    char tmp[4096];
    const char* pathname = filename;

    if (dirname != NULL)
    {
        snprintf(tmp, sizeof(tmp), "%s/%s", dirname, filename);
        pathname = tmp;
    } 
    else
    {
        pathname = filename;
    }

    if ((flags & LIST_LONG) != 0)
    {
        return listfile_long(pathname, flags);
    } 
    else /*((flags & LIST_SIZE) != 0)*/
    {
        return listfile_size(pathname, filename, flags);
    }
}

static int listdir(const char *name, int flags)
{
    char tmp[4096];
    DIR *d;
    struct dirent *de;
    strlist_t  files = STRLIST_INITIALIZER;

    d = opendir(name);
    if(d == 0)
    {
        fprintf(stderr, "opendir failed, %s\n", strerror(errno));
        return -1;
    }

    if ((flags & LIST_SIZE) != 0)
    {
        show_total_size(name, d, flags);
    }

    while((de = readdir(d)) != 0)
    {
        if (!strcmp(de->d_name, ".") || !strcmp(de->d_name, ".."))
            continue;

        if(de->d_name[0] == '.' && (flags & LIST_ALL) == 0)
            continue;

        strlist_append_dup(&files, de->d_name);
    }

    strlist_sort(&files);
    STRLIST_FOREACH(&files, filename, listfile(name, filename, flags));
    strlist_done(&files);

    if (flags & LIST_RECURSIVE)
    {
        strlist_t subdirs = STRLIST_INITIALIZER;

        rewinddir(d);

        while ((de = readdir(d)) != 0)
        {
            struct stat s;
            int err;

            if (!strcmp(de->d_name, ".") || !strcmp(de->d_name, ".."))
                continue;

            if (de->d_name[0] == '.' && (flags & LIST_ALL) == 0)
                continue;

            if (!strcmp(name, "/"))
                snprintf(tmp, sizeof(tmp), "/%s", de->d_name);
            else
                snprintf(tmp, sizeof(tmp), "%s/%s", name, de->d_name);

            /*
             * If the name ends in a '/', use stat() so we treat it like a
             * directory even if it's a symlink.
             */
            if (tmp[strlen(tmp)-1] == '/')
                err = stat(tmp, &s);
            else
                err = lstat(tmp, &s);

            if (err < 0)
            {
                perror(tmp);
                closedir(d);
                return -1;
            }

            if (S_ISDIR(s.st_mode))
            {
                strlist_append_dup(&subdirs, tmp);
            }
        }

        strlist_sort(&subdirs);
        STRLIST_FOREACH(&subdirs, path,
        {
            printf("\n%s:\n", path);
            listdir(path, flags);
        });

        strlist_done(&subdirs);
    }

    closedir(d);
    return 0;
}

static int listpath(const char *name, int flags)
{
    struct stat s;
    int err;

    /*
     * If the name ends in a '/', use stat() so we treat it like a
     * directory even if it's a symlink.
     */
    size_t len = strlen(name);
    if (len > 0 && name[len-1] == '/')  err = stat(name, &s);
    else                                err = lstat(name, &s);

    if (err < 0)
    {
        perror(name);
        return -1;
    }

    if ((flags & LIST_DIRECTORIES) == 0 && S_ISDIR(s.st_mode))
    {
        if (flags & LIST_RECURSIVE)
            printf("\n%s:\n", name);

        return listdir(name, flags);
    }
    else
    {
        /* yeah this calls stat() again*/
        return listfile(NULL, name, flags);
    }
}

static void help(const char *prog)
{
    printf("Usage:\n\t%s [option] [<directory>]\n\n", prog);
    printf("List of files in the directory;\n\n");
    printf("Options:\n");
    printf("\t-h, --help                Displays help;\n");
    printf("\t-V, --version             Displays version;\n");
    printf("\t-l, --long                Displays detailed information;\n");
    printf("\t-n, --numeric-uid-gid     Displays file owners and groups as raw numbers;\n");
    printf("\t-s, --size                Prints the allocated size of files;\n");
    printf("\t-R, --recursive           Display the contents of all directories recursively;\n");
    printf("\t-a, --all                 Don't ignore hidden files;\n");
    printf("\t-F, --classify            Add a character indicating the file type to each file;\n");
    printf("\t-c, --color               Changes colors for folders, files, and executable files;\n");
    printf("\t-d, --directory           List directory entries instead of contents;\n");
    printf("\n");
}

static void about()
{
    printf("ls for Foxdroid; Build %i\n", LS_VERSION);
}

int ls_main(int argc, char *argv[])
{
	int arg, flags = 0, err = 0;

    strlist_t files = STRLIST_INITIALIZER;

    static const struct option options[] =
    {
	    { "long",                   no_argument, NULL, 'l' },
		{ "numeric-uid-gid",        no_argument, NULL, 'n' },
        { "size",                   no_argument, NULL, 's' },
        { "recursive",              no_argument, NULL, 'R' },
        { "directory",              no_argument, NULL, 'd' },
        { "all",                    no_argument, NULL, 'a' },
        { "classify",               no_argument, NULL, 'F' },
        { "color",                  no_argument, NULL, 'c' },
        { "version",                no_argument, NULL, 'V' },
		{ "help",                   no_argument, NULL, 'h' },
		{ NULL, 0, NULL, 0 }
	};

	while ((arg = getopt_long(argc, argv, "lnsRdaFcVh", options, NULL)) != -1)
	{
		switch (arg)
		{
			case 'l':   flags |= LIST_LONG; break;
			case 'n':   flags |= LIST_LONG | LIST_LONG_NUMERIC; break;
			case 's':   flags |= LIST_SIZE; break;
			case 'R':   flags |= LIST_RECURSIVE; break;
            case 'd':   flags |= LIST_DIRECTORIES; break;
			case 'a':   flags |= LIST_ALL; break;
			case 'F':   flags |= LIST_CLASSIFY; break;
			case 'c':   flags |= LIST_COLOR; break;
			case 'V':
				about();
				return 0;
			case 'h':
				help(argv[0]);
				return 0;
            case '?':
			default:
                exit(1);
		}
    }

    int i; // the hell you mean "'for' loop initial declarations are only allowed in C99 mode"
    for (i = optind; i < argc; i++)
    {
        strlist_append_dup(&files, argv[i]);
    }

        if (files.count > 0)
        {
            STRLIST_FOREACH(&files, path,
            {
                if (listpath(path, flags) != 0)
                    err = EXIT_FAILURE;
            });

            strlist_done(&files);
            return err;
        }
    

    return listpath(".", flags);
}