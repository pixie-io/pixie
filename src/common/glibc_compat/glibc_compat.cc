#include <glob.h>

// This file forces the linker to pick older versions of GLIBC so that we are less
// sensitive to distros that we run on. For example, our docker image is based on distroless
// which is based on debian stretch, which has GLIBC of 2.24. We primarily build on Ubuntu 18.04
// which has GLIBC 2.27 and in some rare cases will break backwards compatibility.
//
// In the event of an symbol newer than 2.24 we should add a wrapper here (as shown with glob
// below), and add a wrap flag to the BAZEL.build file in this directory.

extern "C" {

__asm__(".symver glob,glob@GLIBC_2.2.5");
int __wrap_glob(const char* pattern, int flags, int (*errfunc)(const char* epath, int eerrno),
                glob_t* pglob) {
  return glob(pattern, flags, errfunc, pglob);
}
}
