/** \file
 * Entry points into the firmware image.
 *
 * These are the functions that we can call from our tasks
 * in the Canon 1.2.0 firmware for the EOS 6D Mark II.
 *
 * Re-based from the 6D2.111 stub set on 2026-09-01.  1.2.0 is the same code with
 * a few insertions, so ROM0 addresses shift by a per-region constant:
 *
 *     0xe0040000 - 0xe0080000    +0        (cstart/init_task verified directly)
 *     0xe0080000 - 0xe015f000    -0x4
 *     0xe015f000 - 0xe0289000    +0x3c
 *     0xe0289000 - 0xe03d0000    +0x40
 *     0xe03d0000 - end           +0xe8
 *     0xdf000000 - 0xdf020000    +0        (DryOS kernel is byte-identical)
 *
 * Derived from a whole-ROM BL/BLX call-target map and cross-checked against five
 * independent hard anchors:
 *   smemShowFix     eventproc name table          -> +0x3c
 *   LiveViewApp     dialog handler literal        -> +0x40
 *   prop_register_slave  "pRegisterSlave" xref    -> +0xe8
 *   gui_init_event  "-> handleGuiInit" xref       -> -0x4
 *   mpu_recv_cbr    live RAM 0x7c24 = mpu_recv+0xe8 (QEMU dump)
 *
 * NOT yet validated on hardware.  DATA_PTR/NSTUB RAM addresses were left as-is;
 * the ones observable in a QEMU RAM dump (current_task, task_max, mpu_* , sd_device,
 * display_output_mode) still look right.  isr_table_handler (0x65a08) looked wrong
 * in the dump and needs checking.  See FINDINGS_120.md.
 */
/*
 * Copyright (C) 2021 Magic Lantern Team
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the
 * Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor,
 * Boston, MA  02110-1301, USA.
 */

#include <stub.h>

/** Startup **/
NSTUB(MAIN_FIRMWARE_ADDR, firmware_entry)
THUMB_FN(0xe00400fd,  cstart)                 /* calls bzero32 and create_init_task(..., init_task, ...) */
THUMB_FN(0xdf00d299,  bzero32)                /* zeros out a data structure */
THUMB_FN(0xdf006515,  create_init_task)       /* low-level DryOS initialization */
THUMB_FN(0xe0040225,  init_task)              /* USER_MEM size checking, dmSetup, termDriverInit, stdlibSetup etc */
THUMB_FN(0xe06b088c,  dcache_clean)           /* loop with MCR p15 c7,c10,1; DSB */
THUMB_FN(0xe00d9cc8,  dcache_clean_multicore) /* loop writing to c11007b0, called after dcache_clean by copy_mmu_tables() */
THUMB_FN(0xe06b0960,  icache_invalidate)      /* loop with MCR p15 c7,c5,1; c7,c1,6; c7,c1,0; ISB */

/** Tasks **/
THUMB_FN(0xdf008ce7,  task_create)            /* used to start TaskMain, GuiMainTask etc */
THUMB_FN(0xdf008f0b,  task_create_ex)         /* as task_create, but allows selecting CPU for task*/
THUMB_FN(0xdf00880f,  msleep)                 /* argument is always multiple of 10 */
DATA_PTR(    0x1028,  current_task)           // These two can both be found via get_current_task_id(), df003a4e
DATA_PTR(    0x1010,  current_interrupt)      // SJE See task_utils.c for the horrible
                                              // interrupt_level = current_interrupt - 4

/** Dumper **/
THUMB_FN(0xe008099e,  dump_file)              /* tries to save a file to either "A:/%s" or "B:/%s"; calls FIO_RemoveFile/CreateFile/WriteFile/CloseFile/Flush */

/** Memory info **/
THUMB_FN(0xe0289918,  malloc_info)            /* Malloc Information */
THUMB_FN(0xe02899b0,  sysmem_info)            /* System Memory Information */
THUMB_FN(0xe0202768,  memmap_info)            /* Exception vector, DRYOS system memory etc */
THUMB_FN(0xe0175b86,  smemShowFix)            /* Common Lower, Common Upper etc */

/** Memory allocation **/
THUMB_FN(0xdf007b58, _AllocateMemory)
THUMB_FN(0xdf007d10, _FreeMemory)
THUMB_FN(0xdf0079e2,  GetMemoryInformation)   /* called from GUI_Control AllocateMemory */
THUMB_FN(0xdf0079b6,  GetSizeOfMaxRegion)
THUMB_FN(0xdf00ac54, _alloc_dma_memory)
THUMB_FN(0xdf00ac80, _free_dma_memory)
// THUMB_FN(0xe070ad26,  malloc) // commented out to prevent clash with ML symbol of same name
// THUMB_FN(0xe070ad5e,  free) //

/** Debug messages **/
THUMB_FN(0xdf006e6c,  DryosDebugMsg)          /* lots of debug messages; format string is third argument */

/** DMA **/
THUMB_FN(0xe01e569a,  dma_memcpy)

/** EDMAC **/
// EDmac is distinct from XDMAC, they are different controllers.
//
// This is a trimmed set of stubs.  More are known on D7, see 200D.
// This is the minimal required to support raw video, using mem_to_mem_edmac_copy()
THUMB_FN(0xe04aa7f4,  CreateResLockEntry) // via xrefs to a pointer to "ResInfo"
THUMB_FN(0xe04aa8d8,  DeleteResLockEntry)
THUMB_FN(0xe04aa9dc,  LockEngineResources)
THUMB_FN(0xe04aaa7c,  UnLockEngineResources)
THUMB_FN(0x30900, shamem_read) // nearly always inlined, so very few refs.
                               // Can find via "[MEM1DEV] Addr=%#x Data=%#x"
THUMB_FN(0xe01cd9a8, create_mem_to_mem_lock_and_pwrmng_globals)
THUMB_FN(0xe01cdb5e, lock_and_wake_mem_to_mem)
THUMB_FN(0xe01cdb82, unlock_and_sleep_mem_to_mem)
THUMB_FN(0xe01cd9ea, delete_mem_to_mem_lock)
THUMB_FN(0xe01cdb02, set_mem_to_mem_cbr)
THUMB_FN(0xe01cda5e, mem_to_mem_setup_copy)
THUMB_FN(0xe01cdb14, mem_to_mem_set_src_dst_and_start)
THUMB_FN(0xe01cdb0c, set_default_mem_to_mem_cbr)

// This does several things, including releasing / de-associating device 6.
// Device 6 is strongly related to the Channel Switch.  Possibly it is the Channel Switch?
// Possibly it's a device for doing mem 2 mem transfers, which uses Channel Switch.
THUMB_FN(0xe01cdb3e, cleanup_mem_to_mem)

/** MMU **/
THUMB_FN(0xe04dcbba, change_mmu_tables) // Updates TTBRs and does TLB maintenance
//THUMB_FN(0xe0013b58, copy_mmu_tables) // copies from one location to another,
                                      // fixing up addresses held in table start,
                                      // and doing cache maintenance
// slightly MMU related, used by CONFIG_MMU_REMAP
THUMB_FN(0xe078872c, memcpy_dryos) // See 200D stubs for long comment on finding a good memcpy.
THUMB_FN(   0x2d952, _request_RPC) // allows cross-CPU function calls.  See long comment in 200D stubs.S
THUMB_FN(   0x2d972,  clear_RPC_request) // reset the request to prevent calling
                                         // the function forever

/** Eventprocs (call by name) **/
THUMB_FN(0xe04e751a,  call)                   /* many functions called by name (lv_start, lv_stop etc) */

/** File I/O **/
THUMB_FN(0xe04e5b78, _FIO_OpenFile)
THUMB_FN(0xe04e5be2, _FIO_CreateFile)
THUMB_FN(0xe04e5c9c, _FIO_ReadFile)
THUMB_FN(0xe04e5d42, _FIO_WriteFile)
THUMB_FN(0xe04e72c0,  FIO_SeekSkipFile)
THUMB_FN(0xe04e5db4,  FIO_CloseFile)
THUMB_FN(0xe04e66dc, _FIO_CreateDirectory)
//THUMB_FN(0x,  FIO_FindFirst)
THUMB_FN(0xe04e6b06, _FIO_FindFirstEx)
THUMB_FN(0xe04e6b9e,  FIO_FindNextEx)
THUMB_FN(0xe04e6ae6,  FIO_FindClose)
THUMB_FN(0xe04e5ed2, _FIO_GetFileSize)
THUMB_FN(0xe04e5c50, _FIO_RemoveFile)
THUMB_FN(0xe04e6556, _FIO_RenameFile)
THUMB_FN(0xe04e6800,  FIO_Flush)               // to be called after FIO_CloseFile?

THUMB_FN(0xe030c398, autotune_SD)

/** GUI **/
THUMB_FN(0xe00dc454,  GUI_Control)
THUMB_FN(0xe04cc094,  SetGUIRequestMode)
THUMB_FN(0xe0093574,  gui_init_end)
THUMB_FN(0xe00dc2aa,  gui_main_task)

/** GUI timers **/
THUMB_FN(0xe04e30a6,  CancelTimer)            /* from error message */
THUMB_FN(0xe0626236,  SetHPTimerAfterNow)     /* from error message */
THUMB_FN(0xe062628a,  SetHPTimerNextTick)     /* same "worker" function as SetHPTimerAfterNow */
THUMB_FN(0xe04e2ff0,  SetTimerAfter)          /* from error message */
THUMB_FN(0xe06e41dc,  CancelDateTimer)

/** Interrupts **/
DATA_PTR(    0x4030,  pre_isr_hook)
DATA_PTR(    0x4034,  post_isr_hook)
DATA_PTR(   0x65a08,  isr_table_handler)
DATA_PTR(   0x65a0c,  isr_table_param)
THUMB_FN(0xe015fcd8,  cli_spin_lock)          /* used in AllocateMemory/FreeMemory and others */

/** MPU communication **/
THUMB_FN(0xe01f67ea,  mpu_send)                  // "dwSize < TXBD_DATA_SIZE"
THUMB_FN(0xe0644da2,  mpu_recv)                  // passed as last argument by InitializeIntercom and eventually stored into mpu_recv_cbr
DATA_PTR(    0x7c24,  mpu_recv_cbr)              // mpu_recv is called indirectly through this function pointer
DATA_PTR(   0x80ec4,  mpu_send_ring_buffer)      // ring buffer used in mpu_send (see e01f674e)
DATA_PTR(    0x7c08,  mpu_send_ring_buffer_tail) // ring buffer index incremented in mpu_send
DATA_PTR(   0x80d84,  mpu_recv_ring_buffer)      // ring buffer used in SIO3_ISR, subroutine that processes two chars at a time (see e01f694e)
DATA_PTR(    0x7c00,  mpu_recv_ring_buffer_tail) // ring buffer index incremented in the above subroutine

/** Misc **/
THUMB_FN(0xdf009338,  vsnprintf)              /* called near dmstart; references "01234567", "0123456789", "0123456789abcdef"
                                                 and "0123456789ABCDEF"; second arg is size; the one called by DebugMsg only knows %s */

DATA_PTR(    0x100b4,  winsys_sem) // written to in InitializeScreen (see "Winsys_Wait_v"), used in RefreshVrmsSurface

THUMB_FN(0xe048b1d0, _turn_off_display)
THUMB_FN(0xe048b1bc, _turn_on_display)

THUMB_FN(0xdf0031a3,  get_task_by_id) // called indirectly by task_create and functions using string "TASK Name"
THUMB_FN(0xdf004041,  _get_task_info_by_id)
NSTUB(    0x4f40,  dm_names) // in DebugMsg, before the 1st loop target

DATA_PTR(0x20054,  sd_device) // written to in SD_DeviceCreate
NSTUB(    0x4800,  gui_main_struct) // See function using "Queue Clear event" string
                                    // There's a base of struct, and some refs to fields via offsets.
                                    // Find the right one.
THUMB_FN(0xe00dc4ea,  GUI_ChangeMode) // "GUI_ChangeMode:%d" string
THUMB_FN(0xe04e1820,  DispSensorStart) // 0xe7ec looks to be a bool for whether Sensor is started
THUMB_FN(0xe04e41a8,  DeleteMemorySuite)
THUMB_FN(0xe04e43e8,  CreateMemorySuite)
THUMB_FN(0xe04e3914,  CreateMemoryChunk) // called from CreateMemorySuite
//NSTUB(0x,  GetSizeOfMemorySuite) // not needed, but I found it along the way
THUMB_FN(0xdf008515,  take_semaphore) // Fairly sure. Same function sig and followed by give_semaphore
THUMB_FN(0xdf0084e4,  take_semaphore_now)
THUMB_FN(0xdf00858f,  give_semaphore)
THUMB_FN(0xdf008419,  create_named_semaphore) // also see df00b114, very similar, but df008418 is closer to 50D create_named_semaphore.
                                              // Complicated by there being df00b648, which wraps 8418 with an extra assert possible,
                                              // and e0426a20, which is what most sites call, which calls 8418 - but thunked.
                                              // In theory that means DryOS could replace the underlying create_named_semaphore
                                              // at run-time.  No evidence that they do.

THUMB_FN(0xe032c262,  gui_change_mode) // GUI_ChangeMode_Post
THUMB_FN(0xe032aa08,  gui_massive_event_loop) // various "GUICMD_"* strings
THUMB_FN(0xe009a070,  gui_init_event) // "-> handleGuiInit"
THUMB_FN(0xe04e3ec2,  AddMemoryChunk) // called before " 32(Src,bytes,Unit)=(%#lx,%#lx,%#lx)" in many places; see also hAddSuite
THUMB_FN(0xe04e3f46,  GetFirstChunkFromSuite)                  // AJ_PackMemory_PackMem_p3
THUMB_FN(0xe04e4816,  GetNextMemoryChunk)                      // AJ_PackMemory_PackMem.c_IsChunkSignature_p2
THUMB_FN(0xe04e3d60,  GetMemoryAddressOfMemoryChunk)           // AJ_PackMemory_PackMem.c_IsChunkSignature - 200D uses "MemChunk" directly
THUMB_FN(0xe066b086,  AllocateMemoryResource)                  // m_pfAllocMemoryCBR
THUMB_FN(0xe066b0b4,  AllocateContinuousMemoryResource)        // m_pfContAllocMemoryCBR
THUMB_FN(0xe066b15a,  FreeMemoryResource)                      // m_pfFreeMemoryCBR

THUMB_FN(0xdf00b1d9,  msg_queue_receive) // "SystemIF::KerQueue.c" useful for finding some of these msg_ functions
THUMB_FN(0xdf00b337,  msg_queue_post)
THUMB_FN(0xdf00b36d,  msg_queue_count) // Fairly hard to find.  Often used near DispLockStop, which is easy to find
                                       // and not used often.
                                       // This is the thunked address, which is called from e042680c
THUMB_FN(0xdf00b615,  msg_queue_create) // CreateMessageQueue - In 50D, msg_queue_create calls 0xff866424,
                                        // the equiv of which is 0xdf00b114 (and was easier to find)

THUMB_FN(0xe04e53d0,  prop_deliver)
THUMB_FN(0xe04e4e6a,  prop_register_slave) // "pRegisterSlave"
DATA_PTR(0xea8c,      LiveViewApp_dialog) // in StartLiveViewApp()
THUMB_FN(0xe03a1c40,  LiveViewApp_handler) // DebugMsg: "StartLiveViewApp(%#x)", address passed to CreateDialogBox
THUMB_FN(0xe07bce24,  LiveViewShutterApp_handler) // DebugMsg: "StartLiveViewShutterApp", address passed to CreateDialogBox
THUMB_FN(0xe05439fe,  PlayMovieGuideApp_handler) // "StartPlayMovieGuideApp"
THUMB_FN(0xe0487124,  PlayMain_handler) // "DlgPlayMain.c PRESS_ERASE_BUTTON" - code above the string, wasn't found automatically
THUMB_FN(0xe07d08f8,  ErrCardForLVApp_handler) // "DlgLiveViewError.c DELETE_DIALOG_REQUEST"
THUMB_FN(0xe03d5c06,  GetCFnData) // Differs significantly from 200D
THUMB_FN(0xe03d4d68,  SetCFnData) // There are some "SetCFnData" related strings

THUMB_FN(0xe070ae0e, _malloc)
THUMB_FN(0xe070ae46, _free)
THUMB_FN(0xe04f955e,  SRM_AllocateMemoryResourceFor1stJob) // "hJobMemorySuite" strings, function a little earlier
THUMB_FN(0xe04fc402,  SRM_FreeMemoryResourceFor1stJob) // find refs to "pMessage", there are several similar looking functions using it,
                                                       // this is my best guess - this one has pPriorMessage and sets offset 8
                                                       // in the struct to 0, offset 0x10 to param1
THUMB_FN(0xe04e5462, _prop_cleanup) // "./Multicast/PropertyMgr.c" check funcs that ref this string
THUMB_FN(0xdf00b67d,  CreateRecursiveLock) // via "KerRLock.c", CRL() calls a function that regs this string, not many do
THUMB_FN(0xdf00b451,  AcquireRecursiveLock)
THUMB_FN(0xdf00b51b,  ReleaseRecursiveLock)
THUMB_FN(0xe04db200,  dialog_redraw) // Found via checking xrefs to "pDialog->pSignature == pcSignature"
THUMB_FN(0xe04aa7f4,  CreateResLockEntry) // via xrefs to a pointer to "ResInfo"
THUMB_FN(0xe04aa9dc,  LockEngineResources) //  Down a bit from CreateResLockEntry, start v similar but end is different. (see 0xe016a0ea)
THUMB_FN(0xe076be4c,  ErrForCamera_handler) // ERR70, ERR80 etc (DlgErrForCamera.c, AJ_DIALOG.HANDLER_DlgErrForCamera.c)

//THUMB_FN(0xdf00d365,  fsuDecodePartitionTable) // SJE FIXME wrong - this function doesn't exist on 6D2.  See longer notes in 200D stubs.S.
                                               // I think bootflags.c code should be changed so we do this manually for these cams.

THUMB_FN(0xe06a5d58,  ShootOlcApp_handler) // near "StopShootOlcApp PopPalette(%d)"
//DATA_PTR(    0x40d0,  first_task) // unsure on naming here.  First item in an array of task structs.
                                  // Seems to always be "idle" task.
                                  // (we don't use this, but instead calculate it during early init,
                                  //  to save backporting this stub for all older cams)
NSTUB(    0x40f0,  task_max)
NSTUB(    0xffc0,  gui_task_list) // Found in function 0xe06f0ac0.
THUMB_FN(0xe06b464c, _LoadCalendarFromRTC)

// These are required on Digic 678
THUMB_FN(0xe04c78b8, RefreshVrmsSurface) // "Call RefreshVrmsSurface"
DATA_PTR(   0x100cc, display_refresh_needed) // variable named 'fRefreshDisplay'. If 0, RefreshVrmsSurface() does nothing
THUMB_FN(0xe022d60c, XimrExe) // trigger Ximr unit to execute (HW compositor, partially understood)

DATA_PTR(   0x100bc, bmp_vram_info) // Used in function with string "B:/YCC_A_%02d.bin".

DATA_PTR(   0x100b8, _rgb_vram_info) // Used in many places, written to in "InitializeScreen"
                                     // At least for 200D, this is BGRA buffer, earlier in the render
                                     // chain than the YUV ones.  But, changes made to it will not
                                     // render until a refresh occurs (can manually trigger via RefreshVrmsSurface()),
                                     // but direct refresh via XimrExe() is simpler (we think this tells
                                     // the renderer peripheral to take the RGB data and update the screen(s).

DATA_PTR(   0x100d8,  display_output_mode) // on 200D, 1: 960x540, anything else, 720x480

NSTUB(0x56500000,  LCD_Palette) // in InitializeBitmapDisplayDevice, right after 0xc0f14800
                                // SJE FIXME fake address in safe writable region, may not exist
                                // on 200D (doesn't seem to use indexed colour anywhere)

DATA_PTR(    0x412c,  task_dispatch_hook)
DATA_PTR(    0xe789,  additional_version) // this gets written to in early code.  Pick a safe area!
THUMB_FN(0xe04e4f38, _prop_request_change)
THUMB_FN(0xdf0070c5,  dm_set_store_level) // called by "dmstore"
                                          // Called in init_task(), shortly before ICU Firmware version string
                                          // is logged.
THUMB_FN(0xdf00707d,  dm_set_print_level)
THUMB_FN(0xdf00325d,  task_trampoline)

THUMB_FN(0xe0447352, _get_fps_register_a)
THUMB_FN(0xe04472c6, _get_fps_register_b)

//////
// SJE stuff that's not required but can be useful
THUMB_FN(0xe0617708,  debug_assert) // DRYOS_ASSERT_HANDLER related
THUMB_FN(0xe04eb888, uart_printf)
