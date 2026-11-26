// as fill_nowait.s with glc on the store (the blob's stores carry glc)
	v_mov_b32 v1, 0x12345678
	v_lshlrev_b32 v0, 2, v0
	global_store_b32 v0, v1, s[0:1] glc
	s_endpgm
