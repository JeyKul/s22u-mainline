// same, V# word3 in gfx11 format: DST_SEL xyzw | FORMAT(gfx11 32_FLOAT=0x3e?) left 0, OOB_SELECT 3
	s_mov_b32 s4, s0
	s_and_b32 s5, s1, 0xffff
	s_mov_b32 s6, 0x10000
	s_mov_b32 s7, 0x30000fac
	v_mov_b32 v1, 0x44444444
	v_lshlrev_b32 v0, 2, v0
	buffer_store_b32 v1, v0, s[4:7], 0 offen
	s_waitcnt_vscnt null, 0x0
	s_endpgm
