# Style Guide

This style guide covers code formatting and style guidelines for new C code.

We rely on the C99 standard. The code should compile in GCC or clang.

These are not all-encompassing rules, but they cover a lot of the style questions
that might come up. There's still a lot of room for personal style and judgement calls.

Exceptions and deviations from the guidelines may be necessary in some cases, which is fine
as long as justification is provided.

If there are style questions not covered by this guide, then as a fallback, it's preferred to:

* Follow the style of existing code (i.e. try to infer the style)
* Follow the [Linux kernel coding style](https://www.kernel.org/doc/html/latest/process/coding-style.html)
* Ask a style question on [Discord](https://discord.com/invite/qKjmvzMVYR)

# Tools: clang-format, editorconfig, git hooks

Basic style is checked in CI using `clang-format` based on rules defined in
[`.clang-format`](../.clang-format) (e.g. where to put braces, indentation width, etc). If that check
fails, the pull request can not merge.

It's recommended to install an [editorconfig](https://editorconfig.org/) plugin in your editor so
that it can use the formatting rules defined in [`.editorconfig`](../.editorconfig) from this repo.

It's also recommended to manually run `clang-format` before making commits locally, which is useful
to catch formatting errors early:

```sh
git clang-format --diff main
```

# Naming

## General Naming

* Be descriptive
* Avoid using abbreviations that are not well-known
* Avoid using cryptic abbreviations (e.g. removing letters from words)
* Use `lower_snake_case` for non-const variables and functions
* Use `UPPER_SNAKE_CASE` for const variables, enum values, and macros
   ```
   #define WAIT_TIME_MS 1000

   const uint8_t MAX_RETRY_COUNT = 5;

   typedef enum {
       STATE_RESET,
       STATE_IDLE,
       STATE_BUSY,
       STATE_VERIFY
   } state_t;

   #define MIN(a,b)  ((a) < (b) ? (a) : (b))
   ```

## File Names

* Use `lower_snake_case` for filenames
* Do not use filenames that might clash with other header files in any include paths, such as `string.h`.

## Variable Names

* Prefix static globals with `_`. This helps distinguish them from local variables.
* Append units to variable names (e.g. `ms` for milliseconds). This makes the code easier
  to reason about and can help prevent potential math/conversion errors.

## Type Names

* Use `lower_snake_case` for all type names
* For typedefs, add a `_t` suffix

  ```c
  typedef struct {
      float x;
      float y;
  } point_t;

  // syntax for defining a function pointer type
  typedef void (*signal_handler_fn_t)(int32_t signum);
  ```
* For enum values, prefix with a unique name to avoid namespace clashes (enum values are in
  the global namespace in C).

## Function Names

* Public functions (defined in header file, intended to be called from outside) should have
  a prefix that matches the name of the file it's defined/declared in.

  This makes it easy to determine which file the function comes from at the call site.

  ```diff
  // network.h
  -void init(); // should have prefix matching file name
  +void network_init();
  ```

* Prefer to use verb or verb-phrases for function names.

  ```diff
  // network.h
  -static void network_packet_send(); // verb should come first
  +static void network_send_packet();
  ```

* Prefer to use the verbs "create" and "destroy" (as opposed to, e.g. new/free).

  This is for consistency, not because these two verbs are inherently better than
  any of the other possible choices.

  ```c
  widget_t *widget_create(...);
  void widget_destroy(widget_t*);
  ```

# Comments

Try to write your code in a way that is readable without having to rely on comments.
Assume no one will read your comments (because they probably won't).

This is not to say comments are bad and shouldn't be used. Comments can be extremely
helpful in the right context. Rather, you should spend the most effort trying to refactor
the code to make it more readable without comments, and only after doing that,
should you add comments to explain things that might not make sense to readers.

If a code reviewer has questions about a certain piece of code, it could be a good
place to add a code comment.

* Use `//` comments instead of `/* */`
* Comments should explain "why", not "what" or "how"
* Avoid comments that reference implementation specifics

  These kind of comments are prone to getting out of sync with the code.
  If this happens, then the comment is misleading and incorrect, which is
  worse than having no comment at all.

  ```diff
  #define MAX_ARRAY_SIZE 16

  -// Modulo 16, to ensure index stays within array bounds
  +// Ensure the index stays within array bounds
  uint16_t wr_index = (wr_index + 1) % MAX_ARRAY_SIZE;
  ```
* Do not commit commented-out code to source control

## Doxygen Comments

* Use `///` style
* Use `@param [direction] <name> <description>` for function parameters
    - `direction` is one of `in`, `out`, or `inout`
    - Note: a lot of existing code does not specify param direction (these need to be fixed).
* Use separate `@retval <value> <description>` lines for each possible return type.
* For `const char *` string parameters, indicate whether the string is expected
  to be NULL-terminated or not.
* If there are parameters that must not be NULL, indicate this in the description.
* If there are optional parameters which can be NULL or 0, indicate this in the description.

## License Comment

Put this header at the top of each file, updating the copyright year whenever you touch it.

```c
/*
 * Copyright (c) 2022 Golioth, Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */
```

# Header Files

* Prefer to have a `<name>.h` for every `<name>.c` file.
* Use `#pragma once` at the top of every header file, to prevent multiple inclusion
* Use `<>` style of include for system or C library headers (e.g. `#include <stdio.h>`)
* Use `""` style of include for local/internal header files

# Formatting and Style

- Most of the rules in this section are codified in [`.clang-format`](../.clang-format) and
[`.editorconfig`](../.editorconfig).
- They are listed out here for convenience.

If an exception to clang-format is needed, you can wrap your code like this:

```c
// clang format-off
// clang format-on
```

## Line and Function Lengths

* Functions should generally be less than 80 lines
* Line length is limited to 100 characters

## Indentation

* Do not use tabs anywhere (unless required, like in a Makefile)
* Use 4 spaces for all indentation
* Preprocessor directives should have no indentation (start in column 0)

## Whitespace

* One blank line between functions in a source file
  ```c
  void foo(void)
  {
      // ...
  }

  void bar(void)
  {
      // ...
  }
  ```

* Use spaces after control flow constructs
  ```diff
  -if(condition)
  +if (condition)
  ```

* No extra space inside parentheses for control flow constructs
  ```diff
  -if ( condition1, condition2 )
  +if (condition1, condition2)
  ```

* No extra space after function names or inside parentheses

  ```diff
  -foo ();
  +foo();

  -foo( param1, param2 );
  +foo(param1, param2);
  ```

* Pointers should hug the variable, not the type

  ```diff
  -char* my_string;
  +char *my_string;
  ```

* Prefer the "west-const" style (as opposed to "east-const")

  ```diff
  -char const *my_string;
  +const char *my_string;
  ```

* Align arguments after an open bracket.

  ```c
  result_type_t do_something(uint8_t number_of_times)
  ```
  ```c
  result_type_t do_something_tricky(uint8_t number_of_times,
                                    object *target)
  ```
  ```c
  result_type_t do_something_even_trickier(uint8_t number_of_times,
                                           const modifier *modifier,
                                           object *target)
  ```
  ```diff
  -void my_function(uint32_t var_a, uint32_t var_b,
  -                 uint32_t var_c,
  -                 uint32_t var_d)
  +void my_function(uint32_t var_a,
  +                 uint32_t var_b,
  +                 uint32_t var_c,
  +                 uint32_t var_d)
  ```

## Braces

* Use "Allman" style braces:

  ```c
  // comment here if it describes the whole if/else if/else sequence
  if (condition1)
  {
      // comment here; describes "current state at this point in code"
  }
  else if (condition2)
  {
      // comment here
  }
  else
  {
      // comment here
  }
  ```

## Simple Types and Variables

* As a guideline, limit the number of variables to 7 or less.
  A good quote from https://www.kernel.org/doc/html/v4.10/process/coding-style.html#functions:

> Another measure of the function is the number of local variables. They shouldn’t exceed 5-10,
> or you’re doing something wrong. Re-think the function, and split it into smaller pieces.
> A human brain can generally easily keep track of about 7 different things, anything more and
> it gets confused. You know you’re brilliant, but maybe you’d like to understand what you
> did 2 weeks from now.

* Variable declarations should be initialized to a default value
* Do not put multiple variable declarations on a single line

  ```diff
  -int32_t a, b;
  +int32_t a = 0;
  +int32_t b = 0;
  ```
* Avoid magic numbers. Use named constants instead.
* Keep variable declarations close to their usage.
* Keep variable scope minimized.

  ```diff
  -uint32_t x = 0;
  -for (uint32_t i = 0; i < num_elements; i++)
  -{
  -    x = my_array[i];
  -    // more code...
  -}

  +for (uint32_t i = 0; i < num_elements; i++)
  +{
  +    uint32_t x = my_array[i];
  +    // more code...
  +}
  ```

* Use types with explicit ranges, such as uint8_t, int64_t, uint32_t, over the
  machine-dependent int, unsigned int, long etc.

  ```diff
  -int foobar = 4;
  +int32_t foobar = 4;
  ```

# Functions

* For passing data back to the caller, prefer to use return values instead of
  output parameters.
* Returning structs by value can be okay if they're small (e.g. 4-16 bytes).
* Output parameters should appear after input parameters
* Struct input parameters should be passed by `const *`. This makes it clear they're not mutated.
  ```c
  size_t get_widget_size(const widget_t *widget);
  ```
* Prefer to return early from a function on error instead of using cascading if/else branches.
* `goto`'s may be used for error handling and cleanup in a function, to simplify control flow.

# Recommended Design Practices

* Use assertions liberally as a defensive technique to catch potential bugs introduced
  by the programmer, and to make assumptions explicit.
* Check pointers for NULL before dereferencing.
* Global variables are strongly discouraged for the following reasons:
    - Can cause race conditions if accessed by multiple threads
    - Hard to reason about code that has hidden/global state
    - Difficult to test, results in non-determinism when test run multiple times

  Even in cases where it makes sense for something to be a "singleton" (i.e. only
  makes sense for there to be one of those things), you should still avoid globals,
  for the same reasons listed above.

  If global variables must be used, prefer to limit the scope to function or file
  with the `static` keyword. If it needs to be accessed from outside the file, provide
  accessor functions (e.g. `int get_my_global(); void set_my_global(int);`).

* Prefer to zero-initialize structs. This prevents bugs related to uninitialized memory access,
  which can be difficult to track down.
    ```c
    my_struct_t data = {};
    ```
* For variables and structs, the value `0` should be a reasonable default.
  Most data is zero-initialized before first use, so make sure to avoid
  creating variables where the value 0 could result in some kind of undesirable behavior
  (e.g. `uint32_t countdown_until_i_blow_up_the_planet;`).
* Avoid dynamic memory allocation. Exceptions can be made for cases where static allocation
  results in an unreasonable amount of added code complexity or memory usage.
* Don't write recursive functions
* Avoid macro-oriented APIs. Prefer to use normal C types and functions. For people
  new to the API, it's much easier to understand how the API works when it's not hidden
  behind layers of opaque macros.
* For public APIs, reduce the surface area of the API as much as possible. This makes the
  API easier to use, and harder to misuse.
    - Surface area includes: number of types defined, number of functions defined,
      number of macros defined, and how much data is exposed/mutable by outside code.
      In short: how much "stuff" the user has to understand to use the API as intended.
    - Opaque pointers/handles can help hide data behind the API so it's not directly accessible
      by users.
* If indentation is 4 or more levels deep, you should refactor the function.
* If you allow a caller to register a callback function, make sure they can also register
  a user-defined `void *` that will be passed to the callback when called.

  ```c
  static void on_event(event_t event, void *user_arg)
  {
      // ...
  }

  context_t context = { /* ... */ };
  register_event_callback(on_event, &context);
  ```
