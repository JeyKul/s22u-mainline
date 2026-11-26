// gfx11 16-bit write to a high half (v_add_f16 v1.h), then a store
	v_mov_b32 v1, 0
	v_add_f16_e64 v1.h, 1.0, 0 op_sel:[0,0,1]
	v_lshlrev_b32 v0, 2, v0
	global_store_b32 v0, v1, s[0:1]
	s_endpgm
