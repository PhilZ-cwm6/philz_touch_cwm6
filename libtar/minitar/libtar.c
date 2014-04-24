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

char *progname;
int verbose = 0;
int use_gnu = 0;

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
int exclusions=0;

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


void
usage(void *rootdir)
{
	printf("Usage: %s [-C rootdir] [-g] [-z] -x|-t filename.tar\n",
	       progname);
	printf("       %s [-C rootdir] [-g] [-z] -c filename.tar ...\n",
	       progname);
	free(rootdir);
	exit(-1);
}


#define MODE_LIST	1
#define MODE_CREATE	2
#define MODE_EXTRACT	3

int
main(int argc, char *argv[])
{
	char *tarfile = NULL;
	char *rootdir = NULL;
	int c;
	int mode = 0;
	libtar_list_t *l;
	int return_code = -2;

	progname = basename(argv[0]);

	while ((c = getopt(argc, argv, "cC:gtvVxzsX:")) != -1)
		switch (c)
		{
		case 'V':
			printf("libtar %s by Mark D. Roth <roth@uiuc.edu>\n",
			       libtar_version);
			break;
		case 'C':
			rootdir = strdup(optarg);
			break;
		case 'v':
			verbose = 1;
			break;
		case 'g':
			use_gnu = 1;
			break;
		case 'c':
			if (mode)
				usage(rootdir);
			mode = MODE_CREATE;
			break;
		case 'x':
			if (mode)
				usage(rootdir);
			mode = MODE_EXTRACT;
			break;
		case 't':
			if (mode)
				usage(rootdir);
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
				exit(1);
			}
			break;
#ifdef HAVE_LIBZ
		case 'z':
			use_zlib = 1;
			break;
#endif /* HAVE_LIBZ */
		default:
			usage(rootdir);
		}

	if (!mode || ((argc - optind) < (mode == MODE_CREATE ? 2 : 1)))
	{
#ifdef DEBUG
		printf("argc - optind == %d\tmode == %d\n", argc - optind,
		       mode);
#endif
		usage(rootdir);
	}

#ifdef DEBUG
	signal(SIGSEGV, segv_handler);
#endif

	switch (mode)
	{
	case MODE_EXTRACT:
		return_code = extract(argv[optind], rootdir);
		break;
	case MODE_CREATE:
		tarfile = argv[optind];
		l = libtar_list_new(LIST_QUEUE, NULL);
		for (c = optind + 1; c < argc; c++)
			libtar_list_add(l, argv[c]);
		return_code =  create(tarfile, rootdir, l);
		libtar_list_free (l, NULL);
		break;
	case MODE_LIST:
		return_code = list(argv[optind]);
		break;
	default:
		break;
	}

	free(rootdir);
	while (exclusions) {
		free(exclude_list[--exclusions]);
	}
	return return_code;
}


