/* Single-header library to help write build scripts in C.  This library is
   POSIX compliant, so it should work on all respectible UNIX-like systems.

   All functions and macros are documented.  You can figure out the API pretty
   easily by just reading the comments in this file.  Any identifier prefixed
   with a double-underscore (‘__’) is not meant for you to touch, but since this
   file should be downloaded into your repository, you can touch them anyways if
   you really want.

   This file does not support C89.  Fuck C89, that shit is ancient.  Move on.

   IMPORTANT NOTE: All the functions and macros in this library will terminate
   the program on error.  If this is undesired behavior, feel free to edit the
   functions to return errors.

   There are a few exceptions to the above rule, and they are documented. */

#ifndef C_BUILD_SYSTEM_H
#define C_BUILD_SYSTEM_H

#include <sys/stat.h>
#include <sys/wait.h>

#include <errno.h>
#include <stdarg.h>
#include <stdint.h>
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

/* Get the number of items in the array a */
#define lengthof(a) (sizeof(a) / sizeof(*(a)))

/* Clear (but not free) the command c.  Useful for reusing the same command
   struct to minimize allocations. */
#define cmdclr(c) \
	do { \
		(c)->len = 0; \
		*(c)->argv = NULL; \
	} while (0)

/* Struct representing a CLI command that various functions act on.  You will
   basically always want to zero-initialize variables of this type before use.

   After executing a command, you can reuse the already allocated buffer this
   command holds by calling cmdclr().  When you’re really done with an object of
   this type, remember to call free() on .argv. */
struct cmd {
	char **argv;
	size_t len, cap;
};

/* Internal global versions of argc and argv, so our functions and macros can
   access them from anywhere. */
static int __cbs_argc;
static char **__cbs_argv;

/* A wrapper function around realloc().  It behaves exactly the same except
   instead of taking a buffer size as an argument, it takes a count n of
   elements, and a size m of each element.  This allows it to properly check for
   overflow, and errors if overflow would occur. */
static void *bufalloc(void *, size_t n, size_t m);

/* Error reporting functions.  The die() function takes the same arguments as
   printf() and prints the corresponding string to stderr.  It also prefixes the
   string with the command name followed by a colon, and suffixes the string
   with a colon and the error string returned from strerror().

   diex() is the same as die() but does not print a strerror() error string. */
ATTR_FMT noreturn static void die(const char *, ...);
ATTR_FMT noreturn static void diex(const char *, ...);

/* Initializes some data required for this header to work properly.  This should
   be the first thing called in main() with argc and argv passed. */
static void cbsinit(int, char **);

/* cmdadd() adds the variadic string arguments to the given command.
   Alternatively, the cmdaddv() function adds the n strings pointed to by p to
   the given command. */
static void cmdaddv(struct cmd *, char **p, size_t n);
#define cmdadd(cmd, ...) \
	cmdaddv(cmd, ((char *[]){__VA_ARGS__}), lengthof(((char *[]){__VA_ARGS__})))

/* The cmdexec() function executes the given command and waits for it to
   terminate, returning its exit code.  The cmdexeca() function executes the
   given command and returns immediately, returning its process ID.

   The cmdexecb() function is like cmdexec() except it writes the given commands
   standard output to the character buffer pointed to by p.  It also stores the
   size of the output in *n.

   cmdexec() and cmdexecb() have the same return values as cmdwait(). */
static int cmdexec(struct cmd);
static pid_t cmdexeca(struct cmd);
static int cmdexecb(struct cmd, char **p, size_t *n);

/* Wait for the process with the given PID to terminate, and return its exit
   status.  If the process was terminated by a signal 256 is returned. */
static int cmdwait(pid_t);

/* Write a representation of the given command to the given file stream.  This
   can be used to mimick the echoing behavior of make(1).  The cmdput() macro is
   a nice convenience macro so you can avoid writing ‘stdout’ all the time. */
static void cmdputf(FILE *, struct cmd);
#define cmdput(c) cmdputf(stdout, c);

/* Returns if a file exists at the given path.  A return value of false may also
   mean you don’t have the proper file access permissions, which will also set
   errno. */
static bool fexists(char *);

/* Compare the modification dates of the two named files.

   A return value >0 means the LHS is newer than the RHS.
   A return value <0 means the LHS is older than the RHS.
   A return value of 0 means the LHS and RHS have the same modification date.

   The fmdnewer() and fmdolder() macros are wrappers around fmdcmp() that
   return true when the LHS is newer or older than the RHS respectively. */
static int fmdcmp(char *, char *);
#define fmdnewer(lhs, rhs) (fmdcmp(lhs, rhs) > 0)
#define fmdolder(lhs, rhs) (fmdcmp(lhs, rhs) < 0)

/* Rebuild the build script if either it, or this header file have been
   modified, and execute the newly built script.  You should call the rebuild()
   macro at the very beginning of main(), but right after cbsinit().  You
   probably don’t want to call __rebuild() directly. */
static void __rebuild(char *);
#define rebuild() __rebuild(__FILE__)

/* Get the number of available CPUs, or -1 on error.  This function also returns
   -1 if the _SC_NPROCESSORS_ONLN flag to sysconf(3) is not available.  In that
   case, errno will not be set. */
static int nproc(void);

/* Add the arguments returned by an invokation of pkg-config for the library lib
   to the given command.  The flags argument is one-or-more of the flags in the
   pkg_config_flags enum bitwise-ORed together.

   If PKGC_CFLAGS is specified, call pkg-config with ‘--cflags’.
   If PKGC_LIBS is specified, call pkg-config with ‘--libs’.

   This function returns true on success and false if pkg-config is not found on
   the system.  To check for pkg-configs existance without doing anything
   meaningful, you can call this function with flags set to 0 and lib set to a
   VALID library name.

   The arguments this function appends to the given command are heap-allocated.
   If you care about free()ing them, you can figure out their indicies in
   cmd.argv by getting cmd.len both before- and after calling this function. */
static bool pcquery(struct cmd *, char *lib, int flags);
enum pkg_config_flags {
	PKGC_LIBS = 1 << 0,
	PKGC_CFLAGS = 1 << 1,
};

void *
bufalloc(void *p, size_t n, size_t m)
{
	if (n && SIZE_MAX / n < m) {
		errno = EOVERFLOW;
		die(__func__);
	}

	if (!(p = realloc(p, n * m)))
		die(__func__);
	return p;
}

void
die(const char *fmt, ...)
{
	int e = errno;
	va_list ap;

	va_start(ap, fmt);
	fprintf(stderr, "%s: ", *__cbs_argv);
	if (fmt) {
		vfprintf(stderr, fmt, ap);
		fprintf(stderr, ": ");
	}
	fprintf(stderr, "%s\n", strerror(e));
	exit(EXIT_FAILURE);
}

void
diex(const char *fmt, ...)
{
	va_list ap;

	va_start(ap, fmt);
	fprintf(stderr, "%s: ", *__cbs_argv);
	if (fmt)
		vfprintf(stderr, fmt, ap);
	fputc('\n', stderr);
	exit(EXIT_FAILURE);
}

void
cbsinit(int argc, char **argv)
{
	__cbs_argc = argc;
	__cbs_argv = argv;
}

static size_t
__next_powerof2(size_t n)
{
	if (n && !(n & (n - 1)))
		return n;

	n--;
	for (size_t i = 1; i < sizeof(size_t); i <<= 1)
		n |= n >> i;
	return n + 1;
}

void
cmdaddv(struct cmd *cmd, char **xs, size_t n)
{
	if (cmd->len + n >= cmd->cap) {
		cmd->cap = __next_powerof2(cmd->len + n) + 2;
		cmd->argv = bufalloc(cmd->argv, cmd->cap, sizeof(char *));
	}

	memcpy(cmd->argv + cmd->len, xs, n * sizeof(*xs));
	cmd->len += n;
	cmd->argv[cmd->len] = NULL;
}

int
cmdexec(struct cmd c)
{
	return cmdwait(cmdexeca(c));
}

pid_t
cmdexeca(struct cmd c)
{
	pid_t pid;

	switch (pid = fork()) {
	case -1:
		die("fork");
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
		die("pipe");

	switch (pid = fork()) {
	case -1:
		die("fork");
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
			die("read");
		if (!nr)
			break;
		buf = bufalloc(buf, len + nr, sizeof(char));
		memcpy(buf + len, tmp, nr);
		len += nr;
	}

	close(fds[FD_R]);
	*p = buf;
	*n = len;
	return cmdwait(pid);
}

int
cmdwait(pid_t pid)
{
	for (;;) {
		int ws;
		if (waitpid(pid, &ws, 0) == -1)
			die("waitpid");

		if (WIFEXITED(ws))
			return WEXITSTATUS(ws);

		if (WIFSIGNALED(ws))
			return 256;
	}
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
cmdputf(FILE *stream, struct cmd cmd)
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

bool
fexists(char *f)
{
	return !access(f, F_OK);
}

int
fmdcmp(char *lhs, char *rhs)
{
	struct stat sbl, sbr;

	if (stat(lhs, &sbl) == -1)
		die("%s", lhs);
	if (stat(rhs, &sbr) == -1)
		die("%s", rhs);

	return sbl.st_mtim.tv_sec == sbr.st_mtim.tv_sec
	         ? sbl.st_mtim.tv_nsec - sbr.st_mtim.tv_nsec
	         : sbl.st_mtim.tv_sec - sbr.st_mtim.tv_sec;
}

void
__rebuild(char *src)
{
	struct cmd cmd = {0};

	if (fmdnewer(*__cbs_argv, src) && fmdnewer(*__cbs_argv, __FILE__))
		return;

	cmdadd(&cmd, "cc", "-o", *__cbs_argv, src);
	cmdput(cmd);
	if (cmdexec(cmd))
		diex("Compilation of build script failed");

	cmdclr(&cmd);
	cmdaddv(&cmd, __cbs_argv, __cbs_argc);
	cmdput(cmd);
	exit(cmdexec(cmd));
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

bool
pcquery(struct cmd *cmd, char *lib, int flags)
{
	int ec;
	char *p, *q, *s;
	size_t n;
	struct cmd c = {0};

	p = NULL;

	cmdadd(&c, "pkg-config");
	if (flags & PKGC_LIBS)
		cmdadd(&c, "--libs");
	if (flags & PKGC_CFLAGS)
		cmdadd(&c, "--cflags");
	cmdadd(&c, lib);

	if ((ec = cmdexecb(c, &p, &n))) {
		if (errno == ENOENT) {
			free(c.argv);
			return false;
		}
		diex("pkg-config terminated with exit-code %d", ec);
	}

	for (q = strtok(p, " \n\r\t\v"); q; q = strtok(NULL, " \n\r\t\v")) {
		if (!(s = strdup(q)))
			die("strdup");
		cmdadd(cmd, s);
	}

	return true;
}

#endif /* !C_BUILD_SYSTEM_H */
