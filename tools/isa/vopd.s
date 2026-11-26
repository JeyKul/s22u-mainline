// gfx11 VOPD dual-issue (v_dual_mov_b32 x2) then a store: does the Xclipse 920 have VOPD?
	s_mov_b32 s4, 0x12345678
	v_dual_mov_b32 v2, s4 :: v_dual_mov_b32 v3, s4
	v_lshlrev_b32 v0, 3, v0
	global_store_b64 v0, v[2:3], s[0:1]
	s_endpgm
