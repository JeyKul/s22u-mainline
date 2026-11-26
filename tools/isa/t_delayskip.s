// s_delay_alu with instskip (as RADV's copy shader), then a store
	v_and_b32 v1, 7, v0
	v_lshrrev_b32 v2, 10, v0
	s_delay_alu instid0(VALU_DEP_2) | instskip(SKIP_1) | instid1(VALU_DEP_2)
	v_add_nc_u32 v1, 1, v1
	v_add_nc_u32 v2, 1, v2
	v_lshlrev_b32 v0, 2, v0
	global_store_b32 v0, v1, s[0:1]
	s_endpgm
