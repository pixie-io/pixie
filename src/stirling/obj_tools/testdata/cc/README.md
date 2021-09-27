# Versioned symbols in dynamic library

Tried to pull this into `bazel` but couldn't make bazel linker recognize lib_foo.map.
The following is the closest we get.

```
load("@rules_cc//cc:defs.bzl", "cc_library", "cc_binary")
cc_binary(
    name = "lib_foo.so",
    linkopts = ["-Wl,--version-script,$(location lib_foo.map)"],
    data = [":lib_foo_map"],
    srcs = [
        "lib_foo.h",
        "lib_foo.cc",
    ],
    linkshared = 1,
)
```

To reproduce the `lib_foo.so` file:

```
# At the top of the repo.
clang -o lib_foo.o -c src/stirling/obj_tools/testdata/dynlib_versioned_symbols/lib_foo.c
clang -shared -o lib_foo.so lib_foo.o -Wl,--version-script,src/stirling/obj_tools/testdata/dynlib_versioned_symbols/lib_foo.map
# Rename to lib_foo_so to prevent build failure, as bazel ignores .so file.
mv lib_foo.so src/stirling/obj_tools/testdata/dynlib_versioned_symbols/lib_foo_so
```

See https://man7.org/conf/lca2006/shared_libraries/slide19a.html for more details.
