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

CBS is very much inspired by Tsoding’s ‘Nob’.
