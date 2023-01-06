# C++ Style Guide Proposals/Opens

At Pixie, we mostly follow the [Google C++ style guide](<https://google.github.io/styleguide/cppguide.html>), but we do deviate from that style in some ways and have yet to decide a style in others. This document serves as a place where we can collect our unique Pixie style decisions and any open issues and their resolutions.

## Formatted Strings

**Style:** Use appropriate string formatting functions in different scenarios.

- Use std::string::operator+=() when you need to concatenate string.
- Use absl::StrJoin() to format a list of strings with separators.
- Use absl::StrFormat() when you actually need to **format string & non-string values**:
  - absl::StrFormat("%0.4f", 1.0)
- Use absl::Substitute() when you need to **fill string & non-string values into a template**:
  - absl::Substitute("string $0, integer $1, float $2, class $3", "str", 1, 1.0, object);
- Use absl::StrCat() when you need to **concatenate string & non-string values**:
  - Secondary: If performance is required prefer absl::StrCat()

**Example:**

  ```cpp
  absl::Substitute("[PID=$0] Unexpected event. Message: msg=$1.", pid, msg);
  ```

**Note:** The use of the standard << operator to create formatted strings is prohibited.

**Comments:** absl::Substitute is easy to use, and is slightly faster than absl::StrFormat() — even though we don’t typically use either in any performance critical code. Note that we previously used absl::StrFormat() in many places, but we should only use StrFormat() when it’s more powerful formatting is actually required.

## Default Arguments

**Style:** Default arguments are allowed in certain cases, with the general principle that it should not make the code hard to read. Specifically, the use of default arguments are subject to the following rules:

- Default arguments are only allowed on primitive types (no objects).
- Default arguments are not allowed on virtual functions (same as google style guide).
- Multiple default arguments are allowed, but discouraged when it makes the code hard to follow.

**Example:**

  ```cpp
  // This is okay, because it is easy to follow and is on a primitive type.
  void Init(int level = 0);
  ```

## Literal Function Arguments

**Style:** Include the name of inlined literals when using them in a function call, with the name before the literal value.

**Example:**

  ```cpp
  Compile(code, /* opt_level */ 0);
  ```

## Integer types (sizes, signed/unsigned)

**Style:** If the variable is a count of or index into structures in memory, then always use size_t.

- This makes sure the count is appropriately sized for the addressable memory of the machine. Note that this rule covers containers, since they are countable objects in memory.

This rule does not apply to any countable value, however, as counts in time (e.g. stats counters) should not use size_t.

Opens: For all other cases

- When do we use signed vs unsigned values?
- When do we use a size (int32_t) vs not (int)?
- If we use a size, what is our default (uint32_t vs uint64_t)?

## Accessor naming

Should we distinguish const vs. non-const accessors? I used to have mutable_ prefix on non-const accessor, and they should all return pointers (not non-const reference). The reason is to distinguish between 2 more clearly.

## Enum class

Always use enum class, not enum. Enum values should be named in the same style of constants. (Google’s style guide allow MACRO_LIKE_NAME for enums).

When use enum value in switch statement, **do not** provide default branch. This way, the compiler can catch missed values.

## Casts: static_cast vs dynamic_cast

**Proposed Style:** Use static_cast for both up-casting and down-casting. Avoid the use of dynamic_cast.

**Rationale:** dynamic_cast has a run-time cost, and we should be using casts in places where we know which type we are casting to.

**Alternative:** static_cast for up casting, dynamic_cast for down casting.

**Rationale:** Nudge against down casting. More readable to understand the intention.

## Returning multiple values

**Style:** Use structs to return multiple values. Avoid tuples.

**Rationale:** Provides clarity on what the return values are.

## Spacing Before entities (classes/structs)

```cpp
//OPTION 1
namespace pl {
namespace einstein {
struct Energy {
  int a;
  int b;
}

struct Mass {
  double m;
  double exp;
}
}
}

//OPTION 2
namespace pl {
namespace einstein {

struct Energy {
  int a;
  int b;
}

struct Mass {
  double m;
  double exp;
}

}
}
```

## Checks

`CHECK(<true|false>)`and `CHECK_*(<expression>)`: Unconditionally crash if false.

- Use only in startup of a binary to check preconditions specified from external sources. For example, `CHECK()` on flag values would be an ideal use of `CHECK`.

`DCHECK(<true|false>)` and `DCHECK_*(<expression>)`: In debug build, same as `CHECK()`; in non-debug build, this will be removed by compiler.

- Use in places that are perf-critical. For example, `DCHECK()` on nested loop conditions would be ideal.

`ECHECK(<true|false>)` and `ECHECK_*(<expression>)`: In debug build, same as `CHECK()`; in non-debug build, this causes a `LOG(ERROR)`.

- Use in places that are not perf-critical and can happen in real world. For example, to check on certain assumptions that might not be the case in practice.

## `CHECK(false)` vs `LOG(FATAL)`

And similarly, `ECHECK(false)` vs `LOG(DFATAL)`.

Example with a switch statement. Note that this is not specific to a switch statement, though. The style question is relevant where ever a `CHECK(false)` may be used.

  ```cpp
  switch (direction) {
    case kRequest:
      // Do something.
      break;
    case kResponse:
      // Do something.
      break;
    default:
      // Option 1:
      CHECK(false) << "Unexpected direction";

      // Option 2:
      LOG(FATAL) << "Unexpected direction"
  }
  ```

## `emplace` vs `try_emplace`

`try_emplace` is the newer version of `emplace`. Prefer `try_emplace` instead of `emplace` unless the object is not moveable. Note this is also the recommendation of absl.

## Prefer holding member variable over inheritance

Inheritance is usually difficult to maintain than holding a member variable.

Inheritance puts a "Is A" relationship with the subclasses in its semantic, which can easily diverge, and causes confusions.
Eg: The connection between `BCCWrapper` and `BCCSymbolizer` is not obvious.

Inheritance also makes it easier to accidentally use the parent class method without a clear indication.
Eg: There is no way to tell if a function called inside `BCCSymbolizer` is from `BCCWrapper` or its own.
It also makes protected member available to subclasses, which also extend the scope of misuse.

**Benefits of inheritance is convenience**, where accessing parent class' non-private member is without any restriction.
