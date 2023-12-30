/* Single-header library to help write build scripts in C.  This library is
   POSIX compliant, so it should work on all respectible UNIX-like systems.

   All functions and macros are documented.  You can figure out the API pretty
   easily by just reading the comments in this file.

   In many cases you may want to be able to execute commands on multiple threads
   to speed up compilation, such as the -j option when using Make.  Functions
   for creating and using thread pools will be made available if the CBS_PTHREAD
   macro is defined before including this file.  Do note that on POSIX platforms
   it will require linking to the pthreads library when bootstrapping the build
   script.

   This file does not support C89.  Fuck C89, that shit is ancient.  Move on.

   IMPORTANT NOTE: Any identifiers prefixed with an underscore (e.g. ‘_rebuild’)
   are meant for internal use only and you should not touch them unless you know
   what you’re doing.

   IMPORTANT NOTE: All the functions and macros in this library will terminate
   the program on error.  If this is undesired behavior, feel free to edit the
   functions to return errors.

   There are a few exceptions to the above rule, and they are documented.

   This library does not aim to ever support Windows */

#ifndef C_BUILD_SYSTEM_H
#define C_BUILD_SYSTEM_H

/* Assert that the user is building for a supported platform.  The only portable
   way to check for POSIX is to validate that unistd.h exists.  This is only
   possible without compiler extensions in C23 (although some compilers support
   it as an extension in earlier editions), so people compiling for pre-C23
   might not get this error if on a bad platform, and may end up being a bit
   confused.

   It’s just a maybe though, this is nothing more than a sanity check for the
   users sake. */
#if defined(__has_include) && !__has_include(<unistd.h>)
#	error "Non-POSIX platform detected"
#endif

#include <sys/stat.h>
#include <sys/wait.h>

#include <errno.h>
#ifdef CBS_PTHREAD
#	include <pthread.h>
#endif
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

/* Clang defines this attribute, and while it does nothing it does serve as
   good documentation. */
#ifndef _Nullable
#	define _Nullable
#endif

/* Internal global versions of argc and argv, so our functions and macros can
   access them from anywhere. */
static int _cbs_argc;
static char **_cbs_argv;

/* A wrapper function around realloc().  It behaves exactly the same except
   instead of taking a buffer size as an argument, it takes a count n of
   elements, and a size m of each element.  This allows it to properly check for
   overflow, and errors if overflow would occur. */
static void *bufalloc(void *_Nullable, size_t n, size_t m);

/* Error reporting functions.  The die() function takes the same arguments as
   printf() and prints the corresponding string to stderr.  It also prefixes the
   string with the command name followed by a colon, and suffixes the string
   with a colon and the error string returned from strerror().

   If you want to print just the error message and no custom text, NULL may be
   passed to die().  NULL should not be passed to diex().

   diex() is the same as die() but does not print a strerror() error string. */
ATTR_FMT noreturn static void die(const char *_Nullable, ...);
ATTR_FMT noreturn static void diex(const char *, ...);

/* Initializes some data required for this header to work properly.  This should
   be the first thing called in main() with argc and argv passed. */
static void cbsinit(int, char **);

/* Get the number of items in the array a */
#define lengthof(a) (sizeof(a) / sizeof(*(a)))

/* Struct representing a CLI command that various functions act on.  You should
   always zero-initialize variables of this type before use.

   After executing a command, you can reuse the already allocated buffer this
   command holds by calling cmdclr().  When you’re really done with an object of
   this type, remember to call free() on ._argv.

   The ._argv field is a NULL-terminated list of command arguments of length
   ._len.  You may safely read from both of these fields but they should NOT be
   modified without use of cmdadd() and cmdaddv(). */
typedef struct {
	char **_argv;
	size_t _len, _cap;
} cmd_t;

/* cmdadd() adds the variadic string arguments to the given command.
   Alternatively, the cmdaddv() function adds the n strings pointed to by p to
   the given command. */
static void cmdaddv(cmd_t *, char **p, size_t n);
#define cmdadd(cmd, ...) \
	cmdaddv(cmd, ((char *[]){__VA_ARGS__}), lengthof(((char *[]){__VA_ARGS__})))

/* Clear (but not free) the command c.  Useful for reusing the same command
   struct to minimize allocations. */
static void cmdclr(cmd_t *c);

/* The cmdexec() function executes the given command and waits for it to
   terminate, returning its exit code.  The cmdexeca() function executes the
   given command and returns immediately, returning its process ID.

   The cmdexecb() function is like cmdexec() except it writes the given commands
   standard output to the character buffer pointed to by p.  It also stores the
   size of the output in *n.  The character buffer p is null-terminated.

   cmdexec() and cmdexecb() have the same return values as cmdwait(). */
static int cmdexec(cmd_t);
static pid_t cmdexeca(cmd_t);
static int cmdexecb(cmd_t, char **p, size_t *n);

/* Wait for the process with the given PID to terminate, and return its exit
   status.  If the process was terminated by a signal 256 is returned. */
static int cmdwait(pid_t);

/* Write a representation of the given command to the given file stream.  This
   can be used to mimick the echoing behavior of make(1).  The cmdput() function
   is a nice convenience function so you can avoid writing ‘stdout’ all the
   time. */
static void cmdput(cmd_t);
static void cmdputf(FILE *, cmd_t);

/* Returns if a file exists at the given path.  A return value of false may also
   mean you don’t have the proper file access permissions, which will also set
   errno. */
static bool fexists(const char *);

/* Compare the modification dates of the two named files.

   A return value >0 means the LHS is newer than the RHS.
   A return value <0 means the LHS is older than the RHS.
   A return value of 0 means the LHS and RHS have the same modification date.

   The fmdnewer() and fmdolder() functions are wrappers around fmdcmp() that
   return true when the LHS is newer or older than the RHS respectively. */
static int fmdcmp(const char *, const char *);
static bool fmdolder(const char *, const char *);
static bool fmdnewer(const char *, const char *);

/* Rebuild the build script if either it, or this header file have been
   modified, and execute the newly built script.  You should call the rebuild()
   macro at the very beginning of main(), but right after cbsinit().  You
   probably don’t want to call _rebuild() directly. */
static void _rebuild(char *);
#define rebuild() _rebuild(__FILE__)

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
   If you care about deallocating them, you can figure out their indicies in
   the commands ._argv field by getting cmd._len both before and after calling
   this function. */
static bool pcquery(cmd_t *, char *lib, int flags);
enum pkg_config_flags {
	PKGC_LIBS = 1 << 0,
	PKGC_CFLAGS = 1 << 1,
};

#ifdef CBS_PTHREAD

/* A tfunc_t represents a function to be executed by a threads in a thread pool.
   It takes an argument in the form of a void * and returns nothing. */
typedef void (*tfunc_t)(void *);

/* A tfunc_free_t represents a function which frees the argument passed to a
   tfunc_t function. */
typedef void (*tfree_func_t)(void *);

/* A thread pool job queue.  Meant for internal-use only. */
struct _tjob {
	void *arg;
	tfunc_t fn;
	tfree_func_t free;
	struct _tjob *next;
};

/* A basic thread pool.  None of its fields should really be touched. */
typedef struct {
	bool _stop;
	size_t _tcnt, _left;
	pthread_t *_thrds;
	pthread_cond_t _cnd;
	pthread_mutex_t _mtx;
	struct _tjob *_head, *_tail;
} tpool_t;

/* Initialize and destroy a thread pool.  The tpinit() function initializes the
   given thread pool and creates n threads ready to execute work.  The tpfree()
   function should be called after a thread pool has been used to release all
   resources used by the thread pool. */
static void tpinit(tpool_t *, size_t n);
static void tpfree(tpool_t *);

/* Wait for all jobs on the given thread pool to be executed.  Note that this
   function does not destroy the threads or free any resources — those are tasks
   for the tpfree() function. */
static void tpwait(tpool_t *);

/* Enqueue and dequeue jobs to the thread pools job queue.  The tpenq() function
   is threadsafe while the _tpdeq() function is not (so don’t use it).  When
   calling the tpenq() function, the function fn will be queued to be executed
   by a thread in the thread pool with the argument arg.  If the given argument
   needs to be deallocated after the job completes, you can pass the free
   argument which will be called with the given argument after use.  If free is
   NULL, it will be ignored.

   The free() function is a valid argument to the free parameter. */
static void tpenq(tpool_t *, tfunc_t fn, void *arg,
                  tfree_func_t _Nullable free);
static struct _tjob *_tpdeq(tpool_t *);

#endif /* CBS_PTHREAD */

/* BEGIN DEFINITIONS */

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
	flockfile(stderr);
	fprintf(stderr, "%s: ", *_cbs_argv);
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
	flockfile(stderr);
	fprintf(stderr, "%s: ", *_cbs_argv);
	vfprintf(stderr, fmt, ap);
	fputc('\n', stderr);
	exit(EXIT_FAILURE);
}

void
cbsinit(int argc, char **argv)
{
	_cbs_argc = argc;
	_cbs_argv = argv;
}

static size_t
_next_powerof2(size_t n)
{
	if (n && !(n & (n - 1)))
		return n;

	n--;
	for (size_t i = 1; i < sizeof(size_t); i <<= 1)
		n |= n >> i;
	return n + 1;
}

void
cmdaddv(cmd_t *cmd, char **xs, size_t n)
{
	if (cmd->_len + n >= cmd->_cap) {
		cmd->_cap = _next_powerof2(cmd->_len + n) + 2;
		cmd->_argv = bufalloc(cmd->_argv, cmd->_cap, sizeof(char *));
	}

	memcpy(cmd->_argv + cmd->_len, xs, n * sizeof(*xs));
	cmd->_len += n;
	cmd->_argv[cmd->_len] = NULL;
}

void
cmdclr(cmd_t *c)
{
	c->_len = 0;
	*c->_argv = NULL;
}

int
cmdexec(cmd_t c)
{
	return cmdwait(cmdexeca(c));
}

pid_t
cmdexeca(cmd_t c)
{
	pid_t pid;

	switch (pid = fork()) {
	case -1:
		die("fork");
	case 0:
		execvp(*c._argv, c._argv);
		die("execvp: %s", *c._argv);
	}

	return pid;
}

int
cmdexecb(cmd_t c, char **p, size_t *n)
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
		execvp(*c._argv, c._argv);
		die("execvp: %s", *c._argv);
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
		buf = bufalloc(buf, len + nr + 1, sizeof(char));
		memcpy(buf + len, tmp, nr);
		len += nr;
	}

	close(fds[FD_R]);
	buf[len] = 0;
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
cmdput(cmd_t c)
{
	cmdputf(stdout, c);
}

void
cmdputf(FILE *stream, cmd_t cmd)
{
	flockfile(stream);
	for (size_t i = 0; i < cmd._len; i++) {
		bool safe = true;
		char *p, *q;

		p = q = cmd._argv[i];
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

		putc(i == cmd._len - 1 ? '\n' : ' ', stream);
	}
	funlockfile(stream);
}

bool
fexists(const char *f)
{
	return !access(f, F_OK);
}

int
fmdcmp(const char *lhs, const char *rhs)
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

bool
fmdnewer(const char *lhs, const char *rhs)
{
	return fmdcmp(lhs, rhs) > 0;
}

bool
fmdolder(const char *lhs, const char *rhs)
{
	return fmdcmp(lhs, rhs) < 0;
}

void
_rebuild(char *src)
{
	cmd_t cmd = {0};

	if (fmdnewer(*_cbs_argv, src) && fmdnewer(*_cbs_argv, __FILE__))
		return;

	cmdadd(&cmd, "cc");
#ifdef CBS_PTHREAD
	cmdadd(&cmd, "-lpthread");
#endif
	cmdadd(&cmd, "-o", *_cbs_argv, src);
	cmdput(cmd);
	if (cmdexec(cmd))
		diex("Compilation of build script failed");

	cmdclr(&cmd);
	cmdaddv(&cmd, _cbs_argv, _cbs_argc);
	execvp(*cmd._argv, cmd._argv);
	die("execvp: %s", *cmd._argv);
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
pcquery(cmd_t *cmd, char *lib, int flags)
{
	int ec;
	char *p, *q, *s;
	size_t n;
	cmd_t c = {0};

	p = NULL;

	cmdadd(&c, "pkg-config");
	if (flags & PKGC_LIBS)
		cmdadd(&c, "--libs");
	if (flags & PKGC_CFLAGS)
		cmdadd(&c, "--cflags");
	cmdadd(&c, lib);

	if ((ec = cmdexecb(c, &p, &n))) {
		if (errno == ENOENT) {
			free(c._argv);
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

#ifdef CBS_PTHREAD

static void *
_tpwork(void *arg)
{
	tpool_t *tp = arg;

	while (!tp->_stop) {
		struct _tjob *j;

		pthread_mutex_lock(&tp->_mtx);
		while (!tp->_stop && !tp->_head)
			pthread_cond_wait(&tp->_cnd, &tp->_mtx);
		if (tp->_stop) {
			pthread_mutex_unlock(&tp->_mtx);
			break;
		}

		j = _tpdeq(tp);
		pthread_mutex_unlock(&tp->_mtx);

		j->fn(j->arg);
		if (j->free)
			j->free(j->arg);
		free(j);

		pthread_mutex_lock(&tp->_mtx);
		tp->_left--;
		pthread_cond_broadcast(&tp->_cnd);
		pthread_mutex_unlock(&tp->_mtx);
	}

	return NULL;
}

void
tpinit(tpool_t *tp, size_t n)
{
	tp->_tcnt = n;
	tp->_stop = false;
	tp->_left = 0;
	tp->_head = tp->_tail = NULL;
	tp->_thrds = bufalloc(NULL, n, sizeof(pthread_t));
	pthread_cond_init(&tp->_cnd, NULL);
	pthread_mutex_init(&tp->_mtx, NULL);

	for (size_t i = 0; i < n; i++)
		pthread_create(tp->_thrds + i, NULL, _tpwork, tp);
}

void
tpfree(tpool_t *tp)
{
	tp->_stop = true;

	pthread_mutex_lock(&tp->_mtx);
	pthread_cond_broadcast(&tp->_cnd);
	pthread_mutex_unlock(&tp->_mtx);

	for (size_t i = 0; i < tp->_tcnt; i++)
		pthread_join(tp->_thrds[i], NULL);

	free(tp->_thrds);
	while (tp->_head) {
		struct _tjob *j = _tpdeq(tp);
		if (j->free)
			j->free(j->arg);
		free(j);
	}

	pthread_cond_destroy(&tp->_cnd);
	pthread_mutex_destroy(&tp->_mtx);
}

struct _tjob *
_tpdeq(tpool_t *tp)
{
	struct _tjob *j = tp->_head;

	if (j) {
		tp->_head = tp->_head->next;
		if (!tp->_head)
			tp->_tail = NULL;
	}

	return j;
}

void
tpenq(tpool_t *tp, tfunc_t fn, void *arg, tfree_func_t free)
{
	struct _tjob *j = bufalloc(NULL, 1, sizeof(struct _tjob));
	*j = (struct _tjob){
		.fn = fn,
		.arg = arg,
		.free = free,
	};

	pthread_mutex_lock(&tp->_mtx);
	if (tp->_tail)
		tp->_tail->next = j;
	if (!tp->_head)
		tp->_head = j;
	tp->_tail = j;
	tp->_left++;
	pthread_cond_signal(&tp->_cnd);
	pthread_mutex_unlock(&tp->_mtx);
}

void
tpwait(tpool_t *tp)
{
	pthread_mutex_lock(&tp->_mtx);
	while (!tp->_stop && tp->_left)
		pthread_cond_wait(&tp->_cnd, &tp->_mtx);
	pthread_mutex_unlock(&tp->_mtx);
}

#endif /* CBS_PTHREAD */

#endif /* !C_BUILD_SYSTEM_H */
