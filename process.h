#ifndef _PROCESS_H
#define _PROCESS_H

#include "types.h"
#include "x86_desc.h"
#include "filesystem.h"
#include "paging.h"
#include "terminal.h"
#include "syscall.h"

// variables we need
MAX_PROCESS_NUMBER = 6;
MAX_BUFFER_SIZE = 128;
USER_PADDR = 0X08048000;
KERNEL_STACK_SIZE = 0X2000;
END_OF_KERNEL = 0X00800000;
USER_STACK_SIZE = 0X00400000;
EXE_CHECK1 = 0x7F;
EXE_CHECK2 = 0x45;
EXE_CHECK3 = 0x4C;
EXE_CHECK4 = 0x46;
PD_MASK =  0x87;
#define USER_END 0x8400000
#define USER_MEM 0x08000000

typedef strcut{
    fdarray open_file[8];
    uint32_t pid;
    uint32_t parent_pid;
    uint32_t parent_ebp;
    uint32_t parent_tss_esp0;
    uint32_t scheduled_esp;
    uint32_t scheduled_ebp;
    uint32_t scheduled_esp0;        
    uint32_t current_fd;            
    uint32_t mapped_video_addr;  


    uint32_t tss_esp0;
       
} PCB_t;

int32_t execute(const uint8_t* command);

int32_t sigreturn(void);
int32_t set_handler(int32_t signum, void* address);
int32_t getargs(uint8_t* buf, int32_t n)
int32_t vidmap(uint8_t** start)


#endif 

