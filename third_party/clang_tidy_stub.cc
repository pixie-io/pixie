// WARNING HACK: This adds a stub so that we don't have to include all of
// clang-tidy with the LLVM build. We don't need to use clang-tidy since we don't
// do any auto cleanup/formatting during our compile process. If this ever changes,
// this stub will need to be removed.
// Refer to: https://reviews.llvm.org/D55415

extern "C" {
  volatile int ClangTidyPluginAnchorSource = 0;
  volatile int ClangIncludeFixerPluginAnchorSource = 0;
}
