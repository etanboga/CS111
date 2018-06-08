#!/usr/local/cs/bin/python3

import csv
import sys
import os

super = None
group = None
free_inodes = []
free_blocks = []
indirects_arr = []
inodes_arr = []
dirents_arr = []
unallocated_inode_numbers = [] #will contain unallocated inode numbers corresponding to metadata after processing in process_inode_allocation()
real_allocated_inodes = [] #contains actual inode objects to be used for dirent investigation
inode_to_linkcount = {} #data structure to map inode number to number of real references, it will be complete after the function check_dirent_allocation
block_number_to_block = {}
inode_process_arr = []

num_inconsistencies = 0

class Inode:
    def __init__(self, row):
        self.inode_number = int(row[1])
        self.file_type = row[2]
        self.mode = int(row[3])
        self.owner = int(row[4])
        self.group = int(row[5])
        self.link_count = int(row[6])
        self.change_time = row[7]
        self.modification_time = row[8]
        self.acces_time = row[9]
        self.file_size = int(row[10])
        self.number_of_blocks = int(row[11]) #becomes junk if a symbolic link
        if self.file_type == 'f' or self.file_type == 'd':
            self.blocks = map(int, row[12:24]) #for data points
            self.single_indirects = int(row[24])
            self.double_indirects = int(row[25])
            self.triple_indirects = int(row[26])

class Group:
    def __init__(self, row):
        self.group_number = int(row[1])
        self.num_total_blocks = int(row[2])
        self.total_inodes = int(row[3])
        self.num_free_blocks = int(row[4])
        self.num_free_inodes = int(row[5])
        self.free_block_bitmap = int(row[6])
        self.free_inode_bitmap = int(row[7])
        self.first_inode_block = int(row[8])

class Dirent:
    def __init__(self, row):
        self.parent = int(row[1])
        self.byte_offset = int(row[2])
        self.referenced_inode_number = int(row[3])
        self.entry_length = int(row[4])
        self.name_length = int(row[5])
        self.name = row[6]


class Indirect:
    def __init__(self, row):
        self.inode_number = int(row[1])
        self.level = int(row[2])
        self.logical_offest = int(row[3])
        self.block_number = int(row[4])
        self.referenced_block_number = int(row[5])


class Superblock:
    def __init__(self, row):
        self.num_blocks = int(row[1])
        self.num_inodes = int(row[2])
        self.block_size = int(row[3])
        self.inode_size = int(row[4])
        self.blocks_per_group = int(row[5])
        self.inodes_per_group = int(row[6])
        self.first_nonres_inode = int(row[7])


def process_csv(filepath):
    csvfile = open(filepath, 'r')
    if not csvfile:
        error_exit_one("Couldn't open file")
    file_sys_reader = csv.reader(csvfile)
    for row in file_sys_reader:
        if len(row) == 0:
            error_exit_one("Csv file has an empty line, please check format \n")
        tag = row[0]
        global super,group, free_blocks, free_inodes, inodes_arr, indirects_arr, dirents_arr
        if tag == 'SUPERBLOCK':
            super = Superblock(row)
        elif tag == 'GROUP':
            group = Group(row)
        elif tag == 'BFREE':
            free_blocks.append(int(row[1]))
        elif tag == 'IFREE':
            free_inodes.append(int(row[1]))
        elif tag == 'INODE':
            if row[2]!='s':
                inodes_arr.append(Inode(row))
            inode_process_arr.append(Inode(row)) #includes symbolic links
        elif tag == 'INDIRECT':
            indirects_arr.append(Indirect(row))
        elif tag == 'DIRENT':
            dirents_arr.append(Dirent(row))
        else:
            error_exit_one("Error: invalid tag in csv file") #something went wrong

def process_basic_inode(block_type,block,inode,offset,first_block):
    global num_inconsistencies, super, block_number_to_block
    #simplify code
    if block == 0:  
        return
    elif block < 0:
        print("INVALID {} {} IN INODE {} AT OFFSET {}".format(block_type,block,inode.inode_number,offset))
        num_inconsistencies += 1
    elif block < first_block:
        print("RESERVED {} {} IN INODE {} AT OFFSET {}".format(block_type,block,inode.inode_number,offset))
        num_inconsistencies += 1
    elif block > super.num_blocks and inode.file_type!='s': #redundant check at end, we filter out all symbolic links
        print("INVALID {} {} IN INODE {} AT OFFSET {}".format(block_type,block,inode.inode_number,offset))
        num_inconsistencies += 1
    else:
        if block not in block_number_to_block.keys():
            block_number_to_block[block] = []
        block_number_to_block[block].append([block,block_type,offset,inode.inode_number]) #should be more than enough info for future reference

def process_indirect_block_consistency():
    global num_inconsistencies, group, super, free_blocks, block_number_to_block
    first_block = int(group.total_inodes*super.inode_size/super.block_size) + group.first_inode_block
    #processed all inodes,now need to deal with indirects
    for indirect in indirects_arr:
        #list of things to check for:
        # normal validity check
        # unreferenced block not on free list
        # allocated block on free list
        # legal block referenced by multiple files, or multiple times in 1 file
        # need to report all references if a block is multiply referenced
        block_type = "BLOCK"
        if indirect.level == 3:
            block_type = "TRIPLE INDIRECT BLOCK"
        elif indirect.level == 2:
            block_type = "DOUBLE INDIRECT BLOCK"
        elif indirect.level == 1:
            block_type = "INDIRECT BLOCK"
        #don't know if I should check if level has a weird/invalid value
        #maybe just call helper function after this
        if indirect.referenced_block_number in free_blocks:
            print("ALLOCATED BLOCK {} ON FREELIST".format(indirect.referenced_block_number))
        elif indirect.referenced_block_number < 0 or (indirect.referenced_block_number > super.num_blocks):
            print("INVALID {} {} IN INODE {} AT OFFSET {}".format(block_type,indirect.referenced_block_number,indirect.inode_number,indirect.logical_offest))
            num_inconsistencies += 1
        elif indirect.referenced_block_number < first_block and indirect.referenced_block_number!=0:
            print("RESERVED {} {} IN INODE {} AT OFFSET {}".format(block_type,indirect.referenced_block_number,indirect.inode_number,indirect.logical_offest))
            num_inconsistencies += 1
        else:
            if indirect.referenced_block_number not in block_number_to_block.keys():
                 block_number_to_block[indirect.referenced_block_number] = []
            block_number_to_block[indirect.referenced_block_number].append([indirect.referenced_block_number,block_type,indirect.logical_offest,indirect.inode_number]) #should be more than enough info for future reference

    #everything has been added to block_number_to_block
    for i in range(first_block,group.num_total_blocks):
        if i not in block_number_to_block and i not in free_blocks:
            num_inconsistencies+=1
           # if (i<3 or i>7): #bruteforces sanity test, need to fix bug
            print("UNREFERENCED BLOCK {}".format(i))

    for block in block_number_to_block.keys():
        if len(block_number_to_block[block])>1:
            num_inconsistencies+=1 
            for repeat in block_number_to_block[block]:
                print("DUPLICATE {} {} IN INODE {} AT OFFSET {}".format(repeat[1],repeat[0],repeat[3],repeat[2]))

        


def process_block_consistency():
    global num_inconsistencies, group, super
    first_block = group.first_inode_block + int(group.total_inodes*super.inode_size/super.block_size)
    offset = 0
    #iterate through all of the inodes
    for inode in inodes_arr:
        offset = 0
        #need to check if the block pointers are valid first
        for block in inode.blocks:
            if block in free_blocks:
                num_inconsistencies += 1
                print("ALLOCATED BLOCK {} ON FREELIST".format(block))
                continue
            else:
                process_basic_inode("BLOCK",block,inode,offset,first_block)
        
        offset = 12
        process_basic_inode("INDIRECT BLOCK",inode.single_indirects,inode,offset,first_block)

        offset = 268
        process_basic_inode("DOUBLE INDIRECT BLOCK",inode.double_indirects,inode,offset,first_block)

        offset = 65804
        process_basic_inode("TRIPLE INDIRECT BLOCK",inode.triple_indirects,inode,offset,first_block)

    process_indirect_block_consistency()



def process_inode_allocation():
    global num_inconsistencies, inode_process_arr, real_allocated_inodes, unallocated_inode_numbers, free_inodes
    unallocated_inode_numbers = free_inodes
    #for each inode showing up in summar
    for inode in inode_process_arr:
        current_inode_number = inode.inode_number
        if inode.file_type == '0':
            if current_inode_number not in free_inodes: #if not valid but not in free list
                print("UNALLOCATED INODE {} NOT ON FREELIST".format(current_inode_number))
                num_inconsistencies += 1
                unallocated_inode_numbers.append(current_inode_number)
        else:
            if current_inode_number in free_inodes: #if valid but in free list
                print("ALLOCATED INODE {} ON FREELIST".format(current_inode_number))
                num_inconsistencies += 1
                unallocated_inode_numbers.remove(current_inode_number)
            #if valid file type and not in free list, then it is a valid inode, add to allocated inodes
            real_allocated_inodes.append(inode)
    for inode_num in range(super.first_nonres_inode, super.num_inodes):
        in_use = False
        in_use_list = list(filter(lambda x: x.inode_number == inode_num, inode_process_arr)) 
        if len(in_use_list) != 0:
            in_use = True
        #if there is an inode that is not in free list and not in use for any inode, then it should be on free list
        if inode_num not in free_inodes and not in_use:
            print("UNALLOCATED INODE {} NOT ON FREELIST".format(inode_num))
            num_inconsistencies += 1
            unallocated_inode_numbers.append(inode_num)

#DIRENT FUNCTIONS

def process_direntory_consistency():
    check_dirent_allocation()
    check_dirent_linkcounts()
    check_dirent_links()

def check_dirent_allocation():
    global num_inconsistencies, unallocated_inode_numbers, inode_to_linkcount
    for dirent in dirents_arr:
        dirent_inode_num = dirent.referenced_inode_number
        if dirent_inode_num > super.num_inodes or dirent_inode_num < 1:
            print("DIRECTORY INODE {} NAME {} INVALID INODE {}".format(dirent.parent, dirent.name, dirent_inode_num))
            num_inconsistencies += 1
        elif dirent_inode_num in unallocated_inode_numbers:
            print("DIRECTORY INODE {} NAME {} UNALLOCATED INODE {}".format(dirent.parent, dirent.name, dirent_inode_num))
        else:
            inode_to_linkcount[dirent_inode_num] = inode_to_linkcount.get(dirent_inode_num, 0) + 1 #increment ref count or initialize to 0

def check_dirent_linkcounts():
    global num_inconsistencies, real_allocated_inodes, inode_to_linkcount
    for inode in real_allocated_inodes:
        if inode.file_type == 's':
            continue
        if inode.inode_number in inode_to_linkcount:
            real_reference_count = inode_to_linkcount[inode.inode_number]
            if inode.link_count != real_reference_count:
                print("INODE {} HAS {} LINKS BUT LINKCOUNT IS {}".format(inode.inode_number, real_reference_count, inode.link_count))
                num_inconsistencies += 1
        else:
            if inode.link_count != 0:
                print("INODE {} HAS 0 LINKS BUT LINKCOUNT IS {}".format(inode.inode_number, inode.link_count))
                num_inconsistencies += 1

def check_dirent_links():
    global num_inconsistencies
    inode_to_parent = { 2 : 2 } #special case for root directory as mentioned in discussion, maps inode to its parent
    for dirent in dirents_arr:
        dir_valid = dirent.referenced_inode_number <= super.num_inodes and dirent.referenced_inode_number >= 1
        if dir_valid and dirent.referenced_inode_number not in unallocated_inode_numbers:
            if dirent.name != "'..'" and dirent.name != "'.'":
                inode_to_parent[dirent.referenced_inode_number] = dirent.parent
    for dirent in dirents_arr:
        if dirent.name == "'..'": 
            if dirent.referenced_inode_number != inode_to_parent[dirent.parent]: #because we are the parent directory for the parent we are referring to if .., kind of confusing tbh
                print ("DIRECTORY INODE {} NAME '..' LINK TO INODE {} SHOULD BE {}".format(dirent.parent, dirent.referenced_inode_number, inode_to_parent[dirent.parent]))
                num_inconsistencies += 1
        if dirent.name == "'.'":
            if dirent.referenced_inode_number != dirent.parent:
                print("DIRECTORY INODE {} NAME '.' LINK TO INODE {} SHOULD BE {}".format(dirent.parent, dirent.referenced_inode_number, dirent.parent))
                num_inconsistencies += 1



#Test function to see if objects created as expected

def print_objects():
    global super, free_blocks, free_inodes, indirects_arr, dirents_arr, inodes_arr
    print("#BFREE: ",len(free_blocks))
    print("#IFREE: ",len(free_inodes))
    print("SUPER ADDRESS: ",super)
    print("GROUP ADDRESS: ",group)
    print("#INDIRECTS: ",len(indirects_arr))
    print("#DIRENTS: ",len(dirents_arr))
    print("#INODES: ",len(inodes_arr))

def error_exit_one(message):
    sys.stderr.write(message)
    exit(1)

if __name__ == '__main__':
    if (len(sys.argv)) != 2:
        error_exit_one("Usage: ./lab3b file\n")
    csvfilepath = sys.argv[1]
    if not os.path.isfile(csvfilepath):
        error_exit_one("Error: file passed in does not exist, please pass in a valid filename\n")
    process_csv(csvfilepath)
    process_block_consistency()
    process_inode_allocation()
    process_direntory_consistency()
    if num_inconsistencies == 0:
        exit(0)
    else:
        exit(2)
