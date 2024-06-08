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

— C99 and POSIX compliant
— Capturing of command output
— Easy command building and execution
— PkgConfig support
— Simple and easy to understand API
— Thread pool support


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

Coming soon!
