#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <string.h>
#include <unistd.h>
#include <stdint.h>
#include <time.h>
#include "ext2_fs.h"
#define BASE_OFFSET 1024
#define BLOCK_OFFSET(block) (BASE_OFFSET + (block-1)*block_size)

#define SINGLE_INC 1 //go to next, one after another
#define DOUBLE_INC 256 //256 direct blocks in 1 indirect
#define TRIPLE_INC 65536 //65536 direct blocks for 1 double indirect,

#define SINGLE_START 12 //13th block
#define DOUBLE_START 268 //13-268 are the first single indirect, double indirect starts from here
#define TRIPLE_START 65804 //65536+268 is where triple indirect starts

struct ext2_super_block super;
struct ext2_group_desc group;
struct ext2_inode inode;

static unsigned int block_size = 0;

int file_sys_fd = -1; //to have error if not opened

void print_guidelines_and_exit() {
    printf("Usage: ./lab3a filename \n");
    exit(EXIT_FAILURE);
}

void print_error_and_exit(char* error_message, int error_no) {
    fprintf(stderr, "%s, error = %d, message = %s \n", error_message, error_no, strerror(error_no));
    exit(EXIT_FAILURE);
}

off_t secure_lseek(int fd, off_t offset, int whence) {
    off_t location = lseek(fd, offset, whence);
    if (location < 0) {
        print_error_and_exit("Error during lseek", errno);
    }
    return location;
}

ssize_t secure_read(int fd, void* buf, size_t count) {
    ssize_t bytes_read = read(fd, buf, count);
    if (bytes_read < 0) {
        print_error_and_exit("Error during read", errno);
    }
    return bytes_read;
}


void process_superblock() {
    secure_lseek(file_sys_fd, BASE_OFFSET, SEEK_SET);
    secure_read(file_sys_fd, &super, sizeof(super));
    if (super.s_magic != EXT2_SUPER_MAGIC) {
        print_error_and_exit("Error: not an EXT_2 filesystem", EXIT_FAILURE);
    }
    block_size = 1024 << super.s_log_block_size;
    printf("SUPERBLOCK,%u,%u,%u,%u,%u,%u,%u\n", super.s_blocks_count, super.s_inodes_count, block_size, super.s_inode_size, super.s_blocks_per_group, super.s_inodes_per_group, super.s_first_ino);
}

void process_groups() {
    secure_lseek(file_sys_fd, BASE_OFFSET + block_size, SEEK_SET);
    secure_read(file_sys_fd, &group, sizeof(group));
    uint32_t blocks_per_group = super.s_blocks_per_group;
    uint32_t total_blocks = super.s_blocks_count;
    uint32_t inodes_per_group = super.s_inodes_per_group;
    uint32_t total_inodes = super.s_inodes_count;
    uint32_t blocks_in_group = (total_blocks < blocks_per_group) ? total_blocks : blocks_per_group;
    uint32_t inodes_in_group = (total_inodes < inodes_per_group) ? total_inodes : inodes_per_group;
    printf("GROUP,%u,%u,%u,%u,%u,%u,%u,%u\n", 0, blocks_in_group, inodes_in_group, group.bg_free_blocks_count, group.bg_free_inodes_count, group.bg_block_bitmap, group.bg_inode_bitmap, group.bg_inode_table); //we have one group so no need to iterate through all of them
}

void process_free_blocks() {
    unsigned int i;
    for (i = 0; i < block_size; i++) { //go through each byte
        uint8_t block_byte;
        secure_lseek(file_sys_fd, BLOCK_OFFSET(group.bg_block_bitmap) + i, SEEK_SET);
        secure_read(file_sys_fd, &block_byte, 1); //individual byte
        int bitmask = 1;
        unsigned int j;
        for (j = 0; j < 8; j++) {
            if ((block_byte & bitmask) == 0) { //if free
                printf("BFREE,%d\n", i * 8 + j + 1); //i is the byte number and j is the individual byte, starts from 1
            }
            bitmask = bitmask << 1;
        }
    }
}

void process_free_inodes() { //same logic as processing free blocks, just with a different address
    unsigned int i;
    for (i = 0; i < block_size; i++) {
        uint8_t inode_byte;
        secure_lseek(file_sys_fd, BLOCK_OFFSET(group.bg_inode_bitmap) + i, SEEK_SET);
        secure_read(file_sys_fd, &inode_byte, 1);
        int bitmask = 1;
        unsigned int j;
        for (j = 0; j < 8; j++) {
            if ((inode_byte & bitmask) == 0) {
                printf("IFREE,%d\n", i * 8 + j + 1);
            }
            bitmask = bitmask << 1;
        }
    }
}
//need time of last inode change, modification time and time of last access
void print_time_details_for_inode()
{
    time_t change = inode.i_ctime,modif = inode.i_mtime,lastaccess=inode.i_atime;
    struct tm ctime = *gmtime(&change),mtime= *gmtime(&modif),atime= *gmtime(&lastaccess);
    //(mm/dd/yy hh:mm:ss, GMT)
    //printf("%d/%d/%d %d:%d:%d,",(ctime->tm_mon+1),,,ctime->tm_hour,ctime->tm_min,ctime->tm_sec);
    char time_buffer[50],time_buffer2[60];
    strftime(time_buffer,50,"%m/%d/%y %H:%M:%S",&ctime);
    printf("%s,",time_buffer);
    strftime(time_buffer2,60,"%m/%d/%y %H:%M:%S",&mtime);
    printf("%s,",time_buffer2);
    memset(time_buffer,0,50);
    strftime(time_buffer,50,"%m/%d/%y %H:%M:%S",&atime);
    printf("%s,",time_buffer);
    memset(time_buffer,0,50);
}

void process_directory_entries(int parent)
{
  struct ext2_dir_entry *entry;
  unsigned int i;
  unsigned char block[block_size];
  unsigned int size = 0;

  for(i = 0; i < EXT2_N_BLOCKS; i++)
  {
      //need to read in inode
      if(inode.i_block[i]==0)
        continue;
      
        while(size<inode.i_size)
        { 
            secure_lseek(file_sys_fd,block_size*inode.i_block[i]+size,SEEK_SET);
            secure_read(file_sys_fd, (void *)block, block_size);
            entry = (struct ext2_dir_entry*)block; 
            if(entry->inode)
                printf("DIRENT,%u,%u,%u,%u,%u,'%s'\n",parent,size,entry->inode,entry->rec_len,entry->name_len,entry->name); 
            size = size + entry->rec_len;
        }
      
  }
}

void print_block_addresses() //applies only on files/directories
{
    unsigned int j = 0;
    if(inode.i_links_count!=0 && inode.i_mode!=0)
    {
        for(;j<EXT2_N_BLOCKS;j++)
        {
            if(inode.i_block[j]) //first 12 are direct, etc
            {
                //not null
                printf(",%d",(inode.i_block[j]));
            }
            else
            {
                printf(",0");
            }
        }
    }
}

void indirect_block_12(int parent,int logical_offset, int block)
{
    if(block!=0) //single, can't directly use i_block[12]
    {
        /*
        The 13th entry in this array is the block number of the first indirect block; which is a block containing an array of block ID containing the data. Therefore, the 13th block of the file will be the first block ID contained in the indirect block. With a 1KiB block size, blocks 13 to 268 of the file data are contained in this indirect block.
        */
       //fprintf(stderr,"In indirect_block_12\n");
       
       int buffer[block_size];
       secure_lseek(file_sys_fd,block_size * block,SEEK_SET);
       secure_read(file_sys_fd,(void *)&buffer,block_size);
       unsigned int i;
       for(i=0;i<(block_size/4);i++,logical_offset++)
       {
           if(buffer[i])
           {
               printf("INDIRECT,%u,1,%u,%u,%u\n",parent,logical_offset,block,buffer[i]);
           }
       }

        
    }
}
//index->whether 12,13 or 14, offset=logical block offset
void recursive_indirect_block_13_14(int parent, int index,int logical_offset,int block)
{
   //fprintf(stderr,"In rec with parent=%d,index=%d,block[%d]=%d\n",parent,index,index,inode.i_block[index]);
    if(index<12)
    {
        fprintf(stderr,"Error during recursion.\n");
        exit(2);
    }
    else if(index == 12) //base case
        indirect_block_12(parent,logical_offset,block);
    else if(block) //can't use inode.i_block[index] :(
    {
    int buffer[block_size/4];
    unsigned int i;
    secure_lseek(file_sys_fd,block_size*block,SEEK_SET);
    secure_read(file_sys_fd,(void*)&buffer,block_size);

    for(i=0;i<(block_size/4);i++)
    {
        if(buffer[i])
        {
            int level;
            switch(index)
            {
                case 12: //probably redundant
                    level = 1;
                    break;
                case 13:
                    level = 2;
                    break;
                case 14:
                    level = 3;
                    break;
                default:
                    fprintf(stderr,"Error during recursion.\n");
                    exit(2);
            }
            printf("INDIRECT,%u,%d,%u,%u,%u\n",parent,level,logical_offset,block,buffer[i]);
            if(level == 1)
                indirect_block_12(parent,logical_offset,buffer[i]); //redundant code
            else //need to recursive through another indirect
                recursive_indirect_block_13_14(parent,index-1,logical_offset,buffer[i]);
        }
        else
        {
            //just change offset
            switch(index)
            {
                case 12: //probably redundant
                    logical_offset += SINGLE_INC;
                    break;
                case 13:
                    logical_offset += DOUBLE_INC;
                    break;
                case 14:
                    logical_offset += TRIPLE_INC;
                    break;
                default:
                    fprintf(stderr,"Error during recursion.\n");
                    exit(2);
            }

        }
    }

    
    }
}

void process_indirect_block(int parent)
{
    //15 pointers in i_block, 0-11 are direct, 12 is single indirect, 13 is double indirect, 14 is triple indirect
    //we know the inode
    indirect_block_12(parent,SINGLE_START,inode.i_block[12]); 
    recursive_indirect_block_13_14(parent,13,DOUBLE_START,inode.i_block[13]); //since the block may be changing during call need last arg
    recursive_indirect_block_13_14(parent,14,TRIPLE_START,inode.i_block[14]);

}

void process_inode_summary()
{
  unsigned int i;
//   unsigned int inodes_per_block = block_size / sizeof(struct ext2_inode); //# of inodes in 1 blocks
//   unsigned int itable_block_size = super.s_inodes_per_group/inodes_per_block; //size of 1 block in the inode itable_block_size
//   //remember 15 pointers, 0-11 are direct, 12 is a single indirect, 13 is double, 14 is triple
  //EXT2_N_BLOCKS is usually 15
  for(i = 0; i < super.s_inodes_count; i++)
  {
    secure_lseek(file_sys_fd,(group.bg_inode_table*block_size+i*sizeof(struct ext2_inode)),SEEK_SET);
    secure_read(file_sys_fd, (void *)&inode, sizeof(struct ext2_inode));
    //check if inode is allocated
    if(inode.i_links_count==0 || inode.i_mode==0)
        continue;

    char ftype = '?'; //default
        if(S_ISDIR(inode.i_mode)) //directory
        {
            ftype = 'd';
        }
        else if(S_ISREG(inode.i_mode))
        {
            //a file
            ftype = 'f';
        }
        else if(S_ISLNK(inode.i_mode)) //symbolic link
        {
            ftype = 's';
        }
        printf("INODE,%d,%c,%o,%u,%u,%u,",(i+1),ftype,0xfff&inode.i_mode,inode.i_uid,inode.i_gid,inode.i_links_count);
        //print times
        print_time_details_for_inode();
        //print file size and number of blocks
        printf("%u,%u",inode.i_size,inode.i_blocks);

    if(S_ISDIR(inode.i_mode))
        {
            //block addresses
            print_block_addresses();
            printf("\n");
            //need to process all of the directory entries
            process_directory_entries(i+1);
            process_indirect_block(i+1);
        }
        else if(S_ISREG(inode.i_mode))
        {
            //a file
            //print block addresses 
            print_block_addresses(); 
            printf("\n"); 
            process_indirect_block(i+1);
        }
        else if(S_ISLNK(inode.i_mode))
        {
            /*If the file length is less than the size of the block pointers (60 bytes) the file will contain zero data blocks, and the name 
            is stored in the space normally occupied by the block pointers. If this is the case, the fifteen block pointers need not be printed.
                */
               if(inode.i_blocks==0)
               {
                   printf(",%d",inode.i_block[0]);
               }
               else
                print_block_addresses();
            printf("\n");
        }
    
  }

}


int main(int argc, char *argv[]) {
    char* file_sys_name;
    if (argc != 2) {
        print_guidelines_and_exit();
    }
    file_sys_name = argv[1];
    file_sys_fd = open(file_sys_name, O_RDONLY);
    if (file_sys_fd < 0) {
        print_error_and_exit("Couldn't open img file", errno);
    }
    process_superblock();
    process_groups();
    process_free_blocks();
    process_free_inodes();
    process_inode_summary();
    close(file_sys_fd);
    exit(EXIT_SUCCESS);
}
