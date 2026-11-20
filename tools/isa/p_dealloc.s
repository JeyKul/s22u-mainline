// store + s_sendmsg dealloc_vgprs before s_endpgm (ACO gfx11 epilogue)
	v_mov_b32 v1, 0x11111111
	v_lshlrev_b32 v0, 2, v0
	global_store_b32 v0, v1, s[0:1]
	s_sendmsg sendmsg(MSG_DEALLOC_VGPRS)
	s_endpgm
