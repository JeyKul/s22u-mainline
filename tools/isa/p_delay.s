// s_delay_alu and s_clause
	v_mov_b32 v1, 0x22222222
	s_delay_alu instid0(VALU_DEP_1)
	v_lshlrev_b32 v0, 2, v0
	s_delay_alu instid0(VALU_DEP_1)
	v_add_nc_u32 v2, 0x100, v0
	s_clause 0x1
	global_store_b32 v0, v1, s[0:1]
	global_store_b32 v2, v1, s[0:1]
	s_waitcnt_vscnt null, 0x0
	s_endpgm
