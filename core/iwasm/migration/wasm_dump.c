#include <stdio.h>
#include <stdlib.h>

#include "../interpreter/wasm_runtime.h"
#include "../common/wasm_dump.h"

static Frame_Info *root_info = NULL, *tail_info = NULL;

void
wasm_dump_set_root_and_tail(Frame_Info *root, Frame_Info *tail)
{
    root_info = root;
    tail_info = tail;
}

void
wasm_dump_alloc_init_frame(uint32 all_cell_num)
{
    root_info->all_cell_num = all_cell_num;
}

void
wasm_dump_alloc_frame(WASMInterpFrame *frame, WASMExecEnv *exec_env)
{
    Frame_Info *info;
    info = malloc(sizeof(Frame_Info));
    info->frame = frame;
    info->all_cell_num = 0;
    info->prev = tail_info;
    info->next = NULL;

    if (root_info == NULL) {
        root_info = tail_info = info;
    }
    else {
        tail_info->next = info;
        tail_info = tail_info->next;
    }
}

void
wasm_dump_free_frame(void)
{
    if (root_info == tail_info) {
        free(root_info);
        root_info = tail_info = NULL;
    }
    else {
        tail_info = tail_info->prev;
        free(tail_info->next);
        tail_info->next = NULL;
    }
}

void
wasm_dump_frame(WASMExecEnv *exec_env)
{
    WASMModuleInstance *module_inst =
        (WASMModuleInstance *)exec_env->module_inst;
    Frame_Info *info;
    WASMFunctionInstance *function;
    uint32 func_idx;
    FILE *fp;
    int i;

    if (!root_info) {
        printf("dump failed\n");
        exit(1);
    }
    fp = fopen("frame.img", "wb");
    info = root_info;

    while (info) {

        if (info->frame->function == NULL) {
            // 初期フレーム
            func_idx = -1;
            fwrite(&func_idx, sizeof(uint32), 1, fp);
            fwrite(&info->all_cell_num, sizeof(uint32), 1, fp);
        }
        else {
            func_idx = info->frame->function - module_inst->functions;
            fwrite(&func_idx, sizeof(uint32), 1, fp);
            printf("dump func_idx: %d\n", func_idx);

            dump_WASMInterpFrame(info->frame, exec_env, fp);
        }
        info = info->next;
    }
    fclose(fp);
}

static void
dump_WASMInterpFrame(WASMInterpFrame *frame, WASMExecEnv *exec_env, FILE *fp)
{
    int i;
    WASMModuleInstance *module_inst = exec_env->module_inst;

    // struct WASMInterpFrame *prev_frame;
    // struct WASMFunctionInstance *function;
    // uint8 *ip;
    uint32 ip_offset = frame->ip - wasm_get_func_code(frame->function);
    fwrite(&ip_offset, sizeof(uint32), 1, fp);

    // uint32 *sp_bottom;
    // uint32 *sp_boundary;
    // uint32 *sp;
    uint32 sp_offset = frame->sp - frame->sp_bottom;
    fwrite(&sp_offset, sizeof(uint32), 1, fp);

    // WASMBranchBlock *csp_bottom;
    // WASMBranchBlock *csp_boundary;
    // WASMBranchBlock *csp;
    uint32 csp_offset = frame->csp - frame->csp_bottom;
    fwrite(&csp_offset, sizeof(uint32), 1, fp);

    // uint32 lp[1];
    WASMFunctionInstance *func = frame->function;

    uint32 *lp = frame->lp;
    // VALUE_TYPE_I32
    // VALUE_TYPE_F32
    // VALUE_TYPE_I64
    // VALUE_TYPE_F64
    for (i = 0; i < func->param_count; i++) {
        switch (func->param_types[i]) {
            case VALUE_TYPE_I32:
            case VALUE_TYPE_F32:
                fwrite(lp, sizeof(uint32), 1, fp);
                lp++;
                break;
            case VALUE_TYPE_I64:
            case VALUE_TYPE_F64:
                fwrite(lp, sizeof(uint64), 1, fp);
                lp += 2;
                break;
            default:
                printf("TYPE NULL\n");
                break;
        }
    }
    for (i = 0; i < func->local_count; i++) {
        switch (func->local_types[i]) {
            case VALUE_TYPE_I32:
            case VALUE_TYPE_F32:
                fwrite(lp, sizeof(uint32), 1, fp);
                lp++;
                break;
            case VALUE_TYPE_I64:
            case VALUE_TYPE_F64:
                fwrite(lp, sizeof(uint64), 1, fp);
                lp += 2;
                break;
            default:
                printf("TYPE NULL\n");
                break;
        }
    }

    fwrite(frame->sp_bottom, sizeof(uint32), sp_offset, fp);

    WASMBranchBlock *csp = frame->csp_bottom;
    uint32 csp_num = frame->csp - frame->csp_bottom;
    

    for (i = 0; i < csp_num; i++, csp++) {
        // uint8 *begin_addr;
        uint64 addr;
        if (csp->begin_addr == NULL) {
            addr = -1;
            fwrite(&addr, sizeof(uint64), 1, fp);
        }
        else {
            addr = csp->begin_addr - wasm_get_func_code(frame->function);
            fwrite(&addr, sizeof(uint64), 1, fp);
        }

        // uint8 *target_addr;
        if (csp->target_addr == NULL) {
            addr = -1;
            fwrite(&addr, sizeof(uint64), 1, fp);
        }
        else {
            addr = csp->target_addr - wasm_get_func_code(frame->function);
            fwrite(&addr, sizeof(uint64), 1, fp);
        }

        // uint32 *frame_sp;
        if (csp->frame_sp == NULL) {
            addr = -1;
            fwrite(&addr, sizeof(uint64), 1, fp);
        }
        else {
            addr = csp->frame_sp - frame->sp_bottom;
            fwrite(&addr, sizeof(uint64), 1, fp);
        }

        // uint32 cell_num;
        fwrite(&csp->cell_num, sizeof(uint32), 1, fp);
    }
}

int dump(const WASMExecEnv *exec_env,
         const WASMMemoryInstance *meomry,
         const WASMGlobalInstance *globals,
         const WASMFunctionInstance *cur_func,
         const WASMInterpFrame *frame,
         const register uint8 *frame_ip,
         const register uint32 *frame_sp,
         const WASMBranchBlock *frame_csp,
         const uint8 *frame_ip_end,
         const uint8 *else_addr,
         const uint8 *end_addr,
         const uint8 *maddr,
         const bool done_flag) 
{
    FILE *fp;
    fp = fopen("interp.img", "wb");
    if (fp == NULL) {
        perror("failed to open interp.img\n");
        return -1;
    }
    // WASMMemoryInstance *memory = module->default_memory;
    fwrite(memory->memory_data, sizeof(uint8),
           memory->num_bytes_per_page * memory->cur_page_count, fp);
    // uint32 num_bytes_per_page = memory ? memory->num_bytes_per_page :
    // 0;
    // uint8 *global_data = module->global_data;
    for (i = 0; i < module->global_count; i++) {
        switch (globals[i].type) {
            case VALUE_TYPE_I32:
            case VALUE_TYPE_F32:
                global_addr = get_global_addr(global_data, globals + i);
                fwrite(global_addr, sizeof(uint32), 1, fp);
                break;
            case VALUE_TYPE_I64:
            case VALUE_TYPE_F64:
                global_addr = get_global_addr(global_data, globals + i);
                fwrite(global_addr, sizeof(uint64), 1, fp);
                break;
            default:
                printf("type error:B\n");
                break;
        }
    }
    // uint32 linear_mem_size =
    //     memory ? num_bytes_per_page * memory->cur_page_count : 0;
    // WASMType **wasm_types = module->module->types;
    // WASMGlobalInstance *globals = module->globals, *global;
    // uint8 opcode_IMPDEP = WASM_OP_IMPDEP;
    // WASMInterpFrame *frame = NULL;
    SYNC_ALL_TO_FRAME();
    wasm_dump_frame(exec_env);

    uint32 p_offset;
    // register uint8 *frame_ip = &opcode_IMPDEP;
    p_offset = frame_ip - wasm_get_func_code(cur_func);
    fwrite(&p_offset, sizeof(uint32), 1, fp);
    // register uint32 *frame_lp = NULL;
    // register uint32 *frame_sp = NULL;
    p_offset = frame_sp - frame->sp_bottom;
    fwrite(&p_offset, sizeof(uint32), 1, fp);

    // WASMBranchBlock *frame_csp = NULL;
    p_offset = frame_csp - frame->csp_bottom;
    fwrite(&p_offset, sizeof(uint32), 1, fp);
    // BlockAddr *cache_items;
    // uint8 *frame_ip_end = frame_ip + 1;
    // uint8 opcode;
    // uint32 i, depth, cond, count, fidx, tidx, lidx, frame_size = 0;
    // uint64 all_cell_num = 0;
    // int32 val;
    // uint8 *else_addr, *end_addr, *maddr = NULL;
    p_offset = else_addr - wasm_get_func_code(cur_func);
    fwrite(&p_offset, sizeof(uint32), 1, fp);
    p_offset = end_addr - wasm_get_func_code(cur_func);
    fwrite(&p_offset, sizeof(uint32), 1, fp);
    p_offset = maddr - memory->memory_data;
    fwrite(&p_offset, sizeof(uint32), 1, fp);

    fwrite(&done_flag, sizeof(done_flag), 1, fp);

    if (native_handler != NULL) {
        (*native_handler)();
    }

    fclose(fp);

    printf("step:%ld\n", step);
    printf("frame_ip:%x\n", frame_ip - cur_func->u.func->code);

    return 0;
}