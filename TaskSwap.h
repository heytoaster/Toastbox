#pragma once
#include <algorithm>

// (1) Push callee-saved regs onto stack, including $PC if needed
// (2) `spSave` = $SP
// (3) Swap `sp` and `spSave`
//
//     It's crucial to perform this swap at this point (between saving and
//     restoring) to ensure no registers get clobbered:
//       - Do at beginning: potentially clobber registers before they're saved
//       - Do at end: potentially clobber registers after they're restored
//
// (4) $SP = `spSave`
//
// if `initFn`:
//   (5) Jump to `initFn`
//
// else
//   (6) Pop callee-saved registers from stack
//   (7) Return to caller

#if defined(TaskMSP430)

#define TaskSwap(initFn, sp, spSave)                                                    \
                                                                                        \
    if constexpr (sizeof(void*) == 2) {                                                 \
        /* ## Architecture = MSP430, small memory model */                              \
        asm volatile("pushm #7, r10" : : : );                           /* (1) */       \
        asm volatile("mov sp, %0" : "=m" (spSave) : : );                /* (2) */       \
                                                                                        \
        asm volatile("mov %0, r11" : : "m" (sp) : "r11");               /* (3) */       \
        asm volatile("mov %1, %0" : "=m" (sp) : "m" (spSave) : );       /* (3) */       \
        asm volatile("mov r11, %0" : "=m" (spSave) : : );               /* (3) */       \
                                                                                        \
        asm volatile("mov %0, sp" : : "m" (spSave) : );                 /* (4) */       \
        if constexpr (!std::is_null_pointer<decltype(initFn)>::value) {                 \
            asm volatile("br %0" : : "i" (initFn) : );                  /* (5) */       \
        } else {                                                                        \
            asm volatile("popm #7, r10" : : : );                        /* (6) */       \
            asm volatile("ret" : : : );                                 /* (7) */       \
        }                                                                               \
    } else {                                                                            \
        /* ## Architecture = MSP430, large memory model */                              \
        asm volatile("pushm.a #7, r10" : : : );                         /* (1) */       \
        asm volatile("mov.a sp, %0" : "=m" (spSave) : : );              /* (2) */       \
                                                                                        \
        asm volatile("mov.a %0, r11" : : "m" (sp) : "r11");             /* (3) */       \
        /* Use movx.a instead of mov.a because the necessary memory<->memory            \
           addressing mode doesn't exist for mov.a. See TI's documentation              \
           "1.5.2.6 MSP430X Address Instructions" */                                    \
        asm volatile("movx.a %1, %0" : "=m" (sp) : "m" (spSave) : );    /* (3) */       \
        asm volatile("mov.a r11, %0" : "=m" (spSave) : : );             /* (3) */       \
                                                                                        \
        asm volatile("mov.a %0, sp" : : "m" (spSave) : );               /* (4) */       \
        if constexpr (!std::is_null_pointer<decltype(initFn)>::value) {                 \
            asm volatile("br.a %0" : : "i" (initFn) : );                /* (5) */       \
        } else {                                                                        \
            asm volatile("popm.a #7, r10" : : : );                      /* (6) */       \
            asm volatile("ret.a" : : : );                               /* (7) */       \
        }                                                                               \
    }

#elif defined(TaskARM32)
    
#define TaskSwap(initFn, sp, spSave)                                                    \
                                                                                        \
    /* ## Architecture = ARM32 */                                                       \
    asm volatile("push {r4-r11,lr}" : : : );                            /* (1) */       \
    asm volatile("str sp, %0" : "=m" (spSave) : : );                    /* (2) */       \
                                                                                        \
    asm volatile("ldr r0, %0" : : "m" (sp) : "r0");                     /* (3) */       \
    asm volatile("ldr r1, %0" : : "m" (spSave) : "r1");                 /* (3) */       \
    asm volatile("str r0, %0" : "=m" (spSave) : : );                    /* (3) */       \
    asm volatile("str r1, %0" : "=m" (sp) : : );                        /* (3) */       \
                                                                                        \
    asm volatile("ldr sp, %0" : : "m" (spSave) : );                     /* (4) */       \
    if constexpr (!std::is_null_pointer<decltype(initFn)>::value) {                     \
        asm volatile("b %0" : : "i" (initFn) : );                       /* (5) */       \
    } else {                                                                            \
        asm volatile("pop {r4-r11,lr}" : : : );                         /* (6) */       \
        asm volatile("bx lr" : : : );                                   /* (7) */       \
    }
    
#else
    
    #error Task: Unsupported architecture
    
#endif

//template <bool T_Init=false, void T_InitFn(void)=nullptr>
//static void TaskSwap(void*& sp, void*& spSave) {
//    
//    // (1) Push callee-saved regs onto stack (including return address if it's
//    //     not already on the stack)
//    // (2) spSave = SP
//    // (3) std::swap(sp, spSave)
//    //
//    //     It's crucial to perform this swap at this point (between saving and
//    //     restoring) to ensure no registers get clobbered:
//    //       - Do at beginning: potentially clobber registers before they're saved
//    //       - Do at end: potentially clobber registers after they're restored
//    //
//    // (4) SP = spSave
//    //
//    // if T_Init:
//    //   (5) Jump to `T_InitFn`
//    //
//    // else
//    //   (6) Pop callee-saved registers from stack
//    //   (7) Restore PC
//    
//#if defined(TaskMSP430)
//    
//    if constexpr (sizeof(void*) == 2) {
//        // ## Architecture = MSP430, small memory model
//        asm volatile("pushm #7, r10" : : : );                   // (1)
//        asm volatile("mov SP, %0" : "=m" (spSave) : : );        // (2)
//        
//        asm volatile("mov %0, r11" : : "m" (sp) : "r11");            // (3)
//        asm volatile("mov %0, r12" : : "m" (spSave) : "r12");        // (3)
//        asm volatile("mov r11, %0" : "=m" (spSave) : : );       // (3)
//        asm volatile("mov r12, %0" : "=m" (sp) : : );           // (3)
//        
//        asm volatile("mov %0, SP" : : "m" (spSave) : );         // (4)
//        if constexpr (T_Init) {
//            asm volatile("br %0" : : "i" (T_InitFn) : );        // (5)
//        } else {
//            asm volatile("popm #7, r10" : : : );                // (6)
//            asm volatile("ret" : : : );                         // (7)
//        }
//    
//    } else {
//        // ## Architecture = MSP430, large memory model
//        asm volatile("pushm.a #7, r10" : : : );                 // (1)
//        asm volatile("mov.a SP, %0" : "=m" (spSave) : : );      // (2)
//        std::swap(sp, spSave);                                  // (3)
//        asm volatile("mov.a %0, SP" : : "m" (spSave) : );       // (4)
//        if constexpr (T_Init) {
//            asm volatile("mov.a %0, PC" : : "i" (T_InitFn) : ); // (5)
//        } else {
//            asm volatile("popm.a #7, r10" : : : );              // (6)
//            asm volatile("ret.a" : : : );                       // (7)
//        }
//    }
//    
//#elif defined(TaskARM32)
//    
//    // ## Architecture = ARM32, large memory model
//    asm volatile("push {r4-r11,LR}" : : : );                // (1)
//    asm volatile("str SP, %0" : "=m" (spSave) : : );        // (2)
//    std::swap(sp, spSave);                                  // (3)
//    asm volatile("ldr SP, %0" : : "m" (spSave) : );         // (4)
//    if constexpr (T_Init) {
//        asm volatile("b %0" : : "i" (T_InitFn) : );         // (5)
//    } else {
//        asm volatile("pop {r4-r11,LR}" : : : );             // (6)
//        asm volatile("bx LR" : : : );                       // (7)
//    }
//    
//#else
//    
//    #error Task: Unsupported architecture
//    
//#endif
