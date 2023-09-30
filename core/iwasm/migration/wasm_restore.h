#ifndef _WASM_RESTORE_H
#define _WASM_RESTORE_H

#include "../common/wasm_exec_env.h"
#include "../interpreter/wasm_interp.h"

void set_restore_flag(bool f);
bool get_restore_flag();

int wasm_restore(WASMModuleInstance *module,
            WASMExecEnv *exec_env,
            WASMFunctionInstance *cur_func,
            WASMInterpFrame *prev_frame,
            WASMMemoryInstance *memory,
            WASMGlobalInstance *globals,
            uint8 *global_data,
            uint8 *global_addr,
            WASMInterpFrame *frame,
            register uint8 *frame_ip,
            register uint32 *frame_lp,
            register uint32 *frame_sp,
            WASMBranchBlock *frame_csp,
            uint8 *frame_ip_end,
            uint8 *else_addr,
            uint8 *end_addr,
            uint8 *maddr,
            bool *done_flag);
#endif // _WASM_RESTORE_H