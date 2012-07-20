AUTOMAKE_OPTIONS = subdir-objects
ACLOCAL_AMFLAGS = ${ACLOCAL_FLAGS}

bin_PROGRAMS = dataroller
dataroller_SOURCES = src/main.c  \
               src/shared.c \
               src/timer.c \
               src/bitio.c \
               src/file.c \
               src/compress_lzw.c \
               src/decompress_lzw.c               

dist_noinst_SCRIPTS = build.sh clean.sh debug.sh
