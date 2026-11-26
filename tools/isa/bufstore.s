// buffer_store_format_xyzw idxen through an R32_UINT V# (blob layout) over the data buffer
	s_mov_b32 s16, s0
	s_and_b32 s17, s1, 0xffff
	s_or_b32 s17, s17, 0x00040000
	s_mov_b32 s18, 64
	s_mov_b32 s19, 0x10014204
	v_mov_b32 v4, 0x12345678
	v_mov_b32 v5, 0
	v_mov_b32 v6, 0
	v_mov_b32 v7, 0
	buffer_store_format_xyzw v[4:7], v0, s[16:19], 0 idxen
	s_endpgm
