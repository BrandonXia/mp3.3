/* process.c - Functions to initializing page
 * 
 */

#include "lib.h"
#include "syscall.h"
#include "RTC.h"
#include "paging.h"
#include "terminal.h"
#include "x86_desc.h"
#include "process.h"
#include "filesystem.h"
#include "fileoptable.h"
/* 
 * execute
 *   DESCRIPTION: initializing the page, initialize all PTE and PDE in page directory
                  and page tables
 *   INPUTS: none
 *   OUTPUTS: none
 *   RETURN VALUE: void 
 *   SIDE EFFECTS: none
 */

#define SUCCESS 0
#define FAIL -1

uint8_t pid_check[MAX_PROCESS_NUMBER]= {0,0,0,0,0,0};
int32_t cur_pid = -1;
int32_t new_pid = -1;
PCB_t* cur_process_ptr = NULL;
fdarray* filed_array = NULL ;

int32_t execute(const uint8_t* command)
{
    int i; // loop variable
    int cmd_length, fcmd_curindex = 0;
    int cmd_curindex = 0, arg_start = 0;
    int empty_check = 0;
    uint32_t pd_addr = 0;
    uint32_t pd_index = 0;
    uint8_t file_command[FILE_NAME_LENGTH];
    uint8_t arg_command[FILE_NAME_LENGTH];

    dentry_t check_dentry;
    uint8_t data_buf[sizeof(int32_t)];

    cli();
    // 1 - parse argument
    cmd_length = strlen((int8_t*)command);

    if(command == NULL)
    {
        printf("No command now \n");
        return FAIL;
    }

    while(command[cmd_curindex] == ' ')
    {
        cmd_curindex ++;
    }

    for(i=cmd_curindex;i<cmd_length;i++)
    {
        if(command[cmd_curindex] != ' ' )
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

    file_command[fcmd_curindex] = '\0'; 
    
    arg_start = cmd_curindex + fcmd_curindex;
    
    while(command[arg_start] == (uint8_t)' ')
    {
        arg_start ++;
    }

    i = 0;

    while(arg_start < cmd_length)
    {
        if(command[arg_start] != ' ' )
        {
            arg_command[i] = command[arg_start + i];
            i++;
            continue;
        }
        else
        {
            arg_command[i] = '\0';
            break;
        }
    }
    
    // 2 - check file validity
    if(read_dentry_by_name(file_command, &check_dentry)==-1)
    {
        return -1; 
    } 

    if(read_data(check_dentry.inode_index, 0, data_buf, sizeof(int32_t)) == -1) //need check
    {
        return -1;
    }

    if((data_buf[0] != EXE_CHECK1) || (data_buf[1] != EXE_CHECK2) || (data_buf[2] != EXE_CHECK3) || (data_buf[3] != EXE_CHECK4))
    {
        return -1;
    }

    // find current pit
    // Process allocate
    for(i=0;i<MAX_PROCESS_NUMBER;i++)
    {
        if(pid_check[i] == 0)
        {
            pid_check[i] = 1;
            new_pid = i;
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
    pd_addr = 2 + new_pid ; // start from 8MB + current pid
    DT[pd_index] = 0x00000000;
    DT[pd_index] = DT[pd_index] | PD_MASK;
    DT[pd_index] = DT[pd_index] | (pd_addr << 22);
    tlb_flash();

    // 4 - load file into memory
    read_data(check_dentry.inode_index, 0, (uint8_t*)USER_PADDR, USER_STACK_SIZE);
    // 5 - create PCBs  
    if(initialize_PCB((int8_t*) arg_command) == FAIL)
        return FAIL;

    // 6 - prepare for context switch
    tss.esp0 = cur_process_ptr -> tss_esp0 ;
    tss.ss0 = KERNEL_DS ;
    /*store info defined in x86_desc.h into the prepared varible for later use*/
    uint32_t XSS = USER_DS;
    uint32_t ESP = USER_MEM + USER_STACK_SIZE - 4 ;
    uint32_t XCS = USER_CS ; // 4 is the stack_fence, USER_STACK_SIZE is 8kb
    uint8_t* eiptr =  (uint8_t*) (USER_PADDR + 24) ;  // 24 is the offset for EIP
    uint32_t EIP = *(uint32_t*) eiptr;
    sti();


    // // /* context switch*/
    // // "movw  %%ax, %%ds;"
    //     // "pushl %%eax;"
    //     // "pushl %%ebx;" s
    //     // "pushfl  ;"
    //     // "pushl %%ecx;"
    //     // "pushl %%edx;"
    //     // "IRET"
    //     // :
    //     // : "a"(USER_DS), "b"(uESP), "c"(USER_CS), "d"(eiptr)
    //     // : "cc", "memory"
    //     // 4 is for shift 1 memory location
    //     // which is USER_DS
    //     // which is user_stack = USER_MEM + USER_STACK_SIZE - 4
    //     /* push flags */
    //     /*push if_falg*/
    //     // which is if_flag
    //     /*push user_cs*/
    //     /* push EIP, entrypoint */
    //     // "movl 4(%esp), %ebx;" 
    //     // "xorl %edx, %edx;"
    //     // "movw $0x2B, %dx;"
    //     // "pushl %edx;"
    //     // "movl $0x83ffffc, %edx;" 
    //     // "pushl %edx;"
    //     // "pushfl;"
    //     // "xorl %edx, %edx;"
    //     // "popl  %edx;"
    //     // "orl $0x200, %edx;" 
    //     // "pushl %edx;"
    //     // "xorl %edx, %edx;"
    //     // "movw $0x23, %dx;"
    //     // "pushl %edx;"
    //     // "pushl %ebx;"
    //     // "iret"
    asm volatile(
        "movw  %%ax, %%ds;"
        "pushl %%eax;"
        "pushl %%ebx;"
        "pushfl  ;"
        "pushl %%ecx;"
        "pushl %%edx;"
        "IRET"
        :
        : "a"(XSS), "b"(ESP), "c"(XCS), "d"(EIP)
        : "cc", "memory"
    );
    // // actually never here
        
        return SUCCESS;
}



int8_t initialize_PCB(int8_t* arg_command)
{
    /* allocate space in kernel space for the new process control block */
    PCB_t* cur_prc = (PCB_t*)(END_OF_KERNEL - KERNEL_STACK_SIZE * (new_pid + 1));
    cur_prc->pid = new_pid ;
    cur_prc->parent_pid = cur_pid;
    int count;
    // cur_prc->mapped_video_addr = 0 ;
    // update current pointer and pid in the global variable
    cur_process_ptr = cur_prc;
    cur_pid = cur_prc->pid ;
    

    cur_prc->tss_esp0 = (uint32_t)cur_prc + KERNEL_STACK_SIZE - 4 ;

    /* initialization for file descriptor array*/
    if (cur_prc->fd_array == NULL)
    {
        printf("Wrong filed_array pointer when initialize PCB!\n");
        cur_pid = -1;
        execute((uint8_t*)"shell");
    }
    
    filed_array = cur_prc->fd_array ;
    filed_array[0].fileot_pointer = &file_operation_table[3];
    filed_array[0].inode = NULL;
    filed_array[0].flags = 1;
    filed_array[0].position = 0;
    filed_array[1].fileot_pointer = &file_operation_table[3];
    filed_array[1].inode = NULL;
    filed_array[1].flags = 1;
    filed_array[1].position = 0;
    
    for ( count =2 ; count<8 ; count++ ) /* 8 stands for max fdarray size*/
    {
        filed_array[count].flags = 0;        
    }

    /* switch old esp and old ebp */
    int32_t old_pid ;
    PCB_t* old_process_ptr ;
    if(cur_prc->parent_pid != -1){
        old_pid = cur_prc->parent_pid;
        old_process_ptr = (PCB_t*)(END_OF_KERNEL - KERNEL_STACK_SIZE * (old_pid + 1));
        uint32_t esp, ebp;
        asm volatile(
            "movl %%esp, %0 ;"
            "movl %%ebp, %1 ;"
            : "=r" (esp) ,"=r" (ebp)
        );
        old_process_ptr->scheduled_esp = esp;
        old_process_ptr->scheduled_ebp = ebp;
    }
    return SUCCESS;
}



int32_t halt(uint8_t status)
{
    cli();
    int i;
     /* free current pid */
    pid_check[cur_pid] = 0;
    printf(" enter halt !\n");
    /*get parent pid and parent process ptr*/
    uint32_t parentpid ;
    parentpid = cur_process_ptr->parent_pid ;
    PCB_t* parent_process_ptr = (PCB_t*)(END_OF_KERNEL - KERNEL_STACK_SIZE * (parentpid+1));
    /* check for shell process */
    if( cur_process_ptr->parent_pid == -1 ){
        printf("can't halt from this process\n");
        cur_pid = -1;
        execute((uint8_t*)"shell");
    }

    /* set tss ss0 and esp0 by parent process */
    tss.ss0 = KERNEL_DS;
    tss.esp0 = parent_process_ptr->tss_esp0 ;

    /* set up user paging for parent process */
    int pd_index = USER_MEM >> 22;
    int pd_addr = 2 + parentpid ;
    DT[pd_index] = 0x00000000;
    DT[pd_index] = DT[pd_index] | PD_MASK;
    DT[pd_index] = DT[pd_index] | (pd_addr << 22);
    tlb_flash();

   /* free the file descriptor array here */
    for (i=2;i<8;i++)
    {
        close(i);
    }
    filed_array[0].flags = 0;
    close(0);
    filed_array[1].flags = 0;
    close(1);

    /* set file descriptor array to the parent filed_array*/
    if (parent_process_ptr->fd_array == NULL){
        printf("Wrong parent fdarray when halting!\n");
        return FAIL;
    }
    filed_array = (fdarray*)parent_process_ptr->fd_array ;
    
    /* reset the current pid and current process pointer*/
    cur_pid = parentpid ;
    cur_process_ptr = parent_process_ptr ;

    /* deal with the return value to execute */
    int32_t esp = cur_process_ptr->scheduled_esp;
    int32_t ebp = cur_process_ptr->scheduled_ebp;
    uint16_t result;
    if ( status == EXCEPTION_STATUS )
        result = 256 ; /* as the exception return value */
    else
        result = (uint16_t)status;

    asm volatile(
        "movl %0, %%esp ;"
        "movl %1, %%ebp ;"
        "xorl %%eax,%%eax;"
        "movw %2, %%ax ;"
        "leave;"
        "ret;"
        : 
        : "r" (esp), "r" (ebp), "r"(result)
        : "esp", "ebp", "eax"
    );

    return SUCCESS; 
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
 *  int32_t open (const uint8_t* filename)
 *  DESCRIPTION: insert one new file          
 *  INPUTS: the file name
 *  OUTPUTS: file array filled with entry
 *  RETURN VALUE: -1 for failure.
 */
int32_t open (const uint8_t* filename){
    int counter = -1;
    int loop_through_fd;
    dentry_t *current;

    // check validity
    if(filename == NULL || filed_array == NULL || -1 == read_dentry_by_name(filename, current)) {return -1;}

    // check where should we insert the new file
    for(loop_through_fd = 0; loop_through_fd < FDARRAY_MAX; loop_through_fd++)
    {
        if(filed_array[loop_through_fd].flags == 0)
        {   
            counter = loop_through_fd;
            filed_array[counter].fileot_pointer = &file_operation_table[current->file_type];
            filed_array[counter].position = 0;
            filed_array[counter].flags = 1;
            // set -1 for rtc
            if (current->file_type == 0)
            {
                filed_array[counter].inode = -1;
            }
            else
            {
                filed_array[counter].inode = current->inode_index;
            }
            if(filed_array[counter].fileot_pointer->open((char *)filename)==-1) {return -1;}
            return counter;
        }
    }
    return -1;
}

/*
 *  int32_t open (const uint8_t* filename)
 *  DESCRIPTION: insert one new file          
 *  INPUTS: the file name
 *  OUTPUTS: file array filled with entry
 *  RETURN VALUE: -1 for failure.
 */
int32_t close(int i)
{
    int loop_through_fd;
    int loop_through_dentry;
    dentry_t *current;
    char *filename;
    // check validity
    if(filed_array == NULL) {return -1;}
    for(loop_through_fd = 0; loop_through_fd < FDARRAY_MAX; loop_through_fd++)
    {
        for(loop_through_dentry = 0; loop_through_dentry < 63; loop_through_dentry++)
        {
            read_dentry_by_index(loop_through_dentry, current);
            if (filed_array[loop_through_fd].inode == current->inode_index)
            {
                filename = (char*) &current->file_name;
            }
        }
    }
    return filed_array[loop_through_fd].fileot_pointer->close(filename);
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

    int length;

    if(buf_address_start < USER_MEM){  return -1; }

    if(buf_address_end > USER_END){ return -1; } // #define USER_END 0x8400000

    if(offset >= 7 || offset < 0 || offset == 1){ return -1; } // 7 is the size of file array

    if(filed_array[offset].flags == 0){ return -1; }

    /* check if the ptr supports read*/

    int32_t ifread;
    const void *filename;
    ifread = (int32_t)filed_array[offset].fileot_pointer->read(filename,offset,buf,n);

    if(ifread == 0){ return -1; } //doesn't support read function

    /* now we can read file by the position into the buf*/

    dentry_t tem_dentry;
    dentry_t* tem = &tem_dentry;

    int32_t tem_inode = filed_array[offset].inode;

    char *tem_name = NULL;

    for(length = 0; length < 63; length ++)
    {
        read_dentry_by_index(length, tem);
        if (tem->inode_index == tem_inode)
        {
            tem_name = (char*) &tem->file_name;
            break;
        }  
    }
    int i;
    putc(tem_name[0]);
    printf("strlen is %d\n", strlen(tem_name));
    for(i=0;i<strlen(tem_name);i++)
    {
        //printf("%c", tem_name[i]);
        printf("%d\n", i);
        putc(tem_name[i]);
    }
    // 63 is the max number of the file list
    return filed_array[offset].fileot_pointer->read(tem_name, filed_array[offset].position, buf, n); // file system here
}

 

// /*

//  *  int32_t read(int32_t offset, void* buf, int32_t n)

//  *  DESCRIPTION: call the write function        

//  *  INPUTS: int32_t offset  - the index for the file array

//  *          void* buf  - the buffer to store the data read

//  *          int32_t n - the number of how many bytes need to write

//  *  OUTPUTS: write according  to the file

//  *  RETURN VALUE: -1 if fail, the corresponding offset if success

//  */

 

int32_t write(uint32_t offset, void* buf, int32_t n){

    /* check if the arguments is valid*/

    /* almost same thing as the read function*/

    int buf_address_start = (int) buf ;

    int buf_address_end = buf_address_start + n;

    if(buf_address_start < USER_MEM){  return -1; }

    if(buf_address_end > USER_END){ return -1; } // #define USER_END 0x8400000

    if(offset >= 7 || offset < 0 || offset == 0){ return -1; } // 7 is the size of file array

    if(filed_array[offset].flags == 0){ return -1; }

    /* check if the ptr supports write*/

    int32_t ifwrite;

    ifwrite = (int32_t)filed_array[offset].fileot_pointer->write;

    if(ifwrite == 0){ return -1; } //doesn't support write function

    /* now we can read file by the position into the buf*/
    return filed_array[offset].fileot_pointer->write(offset,buf,n); // file system here

}
