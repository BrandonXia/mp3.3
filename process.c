/* process.c - Functions to initializing page
 * 
 */


#include "syscall.h"
#include "rtc.h"
#include "paging.h"
#include "terminal.h"
#include "x86_desc.h"
#include "process.h"
#include "filesystem.h"

/* 
 * execute
 *   DESCRIPTION: initializing the page, initialize all PTE and PDE in page directory
                  and page tables
 *   INPUTS: none
 *   OUTPUTS: none
 *   RETURN VALUE: void 
 *   SIDE EFFECTS: none
 */
typedef struct file_entry_t
{
    fop_t* fileot_pointer;       // pointer to the write, read, open, close function
    int inode;                                  // the file inode number
    int position;                          // file current position, 0 for terminal file and RTC file
    int flags;                                  // 1 for used and 0 for unused
}file_entry_t;

typedef struct fop_t{
    int32_t (*open)     (const uint8_t* filename);
    int32_t (*close)    (int32_t fd);
    int32_t (*read)     (int32_t fd,  uint32_t offset, void* buf, int32_t nbytes);
    int32_t (*write)    (int32_t fd, const void* buf, int32_t nbytes);
} fop_t;

file_entry_t* file_array = 0;

#define SUCCESS = 0;
#define FAIL = -1;

uint8_t pid_check[MAX_PROCESS_NUMBER]= {0,0,0,0,0,0};
int32_t cur_pid = 0;
int32_t new_pid = 0;
process_t* curr_process_ptr = NULL;


int32_t execute(const uint8_t* command)
{
    int i,j; // loop variable
    int cmd_length, fcmd_curindex = 0;
    int cmd_curindex = 0, arg_start = 0, arg_curindex = 0;
    int empty_check = 0;
    uint32_t pd_addr = 0;
    uint32_t pd_index = 0;
    uint8_t file_command[FILE_NAME_LENGTH];
    uint8_t arg_command[FILE_NAME_LENGTH];

    dentry_t check_dentry;
    uint8_t dentry_buf[sizeof(int32_t)];

    cli();
    // 1 - parse argument
    cmd_length = strlen((int8_t*)command);
    if(command == NULL)
    {
        printf("No command now \n")
        return FAIL;
    }
    while(command[cmd_curindex] == " ")
    {
        cmd_curindex ++;
    }
    for(i=cmd_curindex;i<cmd_length;i++)
    {
        if(command[cmd_curindex] != " " )
        {
            file_command[fcmd_curindex] = command[i];
            fcmd_curindex ++;
            continue;
        }
        else
        {
            if(fcmd_curindex != 0)
            {
                break;
            }
        }
    }
    file_command[fcmd_curindex] = "\0"; 
    
    arg_start = cmd_curindex + fcmd_curindex;
    while(command[arg_start] == " ")
    {
        arg_start ++;
    }

    i = 0;

    while(arg_start < cmd_length)
    {
        if(command[arg_start] != " " )
        {
            arg_command[i] = command[arg_start + i];
            i++;
            continue;
        }
        else
        {
            arg_command[i] = "\0";
            break;
        }
    }
    
    // 2 - check file validity
    if(read_dentry_by_name(file_command, &check_dentry)==-1)
    {
        return -1; 
    }

    if(read_data(check_dentry.node_index, 0, dentry_buf, sizeof(int32_t)) == -1) //need check
    {
        return -1;
    }

    if((elf_buf[0] != EXE_CHECK1) || (elf_buf[1] != EXE_CHECK2) || (elf_buf[2] != EXE_CHECK3) || (elf_buf[3] != EXE_CHECK4))
    {
        return -1;
    }

    // find current pit
    for(i=0;i<MAX_PROCESS_NUMBER;i++)
    {
        if(pid_check[i] == 0)
        {
            pid_check[i] = 1;
            cur_pid = i;
            empty_check = 1;
            break;
        }
    }
    if(empty_check == 0)
    {
        printf("No more process avaliable now !\n");
        return FAIL;
    }

    // 3 - set up paging
    pd_index = USER_MEM >> 22; // 22 is to get the offset
    pd_addr = 2 + cur_pid ; // start from 8MB + current pid
    DT[pd_index] = 0x00000000;
    DT[pd_index] = DT[pd_index] | PD_MASK;
    DT[pd_index] = DT[pd_index] | (pd_addr << 22);
    tlb_flash();

    // 4 - load file into memory
    read_data(check_dentry.node_index, 0, (*uint8_t)USER_PADDR, sizeof(int32_t));

    // 5 - create PCBs
    
    // 6 - prepare for context switch
    tss.esp0 = curr_process_ptr -> tss_esp0 ;
    tss.ss0 = KERNEL_DS ;
    /*store info defined in x86_desc.h into the prepared varible for later use*/
    // uint32_t uESP;
    // uESP = USER_MEM + USER_STACK_SIZE - 4 ; // 4 is the stack_fence
    // uint32_t eiptr = *(uint32_t*) ( (uint8_t*) USER_PADDR + 24 )  // 24 is the offset for EIP
    sti();
    /* context switch*/
    asm volatile(
        // "movw  %%ax, %%ds;"
        // "pushl %%eax;"
        // "pushl %%ebx;"
        // "pushfl  ;"
        // "pushl %%ecx;"
        // "pushl %%edx;"
        // "IRET"
        // :
        // : "a"(USER_DS), "b"(uESP), "c"(USER_CS), "d"(eiptr)
        // : "cc", "memory"
        "movl 4(%esp), %ebx;" // 4 is for shift 1 memory location
        "xorl %edx, %edx;"
        "movw $0x002B, %dx;"// which is USER_DS
        "pushl %edx;"
        "movl $0x83ffffc, %edx;" // which is user_stack = USER_MEM + USER_STACK_SIZE - 4
        "pushl %edx;"
        /* push flags */
        "pushfl;"
        /*push if_falg*/
        "xorl %edx, %edx;"
        "popl  %edx;"
        "orl $0x200, %edx;" // which is if_flag
        "pushl %edx;"
        /*push user_cs*/
        "xorl %edx, %edx;"
        "movw $0x0023, %dx;"
        "pushl %edx;"
        /* push EIP, entrypoint */
        "pushl %ebx;"
        "iret"
    );
    // actually never
    return 1;
}

int32_t getargs(uint8_t* buf, int32_t n){     
    return 0;
}

int32_t set_handler(int32_t signum, void* address){
    return -1;
}
int32_t sigreturn(void){
    return -1;
}
int32_t vidmap(uint8_t** start){
    return 0;
}




/*
 *  int32_t read(int32_t offset, void* buf, int32_t n)
 *  DESCRIPTION: call the read function         
 *  INPUTS: int32_t offset  - the index for the file array
 *          void* buf  - the buffer to store the data read
 *          int32_t n - the number of how many bytes need to read
 *  OUTPUTS: read according  to the file
 *  RETURN VALUE: -1 if fail, the corresponding offset if success
 */

int32_t read(uint32_t offset, void* buf, int32_t n){
    /* check if the arguments is valid*/
    int buf_address_start = (int) buf ;
    int buf_address_end = buf_address_start + n;
    if(buf_address_start < USER_MEM){  return -1; }
    if(buf_address_end > USER_END){ return -1; } // #define USER_END 0x8400000
    if(offset >= 7 || offset < 0 || offset == 1){ return -1; } // 7 is the size of file array
    if(file_array[offset].flags == 0){ return -1; }
    /* check if the ptr supports read*/
    int32_t ifread;
    ifread = file_array[offset].fileot_pointer->read;
    if(ifread == 0){ return -1; } //doesn't support read function
    /* now we can read file by the position into the buf*/ 
    dentry_t* tem_dentry;
    int32_t tem_inode = file_array[offset].inode;
    uint8_t tem_name;
    for(int length = 0; length < 63; length ++)
    {
        read_dentry_by_index(length, tem_dentry);
        if (tem_dentry->inode_index == tem_inode)
        {
            tem_name = tem_dentry->file_name;
        }   
    } 
    // 63 is the max number of the file list
    return file_array[offset].fileot_pointer->read(tem_name, file_array[offset].position, buf, n); // file system here
}

/*
 *  int32_t read(int32_t offset, void* buf, int32_t n)
 *  DESCRIPTION: call the write function         
 *  INPUTS: int32_t offset  - the index for the file array
 *          void* buf  - the buffer to store the data read
 *          int32_t n - the number of how many bytes need to write
 *  OUTPUTS: write according  to the file
 *  RETURN VALUE: -1 if fail, the corresponding offset if success
 */

int32_t write(uint32_t offset, void* buf, int32_t n){
    /* check if the arguments is valid*/
    /* almost same thing as the read function*/
    int buf_address_start = (int) buf ;
    int buf_address_end = buf_address_start + n;
    if(buf_address_start < USER_MEM){  return -1; }
    if(buf_address_end > USER_END){ return -1; } // #define USER_END 0x8400000
    if(offset >= 7 || offset < 0 || offset == 0){ return -1; } // 7 is the size of file array
    if(file_array[offset].flags == 0){ return -1; }
    /* check if the ptr supports write*/
    int32_t ifwrite;
    ifwrite = file_array[offset].fileot_pointer->write;
    if(ifwrite == 0){ return -1; } //doesn't support write function
    /* now we can read file by the position into the buf*/
    return file_array[offset].fileot_pointer->write(offset,buf, n); // file system here
}
uint32_t tssesp0;
uint16_t tssss0;


