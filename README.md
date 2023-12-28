# CBS — The C Build System

CBS is a single-header library for writing build scripts in C.  The
philosophy behind this project is that the only tool you should ever need
to build your C projects is a C compiler.  Not Make, not Cmake, not
autoconf — just a C compiler.

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
#include "cbs.h"

#define needs_rebuild(dst, src) (!fexists(dst) || fmdolder(dst, src))

#define CC     "cc"
#define CFLAGS "-Wall", "-Wextra", "-Werror", "-O3"
#define TARGET "my-file"

int
main(int argc, char **argv)
{
	int ec;
	struct cmd cmd = {0};

	cbsinit(argc, argv);
	rebuild();

	if (!needs_rebuild(TARGET, TARGET ".c"))
		return EXIT_SUCCESS;

	cmdadd(&cmd, "cc");
	if (!pcquery(&cmd, "liblux", PKGC_LIBS | PKGC_CFLAGS))
		cmdadd(&cmd, "-llux");
	cmdadd(&cmd, CFLAGS, "-o", TARGET, TARGET ".c");
	cmdput(cmd);
	if ((ec = cmdexec(cmd)) != EXIT_SUCCESS)
		diex("%s failed with exit-code %d", *cmd.argv, ec);

	return EXIT_SUCCESS;
}
```
