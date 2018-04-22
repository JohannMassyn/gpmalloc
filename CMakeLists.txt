cmake_minimum_required (VERSION 3.9.5)
project (gpmalloc)
add_executable(analysis test/analysis.c gpmalloc.c gpmalloc.h)