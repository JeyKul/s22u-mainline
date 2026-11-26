// v_lshl_add_u32 (VOP3) with an SGPR source, then a store
	s_mov_b32 s6, 2
	v_lshl_add_u32 v1, s6, 3, v0
	v_lshlrev_b32 v0, 2, v0
	global_store_b32 v0, v1, s[0:1]
	s_endpgm
