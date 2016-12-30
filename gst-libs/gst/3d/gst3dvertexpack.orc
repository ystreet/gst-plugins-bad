.function vertex_orc_unpack_u8
.dest 4 d1 gint32
.source 1 s1 guint8
.temp 2 t1

convubw t1, s1
convuwl d1, t1

.function vertex_orc_unpack_u8norm
.dest 4 d1 gint32
.source 1 s1 guint8
.temp 4 t3

splatbl t3, s1
shrul d1, t3, 1


.function vertex_orc_unpack_u8norm_trunc
.dest 4 d1 gint32
.source 1 s1 guint8
.const 4 c1 0x80000000
.const 4 c2 24
.temp 4 t3

splatbl t3, s1
shll t3, t3, c2
xorl d1, t3, c1


.function vertex_orc_unpack_s8
.dest 4 d1 gint32
.source 1 s1 guint8
.temp 2 t1

convsbw t1, s1
convswl d1, t1


.function vertex_orc_unpack_s8norm
.dest 4 d1 gint32
.source 1 s1 guint8
.const 4 c1 0x00808080
.temp 4 t3

splatbl t3, s1
xorl d1, t3, c1


.function vertex_orc_unpack_s8norm_trunc
.dest 4 d1 gint32
.source 1 s1 guint8
.const 4 c1 24
.temp 4 t3

splatbl t3, s1
shll d1, t3, c1


.function vertex_orc_unpack_u16
.dest 4 d1 gint32
.source 2 s1 guint8

convuwl d1, s1


.function vertex_orc_unpack_u16norm
.dest 4 d1 gint32
.source 2 s1 guint8
.temp 4 t2


mergewl t2, s1, s1
shrul d1, t2, 1

.function vertex_orc_unpack_u16norm_trunc
.dest 4 d1 gint32
.source 2 s1 guint8
.temp 4 t2


mergewl t2, s1, s1
shll t2, t2, 16
shrul d1, t2, 1

.function vertex_orc_unpack_s16
.dest 4 d1 gint32
.source 2 s1 guint8

convswl d1, s1


.function vertex_orc_unpack_s16norm
.dest 4 d1 gint32
.source 2 s1 guint8
.const 4 c1 0x00008000
.temp 4 t2

mergewl t2, s1, s1
xorl d1, t2, c1


.function vertex_orc_unpack_s16norm_trunc
.dest 4 d1 gint32
.source 2 s1 guint8
.const 4 c1 16
.temp 4 t2

convuwl t2, s1
shll d1, t2, c1


.function vertex_orc_unpack_u16_swap
.dest 4 d1 gint32
.source 2 s1 guint8
.const 4 c1 0x80000000
.temp 2 t1

swapw t1, s1
convuwl d1, t1


.function vertex_orc_unpack_u16norm_swap
.dest 4 d1 gint32
.source 2 s1 guint8
.temp 2 t1
.temp 4 t2

swapw t1, s1
mergewl t2, t1, t1
shrul d1, t2, 1


.function vertex_orc_unpack_u16norm_swap_trunc
.dest 4 d1 gint32
.source 2 s1 guint8
.temp 2 t1
.temp 4 t2

swapw t1, s1
convuwl t2, t1
shll t2, t2, 16
shrul d1, t2, 1


.function vertex_orc_unpack_s16_swap
.dest 4 d1 gint32
.source 2 s1 guint8
.temp 2 t1

swapw t1, s1
convswl d1, t1


.function vertex_orc_unpack_s16norm_swap
.dest 4 d1 gint32
.source 2 s1 guint8
.const 4 c1 0x00008000
.temp 2 t1
.temp 4 t2

swapw t1, s1
mergewl t2, t1, t1
xorl d1, t2, c1


.function vertex_orc_unpack_s16norm_swap_trunc
.dest 4 d1 gint32
.source 2 s1 guint8
.temp 2 t1
.temp 4 t2

swapw t1, s1
convuwl t2, t1
shll d1, t2, 16


.function vertex_orc_unpack_u32
.dest 4 d1 gint32
.source 4 s1 guint8
.const 4 c1 0x7fffffff
.temp 4 t1

andl d1, s1, c1

.function vertex_orc_unpack_u32norm
.dest 4 d1 gint32
.source 4 s1 guint8

shrul d1, s1, 1


.function vertex_orc_unpack_u32_swap
.dest 4 d1 gint32
.source 4 s1 guint8
.const 4 c1 0x7fffffff
.temp 4 t1

swapl t1, s1
andl d1, t1, c1

.function vertex_orc_unpack_u32norm_swap
.dest 4 d1 gint32
.source 4 s1 guint8
.temp 4 t1

swapl t1, s1
shrul d1, t1, 1


.function vertex_orc_unpack_s32
.dest 4 d1 gint32
.source 4 s1 guint8

copyl d1, s1


.function vertex_orc_unpack_s32_swap
.dest 4 d1 gint32
.source 4 s1 guint8

swapl d1, s1


.function vertex_orc_unpack_f32
.dest 8 d1 gdouble
.source 4 s1 gfloat

convfd d1, s1


.function vertex_orc_unpack_f32_swap
.dest 8 d1 gdouble
.source 4 s1 gfloat
.temp 4 t1

swapl t1, s1
convfd d1, t1


.function vertex_orc_unpack_f64
.dest 8 d1 gdouble
.source 8 s1 gdouble

copyq d1, s1


.function vertex_orc_unpack_f64_swap
.dest 8 d1 gdouble
.source 8 s1 gdouble

swapq d1, s1


.function vertex_orc_pack_u8
.dest 1 d1 guint8
.source 4 s1 gint32
.temp 2 t1

convsuslw t1, s1
convuuswb d1, t1


.function vertex_orc_pack_u8norm
.dest 1 d1 guint8
.source 4 s1 gint32
.const 4 c1 0x7fffffff
.temp 4 t1
.temp 2 t2

andl t1, s1, c1
shrul t1, t1, 15
convlw t2, t1
convhwb d1, t2


.function vertex_orc_pack_s8
.dest 1 d1 guint8
.source 4 s1 gint32
.temp 2 t1

convssslw t1, s1
convssswb d1, t1


.function vertex_orc_pack_s8norm
.dest 1 d1 guint8
.source 4 s1 gint32
.temp 2 t2

convhlw t2, s1
convhwb d1, t2


.function vertex_orc_pack_u16
.dest 2 d1 guint8
.source 4 s1 gint32

convsuslw d1, s1


.function vertex_orc_pack_u16norm
.dest 2 d1 guint8
.source 4 s1 gint32
.const 4 c1 0x7fffffff
.temp 4 t1

andl t1, s1, c1
shrul t1, t1, 15
convlw d1, t1


.function vertex_orc_pack_s16
.dest 2 d1 guint8
.source 4 s1 gint32

convssslw d1, s1


.function vertex_orc_pack_s16norm
.dest 2 d1 guint8
.source 4 s1 gint32

convhlw d1, s1


.function vertex_orc_pack_u16_swap
.dest 2 d1 guint8
.source 4 s1 gint32
.temp 2 t1

convsuslw t1, s1
swapw d1, t1


.function vertex_orc_pack_u16norm_swap
.dest 2 d1 guint8
.source 4 s1 gint32
.const 4 c1 0x7fffffff
.temp 4 t1
.temp 2 t2

andl t1, s1, c1
shrul t1, t1, 15
convlw t2, t1
swapw d1, t2


.function vertex_orc_pack_s16_swap
.dest 2 d1 guint8
.source 4 s1 gint32
.temp 2 t2

convssslw t2, s1
swapw d1, t2


.function vertex_orc_pack_s16norm_swap
.dest 2 d1 guint8
.source 4 s1 gint32
.temp 2 t2

convhlw t2, s1
swapw d1, t2


.function vertex_orc_pack_u32
.dest 4 d1 guint8
.source 4 s1 gint32
.const 4 c1 0x7fffffff

andl d1, s1, c1

.function vertex_orc_pack_u32norm
.dest 4 d1 guint8
.source 4 s1 gint32
.temp 4 t1

maxsl t1, s1, 0
shll d1, t1, 1


.function vertex_orc_pack_s32
.dest 4 d1 guint8
.source 4 s1 gint32

copyl d1, s1


.function vertex_orc_pack_u32_swap
.dest 4 d1 guint8
.source 4 s1 gint32
.const 4 c1 0x7fffffff
.temp 4 t1

andl t1, s1, c1
swapl d1, t1

.function vertex_orc_pack_u32norm_swap
.dest 4 d1 guint8
.source 4 s1 gint32
.const 4 c1 0x7fffffff
.temp 4 t1

maxsl t1, s1, 0
shll t1, t1, 1
swapl d1, t1


.function vertex_orc_pack_s32_swap
.dest 4 d1 guint8
.source 4 s1 gint32

swapl d1, s1


.function vertex_orc_pack_f32
.dest 4 d1 gfloat
.source 8 s1 gdouble

convdf d1, s1


.function vertex_orc_pack_f32_swap
.dest 4 d1 gfloat
.source 8 s1 gdouble
.temp 4 t1

convdf t1, s1
swapl d1, t1


.function vertex_orc_pack_f64
.dest 8 d1 gdouble
.source 8 s1 gdouble

copyq d1, s1


.function vertex_orc_pack_f64_swap
.dest 8 d1 gdouble
.source 8 s1 gdouble

swapq d1, s1


.function vertex_orc_s32_to_double
.dest 8 d1 gdouble
.source 4 s1 gint32

convld d1, s1


.function vertex_orc_double_to_s32
.dest 4 d1 gint32
.source 8 s1 gdouble

convdl d1, s1


.function vertex_orc_s32norm_to_double
.dest 8 d1 gdouble
.source 4 s1 gint32
.temp 8 t1

convld t1, s1
divd d1, t1, 2147483648.0L


.function vertex_orc_double_to_s32norm
.dest 4 d1 gint32
.source 8 s1 gdouble
.temp 8 t1

muld t1, s1, 2147483648.0L
convdl d1, t1


.function vertex_orc_swap_16
.dest 2 d1 guint16
.source 2 s1 guint16

swapw d1, s1


.function vertex_orc_swap_32
.dest 4 d1 guint32
.source 4 s1 guint32

swapl d1, s1


.function vertex_orc_swap_64
.dest 8 d1 guint64
.source 8 s1 guint64

swapq d1, s1
