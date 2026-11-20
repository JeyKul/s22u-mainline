// lane 0 writes SHADER_CYCLES (20-bit shader clock) and REALTIME before/after a SALU loop
	s_getreg_b32 s4, hwreg(HW_REG_SHADER_CYCLES, 0, 20)
	s_sendmsg_rtn_b64 s[6:7], sendmsg(MSG_RTN_GET_REALTIME)
	s_mov_b32 s8, 0x8000
L:	s_sub_u32 s8, s8, 1
	s_cmp_lg_u32 s8, 0
	s_cbranch_scc1 L
	s_getreg_b32 s10, hwreg(HW_REG_SHADER_CYCLES, 0, 20)
	s_sendmsg_rtn_b64 s[12:13], sendmsg(MSG_RTN_GET_REALTIME)
	s_waitcnt lgkmcnt(0)
	v_mov_b32 v0, 0
	v_mov_b32 v1, s4
	v_mov_b32 v2, s6
	v_mov_b32 v3, s10
	v_mov_b32 v4, s12
	global_store_b128 v0, v[1:4], s[0:1]
	s_waitcnt_vscnt null, 0x0
	s_endpgm
