// gfx11 NGG GS: every lane atomically adds 1 at the patched VA, then exports a
// (degenerate) primitive and position. Rasterization is killed by the harness.
	s_mov_b32 exec_lo, -1
	s_mov_b32 s20, 0xcafe0001
	s_mov_b32 s21, 0xcafe0002
	v_mov_b32 v10, 1
	v_mov_b32 v11, 0
	global_atomic_add_u32 v11, v10, s[20:21]
	s_waitcnt_vscnt null, 0x0
	v_mov_b32 v12, 0
	exp prim v12, off, off, off done
	exp pos0 v12, v12, v12, v12 done
	s_endpgm
