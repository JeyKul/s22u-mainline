// icache test: store 0x22222222 to data[0] (tools/icache.c)
	v_mov_b32 v1, 0x22222222
	v_mov_b32 v0, 0
	global_store_b32 v0, v1, s[0:1]
	s_endpgm
