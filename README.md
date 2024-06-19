# CBS — The C Build System

CBS is a single-header library for writing build scripts in C.  The
philosophy behind this project is that the only tool you should ever need
to build your C projects is a C compiler.  Not Make, not Cmake, not
Autoconf — just a C compiler.

Using C for your build system also has numerous advantages.  C is
portable to almost any platform, C is a turing-complete language that
makes performing very specific build steps easy, and anyone working on
your C project already knows C.

CBS does not aim to be the most powerful and ultimate high-level API.  It
simply aims to be a set of potentially useful functions and macros to
make writing a build script in C a bit easier.  If there is functionality
you are missing, then add it.  You’re a programmer aren’t you?

All functions and macros are documented in this file.

CBS is very much inspired by Tsoding’s ‘Nob’.


## Features

- C99 and POSIX compliant
- Capturing of command output
- Easy command building and execution
- PkgConfig support
- Simple and easy to understand API
- Thread pool support


## Important

This library works with the assumption that your compiled build script
*and* your build script source code are located in the **same
directory**.  The general project structure of your project is intended
to look like so:

```
.
├── cbs.h   # This library
├── make    # The compiled build script
├── make.c  # The build script source
└── …       # Your own files
```


## Example

Assuming you have a source file `my-file.c` — you can compile this build
script (called `build.c` for example) with `cc build.c` to bootstrap —
and then run `./a.out` to build the `my-file` binary linked against the
liblux library.

If you make any modifications to the build script *or* to the cbs.h
header, there is no need to manually recompile — the script will rebuild
itself.

```c
#include <stdlib.h>

#define CBS_NO_THREADS
#include "cbs.h"

static char *cflags[] = {"-Wall", "-Wextra", "-Werror", "-O3"};

int
main(int argc, char **argv)
{
	/* Initialize the library, and rebuild this script if needed */
	cbsinit(argc, argv);
	rebuild();

	/* If the compiled binary isn’t outdated, do nothing */
	if (!foutdatedl("my-file", "my-file.c"))
		return EXIT_SUCCESS;

	/* Append ‘cc’ and our cflags to the command, but allow the user to use the
	   $CC and $CFLAGS environment variables to override them */
	struct strs cmd = {0};
	strspushenvl(&cmd, "CC", "cc");
	strspushenv(&cmd, "CFLAGS", cflags, lengthof(cflags));

	/* Call pkg-config with the --libs and --cflags options for the library
	  ‘liblux’, appending the result to our command.  If it fails then we
	  fallback to using -llux */
	if (!pcquery(&cmd, "liblux", PC_LIBS | PC_CFLAGS))
		strspushl(&cmd, "-llux");

	/* Push the final arguments to our command */
	strspushl(&cmd, "-o", "my-file", "-c", "my-file.c");

	/* Print our command to stdout, and execute it */
	cmdput(cmd);
	return cmdexec(cmd);
}
```


## Example With Threads

This is like the previous example, but you should compile with -lpthread.

```c
#include <stdlib.h>

#include "cbs.h"

static char *cflags[] = {"-Wall", "-Wextra", "-Werror", "-O3"};
static char *sources[] = {"foo.c", "bar.c", "baz.c"};

static void build(void *);

int
main(int argc, char **argv)
{
	cbsinit(argc, argv);
	rebuild();

	if (!foutdated("my-file", sources, lengthof(sources)))
		return EXIT_SUCCESS;

	/* Get the number of CPUs available.  If this fails we fallback to 8. */
	int cpus = nproc();
	if (cpus == -1)
		cpus = 8;

	/* Create a thread pool, with one thread per CPU */
	tpool tp;
	tpinit(&tp, cpus);

	/* For each of our source files, add a task to the thread pool to build
	   the file ‘sources[i]’ with the function ‘build’ */
	for (size_t i = 0; i < lengthof(sources); i++)
		tpenq(&tp, build, sources[i], NULL);

	/* Wait for all the tasks to complete and free the thread pool */
	tpwait(&tp);
	tpfree(&tp);

	struct strs cmd = {0};
	strspushenvl(&cmd, "CC", "cc");
	strspushl(&cmd, "-o", "my-file");

	for (size_t i = 0; i < lengthof(sources); i++)
		strspushl(&cmd, swpext(sources[i], "o"));

	cmdput(cmd);
	return cmdexec(cmd);
}

void
build(void *arg)
{
	/* This function will be called by the thread pool with ‘arg’ set to a
	   filename such as ‘foo.c’ */

	struct strs cmd = {0};

	strspushenvl(&cmd, "CC", "cc");
	strspushenv(&cmd, "CFLAGS", cflags, lengthof(cflags));

	/* Allocate a copy of the string ‘arg’, with the file extension replaced.
	   This will for example return ‘foo.o’ when given ‘foo.c’ */
	char *object = swpext(arg, "o");

	strspushl(&cmd, "-o", object, "-c", arg);

	cmdput(cmd);
	if (cmdexec(cmd) != EXIT_SUCCESS)
		exit(EXIT_FAILURE);
	free(object);
	strsfree(&cmd);
}
```


## Documentation

### Macros

```c
#define CBS_NO_THREADS
```

If this macro is defined before including `cbs.h`, then support for
thread pools won’t be included meaning you don’t need to link with
`-lpthread` when bootstrapping the build script.

---

```c
#define lengthof(xs) /* … */
```

Return the number of elements in the static array `xs`.

### Startup Functions

These two functions should be called at the very beginning of your
`main()` function in the order in which they are documented here for
everything to work properly.

---

```c
void cbsinit(int argc, char **argv)
```

Should be the first function called in `main()` and passed the same
parameters received from `main()`.  It initializes some internal data,
but it also changes the current working directory so that the running
process is in the same directory as the location of the process.  For
example if your build script is called `make` and you call it as
`./build/make`, this function will change your working directory to
`./build`.

---

```c
#define rebuild() /* … */
```

Should be called right after `cbsinit()`.  This function-like macro
checks to see if the build script is outdated compared to its source
file.  If it finds that the build script is outdated it rebuilds it
before executing the new build script.

### String Array Types and Functions

The following types and functions all work on dynamically-allocated
arrays of string, which make gradually composing a complete command that
can be executed very simple.

---

```c
struct strs {
	char **buf;
	size_t len, cap;
};
```

A type representing a dynamic array of strings.  The `len` and `cap`
fields hold the length and capacity of the string array respectively, and
the `buf` field is the actual array itself.  Despite being a sized array,
`buf` is also guaranteed by all the functions that act on this structure
to always be null-terminated.

There is no initialization function for the `strs` structure.  To
initialize the structure simply zero-initialize it:

```c
int
main(int argc, char **argv)
{
	/* … */
	struct strs cmd = {0};
	strspush(&cmd, "cc");
	/* … */
}
```

---

```c
void strsfree(struct strs *xs)
```

Deallocates all memory associated with the string array `xs`.  Note that
this does **not** deallocate memory associated with the individual
elements in the string array — that must still be done manually.

This function also zeros `xs` after freeing memory, so that the same
structure can be safely reused afterwards.

---

```c
void strszero(struct strs *xs)
```

Zeros the string array `xs` **without** deallocating any memory used by
the string array.  This allows you to reuse the same structure for a
different purpose without needing to reallocate a fresh new array,
instead reusing the old one.

---

```c
void strspush(struct strs *xs, char **ys, size_t n)
```

Append `n` strings from the string array `ys` to the end of `xs`.

---

```c
#define strspushl(xs, ...) /* … */
```

Append the strings specified by the provided variable-arguments to the
end of `xs`.

---

```c
void strspushenv(struct strs *xs, const char *ev, char **ys, size_t n)
```

Append the value of the environment variable `ev` to the end of `xs`.  If
the provided environment variable doesn’t exist or has the value of the
empty string, then fallback to appending `n` strings from `ys` to the end
of `xs`.

---

```c
#define strspushenvl(xs, ev, ...)
```

Append the value of the environment variable `ev` to the end of `xs`.  If
the provided environment variable doesn’t exist or has the value of the
empty string, then fallback to appending the strings specified by the
provided variable-arguments to the end of `xs`.

### File Information Functions

The following functions are useful for performing common checks on files.

---

```c
bool fexists(const char *s);
```

Returns `true` if the file `s` exists, and `false` otherwise.  If you
want to check if a certain binary is present on the host system, you
should use `binexists()` instead.

---

```c
int fmdcmp(const char *x, const char *y);
```

Returns a value greater than 0 if the file `x` was modified more recently
than the file `y`, a value lower than 0 if the file `y` was modified more
recently than the file `x`, and 0 if the two files were modified at the
exact same time.

---

```c
bool fmdnewer(const char *x, const char *y);
bool fmdolder(const char *x, const char *y);
```

The `fmdnewer()` and `fmdolder()` functions return `true` if the file `x`
was modified more- or less recently than the file `y` respectively, and
`false` otherwise.

---

```c
bool foutdated(const char *x, char **xs, size_t n);
```

Returns `true` if any of the `n` files in the array `xs` were modified
more recently than the file `x`.

---

```c
#define foutdatedl(x, ...)
```

Returns `true` if any of the files specified by the variable-arguments
were modified more recently than the file `x`.

### Command Execution Functions

The following functions are used to execute commands.  It is a common
task that you may want to interface with `pkg-config` or change the
extension of a file while building up a command.  Functions to perform
these tasks are not listed in this section, but instead listed later on
in this document.

---

```c
int cmdexec(struct strs cmd);
```

Execute the command composed by the command-line arguments specified in
`cmd`, wait for the command to complete execution, and return its exit
status.

---

```c
int cmdexec_read(struct strs cmd, char **buf, size_t *bufsz);
```

Execute the command composed by the command-line arguments specified in
`cmd`, wait for the command to complete execution, and return its exit
status.  Additionally, the standard output of the command is captured and
stored in the buffer pointed to by `buf`, with the length of the buffer
being stored in `bufsz`.

Note that `buf` will not be null-terminated, and must be freed by a call
to `free()` after use.

---

```c
pid_t cmdexec_async(struct strs cmd);
```

Execute the command composed by the command-line arguments specified in
`cmd` asynchronously and return its process ID.

---

```c
int cmdwait(pid_t pid);
```

Wait for the process specified by `pid` to terminate and return its exit
status.

---

```c
void cmdput(struct strs cmd);
void fcmdput(FILE *stream, struct strs cmd);
```

Print a representation of the command composed by the command-line
arguments specified in `cmd` to the standard output, with shell
metacharacters properly quoted.

This function is useful for implementing `make(1)`-like command-echoing
behaviour.

The `fcmdput()` function is identical to `cmdput()` except the output is
written to `stream` as opposed to `stdout`.

### Thread Pool Types and Functions

TODO

### Miscellaneous Functions

The following functions are all useful, but don’t quite fall into any of
the specific function categories and namespaces documented above.

---

```c
bool binexists(const char *s);
```

Return `true` if a binary of the name `s` is located anywhere in the
users `$PATH`, and `false` otherwise.

---

```c
int nproc(void);
```

Return the number of available CPUs, or `-1` on error.

---

```c
char *swpext(const char *file, const char *ext);
```

Return a copy of the string `file` with the file extension set to the
string `ext`.  The file extension is defined to be the contents following
the last occurance of a period in `file`.

The returned string is allocated via `malloc()` and should be freed by a
call to `free()` after use.

---

```c
enum pkg_config_flags {
    PC_CFLAGS = /* --cflags */,
    PC_LIBS   = /* --libs   */,
    PC_SHARED = /* --shared */,
    PC_STATIC = /* --static */,
};

bool pcquery(struct strs *cmd, const char *lib, int flags);
```

Query `pkg-config` for the library `lib` and append the output to the
command specified by `cmd`, returning `true` if successful and `false` if
`pkg-config` exited with a failing exit code.

`flags` is a bitwise-ORd set of values in the `pkg_config_flags`
enumeration which control the flags passed to `pkg-config`.  The above
synopsis documents which enumeration values map to which command-line
flag.

It may be useful to append a default value to `cmd` if `pkg-config` fails
for whatever reason.  As an example you may do the following when linking
to liburiparser:

```c
struct strs cmd = {0};
strspushl(&cmd, "cc");
if (!pcquery(&cmd, "uriparser", PC_CFLAGS | PC_LIBS))
	strspushl(&cmd, "-luriparser"); /* fallback */
strspushl(&cmd, "-o", "main", "main.c");
```
