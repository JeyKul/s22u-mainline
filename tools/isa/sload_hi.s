// s_load_b256 from RADV's descriptor VA 0xffff8000_00040008 (high range), nothing else
	s_mov_b32 s2, 0x40008
	s_movk_i32 s3, 0x8000
	s_load_b256 s[8:15], s[2:3], null
	s_waitcnt lgkmcnt(0)
	s_endpgm
