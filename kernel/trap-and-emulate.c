#include "types.h"
#include "param.h"
#include "memlayout.h"
#include "riscv.h"
#include "spinlock.h"
#include "proc.h"
#include "defs.h"

#define USERMODE        0x0
#define SUPERVISORMODE  0x1
#define MACHINEMODE     0x2

// Struct to keep VM registers (Sample; feel free to change.)
struct vm_reg {
    int     code;
    int     mode;
    uint64  val;
};

// Keep the virtual state of the VM's privileged registers
struct vm_virtual_state {
    // User trap setup (0x0)
    struct vm_reg v_ustatus;
    struct vm_reg v_uie;
    struct vm_reg v_utvec;

    // User trap handling (0x80)
    struct vm_reg v_uscratch;
    struct vm_reg v_uepc;
    struct vm_reg v_ucause;
    struct vm_reg v_utval;
    struct vm_reg v_uip;

    // Supervisor Trap Setup (0x100)
    struct vm_reg v_sstatus;
    struct vm_reg v_sreserved;
    struct vm_reg v_sedeleg;
    struct vm_reg v_sideleg;
    struct vm_reg v_sie;
    struct vm_reg v_stvec;

    // Supervisor Trap Handling (0x180)
    struct vm_reg v_sscratch;
    struct vm_reg v_sepc;
    struct vm_reg v_scause;
    struct vm_reg v_stval;
    struct vm_reg v_sip;

    // Machine Information Registers (0x300)
    struct vm_reg v_mvendorid;
    struct vm_reg v_marchid;
    struct vm_reg v_mimpid;
    struct vm_reg v_mhartid;

    // Supervisor page table register (0x180)
    struct vm_reg v_satp;

    // Machine Trap Setup (0x340)
    struct vm_reg v_mstatus;
    struct vm_reg v_misa;
    struct vm_reg v_medeleg;
    struct vm_reg v_mie;
    struct vm_reg v_mtvec;

    // Machine Trap Handling (0x380)
    struct vm_reg v_mscratch;
    struct vm_reg v_mepc;
    struct vm_reg v_mcause;
};

struct vm_reg *tmp;

// Struct to keep VM physical memory
struct vm_virtual_state vm_vs;
int vm_vs_current_mode;

void handle_csr_instruction(uint32 op, uint64* rd, uint64* rs1, struct vm_reg* rs2) {
    printf("handle_csr_instruction: op=%d, rd=%p, rs1=%p, rs2=%p\n", op, *rd, *rs1, rs2->code);
    struct proc *p = myproc();
    if (vm_vs_current_mode < rs2->mode) {
        setkilled(p);
        return;
    }

    if (*rd == 0 && *rs1 != 0) {
        rs2->val = *rs1;
    } else if (*rd != 0 && *rs1 == 0) {
        *rd = rs2->val;
    }

    if (*rd == 0 && *rs1 == 0) {
        if (rs2->code == 0x302) {
            unsigned int mstatus_pl = (vm_vs.v_mstatus.val >> 11) & 3;
            p->trapframe->epc = vm_vs.v_mepc.val;
            if (mstatus_pl == 0) {
                vm_vs_current_mode = USERMODE;
            } else if (mstatus_pl == 1) {
                vm_vs_current_mode = SUPERVISORMODE;
            }
            return;
        } else if (rs2->code == 0x102) {
            p->trapframe->epc = vm_vs.v_sepc.val;
            vm_vs_current_mode = USERMODE;
            return;
        }
    }

    p->trapframe->epc += 4;

}


uint64* retrieve_uvm_register(uint32 regcode) {
    // In trapframe, ra always starts from 0x40
    int index = ((regcode-1)*sizeof(uint64));
    struct proc *p = myproc();
    uint64 start = (uint64) &(p->trapframe->ra);
    uint64 addr = start + index;
#if 0
    printf("p->trapframe->ra: %p\n", &(p->trapframe->ra));
    printf("index = %d\n", index);
    printf("addr: %p\n", addr);
#endif
    return (uint64*) addr;
}



struct vm_reg* get_vm_register(uint32 regcode) {
    int nregs = sizeof(vm_vs)/sizeof(struct vm_reg);
    struct vm_reg* cur  = &(vm_vs.v_uie);
    for (int i = 0; i < nregs; i++) {
        if (cur->code == regcode) {
            return cur;
        }
        cur++;
    }

    // Check if this is user trap setup
    return NULL;
}



void trap_and_emulate(void) {

    struct proc *p = myproc();

    uint64 program_counter_addr = r_sepc();

    // Read the actual instruction
    char* instruction_holder = kalloc();
    memset((void*) instruction_holder, 0, PGSIZE);
    copyin(p->pagetable, instruction_holder, program_counter_addr, PGSIZE);
    uint64 bitmask = 0x00000000FFFFFFFF;
    uint32 instruction = *((uint32*) instruction_holder) & bitmask;

    uint32 op     = instruction & 0x0000007F;
    uint32 rd     = (instruction >> 7) & 0x1F;
    uint32 funct3 = (instruction >> 12) & 0x7;
    uint32 rs1    = (instruction >> 15) & 0x1F;
    uint32 uimm   = (instruction >> 20);
    printf("(PI at %p) op = %x, rd = %x, funct3 = %x, rs1 = %x, uimm = %x\n", 
                program_counter_addr, op, rd, funct3, rs1, uimm);

    /* Comes here when a VM tries to execute a supervisor instruction. */

    // uint32 op       = 0;
    // uint32 rd       = 0;
    // uint32 rs1      = 0;
    // uint32 upper    = 0;


    uint64* rd_ptr = retrieve_uvm_register(rd);
    uint64* rs1_ptr = retrieve_uvm_register(rs1);

    struct vm_reg* upper_ptr = get_vm_register(uimm);

    if (op == 0x73) {
        handle_csr_instruction(op, rd_ptr, rs1_ptr, upper_ptr);
    }

}

void trap_and_emulate_ecall(void) {
    struct proc* p = myproc();
    // Save user program counter.
    vm_vs.v_sepc.val = p->trapframe->epc;
    // Set the program counter to the trap handler program_counter_address.
    p->trapframe->epc = vm_vs.v_stvec.val;
    // Set the user mode to supervisor mode.
    vm_vs_current_mode = SUPERVISORMODE;
}

void trap_and_emulate_init(void) {
    /* Create and initialize all state for the VM */

    vm_vs.v_ustatus.code = 0;
    vm_vs.v_ustatus.val  = 0;
    vm_vs.v_ustatus.mode = USERMODE;

    tmp = &(vm_vs.v_uie);
    for (int i = 4; i < 6; i++) {
        tmp->code = i;
        tmp->val  = 0;
        tmp->mode = USERMODE;
        tmp++;
    }

    tmp = &(vm_vs.v_uscratch);
    for (int i = 64; i < 69; i++) {
        tmp->code = i;
        tmp->val  = 0;
        tmp->mode = USERMODE;
        tmp++;
    }

    tmp = &(vm_vs.v_sstatus);
    for (int i = 256; i < 263; i++) {
        tmp->code = i;
        tmp->val  = 0;
        tmp->mode = USERMODE;
        tmp++;
    }

    tmp = &(vm_vs.v_sscratch);
    for (int i = 320; i < 325; i++) {
        tmp->code = i;
        tmp->val  = 0;
        tmp->mode = USERMODE;
        tmp++;
    }

    // SATP Register
    tmp = &(vm_vs.v_satp);
    tmp->code = 384;
    tmp->val = 0;
    tmp->mode = SUPERVISORMODE;

    // MI registers
    tmp = &(vm_vs.v_mvendorid);
    for (int i = 3857; i < 3861; i++) {
        tmp->code = i;
        tmp->val  = 0;
        tmp->mode = MACHINEMODE;
        tmp++;
    }

    // MTS Registers
    tmp = &(vm_vs.v_mstatus);
    for (int i = 768; i < 773; i++) {
        tmp->code = i;
        tmp->val  = 0;
        tmp->mode = MACHINEMODE;
        tmp++;
    }

    // MTH Registers
    tmp = &(vm_vs.v_mscratch);
    for (int i = 832; i < 837; i++) {
        tmp->code = i;
        tmp->val  = 0;
        tmp->mode = MACHINEMODE;
        tmp++;
    }

    // Debugging
    vm_vs.v_mhartid.val = 1;

    // VM starts in machine mode
    vm_vs_current_mode = MACHINEMODE;


}