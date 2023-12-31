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

#include "cbs.h"

#define CC     "cc"
#define CFLAGS "-Wall", "-Wextra", "-Werror", "-O3"
#define TARGET "my-file"

int
main(int argc, char **argv)
{
	int ec;
	cmd_t cmd = {0};
	struct strv v = {0};

	cbsinit(argc, argv);
	rebuild();

	if (!foutdated(TARGET, TARGET ".c"))
		return EXIT_SUCCESS;

	cmdadd(&cmd, CC);
	if (pcquery(&v, "liblux", PKGC_LIBS | PKGC_CFLAGS))
		cmdaddv(&cmd, v.buf, v.len);
	else
		cmdadd(&cmd, "-llux");
	cmdadd(&cmd, CFLAGS, "-o", TARGET, TARGET ".c");

	cmdput(cmd);
	if ((ec = cmdexec(cmd)) != EXIT_SUCCESS)
		diex("%s failed with exit-code %d", *cmd._argv, ec);

	return EXIT_SUCCESS;
}
```


## Example With Threads

This is like the previous example, but you should compile with -lpthread.  This
is not the most efficient way to build a multi-file project, but this is simply
for demonstration purposes.

```c
#include <errno.h>
#include <string.h>

#define CBS_PTHREAD
#include "cbs.h"

#define CC     "cc"
#define CFLAGS "-Wall", "-Wextra", "-Werror", "-O3"
#define TARGET "my-file"

static const char *sources[] = {"foo.c", "bar.c", "baz.c"};
static const char *objects[] = {"foo.o", "bar.o", "baz.o"};

static void build(void *);

int
main(int argc, char **argv)
{
	int ec, cpus;
	cmd_t cmd = {0};
	tpool_t tp;

	cbsinit(argc, argv);
	rebuild();

	if (!foutdatedv(TARGET, sources, lengthof(sources)))
		return EXIT_SUCCESS;

	if ((cpus = nproc()) == -1) {
		if (errno)
			die("nproc");
		/* System not properly supported; default to 8 threads */
		cpus = 8;
	}
	tpinit(&tp, cpus);

	for (size_t i = 0; i < lengthof(sources); i++)
		tpenq(&tp, build, sources[i], NULL);
	tpwait(&tp);
	tpfree(&tp);

	cmdadd(&cmd, CC, "-o", TARGET);
	cmdaddv(&cmd, objects, lengthof(objects));
	cmdput(cmd);
	if ((ec = cmdexec(cmd)) != EXIT_SUCCESS)
		diex("%s failed with exit-code %d", *cmd._argv, ec);

	return EXIT_SUCCESS;
}

void
build(void *arg)
{
	int ec;
	char *dst, *src = arg;
	cmd_t cmd = {0};

	for (size_t i = 0; i < lengthof(sources); i++) {
		/* All the sources and objects have 3 letter names + an extension */
		if (strncmp(src, objects[i], 3) == 0) {
			dst = objects[i];
			break;
		}
	}

	cmdadd(&cmd, CC, CFLAGS, "-o", dst, "-c", src);
	cmdput(cmd);
	if ((ec = cmdexec(cmd)) != EXIT_SUCCESS)
		diex("%s failed with exit-code %d", *cmd._argv, ec);
	free(cmd._argv);
}
```
