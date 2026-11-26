// gfx11 16-bit VALU on VGPR low halves (v_add_nc_u16 v.l), then a store
	s_mov_b32 s5, 0x10
	v_add_nc_u16 v2.l, s5, v0.l
	v_and_b32 v2, 0xffff, v2
	v_lshlrev_b32 v0, 2, v0
	global_store_b32 v0, v2, s[0:1]
	s_endpgm
