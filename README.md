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

All functions and macros are documented in cbs.h.

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

If this macro is defined before including `cbs.h`, then support for thread pools
won’t be included meaning you don’t need to link with `-lpthread` when
bootstrapping the build script.

---

```c
#define lengthof(xs) /* … */
```

Return the number of elements in the static array `xs`.

### Startup Functions

These two functions should be called at the very beginning of your `main()`
function in the order in which they are documented here for everything to work
properly.

---

```c
void cbsinit(int argc, char **argv)
```

Should be the first function called in `main()` and passed the same parameters
received from `main()`.  It initializes some internal data, but it also changes
the current working directory so that the running process is in the same
directory as the location of the process.  For example if your build script is
called `make` and you call it as `./build/make`, this function will change your
working directory to `./build`.

---

```c
#define rebuild() /* … */
```

Should be called right after `cbsinit()`.  This function-like macro checks to
see if the build script is outdated compared to its source file.  If it finds
that the build script is outdated it rebuilds it before executing the new build
script.

### String Array Types and Functions

The following types and functions all work on dynamically-allocated arrays of
string, which make gradually composing a complete command that can be executed
very simple.

---

```c
struct strs {
	char **buf;
	size_t len, cap;
};
```

A type representing a dynamic array of strings.  The `len` and `cap` fields hold
the length and capacity of the string array respectively, and the `buf` field is
the actual array itself.  Despite being a sized array, `buf` is also guaranteed
by all the functions that act on this structure to always be null-terminated.

There is no initialization function for the `strs` structure.  To initialize the
structure simply zero-initialize it:

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

Deallocates all memory associated with the string array `xs`.  Note that this
does **not** deallocate memory associated with the individual elements in the
string array — that must still be done manually.

This function also zeros `xs` after freeing memory, so that the same structure
can be safely reused afterwards.

---

```c
void strszero(struct strs *xs)
```

Zeros the string array `xs` **without** deallocating any memory used by the
string array.  This allows you to reuse the same structure for a different
purpose without needing to reallocate a fresh new array, instead reusing the old
one.

---

```c
void strspush(struct strs *xs, char **ys, size_t n)
```

Append `n` strings from the string array `ys` to the end of `xs`.

---

```c
#define strspushl(xs, ...) /* … */
```

Append the strings specified by the provided variable-arguments to the end of
`xs`.

---

```c
void strspushenv(struct strs *xs, const char *ev, char **ys, size_t n)
```

Append the value of the environment variable `ev` to the end of `xs`.  If the
provided environment variable doesn’t exist or has the value of the empty
string, then fallback to appending `n` strings from `ys` to the end of `xs`.

---

```c
#define strspushenvl(xs, ev, ...)
```

Append the value of the environment variable `ev` to the end of `xs`.  If the
provided environment variable doesn’t exist or has the value of the empty
string, then fallback to appending the strings specified by the provided
variable-arguments to the end of `xs`.
