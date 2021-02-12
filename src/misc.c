/* Miscellaneous functions, not really specific to GNU tar.

   Copyright 1988-2021 Free Software Foundation, Inc.

   This program is free software; you can redistribute it and/or modify it
   under the terms of the GNU General Public License as published by the
   Free Software Foundation; either version 3, or (at your option) any later
   version.

   This program is distributed in the hope that it will be useful, but
   WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General
   Public License for more details.

   You should have received a copy of the GNU General Public License along
   with this program.  If not, see <http://www.gnu.org/licenses/>.  */

#define COMMON_INLINE _GL_EXTERN_INLINE
#include <system.h>
#include <rmt.h>
#include "common.h"
#include <quotearg.h>
#include <xgetcwd.h>
#include <unlinkdir.h>
#include <utimens.h>

#ifndef DOUBLE_SLASH_IS_DISTINCT_ROOT
# define DOUBLE_SLASH_IS_DISTINCT_ROOT 0
#endif

static void namebuf_add_dir (namebuf_t, char const *);
static char *namebuf_finish (namebuf_t);
static const char *tar_getcdpath (int);

char const *
quote_n_colon (int n, char const *arg)
{
  return quotearg_n_style_colon (n, get_quoting_style (NULL), arg);
}

/* Handling strings.  */

/* Assign STRING to a copy of VALUE if not zero, or to zero.  If
   STRING was nonzero, it is freed first.  */
void
assign_string (char **string, const char *value)
{
  free (*string);
  *string = value ? xstrdup (value) : 0;
}

void
assign_string_n (char **string, const char *value, size_t n)
{
  free (*string);
  if (value)
    {
      size_t l = strnlen (value, n);
      char *p = xmalloc (l + 1);
      memcpy (p, value, l);
      p[l] = 0;
      *string = p;
    }
}

#if 0
/* This function is currently unused; perhaps it should be removed?  */

/* Allocate a copy of the string quoted as in C, and returns that.  If
   the string does not have to be quoted, it returns a null pointer.
   The allocated copy should normally be freed with free() after the
   caller is done with it.

   This is used in one context only: generating the directory file in
   incremental dumps.  The quoted string is not intended for human
   consumption; it is intended only for unquote_string.  The quoting
   is locale-independent, so that users needn't worry about locale
   when reading directory files.  This means that we can't use
   quotearg, as quotearg is locale-dependent and is meant for human
   consumption.  */
static char *
quote_copy_string (const char *string)
{
  const char *source = string;
  char *destination = 0;
  char *buffer = 0;
  int copying = 0;

  while (*source)
    {
      int character = *source++;

      switch (character)
	{
	case '\n': case '\\':
	  if (!copying)
	    {
	      size_t length = (source - string) - 1;

	      copying = 1;
	      buffer = xmalloc (length + 2 + 2 * strlen (source) + 1);
	      memcpy (buffer, string, length);
	      destination = buffer + length;
	    }
	  *destination++ = '\\';
	  *destination++ = character == '\\' ? '\\' : 'n';
	  break;

	default:
	  if (copying)
	    *destination++ = character;
	  break;
	}
    }
  if (copying)
    {
      *destination = '\0';
      return buffer;
    }
  return 0;
}
#endif

/* Takes a quoted C string (like those produced by quote_copy_string)
   and turns it back into the un-quoted original.  This is done in
   place.  Returns 0 only if the string was not properly quoted, but
   completes the unquoting anyway.

   This is used for reading the saved directory file in incremental
   dumps.  It is used for decoding old 'N' records (demangling names).
   But also, it is used for decoding file arguments, would they come
   from the shell or a -T file, and for decoding the --exclude
   argument.  */
int
unquote_string (char *string)
{
  int result = 1;
  char *source = string;
  char *destination = string;

  /* Escape sequences other than \\ and \n are no longer generated by
     quote_copy_string, but accept them for backwards compatibility,
     and also because unquote_string is used for purposes other than
     parsing the output of quote_copy_string.  */

  while (*source)
    if (*source == '\\')
      switch (*++source)
	{
	case '\\':
	  *destination++ = '\\';
	  source++;
	  break;

	case 'a':
	  *destination++ = '\a';
	  source++;
	  break;

	case 'b':
	  *destination++ = '\b';
	  source++;
	  break;

	case 'f':
	  *destination++ = '\f';
	  source++;
	  break;

	case 'n':
	  *destination++ = '\n';
	  source++;
	  break;

	case 'r':
	  *destination++ = '\r';
	  source++;
	  break;

	case 't':
	  *destination++ = '\t';
	  source++;
	  break;

	case 'v':
	  *destination++ = '\v';
	  source++;
	  break;

	case '?':
	  *destination++ = 0177;
	  source++;
	  break;

	case '0':
	case '1':
	case '2':
	case '3':
	case '4':
	case '5':
	case '6':
	case '7':
	  {
	    int value = *source++ - '0';

	    if (*source < '0' || *source > '7')
	      {
		*destination++ = value;
		break;
	      }
	    value = value * 8 + *source++ - '0';
	    if (*source < '0' || *source > '7')
	      {
		*destination++ = value;
		break;
	      }
	    value = value * 8 + *source++ - '0';
	    *destination++ = value;
	    break;
	  }

	default:
	  result = 0;
	  *destination++ = '\\';
	  if (*source)
	    *destination++ = *source++;
	  break;
	}
    else if (source != destination)
      *destination++ = *source++;
    else
      source++, destination++;

  if (source != destination)
    *destination = '\0';
  return result;
}

/* Zap trailing slashes.  */
char *
zap_slashes (char *name)
{
  char *q;

  if (!name || *name == 0)
    return name;
  q = name + strlen (name) - 1;
  while (q > name && ISSLASH (*q))
    *q-- = '\0';
  return name;
}

/* Normalize FILE_NAME by removing redundant slashes and "."
   components, including redundant trailing slashes.
   Leave ".." alone, as it may be significant in the presence
   of symlinks and on platforms where "/.." != "/".

   Destructive version: modifies its argument. */
void
normalize_filename_x (char *file_name)
{
  char *name = file_name + FILE_SYSTEM_PREFIX_LEN (file_name);
  char *p;
  char const *q;
  char c;

  /* Don't squeeze leading "//" to "/", on hosts where they're distinct.  */
  name += (DOUBLE_SLASH_IS_DISTINCT_ROOT
	   && ISSLASH (*name) && ISSLASH (name[1]) && ! ISSLASH (name[2]));

  /* Omit redundant leading "." components.  */
  for (q = p = name; (*p = *q) == '.' && ISSLASH (q[1]); p += !*q)
    for (q += 2; ISSLASH (*q); q++)
      continue;

  /* Copy components from Q to P, omitting redundant slashes and
     internal "."  components.  */
  while ((*p++ = c = *q++) != '\0')
    if (ISSLASH (c))
      while (ISSLASH (q[*q == '.']))
	q += (*q == '.') + 1;

  /* Omit redundant trailing "." component and slash.  */
  if (2 < p - name)
    {
      p -= p[-2] == '.' && ISSLASH (p[-3]);
      p -= 2 < p - name && ISSLASH (p[-2]);
      p[-1] = '\0';
    }
}

/* Normalize NAME by removing redundant slashes and "." components,
   including redundant trailing slashes.

   Return a normalized newly-allocated copy.  */

char *
normalize_filename (int cdidx, const char *name)
{
  char *copy = NULL;

  if (IS_RELATIVE_FILE_NAME (name))
    {
      /* Set COPY to the absolute path for this name.

         FIXME: There should be no need to get the absolute file name.
         tar_getcdpath does not return a true "canonical" path, so
         this following approach may lead to situations where the same
         file or directory is processed twice under different absolute
         paths without that duplication being detected.  Perhaps we
         should use dev+ino pairs instead of names?  (See listed03.at for
         a related test case.) */
      const char *cdpath = tar_getcdpath (cdidx);
      size_t copylen;
      bool need_separator;

      copylen = strlen (cdpath);
      need_separator = ! (DOUBLE_SLASH_IS_DISTINCT_ROOT
			  && copylen == 2 && ISSLASH (cdpath[1]));
      copy = xmalloc (copylen + need_separator + strlen (name) + 1);
      strcpy (copy, cdpath);
      copy[copylen] = DIRECTORY_SEPARATOR;
      strcpy (copy + copylen + need_separator, name);
    }

  if (!copy)
    copy = xstrdup (name);
  normalize_filename_x (copy);
  return copy;
}


void
replace_prefix (char **pname, const char *samp, size_t slen,
		const char *repl, size_t rlen)
{
  char *name = *pname;
  size_t nlen = strlen (name);
  if (nlen > slen && memcmp (name, samp, slen) == 0 && ISSLASH (name[slen]))
    {
      if (rlen > slen)
	{
	  name = xrealloc (name, nlen - slen + rlen + 1);
	  *pname = name;
	}
      memmove (name + rlen, name + slen, nlen - slen + 1);
      memcpy (name, repl, rlen);
    }
}


/* Handling numbers.  */

/* Convert VALUE, which is converted from a system integer type whose
   minimum value is MINVAL and maximum MINVAL, to an decimal
   integer string.  Use the storage in BUF and return a pointer to the
   converted string.  If VALUE is converted from a negative integer in
   the range MINVAL .. -1, represent it with a string representation
   of the negative integer, using leading '-'.  */
#if ! (INTMAX_MAX <= UINTMAX_MAX / 2)
# error "sysinttostr: uintmax_t cannot represent all intmax_t values"
#endif
char *
sysinttostr (uintmax_t value, intmax_t minval, uintmax_t maxval,
	     char buf[SYSINT_BUFSIZE])
{
  if (value <= maxval)
    return umaxtostr (value, buf);
  else
    {
      intmax_t i = value - minval;
      return imaxtostr (i + minval, buf);
    }
}

/* Convert a prefix of the string ARG to a system integer type whose
   minimum value is MINVAL and maximum MAXVAL.  If MINVAL is negative,
   negative integers MINVAL .. -1 are assumed to be represented using
   leading '-' in the usual way.  If the represented value exceeds
   INTMAX_MAX, return a negative integer V such that (uintmax_t) V
   yields the represented value.  If ARGLIM is nonnull, store into
   *ARGLIM a pointer to the first character after the prefix.

   This is the inverse of sysinttostr.

   On a normal return, set errno = 0.
   On conversion error, return 0 and set errno = EINVAL.
   On overflow, return an extreme value and set errno = ERANGE.  */
#if ! (INTMAX_MAX <= UINTMAX_MAX)
# error "strtosysint: nonnegative intmax_t does not fit in uintmax_t"
#endif
intmax_t
strtosysint (char const *arg, char **arglim, intmax_t minval, uintmax_t maxval)
{
  errno = 0;
  if (maxval <= INTMAX_MAX)
    {
      if (ISDIGIT (arg[*arg == '-']))
	{
	  intmax_t i = strtoimax (arg, arglim, 10);
	  intmax_t imaxval = maxval;
	  if (minval <= i && i <= imaxval)
	    return i;
	  errno = ERANGE;
	  return i < minval ? minval : maxval;
	}
    }
  else
    {
      if (ISDIGIT (*arg))
	{
	  uintmax_t i = strtoumax (arg, arglim, 10);
	  if (i <= maxval)
	    return represent_uintmax (i);
	  errno = ERANGE;
	  return maxval;
	}
    }

  errno = EINVAL;
  return 0;
}

/* Output fraction and trailing digits appropriate for a nanoseconds
   count equal to NS, but don't output unnecessary '.' or trailing
   zeros.  */

void
code_ns_fraction (int ns, char *p)
{
  if (ns == 0)
    *p = '\0';
  else
    {
      int i = 9;
      *p++ = '.';

      while (ns % 10 == 0)
	{
	  ns /= 10;
	  i--;
	}

      p[i] = '\0';

      for (;;)
	{
	  p[--i] = '0' + ns % 10;
	  if (i == 0)
	    break;
	  ns /= 10;
	}
    }
}

char const *
code_timespec (struct timespec t, char sbuf[TIMESPEC_STRSIZE_BOUND])
{
  time_t s = t.tv_sec;
  int ns = t.tv_nsec;
  char *np;
  bool negative = s < 0;

  /* ignore invalid values of ns */
  if (BILLION <= ns || ns < 0)
    ns = 0;

  if (negative && ns != 0)
    {
      s++;
      ns = BILLION - ns;
    }

  np = umaxtostr (negative ? - (uintmax_t) s : (uintmax_t) s, sbuf + 1);
  if (negative)
    *--np = '-';
  code_ns_fraction (ns, sbuf + UINTMAX_STRSIZE_BOUND);
  return np;
}

struct timespec
decode_timespec (char const *arg, char **arg_lim, bool parse_fraction)
{
  time_t s = TYPE_MINIMUM (time_t);
  int ns = -1;
  char const *p = arg;
  bool negative = *arg == '-';
  struct timespec r;

  if (! ISDIGIT (arg[negative]))
    errno = EINVAL;
  else
    {
      errno = 0;

      if (negative)
	{
	  intmax_t i = strtoimax (arg, arg_lim, 10);
	  if (TYPE_SIGNED (time_t) ? TYPE_MINIMUM (time_t) <= i : 0 <= i)
	    s = i;
	  else
	    errno = ERANGE;
	}
      else
	{
	  uintmax_t i = strtoumax (arg, arg_lim, 10);
	  if (i <= TYPE_MAXIMUM (time_t))
	    s = i;
	  else
	    errno = ERANGE;
	}

      p = *arg_lim;
      ns = 0;

      if (parse_fraction && *p == '.')
	{
	  int digits = 0;
	  bool trailing_nonzero = false;

	  while (ISDIGIT (*++p))
	    if (digits < LOG10_BILLION)
	      digits++, ns = 10 * ns + (*p - '0');
	    else
	      trailing_nonzero |= *p != '0';

	  while (digits < LOG10_BILLION)
	    digits++, ns *= 10;

	  if (negative)
	    {
	      /* Convert "-1.10000000000001" to s == -2, ns == 89999999.
		 I.e., truncate time stamps towards minus infinity while
		 converting them to internal form.  */
	      ns += trailing_nonzero;
	      if (ns != 0)
		{
		  if (s == TYPE_MINIMUM (time_t))
		    ns = -1;
		  else
		    {
		      s--;
		      ns = BILLION - ns;
		    }
		}
	    }
	}

      if (errno == ERANGE)
	ns = -1;
    }

  *arg_lim = (char *) p;
  r.tv_sec = s;
  r.tv_nsec = ns;
  return r;
}

/* File handling.  */

/* Saved names in case backup needs to be undone.  */
static char *before_backup_name;
static char *after_backup_name;

/* Return 1 if FILE_NAME is obviously "." or "/".  */
bool
must_be_dot_or_slash (char const *file_name)
{
  file_name += FILE_SYSTEM_PREFIX_LEN (file_name);

  if (ISSLASH (file_name[0]))
    {
      for (;;)
	if (ISSLASH (file_name[1]))
	  file_name++;
	else if (file_name[1] == '.'
                 && ISSLASH (file_name[2 + (file_name[2] == '.')]))
	  file_name += 2 + (file_name[2] == '.');
	else
	  return ! file_name[1];
    }
  else
    {
      while (file_name[0] == '.' && ISSLASH (file_name[1]))
	{
	  file_name += 2;
	  while (ISSLASH (*file_name))
	    file_name++;
	}

      return ! file_name[0] || (file_name[0] == '.' && ! file_name[1]);
    }
}

/* Some implementations of rmdir let you remove '.' or '/'.
   Report an error with errno set to zero for obvious cases of this;
   otherwise call rmdir.  */
static int
safer_rmdir (const char *file_name)
{
  if (must_be_dot_or_slash (file_name))
    {
      errno = 0;
      return -1;
    }

  if (unlinkat (chdir_fd, file_name, AT_REMOVEDIR) == 0)
    {
      remove_delayed_set_stat (file_name);
      return 0;
    }
  return -1;
}

/* Remove FILE_NAME, returning 1 on success.  If FILE_NAME is a directory,
   then if OPTION is RECURSIVE_REMOVE_OPTION is set remove FILE_NAME
   recursively; otherwise, remove it only if it is empty.  If FILE_NAME is
   a directory that cannot be removed (e.g., because it is nonempty)
   and if OPTION is WANT_DIRECTORY_REMOVE_OPTION, then return -1.
   Return 0 on error, with errno set; if FILE_NAME is obviously the working
   directory return zero with errno set to zero.  */
int
remove_any_file (const char *file_name, enum remove_option option)
{
  /* Try unlink first if we cannot unlink directories, as this saves
     us a system call in the common case where we're removing a
     non-directory.  */
  bool try_unlink_first = cannot_unlink_dir ();

  if (try_unlink_first)
    {
      if (unlinkat (chdir_fd, file_name, 0) == 0)
	return 1;

      /* POSIX 1003.1-2001 requires EPERM when attempting to unlink a
	 directory without appropriate privileges, but many Linux
	 kernels return the more-sensible EISDIR.  */
      if (errno != EPERM && errno != EISDIR)
	return 0;
    }

  if (safer_rmdir (file_name) == 0)
    return 1;

  switch (errno)
    {
    case ENOTDIR:
      return !try_unlink_first && unlinkat (chdir_fd, file_name, 0) == 0;

    case 0:
    case EEXIST:
#if defined ENOTEMPTY && ENOTEMPTY != EEXIST
    case ENOTEMPTY:
#endif
      switch (option)
	{
	case ORDINARY_REMOVE_OPTION:
	  break;

	case WANT_DIRECTORY_REMOVE_OPTION:
	  return -1;

	case RECURSIVE_REMOVE_OPTION:
	  {
	    char *directory = tar_savedir (file_name, 0);
	    char const *entry;
	    size_t entrylen;

	    if (! directory)
	      return 0;

	    for (entry = directory;
		 (entrylen = strlen (entry)) != 0;
		 entry += entrylen + 1)
	      {
		char *file_name_buffer = make_file_name (file_name, entry);
		int r = remove_any_file (file_name_buffer,
                                         RECURSIVE_REMOVE_OPTION);
		int e = errno;
		free (file_name_buffer);

		if (! r)
		  {
		    free (directory);
		    errno = e;
		    return 0;
		  }
	      }

	    free (directory);
	    return safer_rmdir (file_name) == 0;
	  }
	}
      break;
    }

  return 0;
}

/* Check if FILE_NAME already exists and make a backup of it right now.
   Return success (nonzero) only if the backup is either unneeded, or
   successful.  For now, directories are considered to never need
   backup.  If THIS_IS_THE_ARCHIVE is nonzero, this is the archive and
   so, we do not have to backup block or character devices, nor remote
   entities.  */
bool
maybe_backup_file (const char *file_name, bool this_is_the_archive)
{
  struct stat file_stat;

  assign_string (&before_backup_name, file_name);

  /* A run situation may exist between Emacs or other GNU programs trying to
     make a backup for the same file simultaneously.  If theoretically
     possible, real problems are unlikely.  Doing any better would require a
     convention, GNU-wide, for all programs doing backups.  */

  assign_string (&after_backup_name, 0);

  /* Check if we really need to backup the file.  */

  if (this_is_the_archive && _remdev (file_name))
    return true;

  if (deref_stat (file_name, &file_stat) != 0)
    {
      if (errno == ENOENT)
	return true;

      stat_error (file_name);
      return false;
    }

  if (S_ISDIR (file_stat.st_mode))
    return true;

  if (this_is_the_archive
      && (S_ISBLK (file_stat.st_mode) || S_ISCHR (file_stat.st_mode)))
    return true;

  after_backup_name = find_backup_file_name (chdir_fd, file_name, backup_type);
  if (! after_backup_name)
    xalloc_die ();

  if (renameat (chdir_fd, before_backup_name, chdir_fd, after_backup_name)
      == 0)
    {
      if (verbose_option)
	fprintf (stdlis, _("Renaming %s to %s\n"),
		 quote_n (0, before_backup_name),
		 quote_n (1, after_backup_name));
      return true;
    }
  else
    {
      /* The backup operation failed.  */
      int e = errno;
      ERROR ((0, e, _("%s: Cannot rename to %s"),
	      quotearg_colon (before_backup_name),
	      quote_n (1, after_backup_name)));
      assign_string (&after_backup_name, 0);
      return false;
    }
}

/* Try to restore the recently backed up file to its original name.
   This is usually only needed after a failed extraction.  */
void
undo_last_backup (void)
{
  if (after_backup_name)
    {
      if (renameat (chdir_fd, after_backup_name, chdir_fd, before_backup_name)
	  != 0)
	{
	  int e = errno;
	  ERROR ((0, e, _("%s: Cannot rename to %s"),
		  quotearg_colon (after_backup_name),
		  quote_n (1, before_backup_name)));
	}
      if (verbose_option)
	fprintf (stdlis, _("Renaming %s back to %s\n"),
		 quote_n (0, after_backup_name),
		 quote_n (1, before_backup_name));
      assign_string (&after_backup_name, 0);
    }
}

/* Apply either stat or lstat to (NAME, BUF), depending on the
   presence of the --dereference option.  NAME is relative to the
   most-recent argument to chdir_do.  */
int
deref_stat (char const *name, struct stat *buf)
{
  return fstatat (chdir_fd, name, buf, fstatat_flags);
}

/* Read from FD into the buffer BUF with COUNT bytes.  Attempt to fill
   BUF.  Wait until input is available; this matters because files are
   opened O_NONBLOCK for security reasons, and on some file systems
   this can cause read to fail with errno == EAGAIN.  Return the
   actual number of bytes read, zero for EOF, or
   SAFE_READ_ERROR upon error.  */
size_t
blocking_read (int fd, void *buf, size_t count)
{
  size_t bytes = safe_read (fd, buf, count);

#if defined F_SETFL && O_NONBLOCK
  if (bytes == SAFE_READ_ERROR && errno == EAGAIN)
    {
      int flags = fcntl (fd, F_GETFL);
      if (0 <= flags && flags & O_NONBLOCK
	  && fcntl (fd, F_SETFL, flags & ~O_NONBLOCK) != -1)
	bytes = safe_read (fd, buf, count);
    }
#endif

  return bytes;
}

/* Write to FD from the buffer BUF with COUNT bytes.  Do a full write.
   Wait until an output buffer is available; this matters because
   files are opened O_NONBLOCK for security reasons, and on some file
   systems this can cause write to fail with errno == EAGAIN.  Return
   the actual number of bytes written, setting errno if that is less
   than COUNT.  */
size_t
blocking_write (int fd, void const *buf, size_t count)
{
  size_t bytes = full_write (fd, buf, count);

#if defined F_SETFL && O_NONBLOCK
  if (bytes < count && errno == EAGAIN)
    {
      int flags = fcntl (fd, F_GETFL);
      if (0 <= flags && flags & O_NONBLOCK
	  && fcntl (fd, F_SETFL, flags & ~O_NONBLOCK) != -1)
	{
	  char const *buffer = buf;
	  bytes += full_write (fd, buffer + bytes, count - bytes);
	}
    }
#endif

  return bytes;
}

/* Set FD's (i.e., assuming the working directory is PARENTFD, FILE's)
   access time to ATIME.  */
int
set_file_atime (int fd, int parentfd, char const *file, struct timespec atime)
{
  struct timespec ts[2];
  ts[0] = atime;
  ts[1].tv_nsec = UTIME_OMIT;
  return fdutimensat (fd, parentfd, file, ts, fstatat_flags);
}

/* A description of a working directory.  */
struct wd
{
  /* The directory's name.  */
  char const *name;
  /* "Absolute" path representing this directory; in the contrast to
     the real absolute pathname, it can contain /../ components (see
     normalize_filename_x for the reason of it).  It is NULL if the
     absolute path could not be determined.  */
  char *abspath;
  /* If nonzero, the file descriptor of the directory, or AT_FDCWD if
     the working directory.  If zero, the directory needs to be opened
     to be used.  */
  int fd;
};

/* A vector of chdir targets.  wd[0] is the initial working directory.  */
static struct wd *wd;

/* The number of working directories in the vector.  */
static size_t wd_count;

/* The allocated size of the vector.  */
static size_t wd_alloc;

/* The maximum number of chdir targets with open directories.
   Don't make it too large, as many operating systems have a small
   limit on the number of open file descriptors.  Also, the current
   implementation does not scale well.  */
enum { CHDIR_CACHE_SIZE = 16 };

/* Indexes into WD of chdir targets with open file descriptors, sorted
   most-recently used first.  Zero indexes are unused.  */
static int wdcache[CHDIR_CACHE_SIZE];

/* Number of nonzero entries in WDCACHE.  */
static size_t wdcache_count;

int
chdir_count (void)
{
  if (wd_count == 0)
    return wd_count;
  return wd_count - 1;
}

/* DIR is the operand of a -C option; add it to vector of chdir targets,
   and return the index of its location.  */
int
chdir_arg (char const *dir)
{
  char *absdir;

  if (wd_count == wd_alloc)
    {
      if (wd_alloc == 0)
	wd_alloc = 2;
      wd = x2nrealloc (wd, &wd_alloc, sizeof *wd);

      if (! wd_count)
	{
	  wd[wd_count].name = ".";
	  wd[wd_count].abspath = xgetcwd ();
	  if (!wd[wd_count].abspath)
	    call_arg_fatal ("getcwd", ".");
	  wd[wd_count].fd = AT_FDCWD;
	  wd_count++;
	}
    }

  /* Optimize the common special case of the working directory,
     or the working directory as a prefix.  */
  if (dir[0])
    {
      while (dir[0] == '.' && ISSLASH (dir[1]))
	for (dir += 2;  ISSLASH (*dir);  dir++)
	  continue;
      if (! dir[dir[0] == '.'])
	return wd_count - 1;
    }


  /* If the given name is absolute, use it to represent this directory;
     otherwise, construct a name based on the previous -C option.  */
  if (IS_ABSOLUTE_FILE_NAME (dir))
    absdir = xstrdup (dir);
  else if (wd[wd_count - 1].abspath)
    {
      namebuf_t nbuf = namebuf_create (wd[wd_count - 1].abspath);
      namebuf_add_dir (nbuf, dir);
      absdir = namebuf_finish (nbuf);
    }
  else
    absdir = 0;

  wd[wd_count].name = dir;
  wd[wd_count].abspath = absdir;
  wd[wd_count].fd = 0;
  return wd_count++;
}

/* Index of current directory.  */
int chdir_current;

/* Value suitable for use as the first argument to openat, and in
   similar locations for fstatat, etc.  This is an open file
   descriptor, or AT_FDCWD if the working directory is current.  It is
   valid until the next invocation of chdir_do.  */
int chdir_fd = AT_FDCWD;

/* Change to directory I, in a virtual way.  This does not actually
   invoke chdir; it merely sets chdir_fd to an int suitable as the
   first argument for openat, etc.  If I is 0, change to the initial
   working directory; otherwise, I must be a value returned by
   chdir_arg.  */
void
chdir_do (int i)
{
  if (chdir_current != i)
    {
      struct wd *curr = &wd[i];
      int fd = curr->fd;

      if (! fd)
	{
	  if (! IS_ABSOLUTE_FILE_NAME (curr->name))
	    chdir_do (i - 1);
	  fd = openat (chdir_fd, curr->name,
		       open_searchdir_flags & ~ O_NOFOLLOW);
	  if (fd < 0)
	    open_fatal (curr->name);

	  curr->fd = fd;

	  /* Add I to the cache, tossing out the lowest-ranking entry if the
	     cache is full.  */
	  if (wdcache_count < CHDIR_CACHE_SIZE)
	    wdcache[wdcache_count++] = i;
	  else
	    {
	      struct wd *stale = &wd[wdcache[CHDIR_CACHE_SIZE - 1]];
	      if (close (stale->fd) != 0)
		close_diag (stale->name);
	      stale->fd = 0;
	      wdcache[CHDIR_CACHE_SIZE - 1] = i;
	    }
	}

      if (0 < fd)
	{
	  /* Move the i value to the front of the cache.  This is
	     O(CHDIR_CACHE_SIZE), but the cache is small.  */
	  size_t ci;
	  int prev = wdcache[0];
	  for (ci = 1; prev != i; ci++)
	    {
	      int cur = wdcache[ci];
	      wdcache[ci] = prev;
	      if (cur == i)
		break;
	      prev = cur;
	    }
	  wdcache[0] = i;
	}

      chdir_current = i;
      chdir_fd = fd;
    }
}

const char *
tar_dirname (void)
{
  return wd[chdir_current].name;
}

/* Return the absolute path that represents the working
   directory referenced by IDX.

   If wd is empty, then there were no -C options given, and
   chdir_args() has never been called, so we simply return the
   process's actual cwd.  (Note that in this case IDX is ignored,
   since it should always be 0.) */
static const char *
tar_getcdpath (int idx)
{
  if (!wd)
    {
      static char *cwd;
      if (!cwd)
	{
	  cwd = xgetcwd ();
	  if (!cwd)
	    call_arg_fatal ("getcwd", ".");
	}
      return cwd;
    }
  return wd[idx].abspath;
}

void
close_diag (char const *name)
{
  if (ignore_failed_read_option)
    {
      if (WARNING_ENABLED(WARN_FAILED_READ))
	close_warn (name);
    }
  else
    close_error (name);
}

void
open_diag (char const *name)
{
  if (ignore_failed_read_option)
    {
      if (WARNING_ENABLED(WARN_FAILED_READ))
	open_warn (name);
    }
  else
    open_error (name);
}

void
read_diag_details (char const *name, off_t offset, size_t size)
{
  if (ignore_failed_read_option)
    {
      if (WARNING_ENABLED(WARN_FAILED_READ))
	read_warn_details (name, offset, size);
    }
  else
    read_error_details (name, offset, size);
}

void
readlink_diag (char const *name)
{
  if (ignore_failed_read_option)
    {
      if (WARNING_ENABLED(WARN_FAILED_READ))
	readlink_warn (name);
    }
  else
    readlink_error (name);
}

void
savedir_diag (char const *name)
{
  if (ignore_failed_read_option)
    {
      if (WARNING_ENABLED(WARN_FAILED_READ))
	savedir_warn (name);
    }
  else
    savedir_error (name);
}

void
seek_diag_details (char const *name, off_t offset)
{
  if (ignore_failed_read_option)
    {
      if (WARNING_ENABLED(WARN_FAILED_READ))
	seek_warn_details (name, offset);
    }
  else
    seek_error_details (name, offset);
}

void
stat_diag (char const *name)
{
  if (ignore_failed_read_option)
    {
      if (WARNING_ENABLED(WARN_FAILED_READ))
	stat_warn (name);
    }
  else
    stat_error (name);
}

void
file_removed_diag (const char *name, bool top_level,
		   void (*diagfn) (char const *name))
{
  if (!top_level && errno == ENOENT)
    {
      WARNOPT (WARN_FILE_REMOVED,
	       (0, 0, _("%s: File removed before we read it"),
		quotearg_colon (name)));
      set_exit_status (TAREXIT_DIFFERS);
    }
  else
    diagfn (name);
}

/* Fork, aborting if unsuccessful.  */
pid_t
xfork (void)
{
  pid_t p = fork ();
  if (p == (pid_t) -1)
    call_arg_fatal ("fork", _("child process"));
  return p;
}

/* Create a pipe, aborting if unsuccessful.  */
void
xpipe (int fd[2])
{
  if (pipe (fd) < 0)
    call_arg_fatal ("pipe", _("interprocess channel"));
}

/* Return PTR, aligned upward to the next multiple of ALIGNMENT.
   ALIGNMENT must be nonzero.  The caller must arrange for ((char *)
   PTR) through ((char *) PTR + ALIGNMENT - 1) to be addressable
   locations.  */

static inline void *
ptr_align (void *ptr, size_t alignment)
{
  char *p0 = ptr;
  char *p1 = p0 + alignment - 1;
  return p1 - (size_t) p1 % alignment;
}

/* Return the address of a page-aligned buffer of at least SIZE bytes.
   The caller should free *PTR when done with the buffer.  */

void *
page_aligned_alloc (void **ptr, size_t size)
{
  size_t alignment = getpagesize ();
  size_t size1 = size + alignment;
  if (size1 < size)
    xalloc_die ();
  *ptr = xmalloc (size1);
  return ptr_align (*ptr, alignment);
}



struct namebuf
{
  char *buffer;		/* directory, '/', and directory member */
  size_t buffer_size;	/* allocated size of name_buffer */
  size_t dir_length;	/* length of directory part in buffer */
};

namebuf_t
namebuf_create (const char *dir)
{
  namebuf_t buf = xmalloc (sizeof (*buf));
  buf->buffer_size = strlen (dir) + 2;
  buf->buffer = xmalloc (buf->buffer_size);
  strcpy (buf->buffer, dir);
  buf->dir_length = strlen (buf->buffer);
  if (!ISSLASH (buf->buffer[buf->dir_length - 1]))
    buf->buffer[buf->dir_length++] = DIRECTORY_SEPARATOR;
  return buf;
}

void
namebuf_free (namebuf_t buf)
{
  free (buf->buffer);
  free (buf);
}

char *
namebuf_name (namebuf_t buf, const char *name)
{
  size_t len = strlen (name);
  while (buf->dir_length + len + 1 >= buf->buffer_size)
    buf->buffer = x2realloc (buf->buffer, &buf->buffer_size);
  strcpy (buf->buffer + buf->dir_length, name);
  return buf->buffer;
}

static void
namebuf_add_dir (namebuf_t buf, const char *name)
{
  static char dirsep[] = { DIRECTORY_SEPARATOR, 0 };
  if (!ISSLASH (buf->buffer[buf->dir_length - 1]))
    {
      namebuf_name (buf, dirsep);
      buf->dir_length++;
    }
  namebuf_name (buf, name);
  buf->dir_length += strlen (name);
}

static char *
namebuf_finish (namebuf_t buf)
{
  char *res = buf->buffer;

  if (ISSLASH (buf->buffer[buf->dir_length - 1]))
    buf->buffer[buf->dir_length] = 0;
  free (buf);
  return res;
}

/* Return the filenames in directory NAME, relative to the chdir_fd.
   If the directory does not exist, report error if MUST_EXIST is
   true.

   Return NULL on errors.
*/
char *
tar_savedir (const char *name, int must_exist)
{
  char *ret = NULL;
  DIR *dir = NULL;
  int fd = openat (chdir_fd, name, open_read_flags | O_DIRECTORY);
  if (fd < 0)
    {
      if (!must_exist && errno == ENOENT)
	return NULL;
      open_error (name);
    }
  else if (! ((dir = fdopendir (fd))
	      && (ret = streamsavedir (dir, savedir_sort_order))))
    savedir_error (name);

  if (dir ? closedir (dir) != 0 : 0 <= fd && close (fd) != 0)
    savedir_error (name);

  return ret;
}
