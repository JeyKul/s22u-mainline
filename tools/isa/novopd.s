// as vopd.s with two plain v_mov_b32
	s_mov_b32 s4, 0x12345678
	v_mov_b32 v2, s4
	v_mov_b32 v3, s4
	v_lshlrev_b32 v0, 3, v0
	global_store_b64 v0, v[2:3], s[0:1]
	s_endpgm
