/*
**  Copyright 1998-2003 University of Illinois Board of Trustees
**  Copyright 1998-2003 Mark D. Roth
**  All rights reserved.
**
**  extract.c - libtar code to extract a file from a tar archive
**
**  Mark D. Roth <roth@uiuc.edu>
**  Campus Information Technologies and Educational Services
**  University of Illinois at Urbana-Champaign
*/

#include <internal.h>

#include <stdio.h>
#include <sys/param.h>
#include <sys/types.h>
#include <fcntl.h>
#include <errno.h>
#include <utime.h>

#ifdef STDC_HEADERS
# include <stdlib.h>
# include <string.h>
#endif

#ifdef HAVE_UNISTD_H
# include <unistd.h>
#endif

#ifdef HAVE_SELINUX
# include <selinux/selinux.h>
#endif

struct linkname
{
	char ln_save[MAXPATHLEN];
	char ln_real[MAXPATHLEN];
};
typedef struct linkname linkname_t;


static int
tar_set_file_perms(TAR *t, const char *realname)
{
	mode_t mode;
	uid_t uid;
	gid_t gid;
	struct utimbuf ut;
	const char *filename;
	char *pn;

	pn = th_get_pathname(t);
	filename = (realname ? realname : pn);
	mode = th_get_mode(t);
	uid = th_get_uid(t);
	gid = th_get_gid(t);
	ut.modtime = ut.actime = th_get_mtime(t);

	/* change owner/group */
	if (geteuid() == 0)
#ifdef HAVE_LCHOWN
		if (lchown(filename, uid, gid) == -1)
		{
# ifdef DEBUG
			fprintf(stderr, "lchown(\"%s\", %d, %d): %s\n",
				filename, uid, gid, strerror(errno));
# endif
#else /* ! HAVE_LCHOWN */
		if (!TH_ISSYM(t) && chown(filename, uid, gid) == -1)
		{
# ifdef DEBUG
			fprintf(stderr, "chown(\"%s\", %d, %d): %s\n",
				filename, uid, gid, strerror(errno));
# endif
#endif /* HAVE_LCHOWN */
			free (pn);
			return -1;
		}

	/* change access/modification time */
	if (!TH_ISSYM(t) && utime(filename, &ut) == -1)
	{
#ifdef DEBUG
		perror("utime()");
#endif
		free (pn);
		return -1;
	}

	/* change permissions */
	if (!TH_ISSYM(t) && chmod(filename, mode) == -1)
	{
#ifdef DEBUG
		perror("chmod()");
#endif
		free (pn);
		return -1;
	}

	free (pn);
	return 0;
}


/* switchboard */
int
tar_extract_file(TAR *t, const char *realname)
{
	int i;
	linkname_t *lnp;
	char *pathname;

	if (t->options & TAR_NOOVERWRITE)
	{
		struct stat s;

		if (lstat(realname, &s) == 0 || errno != ENOENT)
		{
			errno = EEXIST;
			return -1;
		}
	}

	if (TH_ISDIR(t))
	{
		i = tar_extract_dir(t, realname);
		if (i == 1)
			i = 0;
	}
	else if (TH_ISLNK(t))
		i = tar_extract_hardlink(t, realname);
	else if (TH_ISSYM(t))
		i = tar_extract_symlink(t, realname);
	else if (TH_ISCHR(t))
		i = tar_extract_chardev(t, realname);
	else if (TH_ISBLK(t))
		i = tar_extract_blockdev(t, realname);
	else if (TH_ISFIFO(t))
		i = tar_extract_fifo(t, realname);
	else /* if (TH_ISREG(t)) */
		i = tar_extract_regfile(t, realname);

	if (i != 0)
		return i;

	i = tar_set_file_perms(t, realname);
	if (i != 0)
		return i;

#ifdef HAVE_SELINUX
	if((t->options & TAR_STORE_SELINUX) && t->th_buf.selinux_context != NULL)
	{
#ifdef DEBUG
		printf("    Restoring SELinux context %s to file %s\n", t->th_buf.selinux_context, realname);
#endif
		if(lsetfilecon(realname, t->th_buf.selinux_context) < 0)
			fprintf(stderr, "Failed to restore SELinux context %s!\n", strerror(errno));
	}
#endif

	lnp = (linkname_t *)calloc(1, sizeof(linkname_t));
	if (lnp == NULL)
		return -1;
	pathname = th_get_pathname(t);
	strlcpy(lnp->ln_save, pathname, sizeof(lnp->ln_save));
	strlcpy(lnp->ln_real, realname, sizeof(lnp->ln_real));
#ifdef DEBUG
	printf("tar_extract_file(): calling libtar_hash_add(): key=\"%s\", "
	       "value=\"%s\"\n", pathname, realname);
#endif
	free(pathname);
	if (libtar_hash_add(t->h, lnp) != 0)
		return -1;

	return 0;
}


/* extract regular file */
int
tar_extract_regfile(TAR *t, const char *realname)
{
	mode_t mode;
	size_t size;
	uid_t uid;
	gid_t gid;
	int fdout;
	int i, k;
	char buf[T_BLOCKSIZE];
	const char *filename;
	char *pn;

#ifdef DEBUG
	printf("==> tar_extract_regfile(t=0x%lx, realname=\"%s\")\n", t,
	       realname);
#endif

	if (!TH_ISREG(t))
	{
		errno = EINVAL;
		return -1;
	}

	pn = th_get_pathname(t);
	filename = (realname ? realname : pn);
	mode = th_get_mode(t);
	size = th_get_size(t);
	uid = th_get_uid(t);
	gid = th_get_gid(t);

	if (mkdirhier(dirname(filename)) == -1)
	{
		free (pn);
		return -1;
	}

#ifdef DEBUG
	printf("  ==> extracting: %s (mode %04o, uid %d, gid %d, %d bytes)\n",
	       filename, mode, uid, gid, size);
#endif
	fdout = open(filename, O_WRONLY | O_CREAT | O_TRUNC
#ifdef O_BINARY
		     | O_BINARY
#endif
		    , 0666);
	if (fdout == -1)
	{
#ifdef DEBUG
		perror("open()");
#endif
		free (pn);
		return -1;
	}

#if 0
	/* change the owner.  (will only work if run as root) */
	if (fchown(fdout, uid, gid) == -1 && errno != EPERM)
	{
#ifdef DEBUG
		perror("fchown()");
#endif
		return -1;
	}

	/* make sure the mode isn't inheritted from a file we're overwriting */
	if (fchmod(fdout, mode & 07777) == -1)
	{
#ifdef DEBUG
		perror("fchmod()");
#endif
		return -1;
	}
#endif

	/* extract the file */
	for (i = size; i > 0; i -= T_BLOCKSIZE)
	{
		k = tar_block_read(t, buf);
		if (k != T_BLOCKSIZE)
		{
			if (k != -1)
				errno = EINVAL;
			free (pn);
			return -1;
		}

		/* write block to output file */
		if (write(fdout, buf,
			  ((i > T_BLOCKSIZE) ? T_BLOCKSIZE : i)) == -1)
		{
			free (pn);
			return -1;
		}
	}

	/* close output file */
	if (close(fdout) == -1)
	{
		free (pn);
		return -1;
	}

#ifdef DEBUG
	printf("### done extracting %s\n", filename);
#endif
	free (pn);
	return 0;
}


/* skip regfile */
int
tar_skip_regfile(TAR *t)
{
	int i, k;
	size_t size;
	char buf[T_BLOCKSIZE];

	if (!TH_ISREG(t))
	{
		errno = EINVAL;
		return -1;
	}

	size = th_get_size(t);
	for (i = size; i > 0; i -= T_BLOCKSIZE)
	{
		k = tar_block_read(t, buf);
		if (k != T_BLOCKSIZE)
		{
			if (k != -1)
				errno = EINVAL;
			return -1;
		}
	}

	return 0;
}


/* hardlink */
int
tar_extract_hardlink(TAR * t, const char *realname)
{
	const char *filename;
	char *pn;
	char *linktgt = NULL;
	linkname_t *lnp;
	libtar_hashptr_t hp;

	if (!TH_ISLNK(t))
	{
		errno = EINVAL;
		return -1;
	}

	pn = th_get_pathname(t);
	filename = (realname ? realname : pn);
	if (mkdirhier(dirname(filename)) == -1)
	{
		free (pn);
		return -1;
	}
	libtar_hashptr_reset(&hp);
	if (libtar_hash_getkey(t->h, &hp, th_get_linkname(t),
			       (libtar_matchfunc_t)libtar_str_match) != 0)
	{
		lnp = (linkname_t *)libtar_hashptr_data(&hp);
		linktgt = lnp->ln_real;
	}
	else
		linktgt = th_get_linkname(t);

#ifdef DEBUG
	printf("  ==> extracting: %s (link to %s)\n", filename, linktgt);
#endif
	if (link(linktgt, filename) == -1)
	{
#ifdef DEBUG
		perror("link()");
#endif
		free (pn);
		return -1;
	}

	free (pn);
	return 0;
}


/* symlink */
int
tar_extract_symlink(TAR *t, const char *realname)
{
	const char *filename;
	char *pn;

	if (!TH_ISSYM(t))
	{
		errno = EINVAL;
		return -1;
	}

	pn = th_get_pathname(t);
	filename = (realname ? realname : pn);
	if (mkdirhier(dirname(filename)) == -1)
	{
		free (pn);
		return -1;
	}

	if (unlink(filename) == -1 && errno != ENOENT)
		return -1;

#ifdef DEBUG
	printf("  ==> extracting: %s (symlink to %s)\n",
	       filename, th_get_linkname(t));
#endif
	if (symlink(th_get_linkname(t), filename) == -1)
	{
#ifdef DEBUG
		perror("symlink()");
#endif
		free (pn);
		return -1;
	}

	free (pn);
	return 0;
}


/* character device */
int
tar_extract_chardev(TAR *t, const char *realname)
{
	mode_t mode;
	unsigned long devmaj, devmin;
	const char *filename;
	char *pn;

	if (!TH_ISCHR(t))
	{
		errno = EINVAL;
		return -1;
	}

	pn = th_get_pathname(t);
	filename = (realname ? realname : pn);
	mode = th_get_mode(t);
	devmaj = th_get_devmajor(t);
	devmin = th_get_devminor(t);

	if (mkdirhier(dirname(filename)) == -1)
	{
		free (pn);
		return -1;
	}
	
#ifdef DEBUG
	printf("  ==> extracting: %s (character device %ld,%ld)\n",
	       filename, devmaj, devmin);
#endif
	if (mknod(filename, mode | S_IFCHR,
		  compat_makedev(devmaj, devmin)) == -1)
	{
#ifdef DEBUG
		perror("mknod()");
#endif
		free (pn);
		return -1;
	}

	free (pn);
	return 0;
}


/* block device */
int
tar_extract_blockdev(TAR *t, const char *realname)
{
	mode_t mode;
	unsigned long devmaj, devmin;
	const char *filename;
	char *pn;

	if (!TH_ISBLK(t))
	{
		errno = EINVAL;
		return -1;
	}

	pn = th_get_pathname(t);
	filename = (realname ? realname : pn);
	mode = th_get_mode(t);
	devmaj = th_get_devmajor(t);
	devmin = th_get_devminor(t);

	if (mkdirhier(dirname(filename)) == -1)
	{
		free (pn);
		return -1;
	}

#ifdef DEBUG
	printf("  ==> extracting: %s (block device %ld,%ld)\n",
	       filename, devmaj, devmin);
#endif
	if (mknod(filename, mode | S_IFBLK,
		  compat_makedev(devmaj, devmin)) == -1)
	{
#ifdef DEBUG
		perror("mknod()");
#endif
		free (pn);
		return -1;
	}

	free (pn);
	return 0;
}


/* directory */
int
tar_extract_dir(TAR *t, const char *realname)
{
	mode_t mode;
	const char *filename;
	char *pn;

	if (!TH_ISDIR(t))
	{
		errno = EINVAL;
		return -1;
	}
	pn = th_get_pathname(t);
	filename = (realname ? realname : pn);
	mode = th_get_mode(t);

	if (mkdirhier(dirname(filename)) == -1)
	{
		free (pn);
		return -1;
	}

#ifdef DEBUG
	printf("  ==> extracting: %s (mode %04o, directory)\n", filename,
	       mode);
#endif
	if (mkdir(filename, mode) == -1)
	{
		if (errno == EEXIST)
		{
			if (chmod(filename, mode) == -1)
			{
#ifdef DEBUG
				perror("chmod()");
#endif
				free (pn);
				return -1;
			}
			else
			{
#ifdef DEBUG
				puts("  *** using existing directory");
#endif
				free (pn);
				return 1;
			}
		}
		else
		{
#ifdef DEBUG
			perror("mkdir()");
#endif
			free (pn);
			return -1;
		}
	}
	
	free (pn);
	return 0;
}


/* FIFO */
int
tar_extract_fifo(TAR *t, const char *realname)
{
	mode_t mode;
	const char *filename;
	char *pn;

	if (!TH_ISFIFO(t))
	{
		errno = EINVAL;
		return -1;
	}

	pn = th_get_pathname(t);
	filename = (realname ? realname : pn);
	mode = th_get_mode(t);

	if (mkdirhier(dirname(filename)) == -1)
	{
		free (pn);
		return -1;
	}

#ifdef DEBUG
	printf("  ==> extracting: %s (fifo)\n", filename);
#endif
	if (mkfifo(filename, mode) == -1)
	{
#ifdef DEBUG
		perror("mkfifo()");
#endif
		free (pn);
		return -1;
	}

	free (pn);
	return 0;
}


