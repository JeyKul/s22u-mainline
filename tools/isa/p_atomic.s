// MUBUF atomic add on buf[0] (gfx10.3 V#) + per-lane store of the returned value at buf[16+tid]
	s_mov_b32 s4, s0
	s_and_b32 s5, s1, 0xffff
	s_mov_b32 s6, 0x10000
	s_mov_b32 s7, 0x31016fac
	v_mov_b32 v1, 1
	v_mov_b32 v3, 0
	buffer_atomic_add_u32 v1, v3, s[4:7], 0 offen glc
	s_waitcnt vmcnt(0)
	v_lshlrev_b32 v0, 2, v0
	v_add_nc_u32 v0, 64, v0
	global_store_b32 v0, v1, s[0:1]
	s_waitcnt_vscnt null, 0x0
	s_endpgm
