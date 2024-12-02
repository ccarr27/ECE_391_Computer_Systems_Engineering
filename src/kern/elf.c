//           elf.c - ELF executable loader

#include "elf.h"
#include "io.h"
#include "config.h"


#include <stddef.h>
#include <stdint.h>
#include <string.h>     //need for memset


//define ELFMAG 0x464C7F     //this is x7FELF but flipped becuase little endian and 0x7F is in ELFMAG0

#define	ELFMAG0		0x7f		//EI_MAG
#define	ELFMAG1		'E'
#define	ELFMAG2		'L'
#define	ELFMAG3		'F'

/* These constants define the permissions on sections in the program
   header, p_flags. */
#define PF_R		0x4
#define PF_W		0x2
#define PF_X		0x1

//e_ident array item identifiers
#define EI_MAG0     0
#define EI_MAG1     1
#define EI_MAG2     2
#define EI_MAG3     3
#define EI_CLASS    4       //is what tells us if it is 32 bit or 64 bit
#define	EI_DATA		5       //this is where we especify if it is little endian or not

//EI CLASS
#define ELFCLASS64  2       //this is the identifier for elf64 bit

/* e_ident[EI_DATA] */
#define ELFDATA2LSB	1       //this is what specifies little endian


//memory ranges
#define min_prog_range 0x80100000
#define max_prog_range 0x81000000



/* 64-bit ELF base types. */
typedef uint64_t    Elf64_Addr;
typedef uint16_t	Elf64_Half;
typedef uint64_t	Elf64_Off;
typedef uint32_t	Elf64_Word;

/* These constants are for the segment types stored in the image headers but we only want to load types PT_LOAD*/
#define PT_LOAD    1        //only need type PL_LOAD


/* These constants define the different elf file types but we only really need ET_EXC since we only care about executables*/
#define ET_EXEC   2         //only care about executables

//machine time
#define EM_RISCV	243	/* RISC-V */


/* size of e_ident array (first item in elf) */
#define EI_NIDENT 16        //should be 16


/* 64 bit elf header struct */
typedef struct {
    unsigned char e_ident[EI_NIDENT]; //array for file interpretation and identification
    uint16_t e_type;           //the object file type
    uint16_t e_machine;        //specifies the required architecture for an individual file
    uint32_t e_version;        //file version
    Elf64_Addr e_entry;        //address to first tranfer control to and if there is no entry then 0
    Elf64_Off e_phoff;         //holds the program header table's file offset in bytes.  If the file has no program header table, this member holds zero.
    Elf64_Off e_shoff;         //holds the section header table's file offset in bytes.  If the file has no section header table, this member holds zero.
    uint32_t e_flags;          //holds processor-specific flags associated with the file.
    uint16_t e_ehsize;         //ELF header's size in bytes
    uint16_t e_phentsize;      //the size in bytes of one entry in the file's program header table
    uint16_t e_phnum;          //holds the number of entries in the program header table.  Thus the product of e_phentsize and e_phnum gives the table's size in bytes.
    uint16_t e_shentsize;      //holds a sections header's size in bytes.  A section header is one entry in the section header table; all entries are the same size.
    uint16_t e_shnum;          //holds the number of entries in the section header table.  Thus the product of e_shentsize and e_shnum gives the section header table's size in bytes.
    uint16_t e_shstrndx;       //holds the section header table index of the entry associated with the section name string table.  If the file has no section name string table, this member holds the value SHN_UNDEF.
} Elf64_Ehdr;

/* 64 bit program header */
typedef struct {
    uint32_t p_type;            //indicates how to interpret then array element's information
    uint32_t p_flags;           //holds segment flags relevant to the segment
    Elf64_Off p_offset;         //holds the offset from the beginning of the file at which the first byte of the segment resides
    Elf64_Addr p_vaddr;         //holds the virtual address at which the first byte of the segment resides in memory
    Elf64_Addr p_paddr;         //On systems for which physical addressing is relevant, this member is reserved for the segment's physical address.
    uint64_t p_filesz;          //size of segment in the file image, so only initialized data
    uint64_t p_memsz;           //size of segment in memory image, so initialized and uninitialized data
    uint64_t p_align;           //holds the value to which the segments are aligned in memory and in the file.
} Elf64_Phdr;


/*
Description: The point of this function it to have already set up an io interface and then use that to load a buffer
from which we can read the contents of an elf file. Then it sets everything up in memory
Purpose: this then can return the pointer to where to hand over control and have everything set up in memory
inputs: io interface and function pointer
outputs: int which is 0 if success or neg if error
*/

int elf_load(struct io_intf *io, void (**entryptr)(void)){
    // console_printf("Executing line %d in file %s\n", __LINE__, __FILE__);
    
    Elf64_Ehdr elf_header;  //create a pointer to an empty elf64 header
    
    // //first step is to load the elf file so call ioread to read it into elf_header

    // long ioreturn = ioread(io, &elf_header, sizeof(Elf64_Ehdr));

    // console_printf("Executing line %d ioreturn = %ld\n", __LINE__, ioreturn);
    // console_printf("Executing line %d elf64size= %ld\n", __LINE__, (long)sizeof(Elf64_Ehdr));




    if( ioread(io, &elf_header, sizeof(Elf64_Ehdr)) != (long)sizeof(Elf64_Ehdr)){      //ioread returns size of what we just read so it should be the sizeof(Elf64_Ehdr)
        return -1;  //not same size as we expected so return -1
    }

    // console_printf("Executing line %d in file %s\n", __LINE__, __FILE__);

    //we have now read into elf_header when we called ioread inside the previous if statement

    //next make sure it has the elf mag number
    if (elf_header.e_ident[EI_MAG0] != ELFMAG0 || elf_header.e_ident[EI_MAG1] != ELFMAG1 || elf_header.e_ident[EI_MAG2] != ELFMAG2 || (elf_header.e_ident[EI_MAG3] != ELFMAG3)) {
        return -2;
    }
    
    //make sure 64 bits
    if(elf_header.e_ident[EI_CLASS] != ELFCLASS64){
        return -9;
    }

    //make sure this is little endian
    if(elf_header.e_ident[EI_DATA] != ELFDATA2LSB){
        return -10;
    }

    if (elf_header.e_machine != EM_RISCV) {
        return -11;  //was not a RISC-V file
    }

    //make sure we are looking at an executable file
    if(elf_header.e_type != ET_EXEC){
        return -3;
    }


    //at this point we validated the elf header struct so now look at each program header
    for(int i = 0; i < elf_header.e_phnum; i++){
        Elf64_Phdr prog_header;     //create pointer to an empty prog header

        //now seek the program headers to know where to read from

        // console_printf("program header size using sizeof: %lx\n", sizeof(Elf64_Phdr));
        // console_printf("program header size using e_phentsize: %lx\n", elf_header.e_phentsize);



        uint64_t seek_pos = elf_header.e_phoff + i * sizeof(Elf64_Phdr);
        // console_printf("Seeking to program header position: %lx\n", seek_pos);

        // int seek = ioseek(io, seek_pos);
        // console_printf("ioseek return: %d\n", seek);

        if(ioseek(io, seek_pos) < 0){        //if ioseek returns less than 0 (in this case -4) then we have an error
            // console_printf("ioseek failed at position %lx\n", seek_pos);
            return -4;
        }

        //now we can read
        if(ioread(io, &prog_header, sizeof(Elf64_Phdr)) != (long)(sizeof(Elf64_Phdr))){ //we expect to get the same size back
            return -5; 
        }

        //now only continue if our program is type PT Load
        if(prog_header.p_type == PT_LOAD){

            // console_printf("Executing line %d in file %s\n", __LINE__, __FILE__);
            //get the address of the program
            // void *v_addr = prog_header.p_vaddr;
            void *v_addr = (void *)(uintptr_t)prog_header.p_vaddr;


            //now we make sure it falls within the bounds of x80100000 and 0x81000000
            if((Elf64_Addr)v_addr < USER_START_VMA || ((Elf64_Addr)v_addr + prog_header.p_memsz > USER_END_VMA)){
                return -6;
            }


            //set the flags
            uint_fast8_t rwx_flags = 0;
            if(prog_header.p_flags & PF_X){
                rwx_flags |= PTE_X;
            }
            if(prog_header.p_flags & PF_W){
                rwx_flags |= PTE_W;
            }
            if(prog_header.p_flags & PF_R){
                rwx_flags |= PTE_R;
            }

            rwx_flags |= PTE_U;

            //rwx now has the flags we want

            // console_printf("p_offset: %d \n", prog_header.p_offset);
            
            //we know we are within range so seek to the beginning of the file
            if(ioseek(io, prog_header.p_offset) < 0){ //make sure we dont get an error
                return -7;
            }


            // console_printf("pos before read: %d \n", pos);
            // console_printf("we have to read: %d \n", prog_header.p_filesz);
            //load this file into memory
            // console_printf("p_filesz: %d \n", prog_header.p_filesz);
            
            console_printf("file: %s line: %d \n",__FILE__, __LINE__);

            memory_alloc_and_map_range((uintptr_t)v_addr, prog_header.p_memsz, PTE_W |PTE_R|PTE_U);


            console_printf("file: %s line: %d \n",__FILE__, __LINE__);

            

            if(ioread(io, v_addr, prog_header.p_filesz) != (long)prog_header.p_filesz){ //make sure the file is the same size
                return -8;
                console_printf("line: %d \n", __LINE__);
                
            }
            console_printf("line: %d \n", __LINE__);

            memory_set_range_flags(v_addr,prog_header.p_memsz, rwx_flags);

            // console_printf("pos after read: %d \n", pos);
            // console_printf("we read: %d \n", prog_header.p_filesz);
            //initialize everything else to 0
            
            if(prog_header.p_memsz > prog_header.p_filesz){
                // void * memset(void * s, int c, size_t n)
                memset((void *)((Elf64_Addr)(v_addr+ prog_header.p_filesz)), 0, prog_header.p_memsz - prog_header.p_filesz); //initialize the uninitialized to 0
            }
            
        }
    }

    //set the rentry point of execution
    *entryptr = (void (*)(void)) elf_header.e_entry;

    console_printf("e_entry:%x \n", elf_header.e_entry);

    //close the file since we extracted what we needed
    // ioclose(io);

    // console_printf("success \n");

    return 0; //success
}


