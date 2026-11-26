// gfx11 v_mad_u16 with op_sel (as radeonsi's readback shader), then a store
	s_mov_b32 s5, 0x10
	v_mad_u16 v3.l, v0.l, 2, s5 op_sel:[0,0,1,0]
	v_and_b32 v3, 0xffff, v3
	v_lshlrev_b32 v0, 2, v0
	global_store_b32 v0, v3, s[0:1]
	s_endpgm
