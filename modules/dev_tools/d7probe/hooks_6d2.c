/* Thumb entry stubs for the two 6D2 1.1.1 hooks.  The MMU patch replaces the
 * first 8 bytes of the Canon function with "ldr pc, [pc]; .word target", so
 * each stub must (1) save everything, (2) hand the saved registers to the C
 * capture routine, (3) restore, (4) replay the 8 bytes it displaced - with any
 * pc-relative literal resolved by hand - and (5) jump to patch_addr + 8 (Thumb).
 * Same construction as adtglog2's hook_CMOS_write_6D2.  Nothing here modifies
 * a register or a memory word Canon owns. */
#include "d7probe.h"

/* 0xe014493e  PreproPath.c update (LiveView pre-processing path parameters)
 *   2d e9 ff 5f   push.w {r0-r8, sb, sl, fp, ip, lr}
 *   00 25         movs   r5, #0
 *   e2 4f         ldr    r7, [pc, #0x388]     ; = 0x000ca3b8 (from 0xe0144cd0)
 */
void __attribute__((noreturn,noinline,naked,aligned(4))) d7probe_hook_prepro_6D2(void)
{
    uint32_t *regs;
    asm __volatile__ (
        "push {r0-r12, lr}\n"
        "mov %0, sp\n" : "=r"(regs)
    );
    d7probe_prepro_capture(regs);      /* regs[0..12] = r0..r12, regs[13] = lr */
    asm __volatile__ (
        "pop  {r0-r12, lr}\n"
        "push {r0-r12, lr}\n"
        "movs r5, #0\n"
        "ldr  r7, =0x000ca3b8\n"
        "ldr  pc, =0xe0144947\n"
    );
}

/* 0xe0292d1c  SetSDClkFrequency(slot, mode, idx) (LimeSdIo/SdDom/SdClk.c)
 *   2d e9 f0 41   push.w {r4-r8, lr}
 *   17 46         mov    r7, r2
 *   1c 4a         ldr    r2, [pc, #0x70]      ; = 0xe0975470 (from 0xe0292d94)
 */
void __attribute__((noreturn,noinline,naked,aligned(4))) d7probe_hook_sdclk_6D2(void)
{
    uint32_t *regs;
    asm __volatile__ (
        "push {r0-r12, lr}\n"
        "mov %0, sp\n" : "=r"(regs)
    );
    d7probe_sdclk_capture(regs);
    asm __volatile__ (
        "pop  {r0-r12, lr}\n"
        "push {r4-r8, lr}\n"
        "mov  r7, r2\n"
        "ldr  r2, =0xe0975470\n"
        "ldr  pc, =0xe0292d25\n"
    );
}

/* 0xe0292568  SDCON register-set writer (slot, table): writes 11 words from
 * table[slot*11..] to 0xd2090600 + {600,618,61c,620,62c,630,624,628,638,604,60c}
 *   70 b5         push   {r4, r5, r6, lr}
 *   05 46         mov    r5, r0
 *   0e 46         mov    r6, r1
 *   00 eb 45 00   add.w  r0, r0, r5, lsl #1     (the patch cuts this one in half)
 * so the replay covers 10 bytes and resumes at +10.
 */
void __attribute__((noreturn,noinline,naked,aligned(4))) d7probe_hook_sdset_6D2(void)
{
    uint32_t *regs;
    asm __volatile__ (
        "push {r0-r12, lr}\n"
        "mov %0, sp\n" : "=r"(regs)
    );
    d7probe_sdset_capture(regs);
    asm __volatile__ (
        "pop  {r0-r12, lr}\n"
        "push {r4, r5, r6, lr}\n"
        "mov  r5, r0\n"
        "mov  r6, r1\n"
        "add.w r0, r0, r5, lsl #1\n"
        "ldr  pc, =0xe0292573\n"
    );
}

/* 0xe051b560  SetMem1ToRaw(param, cbr): Canon's still-photo lossless path (CR2)
 * hands its parameter block to the engine
 *   70 b5         push   {r4, r5, r6, lr}
 *   0c 46         mov    r4, r1
 *   05 46         mov    r5, r0
 *   02 21         movs   r1, #2
 */
void __attribute__((noreturn,noinline,naked,aligned(4))) d7probe_hook_m2r_set_6D2(void)
{
    uint32_t *regs;
    asm __volatile__ (
        "push {r0-r12, lr}\n"
        "mov %0, sp\n" : "=r"(regs)
    );
    d7probe_m2r_set_capture(regs);
    asm __volatile__ (
        "pop  {r0-r12, lr}\n"
        "push {r4, r5, r6, lr}\n"
        "mov  r4, r1\n"
        "mov  r5, r0\n"
        "movs r1, #2\n"
        "ldr  pc, =0xe051b569\n"
    );
}

/* 0xe04aa8f4  LockEngineResources(entry) (EngResMng): which resource lists the
 * photo path locks around its lossless encode
 *   f8 b5         push   {r3, r4, r5, r6, r7, lr}
 *   04 46         mov    r4, r0
 *   5e 4e         ldr    r6, [pc, #0x178]     ; = 0x00010bf0 (from 0xe04aaa74)
 *   00 68         ldr    r0, [r0]
 */
void __attribute__((noreturn,noinline,naked,aligned(4))) d7probe_hook_lock_6D2(void)
{
    uint32_t *regs;
    asm __volatile__ (
        "push {r0-r12, lr}\n"
        "mov %0, sp\n" : "=r"(regs)
    );
    d7probe_lock_capture(regs);
    asm __volatile__ (
        "pop  {r0-r12, lr}\n"
        "push {r3, r4, r5, r6, r7, lr}\n"
        "mov  r4, r0\n"
        "ldr  r6, =0x00010bf0\n"
        "ldr  r0, [r0]\n"
        "ldr  pc, =0xe04aa8fd\n"
    );
}
