// RADV's vkCmdFillBuffer shader shape (gfx11), data at s[0:1], no wait before s_endpgm
	v_mov_b32 v1, 0x12345678
	v_lshlrev_b32 v0, 2, v0
	global_store_b32 v0, v1, s[0:1]
	s_endpgm
