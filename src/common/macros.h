// This file defines some commonly used macros in our code base.

#pragma once

// Warn if a result is unused.
#ifdef __clang__
#define PL_MUST_USE_RESULT __attribute__((warn_unused_result))
#else
#define PL_MUST_USE_RESULT
#endif

#define PL_UNUSED(x) (void)(x)

// Internal helper for concatenating macro values.
#define PL_CONCAT_NAME_INNER(x, y) x##y
#define PL_CONCAT_NAME(x, y) PL_CONCAT_NAME_INNER(x, y)
