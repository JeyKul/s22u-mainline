// image_load 2D (two in an s_clause) from an 8x8 R32_UINT linear T# over the data buffer (blob T# layout, gfx11),
// result -> data + 0x1000 + tid*4
	s_lshr_b64 s[8:9], s[0:1], 8
	s_or_b32 s9, s9, 0xc1400000
	s_mov_b32 s10, 0x0001c001
	s_mov_b32 s11, 0x90000204
	s_mov_b32 s12, 0
	s_mov_b32 s13, 0x00400000
	s_mov_b32 s14, 0
	s_mov_b32 s15, 0
	v_and_b32 v2, 7, v0
	v_lshrrev_b32 v3, 3, v0
	s_clause 0x1
	image_load v[4:7], v[2:3], s[8:15] dmask:0xf dim:SQ_RSRC_IMG_2D
	image_load v[8:11], v[2:3], s[8:15] dmask:0xf dim:SQ_RSRC_IMG_2D
	v_lshlrev_b32 v1, 2, v0
	v_add_nc_u32 v1, 0x1000, v1
	s_waitcnt vmcnt(0)
	global_store_b32 v1, v4, s[0:1]
	s_endpgm
