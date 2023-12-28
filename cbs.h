#ifndef C_BUILD_SYSTEM_H
#define C_BUILD_SYSTEM_H

#include <sys/stat.h>
#include <sys/types.h>
#include <sys/wait.h>

#include <errno.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

/* C23 changed a lot so we want to check for it, and some idiot decided that
   __STDC_VERSION__ is an optional macro */
#if defined(__STDC_VERSION__) && __STDC_VERSION__ >= 202000
#	define CBS_IS_C23
#endif

/* Some C23 compat.  In C23 booleans are actual keywords, and the noreturn
   attribute is different. */
#ifdef CBS_IS_C23
#	define noreturn [[noreturn]]
#else
#	include <stdbool.h>
#	include <stdnoreturn.h>
#endif

/* Give helpful diagnostics when people use die() incorrectly on GCC.  C23
   introduced C++ attribute syntax, so we need a check for that too. */
#ifdef __GNUC__
#	ifdef CBS_IS_C23
#		define ATTR_FMT [[gnu::format(printf, 1, 2)]]
#	else
#		define ATTR_FMT __attribute__((format(printf, 1, 2)))
#	endif
#else
#	define ATTR_FMT
#endif

/* Classic min/max macros */
#define min(x, y) ((x) > (y) ? (y) : (x))
#define max(x, y) ((x) > (y) ? (x) : (y))

/* Clamp v within the bounds of [n, m] */
#define clamp(v, n, m) max(min((v), (m)), (n))

/* Get the number of items in the array a */
#define lengthof(a) (sizeof(a) / sizeof(*(a)))

/* Clear (but not free) the command c.  Useful for reusing the same command
   struct to minimize allocations. */
#define cmdclr(c) \
	do { \
		(c)->len = 0; \
		(c)->argv = NULL; \
	} while (0)

/* Struct representing a CLI command that various functions act on.  You will
   basically always want to zero-initialize variables of this type before use.
 */
struct cmd {
	char **argv;
	size_t len, cap;
};

/* Internal global versions of argc and argv, so our functions and macros can
   access them from anywhere. */
static int cbs_argc;
static char **cbs_argv;

#ifdef __cplusplus
extern "C" {
#endif

/* Initializes some data required for this header to work properly.  This should
   be the first thing called in main() with argc and argv passed. */
static void cbsinit(int, char **);

/* cmdadd() adds the variadic string arguments to the given command.
   Alternatively, the cmdaddv() function adds the n strings pointed to by p to
   the given command.

   These functions return true on success and false on failure while setting
   errno. */
static bool cmdaddv(struct cmd *, char **p, size_t n);
#define cmdadd(cmd, ...) \
	cmdaddv(cmd, ((char *[]){__VA_ARGS__}), lengthof(((char *[]){__VA_ARGS__})))

/* Rebuild the build script if either it, or this header file have been
   modified, and execute the newly built script.  You should call the rebuild()
   macro at the very beginning of main(), but right after cbsinit().  You
   probably don’t want to call __rebuild() directly.

   This function returns true on success and false on failure.  On failure errno
   may or may not be set. */
static bool __rebuild(char *);
#define rebuild() __rebuild(__FILE__)

/* Returns if a file exists at the given path.  A return value of false may also
   mean you don’t have the proper file access permissions, which will also set
   errno. */
static bool fexists(char *);

/* The cmdexec() function executes the given command and waits for it to
   terminate, returning its exit code.  The cmdexeca() function executes the
   given command and returns immediately, returning its process ID.

   The cmdexecb() function is like cmdexec() except it writes the given commands
   standard output to the character buffer pointed to by p.  It also stores the
   size of the output in *n.

   Both of these functions return -1 on error and set errno.  cmdexec() also
   returns the same values as cmdwait(). */
static int cmdexec(struct cmd);
static pid_t cmdexeca(struct cmd);
static int cmdexecb(struct cmd, char **p, size_t *n);

/* Wait for the process with the given PID to terminate, and return its exit
   status.  If the process was terminated by a signal 256 is returned.

   On error this function returns -1 and errno is set. */
static int cmdwait(pid_t);

/* Compare the modification dates of the two named files.

   A return value of +1 means the LHS is newer than the RHS.
   A return value of -1 means the LHS is older than the RHS.
   A return value of 0 means the LHS and RHS have the same modification date.
   On error, FMDCMP_FAIL is returned and errno is set.

   The fmdnewer() and fmdolder() functions are wrappers around fmdcmp() that
   return true when the LHS is newer or older than the RHS respectively.  These
   functions will cause the caller to exit with EXIT_FAILURE on error. */
static int fmdcmp(char *, char *);
static bool fmdnewer(char *, char *);
static bool fmdolder(char *, char *);

/* Get the number of available CPUs, or -1 on error.  This function also returns
   -1 if the _SC_NPROCESSORS_ONLN flag to sysconf(3) is not available.  In that
   case, errno will not be set. */
static int nproc(void);

/* Write a representation of the given command to the given file stream.  This
   can be used to mimick the echoing behavior of make(1). */
static void cmdput(FILE *, struct cmd);

enum pkg_config_flags {
	PKGC_LIBS = 1 << 0,
	PKGC_CFLAGS = 1 << 1,
};
static bool pcquery(struct cmd *, char *, enum pkg_config_flags);

ATTR_FMT noreturn static void die(const char *, ...);

#ifdef __cplusplus
}
#endif

void
die(const char *fmt, ...)
{
	int e = errno;
	va_list ap;

	va_start(ap, fmt);
	fprintf(stderr, "%s: ", *cbs_argv);
	if (fmt) {
		vfprintf(stderr, fmt, ap);
		fprintf(stderr, ": ");
	}
	fprintf(stderr, "%s\n", strerror(e));
	exit(EXIT_FAILURE);
}

void
cbsinit(int argc, char **argv)
{
	cbs_argc = argc;
	cbs_argv = argv;
}

int
nproc(void)
{
#ifdef _SC_NPROCESSORS_ONLN
	return (int)sysconf(_SC_NPROCESSORS_ONLN);
#else
	errno = 0;
	return -1;
#endif
}

int
cmdwait(pid_t pid)
{
	for (;;) {
		int ws;
		if (waitpid(pid, &ws, 0) == -1)
			return -1;

		if (WIFEXITED(ws))
			return WEXITSTATUS(ws);

		if (WIFSIGNALED(ws))
			return 256;
	}
}

int
cmdexec(struct cmd c)
{
	pid_t pid;
	return (pid = cmdexeca(c)) == -1 ? -1 : cmdwait(pid);
}

pid_t
cmdexeca(struct cmd c)
{
	pid_t pid;

	switch (pid = fork()) {
	case -1:
		return -1;
	case 0:
		execvp(*c.argv, c.argv);
		die("execvp: %s", *c.argv);
	}

	return pid;
}

int
cmdexecb(struct cmd c, char **p, size_t *n)
{
	enum {
		FD_R,
		FD_W,
	};
	pid_t pid;
	int fds[2];
	char *buf;
	size_t len, blksize;
	struct stat sb;

	if (pipe(fds) == -1)
		return -1;

	switch (pid = fork()) {
	case -1:
		return -1;
	case 0:
		close(fds[FD_R]);
		if (dup2(fds[FD_W], STDOUT_FILENO) == -1)
			die("dup2");
		execvp(*c.argv, c.argv);
		die("execvp: %s", *c.argv);
	}

	close(fds[FD_W]);

	buf = NULL;
	len = 0;

	blksize = fstat(fds[FD_R], &sb) == -1 ? BUFSIZ : sb.st_blksize;
	for (;;) {
		/* This can maybe somewhere somehow break some system.  I do not care */
		char tmp[blksize];
		ssize_t nr;

		if ((nr = read(fds[FD_R], tmp, blksize)) == -1)
			return -1;
		if (!nr)
			break;
		if (!(buf = realloc(buf, len + nr))) {
			free(buf);
			return -1;
		}
		memcpy(buf + len, tmp, nr);
		len += nr;
	}

	close(fds[FD_R]);
	*p = buf;
	*n = len;
	return cmdwait(pid);
}

bool
cmdaddv(struct cmd *cmd, char **xs, size_t n)
{
	size_t old = cmd->cap;

	while (cmd->len + n >= cmd->cap)
		cmd->cap = cmd->cap * 2 + 2;

	if (old < cmd->cap) {
		cmd->argv = (char **)realloc(cmd->argv, cmd->cap * sizeof(char *));
		if (!cmd->argv)
			return false;
	}

	memcpy(cmd->argv + cmd->len, xs, n * sizeof(*xs));
	cmd->len += n;
	cmd->argv[cmd->len] = NULL;
	return true;
}

/* import shlex

   s = '#define SHELL_SAFE "'
   for c in map(chr, range(128)):
       if not shlex._find_unsafe(c):
           s += c
   print(s + '"') */
#define SHELL_SAFE \
	"%+,-./0123456789:=@ABCDEFGHIJKLMNOPQRSTUVWXYZ_abcdefghijklmnopqrstuvwxyz"

void
cmdput(FILE *stream, struct cmd cmd)
{
	for (size_t i = 0; i < cmd.len; i++) {
		bool safe = true;
		char *p, *q;

		p = q = cmd.argv[i];
		for (; *q; q++) {
			if (!strchr(SHELL_SAFE, *q)) {
				safe = false;
				break;
			}
		}

		if (safe)
			fputs(p, stream);
		else {
			putc('\'', stream);
			for (q = p; *q; q++) {
				if (*q == '\'')
					fputs("'\"'\"'", stream);
				else
					putc(*q, stream);
			}
			putc('\'', stream);
		}

		putc(i == cmd.len - 1 ? '\n' : ' ', stream);
	}
}

#define FMDCMP_FAIL -2

int
fmdcmp(char *lhs, char *rhs)
{
	struct stat sbl, sbr;

	return stat(lhs, &sbl) == -1 || stat(rhs, &sbr) == -1 ? FMDCMP_FAIL
	     : sbl.st_mtim.tv_sec == sbr.st_mtim.tv_sec
	         ? clamp(sbl.st_mtim.tv_nsec - sbr.st_mtim.tv_nsec, -1, +1)
	         : clamp(sbl.st_mtim.tv_sec - sbr.st_mtim.tv_sec, -1, +1);
}

#define __fmd_newer_older(t) \
	int n = fmdcmp(lhs, rhs); \
	if (n == FMDCMP_FAIL) \
		die("failed to stat"); \
	return n == t;

bool
fmdnewer(char *lhs, char *rhs)
{
	__fmd_newer_older(+1)
}

bool
fmdolder(char *lhs, char *rhs)
{
	__fmd_newer_older(-1)
}

#undef __fmd_newer_older

bool
fexists(char *f)
{
	return !access(f, F_OK);
}

bool
__rebuild(char *src)
{
	struct cmd cmd = {0};

	if (fmdnewer(*cbs_argv, src) && fmdnewer(*cbs_argv, __FILE__))
		return true;

	if (!cmdadd(&cmd, "cc", "-o", *cbs_argv, src))
		return false;
	cmdput(stdout, cmd);
	if (cmdexec(cmd))
		return false;

	cmdclr(&cmd);
	if (!cmdaddv(&cmd, cbs_argv, cbs_argc))
		return false;
	cmdput(stdout, cmd);
	exit(cmdexec(cmd));
}

bool
pcquery(struct cmd *cmd, char *lib, enum pkg_config_flags flags)
{
	char *p = NULL;
	size_t n;
	bool ret = false;
	struct cmd c = {0};

	if (!cmdadd(&c, "pkg-config"))
		goto out;
	if ((flags & PKGC_LIBS) && !cmdadd(&c, "--libs"))
		goto out;
	if ((flags & PKGC_CFLAGS) && !cmdadd(&c, "--cflags"))
		goto out;
	if (!cmdadd(&c, lib))
		goto out;

	if (cmdexecb(c, &p, &n) != EXIT_SUCCESS)
		goto out;
	printf("%.*s", (int)n, p);

	ret = true;
out:
	free(p);
	free(c.argv);
	return ret;
}

#endif /* !C_BUILD_SYSTEM_H */
