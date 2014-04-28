/*
**  Copyright 1998-2003 University of Illinois Board of Trustees
**  Copyright 1998-2003 Mark D. Roth
**  All rights reserved.
**
**  libtar.c - demo driver program for libtar
**
**  Mark D. Roth <roth@uiuc.edu>
**  Campus Information Technologies and Educational Services
**  University of Illinois at Urbana-Champaign
*/

#include <config.h>
#include <libtar.h>

#include <stdio.h>
#include <fcntl.h>
#include <dirent.h>
#include <errno.h>
#include <sys/param.h>

#ifdef STDC_HEADERS
# include <stdlib.h>
# include <string.h>
#endif

#ifdef HAVE_UNISTD_H
# include <unistd.h>
#endif

#ifdef DEBUG
# include <signal.h>
#endif

#ifdef HAVE_LIBZ
# include <zlib.h>
#endif

#include <compat.h>

#include <fnmatch.h>

#include <getopt.h>

char *progname;
int verbose = 0;
int use_gnu = 1;

#ifdef DEBUG
void
segv_handler(int sig)
{
    puts("OOPS!  Caught SIGSEGV, bailing out...");
    fflush(stdout);
    fflush(stderr);
}
#endif


int store_selinux_ctx = 0;

#define EXCLUDES_MAX 16
char *exclude_list[EXCLUDES_MAX];
int exclusions = 0;

#ifdef HAVE_LIBZ

int use_zlib = 0;

int
gzopen_frontend(char *pathname, int oflags, int mode)
{
    char *gzoflags;
    gzFile gzf;
    int fd;

    switch (oflags & O_ACCMODE)
    {
    case O_WRONLY:
        gzoflags = "wb";
        break;
    case O_RDONLY:
        gzoflags = "rb";
        break;
    default:
    case O_RDWR:
        errno = EINVAL;
        return -1;
    }

    fd = open(pathname, oflags, mode);
    if (fd == -1)
        return -1;

    if ((oflags & O_CREAT) && fchmod(fd, mode))
        return -1;

    gzf = gzdopen(fd, gzoflags);
    if (!gzf)
    {
        errno = ENOMEM;
        return -1;
    }

    /* This is a bad thing to do on big-endian lp64 systems, where the
       size and placement of integers is different than pointers.
       However, to fix the problem 4 wrapper functions would be needed and
       an extra bit of data associating GZF with the wrapper functions.  */
    return (int)gzf;
}

tartype_t gztype = { (openfunc_t) gzopen_frontend, (closefunc_t) gzclose,
    (readfunc_t) gzread, (writefunc_t) gzwrite
};

#endif /* HAVE_LIBZ */

int
tar_append_tree_with_exceptions(TAR *t, char *realdir, char *savedir)
{
    char realpath[MAXPATHLEN];
    char savepath[MAXPATHLEN];
    struct dirent *dent;
    DIR *dp;
    struct stat s;

#ifdef DEBUG
    printf("==> tar_append_tree(0x%lx, \"%s\", \"%s\")\n",
           t, realdir, (savedir ? savedir : "[NULL]"));
#endif

    if (tar_append_file(t, realdir, savedir) != 0)
        return -1;

#ifdef DEBUG
    puts("    tar_append_tree(): done with tar_append_file()...");
#endif

    dp = opendir(realdir);
    if (dp == NULL)
    {
        if (errno == ENOTDIR)
            return 0;
        return -1;
    }
    while ((dent = readdir(dp)) != NULL)
    {
        int skip=0;
        if (strcmp(dent->d_name, ".") == 0 ||
            strcmp(dent->d_name, "..") == 0)
            continue;

        snprintf(realpath, MAXPATHLEN, "%s/%s", realdir,
             dent->d_name);
        if (savedir)
            snprintf(savepath, MAXPATHLEN, "%s/%s", savedir,
                 dent->d_name);

        if (lstat(realpath, &s) != 0)
            return -1;

        if (exclusions) {
            int x=0;
            for (x=0; x<exclusions; x++) {
                if (!fnmatch(exclude_list[x],
                        (strchr(exclude_list[x],'/') ? realpath : dent->d_name),
                        FNM_PATHNAME | FNM_LEADING_DIR)) {
                    skip=1;
                    continue;
                }
            }
        }
        if (skip) {
            continue;
        }

        if (S_ISDIR(s.st_mode))
        {
            if (tar_append_tree_with_exceptions(t, realpath,
                        (savedir ? savepath : NULL)) != 0)
                return -1;
            continue;
        }

        if (verbose == 2) {
            fprintf(stderr, "%s\n", th_get_pathname(t));
        }
        if (tar_append_file(t, realpath,
                    (savedir ? savepath : NULL)) != 0)
            return -1;
    }

    closedir(dp);

    return 0;
}

int
create(char *tarfile, char *rootdir, libtar_list_t *l)
{
    TAR *t;
    char *pathname;
    char buf[MAXPATHLEN];
    libtar_listptr_t lp;

    if (strnlen(tarfile,2) == 1 && !strncmp(tarfile,"-",1)) {
        if (verbose) {
            // stdout is busy with the archive, we'll print this ourselves, to stderr
            verbose = 2;
        }
        if (tar_fdopen(&t, fileno(stdout), tarfile,
#ifdef HAVE_LIBZ
             (use_zlib ? &gztype : NULL),
#else
             NULL,
#endif
             O_WRONLY | O_CREAT, 0644,
             (verbose == 1 ? TAR_VERBOSE : 0)
             | (store_selinux_ctx ? TAR_STORE_SELINUX : 0)
             | (use_gnu ? TAR_GNU : 0)) == -1)
        {
            fprintf(stderr, "tar_open(): %s\n", strerror(errno));
            return -1;
        }
    } else {
        if (tar_open(&t, tarfile,
#ifdef HAVE_LIBZ
             (use_zlib ? &gztype : NULL),
#else
             NULL,
#endif
             O_WRONLY | O_CREAT, 0644,
             (verbose == 1 ? TAR_VERBOSE : 0)
             | (store_selinux_ctx ? TAR_STORE_SELINUX : 0)
             | (use_gnu ? TAR_GNU : 0)) == -1)
        {
            fprintf(stderr, "tar_open(): %s\n", strerror(errno));
            return -1;
        }
    }

    libtar_listptr_reset(&lp);
    while (libtar_list_next(l, &lp) != 0)
    {
        pathname = (char *)libtar_listptr_data(&lp);
        if (pathname[0] != '/' && rootdir != NULL)
            snprintf(buf, sizeof(buf), "%s/%s", rootdir, pathname);
        else
            strlcpy(buf, pathname, sizeof(buf));
        if (tar_append_tree_with_exceptions(t, buf, pathname) != 0)
        {
            fprintf(stderr,
                "tar_append_tree(\"%s\", \"%s\"): %s\n", buf,
                pathname, strerror(errno));
            tar_close(t);
            return -1;
        }
    }

    if (tar_append_eof(t) != 0)
    {
        fprintf(stderr, "tar_append_eof(): %s\n", strerror(errno));
        tar_close(t);
        return -1;
    }

    if (tar_close(t) != 0)
    {
        fprintf(stderr, "tar_close(): %s\n", strerror(errno));
        return -1;
    }

    return 0;
}


int
list(char *tarfile)
{
    TAR *t;
    int i;

    if (strnlen(tarfile,2) == 1 && !strncmp(tarfile,"-",1))
    {
        if (tar_fdopen(&t, fileno(stdin), tarfile,
#ifdef HAVE_LIBZ
             (use_zlib ? &gztype : NULL),
#else
             NULL,
#endif
             O_RDONLY, 0,
             (verbose == 1 ? TAR_VERBOSE : 0)
             | (store_selinux_ctx ? TAR_STORE_SELINUX : 0)
             | (use_gnu ? TAR_GNU : 0)) == -1)
        {
            fprintf(stderr, "tar_open(): %s\n", strerror(errno));
            return -1;
        }
    }
    else if (tar_open(&t, tarfile,
#ifdef HAVE_LIBZ
             (use_zlib ? &gztype : NULL),
#else
             NULL,
#endif
             O_RDONLY, 0,
             (verbose == 1 ? TAR_VERBOSE : 0)
             | (store_selinux_ctx ? TAR_STORE_SELINUX : 0)
             | (use_gnu ? TAR_GNU : 0)) == -1)
    {
        fprintf(stderr, "tar_open(): %s\n", strerror(errno));
        return -1;
    }

    while ((i = th_read(t)) == 0)
    {
        th_print_long_ls(t);
#ifdef DEBUG
        th_print(t);
#endif
        if (TH_ISREG(t) && tar_skip_regfile(t) != 0)
        {
            fprintf(stderr, "tar_skip_regfile(): %s\n",
                strerror(errno));
            return -1;
        }
    }

#ifdef DEBUG
    printf("th_read() returned %d\n", i);
    printf("EOF mark encountered after %ld bytes\n",
# ifdef HAVE_LIBZ
           (use_zlib
        ? gzseek((gzFile) t->fd, 0, SEEK_CUR)
        :
# endif
           lseek(t->fd, 0, SEEK_CUR)
# ifdef HAVE_LIBZ
           )
# endif
           );
#endif

    if (tar_close(t) != 0)
    {
        fprintf(stderr, "tar_close(): %s\n", strerror(errno));
        return -1;
    }

    return 0;
}

/* Modified copy of tar_extract_all that deals with exclusions */
int
tar_extract_all_with_exceptions(TAR *t, char *prefix)
{
    char *filename;
    char buf[MAXPATHLEN];
    int i;

#ifdef DEBUG
    printf("==> tar_extract_all(TAR *t, \"%s\")\n",
            (prefix ? prefix : "(null)"));
#endif

    while ((i = th_read(t)) == 0)
    {
        int skip = 0;
#ifdef DEBUG
        puts("    tar_extract_all(): calling th_get_pathname()");
#endif
        filename = th_get_pathname(t);
        if (prefix != NULL)
            snprintf(buf, sizeof(buf), "%s/%s", prefix, filename);
        else
            strlcpy(buf, filename, sizeof(buf));

        if (exclusions) {
            int x=0;
            for (x=0; x<exclusions; x++) {
                if (!fnmatch(exclude_list[x],
                        (strchr(exclude_list[x],'/') ? buf : filename),
                        FNM_PATHNAME | FNM_LEADING_DIR)) {
                    skip=1;
                    continue;
                }
            }
        }
        if (skip) {
            tar_skip_regfile(t);
            continue;
        }

        if (t->options & TAR_VERBOSE)
            printf("%s\n", th_get_pathname(t));
        //th_print_long_ls(t);
#ifdef DEBUG
        printf("    tar_extract_all(): calling tar_extract_file(t, "
                "\"%s\")\n", buf);
#endif
        if (tar_extract_file(t, buf) != 0)
        {
            free (filename);
            return -1;
        }
        free (filename);
    }

    return (i == 1 ? 0 : -1);
}

int
extract(char *tarfile, char *rootdir)
{
    TAR *t;

#ifdef DEBUG
    puts("opening tarfile...");
#endif
    if (strnlen(tarfile,2) == 1 && !strncmp(tarfile,"-",1)) {
        if (tar_fdopen(&t, fileno(stdin), tarfile,
#ifdef HAVE_LIBZ
             (use_zlib ? &gztype : NULL),
#else
             NULL,
#endif
             O_RDONLY, 0,
             (verbose == 1 ? TAR_VERBOSE : 0)
             | (store_selinux_ctx ? TAR_STORE_SELINUX : 0)
             | (use_gnu ? TAR_GNU : 0)) == -1)
        {
            fprintf(stderr, "tar_open(): %s\n", strerror(errno));
            return -1;
        }
    } else {
        if (tar_open(&t, tarfile,
#ifdef HAVE_LIBZ
             (use_zlib ? &gztype : NULL),
#else
             NULL,
#endif
             O_RDONLY, 0,
             (verbose == 1 ? TAR_VERBOSE : 0)
             | (store_selinux_ctx ? TAR_STORE_SELINUX : 0)
             | (use_gnu ? TAR_GNU : 0)) == -1)
        {
            fprintf(stderr, "tar_open(): %s\n", strerror(errno));
            return -1;
        }
    }

#ifdef DEBUG
    puts("extracting tarfile...");
#endif
    if (tar_extract_all_with_exceptions(t, rootdir) != 0)
    {
        
        fprintf(stderr, "tar_extract_all(): %s\n", strerror(errno));
        tar_close(t);
        return -1;
    }

#ifdef DEBUG
    puts("closing tarfile...");
#endif
    if (tar_close(t) != 0)
    {
        fprintf(stderr, "tar_close(): %s\n", strerror(errno));
        return -1;
    }

    return 0;
}

static void usage() {
    printf("Usage: %s [OPTION...] [FILE]...\n", progname);
    printf("Examples:\n"
           "   tar -cf archive.tar foo bar  # Create archive.tar from files foo and bar.\n"
           "   tar -tvf archive.tar         # List all files in archive.tar verbosely.\n"
           "   tar -xf archive.tar          # Extract all files from archive.tar.\n"
    );
    printf("\n");
    printf("Main operation mode:\n"
           "   -c, --create     create a new archive\n"
           "   -t, --list       list the contents of an archive\n"
           "   -x, --extract    extract files from an archive\n"
    );
    printf("\n");
    printf("Valid Options:\n"
           "   -V, --version\n"
           "   -f, --file\n"
           "   -z, --gzip\n"
           "   -C, --directory\n"
           "   -v, --verbose\n"
           "   -H, --format [posix][gnu](default)\n"
           "   -T, --files-from\n"
           "   -s, --selinux\n"
           "   -X, --exclude\n"
    );
}

static void free_string_array(char** array, int num) {
    if (array == NULL || num == 0)
        return;

    char* cursor = array[0];
    int i = 0;
    while (i < num) {
        free(cursor);
        cursor = array[++i];
    }
    free(array);
}

#define MODE_LIST       1
#define MODE_CREATE     2
#define MODE_EXTRACT    3

int minitar_main(int argc, char **argv)
{
    char *tarfile = NULL;
    char *rootdir = NULL;
    char *file_list_path = NULL;
    int c = 0;
    int mode = 0;
    int ret = 0;
    libtar_list_t *l;

    progname = basename(argv[0]);

    static struct option long_options[] = {
        {"version", no_argument, 0, 'V'},
        {"directory", required_argument, 0, 'C'},
        {"verbose", no_argument, 0, 'v'},
        {"format", required_argument, 0, 'H'},
        {"create", no_argument, 0, 'c'},
        {"files-from", required_argument, 0, 'T'},
        {"file", required_argument, 0, 'f'},
        {"extract", no_argument, 0, 'x'},
        {"list", no_argument, 0, 't'},
        {"selinux", no_argument, 0, 's'},
        {"exclude", required_argument, 0, 'X'},
#ifdef HAVE_LIBZ
        {"gzip", no_argument, 0, 'z'},
#endif
        {0, 0, 0, 0}
    };

    int option_index = 0;
    while (ret == 0 && (c = getopt_long(argc, argv, "cf:T:C:gtvVxzsX:", long_options, &option_index)) != -1) {
        switch (c) {
            case 'V':
                printf("libtar %s by Mark D. Roth <roth@uiuc.edu>\n", libtar_version);
                printf("minitar 1.0 <2014>\n");
                break;
            case 'C':
                rootdir = strdup(optarg);
                break;
            case 'v':
                verbose = 1;
                break;
            case 'H':
                if (strcmp(optarg, "gnu") == 0)
                    use_gnu = 1; // default format
                else if (strcmp(optarg, "posix") == 0)
                    use_gnu = 0;
                else
                    ret = 2;
                break;
            case 'c':
                if (mode)
                    ret = 2;
                else
                    mode = MODE_CREATE;
                break;
            case 'T':
                file_list_path = strdup(optarg);
                break;
            case 'f':
                tarfile = strdup(optarg);
                break;
            case 'x':
                if (mode)
                    ret = 2;
                else
                    mode = MODE_EXTRACT;
                break;
            case 't':
                if (mode)
                    ret = 2;
                else
                    mode = MODE_LIST;
                break;
            case 's':
                store_selinux_ctx = 1;
                break;
            case 'X':
                if (exclusions < EXCLUDES_MAX-1) {
                    exclude_list[exclusions++] = strdup(optarg);
                } else {
                    fprintf(stderr, "Too many exclusions\n");
                    ret = 2;
                }
                break;
#ifdef HAVE_LIBZ
            case 'z':
                use_zlib = 1;
                break;
#endif /* HAVE_LIBZ */
            default:
                ret = 2;
        }
    }

    // check if we have a command with valid options
    // --files-from (-T) use
    if (file_list_path != NULL) {
        if (mode != MODE_CREATE) {
            printf("option -T (--files-from) must be used with -c\n");
            ret = 2;
        }
        if (optind < argc) {
            printf("(-T): non used options!\n");
            ret = 2;
        }
    }

    // do we have a main command ? (c, x, t)
    if (!mode)
        ret = 2;

    // extra invalid options?
    if (optind < argc && mode != MODE_CREATE) {
        printf("Non used options while in non create mode!\n");
        ret = 2;
    }

    // fail
    if (ret != 0) {
        usage();
        goto out;
    }

#ifdef DEBUG
    signal(SIGSEGV, segv_handler);
#endif

    ret = 2;
    switch (mode) {
        case MODE_EXTRACT: {
            if (tarfile == NULL)
                tarfile = strdup("-");
            ret = extract(tarfile, rootdir);
            break;
        }
        case MODE_CREATE: {
            char **file_list_entries;
            int file_list_count = 0;
            int i = 0;

            // default to stdout if -f option is not used
            if (tarfile == NULL)
                tarfile = strdup("-");
            l = libtar_list_new(LIST_QUEUE, NULL);

            if (file_list_path != NULL) {
                // process files from --files-from (-T) option
                char line[4096];
                FILE* fp = fopen(file_list_path, "rb");
                if (fp == NULL) {
                    printf("can't open file list '%s'\n", file_list_path);
                    goto out;
                }

                // get list items num
                while (fgets(line, sizeof(line), fp) != NULL) {
                    // skip empty lines (no support for file names with new line in GNU unless we use --null option
                    if (line[0] == '\n')
                        continue;
                    size_t len = strlen(line);
                    if (line[len - 1] == '\n')
                        line[len - 1] = '\0';
                    ++file_list_count;
                }

                if (file_list_count != 0) {
                    // malloc list items (needed as libtar_list_add() is just pointing to 2nd argument)
                    file_list_entries = (char**)malloc(file_list_count * sizeof(*file_list_entries));

                    // read again through files in list and add them to libtar queue
                    rewind(fp);
                    while (fgets(line, sizeof(line), fp) != NULL && i < file_list_count) {
                        // skip empty lines (no support for file names with new line in GNU unless we use --null option
                        if (line[0] == '\n')
                            continue;
                        size_t len = strlen(line);
                        if (line[len - 1] == '\n')
                            line[len - 1] = '\0';
                        file_list_entries[i] = strdup(line);
                        libtar_list_add(l, file_list_entries[i]);
                        ++i;
                    }
                }

                fclose(fp);
                if (file_list_count == 0) {
                    printf("empty file list '%s'\n", file_list_path);
                    goto out;
                }
            } else {
                // process files from command arguments
                for (c = optind; c < argc; c++) {
                    libtar_list_add(l, argv[c]);
                }
            }

            ret = create(tarfile, rootdir, l);
            libtar_list_free(l, NULL);
            if (file_list_count != 0)
                free_string_array(file_list_entries, file_list_count);
            break;
        }
        case MODE_LIST: {
            if (tarfile == NULL)
                tarfile = strdup("-");
            ret = list(tarfile);
            break;
        }
        default:
            break;
    }

out:
    if (rootdir != NULL)
        free(rootdir);
    if (tarfile != NULL)
        free(tarfile);
    if (file_list_path != NULL)
        free(file_list_path);
    while (exclusions) {
        free(exclude_list[--exclusions]);
    }

    return ret;
}
