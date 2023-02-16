// Golioth comment:
//
// This file is required by the miniz library, but is not part of the
// source-controlled files in that library.
//
// Normally it is generated at build time in miniz's CMakeLists.txt.
// Since we're not using that CMakeLists.txt, we will just add this dir
// to the include path of our build.

#ifndef MINIZ_EXPORT_H
#define MINIZ_EXPORT_H

#ifndef MINIZ_EXPORT
#define MINIZ_EXPORT
#endif

#ifndef MINIZ_NO_EXPORT
#define MINIZ_NO_EXPORT
#endif

#ifndef MINIZ_DEPRECATED
#define MINIZ_DEPRECATED __attribute__((__deprecated__))
#endif

#ifndef MINIZ_DEPRECATED_EXPORT
#define MINIZ_DEPRECATED_EXPORT MINIZ_EXPORT MINIZ_DEPRECATED
#endif

#ifndef MINIZ_DEPRECATED_NO_EXPORT
#define MINIZ_DEPRECATED_NO_EXPORT MINIZ_NO_EXPORT MINIZ_DEPRECATED
#endif

// Configuration
#define MINIZ_NO_MALLOC 1

#endif /* MINIZ_EXPORT_H */
