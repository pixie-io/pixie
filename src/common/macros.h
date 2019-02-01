// This file defines some commonly used macros in our code base.

#pragma once

// Disallow copy and assign constructors. This can be used to prevent
// implicit copying of objects. Should be added to the public: section
// of a class.
#ifndef PL_DISALLOW_COPY_AND_ASSIGN
#define PL_DISALLOW_COPY_AND_ASSIGN(TypeName) \
  TypeName(const TypeName&) = delete;         \
  void operator=(const TypeName&) = delete
#endif

// Warn if a result is unused.
#define PL_MUST_USE_RESULT __attribute__((warn_unused_result))

#define PL_UNUSED(x) (void)(x)

// Internal helper for concatenating macro values.
#define PL_CONCAT_NAME_INNER(x, y) x##y
#define PL_CONCAT_NAME(x, y) PL_CONCAT_NAME_INNER(x, y)
