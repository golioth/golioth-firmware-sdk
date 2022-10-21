add_library(unity STATIC unity/unity.c)
target_compile_definitions(unity PUBLIC -DUNITY_INCLUDE_CONFIG_H)
target_include_directories(unity PUBLIC unity .)
