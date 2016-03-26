projects := twx

twx_prod := slib dlib

twx_csrc := twx.c blank.c htxt.c itxt.c
twx_chdr := twx.h

# xxx_cflags (1: prj, 2: prod, 3: cfg, 4: bld, 5: src)
twx_cflags = -DTWX_TARGET='"$($4_target)"' -DTWX_CONFIG='"$3"' -DTWX_COMPILER='"$($4_compiler)"'
twx_slib_cflags := -DTWX_STATIC -DZLX_STATIC -DACX1_STATIC -DHBS_STATIC
twx_dlib_cflags := -DTWX_DYNAMIC
twx_ldflags = -lacx1 -lhbs$($3_sfx) -lzlx$($3_sfx)

include icobld.mk

