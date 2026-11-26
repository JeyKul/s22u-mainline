// s_load_b256 from a low VA (0x200008), nothing else
	s_mov_b32 s2, 0x200008
	s_mov_b32 s3, 0
	s_load_b256 s[8:15], s[2:3], null
	s_waitcnt lgkmcnt(0)
	s_endpgm
