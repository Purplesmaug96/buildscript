# Xenos GPU: raw PM4 driver research & design

Everything here was extracted from the xenia sources on disk — treat
file:line references as ground truth, not memory.

Source trees:

- Modern xenia, Vulkan Xenos implementation. **AUTHORITATIVE.**
  - `src/xenia/gpu/register_table.inc` (3705 lines, 3446 registers via
    `XE_GPU_REGISTER(index, kDword|kFloat, name)`)
  - `src/xenia/gpu/registers.h` (index→struct accessors)
  - `src/xenia/gpu/xenos.h` (PM4 opcodes ~1622, fetch structs 1162/1233,
    memexport stream 1547)
  - `src/xenia/gpu/ucode.h` (2096 lines — complete microcode documentation)
  - `src/xenia/gpu/command_processor.cc` (`InitializeRingBuffer`,
    `EnableReadPointerWriteBack`, `WriteRegister`)
  - `src/xenia/gpu/graphics_system.cc` (CS MMIO special cases)
  - `src/xenia/gpu/pm4_command_processor_implement.h` (`ExecutePacket`,
    Type0/1/3 handlers, kXeSwap at :258)
  - `src/xenia/gpu/packet_disassembler.cc` (packet/opcode decode)
  - `src/xenia/gpu/shared_memory.h`
  - `src/xenia/gpu/vulkan/vulkan_command_processor.cc` (`IssueDraw` :2363,
    `WriteRegister` :1370)
  - `src/xenia/gpu/vulkan/vulkan_pipeline_cache.cc` (`LoadShader` :336)

## Current state (softpipe path)

Mesa (softpipe) renders into a system-memory colour buffer; samples blit it to
the front buffer via D3D9-style calls in `samples/common/screen.c`, which
drives the GPU ring directly. This works but is
software-rendered and slow. Goal: replace the rasterizer with the real Xenos.

- The GL frontend glue (`targets/xbox360/xbox360_api.c`) is renderer-agnostic:
  `xbox360_screen_create()` returns the softpipe screen
  (`targets/xbox360/softpipe/`, the fallback) or the hardware screen
  (`targets/xbox360/xenos/` + `src/gallium/drivers/xenos/`), selected by
  `MESA_OP_XBOX360_XENOS`.
- Ring: `MmAllocatePhysicalMemoryEx` (xenia heap ≥ 0x80000000),
  `VdInitializeRingBuffer(ptr, size_log2=13)` → 64 KiB = 16384 dwords.
- Present: `VdSwap()` writes a fetch-example command block into the ring then
  punches `CP_RB_WPTR` MMIO `0x7FC80714` (register index 0x1C5).
  `graphics_system.cc`: `case 0x01C5 → command_processor_->UpdateWritePointer`.
- Read pointer writeback: `CP_RB_RPTR_ADDR` reg 0x70C, `CP_RB_CNTL` reg 0x704
  (`EnableReadPointerWriteBack(ptr, block_size_log2)`).

## Toolchain landmines — RESOLVED (2026-08-20)

- **`-g` crashes the ppc32 clang backend** (SIGTRAP in codegen, all `-O*`):
  `streaming-load-memcpy.c`, `anon_file.c` and friends only compile
  **without DWARF debug info**. The default build type for the console is
  therefore `Release` (`-O3`, no `-g`). Do NOT configure with
  `RelWithDebInfo`/`Debug` for the xbox360 cross build — it is not a
  workable debugging setup.
- **`-fno-vectorize -fno-slp-vectorize`** are force-added for the xbox360
  target (mesa/CMakeLists.txt, `if(with_gallium_xbox360)`): the PPC32
  backend has no SIMD and LLVM 14+ auto-vectorizes at `-O2`; with `-g` this
  crashed (see above) and it can only hurt codegen anyway.
- The xenos driver had to track Mesa 26.3.0 API drift: no
  `pipe_index_buffer` / `set_index_buffer` (index state rides in
  `pipe_draw_info.index`), no `enum pipe_shader_type` (`mesa_shader_stage`),
  transfers are `buffer_map`/`texture_map` + `buffer_unmap`/`texture_unmap`,
  `create_depth_stencil_alpha_state` (was `create_dsa_state`), `clear` takes
  `color_clear_mask`/`stencil_clear_mask`, `set_stencil_ref` by value,
  `set_vertex_buffers(count, buffers)` without start/ownership.
- xecore: `MmFreePhysicalMemory(REGION, base)` takes the region argument
  (REGION_AUTO for REGION_AUTO allocations).

## Shader IR reality — RESOLVED (2026-08-20)

**The state tracker always hands the pipe driver NIR in this Mesa** —
`st_create_common_variant` hardcodes `state.type = PIPE_SHADER_IR_NIR`
(st_program.c ~805), and fixed-function programs are built as NIR
(ffvertex_prog.c `build_tnl_program`, `_mesa_get_fixed_func_vertex_program`).
There is no `PIPE_CAP_PREFER_IR` / TGSI delivery path anymore.

Implications for the xenos driver:

- `screen->shader_caps[].supported_irs` must advertise
  `1 << PIPE_SHADER_IR_NIR` (nothing else is needed).
- The compiler is a **NIR→Xenos** translator, not TGSI→Xenos.  Plan: run
  `nir_lower_*` passes (already largely decided by
  `xenos_compiler_options`) to reduce the fixed-function subset, then walk the
  NIR and emit CF + vfetch + ALU microcode (`gpu/xenos_ucode.h`).  The TGSI
  decodes below are now only of historical/debug value (softpipe's internal
  NIR→TGSI conversion is what the ureg traces captured).
- Bonus fact: the TGSI src swizzle byte `(SwX<<6|SwY<<4|SwZ<<2|SwW)` is
  **byte-identical to the Xenos ALU src swizzle encoding** — NIR component
  swizzles translate directly.

## TGSI token encoding — RESOLVED (2026-08-20)

PPC32 clang bitfields are **MSB-first, no byte swap** (verified against
ureg dumps in run_tri11.log):

- Header: `HeaderSize=bits31:24, BodySize=bits23:0` (0x02000020 → 2, 32).
- Declaration: `Type(31:28), NrTokens(27:20), File(19:16), UsageMask(15:12),
  Dimension(11), Semantic(10), Interpolate(9), Invariant(8)`.
  FS decl order in ureg output is type, range, **interp, semantic**.
  Range: `First(31:16), Last(15:0)`.  Interp: `Interpolate(31:28),
  Location(27:26)`.  Semantic: `Name(31:24), Index(23:8)`.
- Instruction: `Type(31:28), NrTokens(27:20), Opcode(19:12), Saturate(11),
  NumDstRegs(10:9), NumSrcRegs(8:5), Label(4), Texture(3), Memory(2)`.
- Src register: `File(31:28), Indirect(27), Dimension(26), Index(25:10),
  Swizzle(9:2), Absolute(1), Negate(0)`.  Dst register:
  `File(31:28), WriteMask(27:24), Indirect(23), Dimension(22), Index(21:6)`.
- Warning: the ESRCD/ESRCW prints in tgsi_ureg.c were pre-conversion host
  structs (0x11B20000 contradicts bitfield reads) — don't decode those.

The magenta run's PS programs decode as `TEX TEMP[0], INPUT[0], SAMP[0];
MOV OUTPUT[0], TEMP[0]` — the texture-copy blit/clear shaders from
softpipe's NIR→TGSI conversion (tgsi_ureg.c via nir_to_tgsi).

## Addressing

- MMIO base `0x7FC80000`; byte MMIO = `0x7FC80000 + reg_index * 4`.
- PM4 register indices are dword indices of the register table (0x2000+
  range). `registers.h` accessors use those indices directly on the register
  file.
- Ring/TLB addresses in PM4 payloads are **guest physical** addresses.

## PM4 packets

Guest memory is **big-endian**; xenia reads with `load_and_swap<uint32_t>`.
Header word: `packet_type = word >> 30`.

- **Type0** (`XE_GPU_PACKET_TYPE0`): `count = ((word>>16)&0x3FFF)+1`,
  `base = word & 0x7FFF`, `write_one_reg = bit15`. Writes `count` dwords to
  consecutive registers from `base<<2`... (registers `base..base+count-1`),
  or all `count` dwords to register `base` when write_one_reg is set
  (and `count` is ignored — actually writes `count` copies).
- **Type1**: `reg1 = word&0x7FF`, `reg2 = (word>>11)&0x7FF`, followed by 2
  payload dwords.
- **Type2**: filler → `handle_bad_packet` (must be skipped by us).
- **Type3**: `opcode = (word>>8)&0x7F`, `count = ((word>>16)&0x3FFF)+1`,
  bit0 = predicated (execute only if predicate true — set 0).

## PM4 opcodes (xenos.h ~1622)

| op | name | notes |
|----|------|-------|
| 0x10 | NOP | with optional payload |
| 0x22 | DRAW_INDX | payload: viz_query dword, VGT_DRAW_INITIATOR dword, then kDMA: VGT_DMA_BASE + VGT_DMA_SIZE |
| 0x23 | DRAW_INDX_2 | same without viz query |
| 0x26 | WAIT_FOR_IDLE | |
| 0x3c | WAIT_REG_MEM | poll reg vs mem |
| 0x3d | MEM_WRITE | write dword(s) to memory |
| 0x3e | REG_TO_MEM | copy register to memory |
| 0x3f | INDIRECT_BUFFER | nested command buffer at addr/word count |
| 0x37 | INDIRECT_BUFFER_PFD | |
| 0x44 | COND_EXEC | |
| 0x46 | EVENT_WRITE | |
| 0x48 | ME_INIT | micro-engine init |
| 0x4f | MEM_WRITE_CNTR | |
| 0x52 | WAIT_REG_EQ | |
| 0x53 | WAIT_REG_GTE | |
| 0x58 | EVENT_WRITE_SHD | |
| 0x59 | EVENT_WRITE_CFL | |
| 0x5a | EVENT_WRITE_EXT | |
| 0x5b | EVENT_WRITE_ZPD | |
| 0x5c | WAIT_UNTIL_READ | |
| 0x5d | WAIT_IB_PFD_COMPLETE | |
| 0x2f | LOAD_ALU_CONSTANT | ALU constants from memory |
| 0x21 | REG_RMW | read-modify-write register |
| 0x45 | COND_WRITE | |
| 0x64 | **XE_SWAP** | swap/present (pm4_command_processor_implement.h:258) |

Draw flow: Type3 DRAW_INDX → `ExecutePacketType3Draw` →
`vulkan_command_processor.cc IssueDraw` (:2363); if
`RB_MODECONTROL.edram_mode == kCopy` → `IssueCopy` (color-buffer copy path —
this is what a "clear/readback without shader" uses).

## Key registers (register_table.inc dword indices)

State:
- 0x2000 RB_SURFACE_INFO | 0x2001 RB_COLOR_INFO | 0x2002 RB_DEPTH_INFO
- 0x2003..0x2005 RB_COLOR1_3_INFO
- 0x200E/0x200F PA_SC_SCREEN_SCISSOR_TL/BR
- 0x2080 PA_SC_WINDOW_OFFSET | 0x2081/0x2082 WINDOW_SCISSOR_TL/BR
- 0x2083 PA_SC_CLIPRECT_RULE | 0x2084..0x208B CLIPRECT_0..3 TL/BR
- 0x2100 VGT_MAX_VTX_INDX | 0x2101 VGT_MIN_VTX_INDX | 0x2102 VGT_INDX_OFFSET
- 0x2104 RB_COLOR_MASK | 0x2105..0x2108 RB_BLEND_RED/GREEN/BLUE/ALPHA (kFloat)
- 0x2180 SQ_PROGRAM_CNTL | 0x2182 SQ_INTERPOLATOR_CNTL
- 0x2205 PA_SU_SC_MODE_CNTL
Shaders / draw:
- 0x21F6 SQ_PS_PROGRAM | 0x21F7 SQ_VS_PROGRAM (see "Shader loading" below)
- 0x21FC VGT_DRAW_INITIATOR (written by CP from the draw packet; also hand-writable)
- 0x4000+ SHADER_CONSTANT_000_X.. (4 dwords per constant, kFloat)
- 0x4800+ SHADER_CONSTANT_FETCH_00_0.._5 (6 dwords per fetch constant)
Read-only specials (graphics_system.cc):
- 0x0F00 RB_EDRAM_TIMING → 0x08100748
- 0x0F01 RB_BC_CONTROL → 0x0000200E
- 0x1951 interrupt status → 1 (vblank)
- 0x1961 AVIVO_D1MODE_VIEWPORT_SIZE → 0x050002D0
Register write routing: `CommandProcessor::WriteRegister(index, value)` stores
into register_file values with specials: SCRATCH_REG0..7, COHER_STATUS_HOST,
DC_LUT_* range. `WriteRegisterRangeFromRing` bases: +0x2000 (RB/PA/SC),
+0x4000 (constants), +0x4800 (fetch), +0x4900, +0x4908 (texture fetch?).
Only `0x01C5 CP_RB_WPTR` is special-cased by GraphicsSystem.

## Fetch constants (SHADER_CONSTANT_FETCH_*, 6 dwords per entry, index i at 0x4800 + i*6)

`xe_gpu_vertex_fetch_t` (2 dwords):
```c
union { uint32_t type:2; uint32_t address:30;   /* dword0 */
        uint32_t endian:2; uint32_t size:24;    /* dword1, size in bytes */ };
```
- `type` = FetchConstantType (kVertexFetch = 1?; dword0 0 = raw/generic)
- address is guest physical >> 2 relative? — check xenos.h loader
  (`xe_gpu_fetch_const` vs direct buffer). For vertex buffers: address =
  (guest_phys >> 2); small EOF hack on Xbox: bits are physical>>2, and TRUE
  GPU addresses are physical>>2 in almost all xenos fields.

`xe_gpu_texture_fetch_t` (6 dwords): texture layout, tile 32x32x4, subresource
alignment 12 bits (kTextureSubresourceAlignmentBytesLog2).

vfetch constants 0..95 (D3D9 slot = const*3 + sub in ucode).

## Microcode (ucode.h)

Instruction stream: 64-bit words. Types discriminated by opcode bits.

- `ControlFlowOpcode` (:84): kNop=0, kExec=1, kExecEnd=2, kCondExec=3,
  kCondExecEnd=4, kCondExecPred=5, …
- `ControlFlowExecInstruction` (:218):
  `{address_:12, count_:3, is_yield_:1, sequence_:12, vc_hi_:4;}`
  `{vc_lo_:2, bool_address_:8, condition_:1, address_mode_:1, opcode_:4;}`
  count_ = number of *pairs* of ISA (fetch/ALU) instructions following the CF.
- `VertexFetchInstruction` (:690): fetch_constant_index = const_index*3 +
  const_index_sel; src/dest registers, swizzles, format, exp_adjust,
  signed/unsigned, normalized, is_mini_fetch.
- `AluInstruction` (:1849): vector_opc, vector_write_mask, vector_dest,
  scalar_opc, scalar_dest, is_export (export_data), is_predicated,
  pred_condition, src_reg(i) packed {0x3F reg, 0x40 relative, 0x80 absolute}.
- Exports: interpolator export (storage_index 0-15 → PS inputs), position
  (gl_Position), misc, memexport address / data / eM (shader.h:51-64).
- VS position export on Xenos: memexport-based. xenia:
  `Shader::GetMemExportStreamConstant` (shader.h:613),
  `xe_gpu_memexport_stream_t` (xenos.h:1547, 4 dwords: base_address:30,
  const_0x1:2, dword_1=0x4b000000, endianness:3, format:6, num_format:3,
  red_blue_swap:1, const_0x4b0:12, index_count:23, const_0x96:9).
  Analysis: `cf_memexport_info_` (shader.h:773),
  spirv_shader_translator_memexport.cc `ExportToMemory`.
  Position memexport format: 0x4B (6 float dwords: xyzw + extra).
- 3Dc/other fetch formats, mini-fetch, etc: see ucode.h comments.

## Shader loading — RESOLVED (2026-08-17)

Modern xenia does NOT read SQ_VS_PROGRAM / SQ_PS_PROGRAM at all (register-table
entries only). Shaders are uploaded via PM4 packets and cached in
`active_vertex_shader_`/`active_pixel_shader_`:

- `PM4_IM_LOAD` (0x2f? — see xenos.h; in
  pm4_command_processor_implement.h, handler at :1312): payload =
  `addr_type` (shader type in bits 0-1: 0=VS, 1=PS; rest = guest GPU address),
  `start_size` (start = size>>16, `size_dwords = size & 0xFFFF`). Calls
  `LoadShader(type, addr, TranslatePhysical(addr), size_dwords)`.
- `PM4_IM_LOAD_IMMEDIATE` (~:1342): dword0 = shader type, dword1 =
  start_size, followed by `size_dwords` ucode dwords **embedded in the packet**
  — no guest memory mapping needed.
- `LoadShader` → `pipeline_cache_->LoadShader` (vulkan_pipeline_cache.cc:336)
  → shader object; `IssueDraw` uses `active_vertex_shader()` /
  `active_pixel_shader()` directly (vulkan_command_processor.cc:2402).

Draw flow (pm4_command_processor_implement.h:1053): `PM4_DRAW_INDX_2`
(0x23) / `PM4_DRAW_INDX` (0x22) → payload dword 0 = **VGT_DRAW_INITIATOR**
(read from the packet, not from the register file):
`source_select`: kDMA (0x1, indexed, +VGT_DMA_BASE + VGT_DMA_SIZE dwords) or
**kAutoIndex (0x3, non-indexed autodraw)**; also carries prim_type + num_indices.
→ `IssueDraw(regs)` reads all render state from the register file
(RB_MODECONTROL.edram_mode must be kColorDepth else kCopy=IssueCopy();
RB_SURFACE/COLOR/DEPTH_INFO, PA_SC_SCISSOR/CLIPRECT, RB_COLOR_MASK,
RB_BLEND_*, VGT_MAX/MIN_VTX_INDX, SQ_PROGRAM_CNTL, SQ_INTERPOLATOR_CNTL,
PA_SU_SC_MODE_CNTL, fetch constants 0x4800, VGT_*). Pipeline build also needs
the fetch constants for vfetch translating and state for the render pass:
the registers must be written (Type0) BEFORE the draw packet.

## Microcode register encoding — RESOLVED (2026-08-17)

- GPR space is flat 0-63 in both vfetch dest (6-bit field) and ALU src fields;
  the packed src byte is `{0x3F reg, 0x40 aL-relative, 0x80 abs-value}` — the
  0x80 bit is the *mathematical* absolute flag (sign mask, see
  shader_interpreter.cc:309). "R200" input naming is legacy/disassembler
  convention only; there is no bank encoding in the modern decoder.
- Constants: `srcX_sel=0` → the 8-bit field is the constant index 0-255
  directly.
- VS position export = ALU instr with `export_data=1`, `vector_dest` =
  `ExportRegister::kVSPosition` (62). PS color export = export ALU,
  `vector_dest` = `kPSColor0`..3 (0-3). Exports use vector_dest for both
  vector and scalar ops; masks: c0/c1 write via scalar_dest_rel (
  ucode.h:2018-2057, shader_translator.cc:1298-1335).
- CF exec (ucode.h:218): word0 = `address_:12, count_:3, is_yield_:1,
  sequence_:12, vc_hi_:4`; word1 = `vc_lo_:2, pad7, is_predicate_clean_:1,
  pad1, address_mode_:1, opcode_:4` (kExec=1, kExecEnd=2). sequence = 2 bits
  per instruction ([0]=ALU/fetch, [1]=serialize); count_ = instruction pairs
  (0-based). AluInstruction = 3 dwords (ucode.h:1849). IM_LOAD dword count =
  CF(2) + 3*N.
- Old d3d11-era note: the "not simply forwarding" position export comment
  (shader_translator.cc:2087) applies to the *interpreter*; the dxbc
  translator path uses the ALU export flags above.

## ureg crash-fix record (do not reopen)

`tgsi_ureg.c::ureg_emit_src` now emits the dimension token as a **direct** raw
word (`vo[di].value = 0`, no Indirect bit — DimensionIndex dropped), after
Indirect=1 dim tokens desynced tgsi_parse (extra DimIndirect consumed →
garbage INPUT index → VS crash pc=0x8235998C). `ureg_emit_dst` uses the same
base/di scheme. Only bitfield reads are trusted (see the resolved encoding
above). Debug prints ESRCD/ESRCW remain in the function (log ureg10.log,
cap 4000).

## PM4 semantics — verified from source and on emulator (do not re-learn)

- **Type3 packet headers MUST carry the type bits** — `0xC0000000 | opcode<<8 |
  count_field<<16`. Without them the CP decodes the header as a **Type0
  register write** (silent corruption; the wptr punch can then trap). Type
  bits: Type0=0b00, Type1=0b01 (0x40000000), Type2=0b10 (0x80000000),
  Type3=0b11.
- **Type3 count field = payload dwords − 1** (xenia `count = field + 1`;
  handlers consume `count` payload dwords: NOP `AdvanceRead(count*4)`,
  XE_SWAP reads 4 of count=5 then `AdvanceRead((count-4)*4)`). Every Type3
  packet therefore carries ≥ 1 payload dword; a bare header is NOT legal.
- **Type0 count field = registers written − 1** (base_index = reg&0x7FFF,
  write_one = bit15).
- **Submissions MUST be 64-dword-aligned blocks** — the CP breaks
  (silent host SIGTRAP) when the write pointer gets non-64-aligned values
  (wptr 66/69 died; 64/128 fine). Pad with Type2 fillers (0x80000000) inside
  the block; `xe_gpu_ring_submit` auto-pads now.
- **No PM4_WAIT_FOR_IDLE handler in the modern CP** — opcode 0x26 hits
  `HitUnimplementedOpcode` → `assert_always()` → host SIGTRAP. Use a NOP pad
  (field 0 + 1 payload dword) instead.
- **PM4_MEM_WRITE / event-writeback addresses are guest PHYSICAL**; a guest
  virtual address of a static causes an OOB write → XENIA_CRASH
  (fault=0x100000000, pc in the caller). Allocate targets via
  `MmAllocatePhysicalMemoryEx(REGION_AUTO, size, 4, 0, 0xFFFFFFFFu, 0x1000)`
  and pass `MmGetPhysicalAddress(ptr)`. NOTE: a physical-heap target written
  by the CP still did NOT show up when polled at its virtual alias
  (marker stayed 0) — xenia's VA↔physical aliasing for that heap is NOT
  verified; the ring-only smoke test skips MEM_WRITE for this reason.
- **Ring content on xenia is written by the emulator, not the guest**: the
  working present path calls `VdSwap` (xboxkrnl_video.cc:466) which builds a
  64-dword block INTO the ring at `buffer_ptr` — `MakePacketType0(
  SHADER_CONSTANT_FETCH_00_0, 6)` + fetch + `MakePacketType3(XE_SWAP, 4)` +
  `kSwapSignature("SWAP")` + frontbuffer physical + w + h + Type2 filler to
  64 dwords, and only THEN does the guest punch CP_RB_WPTR.
- XE_SWAP payload = kSwapSignature, frontbuffer_physical, width, height.
- kSwapSignature = make_fourcc("SWAP") = 0x50415753.
- **The guest ring dwords must be stored big-endian** — xenia reads with
  `load_and_swap`; storing bswap'd logical words round-trips correctly
  (verified: the CP parsed real packets and processing stayed aligned).

## PM4 backend status (current)

- `drivers/xenos/gpu/` (moved from `targets/xbox360/gpu/`): xenos_regs.h
  (register indices + state unions), xenos_pm4.h (opcodes + BE packet
  emitters with type bits), xenos_fetch.h (vfetch builder), xenos_gpu.h/.c —
  ring init/submit at arbitrary wptr with auto 64-dword padding, cmdbuf +
  submit, baseline block, MEM_WRITE/EVENT/DRAW builders,
  `xe_gpu_dev_verify` smoke test (hooked into glsample.c after screen init).
- **Verified on emulator (final runs)**: two 64-dword submissions parse and
  execute end-to-end ("PM4 dev verify OK"); the game continues rendering
  afterwards (screen_present wptr 64→3200+, zero XENIA_CRASH). The 43 s
  "hang" seen mid-debugging was just the debug build's slow module load, not
  a regression — with a 120 s window the launch+render path is fully green.
- Remaining caveats: MEM_WRITE physical-alias writeback unverified (see
  above); no real draw (state + fetch + ucode + DRAW_INDX_2) has been
  submitted yet — that is the next phase.

## Roadmap

1. **PM4 layer** (`drivers/xenos/gpu/`): register indices, packet builders
   (type0/type3), ring submit (screen.c pattern: copy + `CP_RB_WPTR` write),
   fetch-constant builders. *DONE-in-progress (xenos_gpu).*
2. **Vanilla CP traffic test**: submit NOP/type0/MEM_WRITE/EVENT_WRITE /
   WAIT_FOR_IDLE blocks on the ring via the new API; verify under xenia
   (no crash, registers land, MEM_WRITE visible from CPU side).
   *DONE — "PM4 dev verify OK" (see backend status above).*
3. **Microcode**: minimal VS (CF + vfetch_full const0→r0 + ALU export
   pos from r0) + PS (CF + ALU export-color from a constant, no interpolator)
   hand-encoded against the layouts in gpu/xenos_ucode.h; validate by
   loading in xenia's pipeline cache (`PM4_IM_LOAD* → LoadShader` logs).
   *PENDING — hand-encoded variants partly built; superseded by the NIR
   compiler (step 5) which emits the same encodings.*
4. **First real draw**: Type0 register state (RB_MODECONTROL kColorDepth,
   RB_SURFACE/COLOR/DEPTH_INFO, PA_SC_SCISSOR/CLIPRECT, RB_COLOR_MASK,
   RB_BLEND, VGT_MAX/MIN, SQ_PROGRAM_CNTL, PA_SU_SC_MODE_CNTL) + fetch
   constants 0x4800 + `PM4_IM_LOAD_IMMEDIATE` (ucode embedded, no guest
   memory map) + `PM4_DRAW_INDX_2` (VGT_DRAW_INITIATOR, source_select=
   kAutoIndex, num_indices) → triangle on screen. Clear via kCopy
   (RB_MODECONTROL) or a 1-triangle PS constant write.
5. **xenos gallium driver** (`src/gallium/drivers/xenos/`) — *DONE
   (skeleton, builds both ways)*: screen/context/state capture, resources in
   guest-physical memory via the winsys, NIR→Xenos compile stub,
   `MESA_OP_XBOX360_XENOS` selects it over softpipe (softpipe path parked in
   `targets/xbox360/softpipe/`, to be deleted once the hw path works).
   Verified: `ninja xenos` + full `xbox360_merged` build with
   `MESA_OP_XBOX360_XENOS=ON` (xenos_*.o in the merged archive) and OFF
   (softpipe intact).
6. **NIR→Xenos microcode compiler** (the core of step 5): translate the
   fixed-function NIR subset (mov/mul/add/dp4/lit/tex...) into CF + vfetch +
   ALU microcode; hook it into `xenos_create_shader` so the samples' VS/FS
   compile for real.
7. **State conversion**: turn the captured gallium state into PM4 register
   writes (RB/PA/SC/SQ from the CSO states captured by the context), EDRAM
   framebuffers, vertex fetch constants, and `DRAW_INDX_2`; clear via the
   kCopy path.  This replaces softpipe for the samples end-to-end.
8. Textures (tfetch), indexed draws (VGT_DMA kDMA source), occlusion /
   VGT_DRAW_INITIATOR variants as needed.