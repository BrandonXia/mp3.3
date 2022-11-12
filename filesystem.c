/*For file system driver. Created by YG*/

#include "filesystem.h"
#include "lib.h"

#define success 0
#define failure -1

// Some globle stuff
bootblock *bootblock_head;      // head of bootblock
indexnode *indexnode_head;
datab *datablock_ptr;
dentry_t *curdentry;


/* filesystem_init()
    Description: initializing the file system.
    Input: modstart. Just assign values to some global pointers
    Output: nothing
    Return: success or failure
*/
int filesystem_init(bootblock *modstart)
{
    bootblock_head = (bootblock*)modstart;
    indexnode_head = (indexnode*)bootblock_head + 1;
    datablock_ptr = (datab*)indexnode_head + bootblock_head->inodes_num;
    curdentry = (dentry_t*)modstart;
    return success;                 // Should always be success as space is quite adequate
}

/* read_dentry_by_name
    Description: find dentry by its name
    Input:  filename fname; 
            pointer to structure dentry_t dentry
    Output: dentry (content copied to the pointer)
    Return: success or failure
    Side affect: nan
*/
int32_t read_dentry_by_name (const uint8_t* fname, dentry_t* mydentry)
{   
    int scan_loop;              // loop to scan through the entries
    // first check null pointer and invalid inputs
    if ((fname == NULL) ) {return failure;}
    // Scans through the directory entries in the “boot block” to find the file name
    for (scan_loop = 0; scan_loop < bootblock_head->dentry_num; scan_loop++)
    {  
        if(strncmp((const int8_t *)fname,(const int8_t *)bootblock_head->dentry[scan_loop].file_name, (uint32_t)strlen(bootblock_head->dentry[scan_loop].file_name))==0)
        {
            read_dentry_by_index (scan_loop, mydentry);     // directly copy the pointer
            return success;
        }
    }
    return failure;     // If cannot succeed after the loop
}

/* read_dentry_by_index
    Description: find dentry by its index
    Input:  index; 
            pointer to structure dentry_t dentry
    Output: dentry (content copied to the pointer)
    Return: success or failure
    Side affect: nan
*/
int32_t read_dentry_by_index (uint32_t index, dentry_t* mydentry)
{
    // first check null pointer
    printf("boot block-> dentry[index].file type is %d\n", bootblock_head->dentry[index].file_type);
    printf("boot block-> dentry[index].inode_index is %d\n", bootblock_head->dentry[index].inode_index);
    if ((index > bootblock_head->dentry_num)) {return failure;}
    // Then copy everything
    mydentry->file_type = bootblock_head->dentry[index].file_type;
    mydentry->inode_index = bootblock_head->dentry[index].inode_index;
    strcpy((int8_t *)mydentry->file_name, (int8_t *)bootblock_head->dentry[index].file_name);
    strcpy((int8_t *)mydentry->reserved, (int8_t *)bootblock_head->dentry[index].reserved);
    printf("%d\n", mydentry->inode_index);
    return success;
}

/*  Read data
    Description: read data given the input
    Input:  inode, to locate the inode;
            offset, into the inode;
            buf,  copy to it
            length: required length of data
    Output: buf, storing the required data
    Return value: length of the copied value
*/
int32_t read_data (uint32_t inode, uint32_t offset, uint8_t* buf, uint32_t length)
{   
    int starting;
    int ending;
    int i;
    int memcpy_len;
    uint32_t buf_offset = 0;    // initialize to 0; will be calculator in the future
    uint32_t data_id;           // id of the target block
    datab* datablock1;          // pointer to each target block in the loop 
    // first check null pointer & bootblock; For exceeding length, correct it by fetching all content after offset
    if (bootblock_head == NULL) {return failure;}
    if (inode > bootblock_head->inodes_num || offset > indexnode_head[inode].file_length) {return failure;}
    if (length + offset > indexnode_head[inode].file_length) {length = indexnode_head[inode].file_length - offset;}
    // Calculate the range of data to be read
    starting = offset/DATA_BLOCK_SIZE;
    ending = (offset + length)/DATA_BLOCK_SIZE;
    // memcpy for each block
    for(i=starting; i<=ending; i++)
    {   
        // First find where it is
        data_id = indexnode_head[inode].data_blocks[i];
        // Decide the length of memcpy by discussing whether the data_block_size exceeds required length
        if (DATA_BLOCK_SIZE > length - buf_offset){memcpy_len = length - buf_offset;}
        else{memcpy_len = DATA_BLOCK_SIZE;}
        datablock1 = datablock_ptr + data_id;
        memcpy(buf+buf_offset, datablock1->datablock, memcpy_len); 
        buf_offset += memcpy_len;
    }
    return buf_offset;
}

/* Open file. 1 on success, -1 on failure*/

int file_open(const char* fname)
{
    // first check valid pointer
    if (fname == NULL) {return failure;}
    // if not null, then check
    return read_dentry_by_name((const uint8_t *)fname, curdentry);
}

/* file_close
    Description: close the files
    Input: nan
    Output: nan
    Return value: always success. Nothing to do.
*/
int file_close(const char* fname)
{
    return success;
}
/* file_read
    Description: read files
    Input: same as read_data
    Output: same as read_data
    Return value: same as read_data
*/
int file_read(const char* fname, uint32_t offset, void* buf, int32_t length)
{
    int result;
    if(!bootblock_head) return failure;  // FS not initialized
    if(read_dentry_by_name((uint8_t*)fname, curdentry) == failure){return failure;}
    result = read_data(curdentry->inode_index, offset, buf, length);
    return result;
}

/* file_write
    Description: write to files
    Input: nan
    Output: nan
    Return value: always failure. Can't write to read-only files
*/
int file_write(int32_t fd, const void* buf, int32_t nbytes)
{
    return failure;
}

/* Open  directory. 1 on success, -1 on failure*/
int directory_open(const char* fname)
{
    // first check valid pointer
    if (fname == NULL) {return failure;}
    // if not null, then check
    if(read_dentry_by_name((const uint8_t *)fname, curdentry)) {return failure;}
    return success;
}

/* directory_close
    Description: close the directory
    Input: nan
    Output: nan
    Return value: always success. Nothing to do.
*/
int directory_close(const char* fname)
{
    return success;
}

/* directory_read
    Description: read directory
    Input: offset, buf, length
    Output: stored in buf
    Return value: size of copied
*/
int directory_read(const char* fname,uint32_t offset, void* buf, int32_t length)
{
    if (buf == NULL){return failure;}
    if(length > FILE_NAME_LENGTH) length = FILE_NAME_LENGTH;
    if(length > strlen(curdentry->file_name)) {length = strlen(curdentry->file_name);}
    read_dentry_by_name((const uint8_t *)fname, curdentry);
    memcpy((char*) buf, (char*) curdentry->file_name, length);
    return length;
}

/* directory_write
    Description: write to directories
    Input: nan
    Output: nan
    Return value: always failure. Can't write to read-only files
*/
int directory_write(int32_t fd, const void* buf, int32_t nbytes)
{
    return failure;
}


// /* Below are the test helping functions*/
// /* list_all_file (for cp2)
//     Description: list information of all files
//     Input: nan
//     Output: nan
//     Return value: nan
// */
// void list_all_file()
// {
//     clean_screen();
//     int i;
//     for (i = 0; i < bootblock_head->dentry_num; i++)
//     {   
//         bootblock_head->dentry[i].file_name[FILE_NAME_LENGTH] = '\0';
//         printf("%d: ",i);
//         printf("File name: ");
//         printf("%s ", bootblock_head->dentry[i].file_name);
//         printf("File type: ");
//         printf("%d ", bootblock_head->dentry[i].file_type);
//         printf("file_size: ");
//         printf("%d \n", indexnode_head[bootblock_head->dentry[i].inode_index].file_length);
//     }
// }
// /* read test helper (for cp2)
//     Description: help test reading
//     Input: target file
//     Output: printing somestuff
//     Return value: nan
// */
// void read_test_help(int filechosen)
// {
//     int retval;
//     uint8_t *buf;
//     int i;
//     retval = file_open(bootblock_head->dentry[filechosen].file_name);
//     retval = file_read(0, buf, indexnode_head[bootblock_head->dentry[filechosen].inode_index].file_length); 
//     for(i = 0; i < retval; i++)
//     {
//         if (buf[i] != '\0')
//         putc(buf[i]);
//     }
// }

