// v_mul_lo_u32 (VOP3, quarter rate), then a store
	s_mov_b32 s5, 3
	v_mul_lo_u32 v1, v0, s5
	v_lshlrev_b32 v0, 2, v0
	global_store_b32 v0, v1, s[0:1]
	s_endpgm
