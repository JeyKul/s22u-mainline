// gfx11: every lane stores 0xdeadbeef at buf[tid]
	v_mov_b32 v1, 0xdeadbeef
	v_lshlrev_b32 v0, 2, v0
	global_store_b32 v0, v1, s[0:1]
	s_waitcnt vmcnt(0)
	s_endpgm
