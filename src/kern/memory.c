// memory.c - Memory management
//

/*
QUESTIONS TO ASK DURING OH:
- Implementation of linked list (not adding padding value yet?)
- How to reclaim physical pages mapped by memory space (what does reclaim mean, how do we find this?)
- How to get page using void * and is way of adding it back to list okay? Need to change values?
- What do we do with vma? Are we supposed to just use subtables in reverse to map vma to a new physical address using mem_alloc_page
- For range functions, do we just do a for loopand increment virtual address each time for size?
*/

#define NEXT 1


#ifndef TRACE
#ifdef MEMORY_TRACE
#define TRACE
#endif
#endif

#ifndef DEBUG
#ifdef MEMORY_DEBUG
#define DEBUG
#endif
#endif

#include "config.h"

#include "memory.h"
#include "console.h"
#include "halt.h"
#include "heap.h"
#include "csr.h"
#include "string.h"
#include "error.h"
#include "thread.h"
#include "process.h"

#include <stdint.h>

// EXPORTED VARIABLE DEFINITIONS
//

char memory_initialized = 0;
uintptr_t main_mtag;

void * start; // Global variable to hold address space

// IMPORTED VARIABLE DECLARATIONS
//

// The following are provided by the linker (kernel.ld)

extern char _kimg_start[];
extern char _kimg_text_start[];
extern char _kimg_text_end[];
extern char _kimg_rodata_start[];
extern char _kimg_rodata_end[];
extern char _kimg_data_start[];
extern char _kimg_data_end[];
extern char _kimg_end[];

// INTERNAL TYPE DEFINITIONS
//

union linked_page {
    union linked_page * next;
    char padding[PAGE_SIZE];
};

struct pte {
    uint64_t flags:8;
    uint64_t rsw:2;
    uint64_t ppn:44;
    uint64_t reserved:7;
    uint64_t pbmt:2;
    uint64_t n:1;
};

// INTERNAL MACRO DEFINITIONS
//

#define VPN2(vma) (((vma) >> (9+9+12)) & 0x1FF)     // gets VPN[2]
#define VPN1(vma) (((vma) >> (9+12)) & 0x1FF)       //gets VPN[1]
#define VPN0(vma) (((vma) >> 12) & 0x1FF)           //gets the VPN[0]
#define MIN(a,b) (((a)<(b))?(a):(b))

// INTERNAL FUNCTION DECLARATIONS
//

struct pte * walk_pt(struct pte * root, uintptr_t vma, int create);

static inline int wellformed_vma(uintptr_t vma);
static inline int wellformed_vptr(const void * vp);
static inline int aligned_addr(uintptr_t vma, size_t blksz);
static inline int aligned_ptr(const void * p, size_t blksz);
static inline int aligned_size(size_t size, size_t blksz);

static inline uintptr_t active_space_mtag(void);
static inline struct pte * mtag_to_root(uintptr_t mtag);
static inline struct pte * active_space_root(void);

static inline void * pagenum_to_pageptr(uintptr_t n);
static inline uintptr_t pageptr_to_pagenum(const void * p);

static inline void * round_up_ptr(void * p, size_t blksz);
static inline uintptr_t round_up_addr(uintptr_t addr, size_t blksz);
static inline size_t round_up_size(size_t n, size_t blksz);
static inline void * round_down_ptr(void * p, size_t blksz);
static inline size_t round_down_size(size_t n, size_t blksz);
static inline uintptr_t round_down_addr(uintptr_t addr, size_t blksz);

static inline struct pte leaf_pte (
    const void * pptr, uint_fast8_t rwxug_flags);
static inline struct pte ptab_pte (
    const struct pte * ptab, uint_fast8_t g_flag);
static inline struct pte null_pte(void);

static inline void sfence_vma(void);

// INTERNAL GLOBAL VARIABLES
//

static union linked_page * free_list;

//made the not free list

static struct pte main_pt2[PTE_CNT]
    __attribute__ ((section(".bss.pagetable"), aligned(4096)));
static struct pte main_pt1_0x80000[PTE_CNT]
    __attribute__ ((section(".bss.pagetable"), aligned(4096)));
static struct pte main_pt0_0x80000[PTE_CNT]
    __attribute__ ((section(".bss.pagetable"), aligned(4096)));

// EXPORTED VARIABLE DEFINITIONS
//

// EXPORTED FUNCTION DEFINITIONS
// 

/*
void memory_init(void)
Inputs:
None

Outputs:
None

Effects:
memory_init sets up page tables and performs mapping of the kernel megapage, as well as starts making the free pages list.


Description:
Most of this function was completed for us. The given code set up the page tables, performed the 1:1 mapping, enabled paging, and initialized the heap memory manager.
We created the free page list, by creating a linked list of count (page count), which allows us to allocate and free pages later on in the process.
*/

void memory_init(void) {
    const void * const text_start = _kimg_text_start;
    const void * const text_end = _kimg_text_end;
    const void * const rodata_start = _kimg_rodata_start;
    const void * const rodata_end = _kimg_rodata_end;
    const void * const data_start = _kimg_data_start;
    union linked_page * page;
    void * heap_start;
    void * heap_end;
    size_t page_cnt;
    uintptr_t pma;      
    const void * pp;

    trace("%s()", __func__);

    assert (RAM_START == _kimg_start);

    kprintf("           RAM: [%p,%p): %zu MB\n",
        RAM_START, RAM_END, RAM_SIZE / 1024 / 1024);
    kprintf("  Kernel image: [%p,%p)\n", _kimg_start, _kimg_end);

    // Kernel must fit inside 2MB megapage (one level 1 PTE)
    
    if (MEGA_SIZE < _kimg_end - _kimg_start)
        panic("Kernel too large");

    // Initialize main page table with the following direct mapping:
    // 
    //         0 to RAM_START:           RW gigapages (MMIO region)
    // RAM_START to _kimg_end:           RX/R/RW pages based on kernel image
    // _kimg_end to RAM_START+MEGA_SIZE: RW pages (heap and free page pool)
    // RAM_START+MEGA_SIZE to RAM_END:   RW megapages (free page pool)
    //
    // RAM_START = 0x80000000
    // MEGA_SIZE = 2 MB
    // GIGA_SIZE = 1 GB
    
    // Identity mapping of two gigabytes (as two gigapage mappings)
    for (pma = 0; pma < RAM_START_PMA; pma += GIGA_SIZE)
        main_pt2[VPN2(pma)] = leaf_pte((void*)pma, PTE_R | PTE_W | PTE_G);
    
    // Third gigarange has a second-level page table
    main_pt2[VPN2(RAM_START_PMA)] = ptab_pte(main_pt1_0x80000, PTE_G);

    // First physical megarange of RAM is mapped as individual pages with
    // permissions based on kernel image region.

    main_pt1_0x80000[VPN1(RAM_START_PMA)] =
        ptab_pte(main_pt0_0x80000, PTE_G);

    for (pp = text_start; pp < text_end; pp += PAGE_SIZE) {
        main_pt0_0x80000[VPN0((uintptr_t)pp)] =
            leaf_pte(pp, PTE_R | PTE_X | PTE_G);
    }

    for (pp = rodata_start; pp < rodata_end; pp += PAGE_SIZE) {
        main_pt0_0x80000[VPN0((uintptr_t)pp)] =
            leaf_pte(pp, PTE_R | PTE_G);
    }

    for (pp = data_start; pp < RAM_START + MEGA_SIZE; pp += PAGE_SIZE) {
        main_pt0_0x80000[VPN0((uintptr_t)pp)] =
            leaf_pte(pp, PTE_R | PTE_W | PTE_G);
    }

    // Remaining RAM mapped in 2MB megapages

    for (pp = RAM_START + MEGA_SIZE; pp < RAM_END; pp += MEGA_SIZE) {
        main_pt1_0x80000[VPN1((uintptr_t)pp)] =
            leaf_pte(pp, PTE_R | PTE_W | PTE_G);
    }

    // Enable paging. This part always makes me nervous.

    main_mtag =  // Sv39
        ((uintptr_t)RISCV_SATP_MODE_Sv39 << RISCV_SATP_MODE_shift) |
        pageptr_to_pagenum(main_pt2);
    
    csrw_satp(main_mtag);
    sfence_vma();

    // Give the memory between the end of the kernel image and the next page
    // boundary to the heap allocator, but make sure it is at least
    // HEAP_INIT_MIN bytes.

    heap_start = _kimg_end;
    heap_end = round_up_ptr(heap_start, PAGE_SIZE);
    if (heap_end - heap_start < HEAP_INIT_MIN) {
        heap_end += round_up_size (
            HEAP_INIT_MIN - (heap_end - heap_start), PAGE_SIZE);
    }

    if (RAM_END < heap_end)
        panic("Not enough memory");
    
    // Initialize heap memory manager

    heap_init(heap_start, heap_end);

    kprintf("Heap allocator: [%p,%p): %zu KB free\n",
        heap_start, heap_end, (heap_end - heap_start) / 1024);

    free_list = heap_end; // heap_end is page aligned
    page_cnt = (RAM_END - heap_end) / PAGE_SIZE;

    kprintf("Page allocator: [%p,%p): %lu pages free\n",
        free_list, RAM_END, page_cnt);

    // Put free pages on the free page list
    // TODO: FIXME implement this (must work with your implementation of
    // memory_alloc_page and memory_free_page).


    // Set page equal to start of free page list
    page = free_list;

    // Create page_cnt # of nodes
    for(int x = 0; x < page_cnt - NEXT; x++)
    {
        union linked_page * nextPage = (union linked_page *)page + 1;       // For each linked_page, get the address of it and set the next page of the current page to this
        page->next = nextPage;
        page = page->next;
    }
    page->next = NULL;  //last next should be NULL so we know we're at the end when traversinag the linked list later on


    // Allow supervisor to access user memory. We could be more precise by only
    // enabling it when we are accessing user memory, and disable it at other
    // times to catch bugs.

    csrs_sstatus(RISCV_SSTATUS_SUM);

    memory_initialized = 1;
}

/*
void memory_space_reclaim(void)
Inputs:
None

Outputs:
None

Effects:
memory_space_reclaim switches the active memory space to the main memory space and reclaimed all
physical pages not part of the global mapping.

Description:
memory_space_reclaim reclaims the memory space that was active on entry. It does this by calling
memory_unmap_and_free_user. It then switches to the main memory space, which is the intended function.
*/

void memory_space_reclaim(void)
{

    memory_unmap_and_free_user(); // Reclaim all pages that aren't global

    // Switch active memory space to main memory space
    memory_space_switch(main_mtag);

    sfence_vma();

}

/*
void * memory_alloc_page(void)
Inputs:
None

Outputs:
void * newPage - the page taken off the free list

Effects:
memory_alloc_page takes a page off the free list, so it moves the head of the free_page list

Description:
memory_alloc_page allocates a page of memory using the free pages list. It takes a page off the list and returns it so that
it can be used as a page.
*/

void *memory_alloc_page(void)
{
    // Panics if no free pages available
    if(free_list == NULL)
    {
        panic("No free pages available");
    }

    union linked_page * newPage = free_list;    // Get the page from the head of the free list
    free_list = free_list -> next;      // Move to the next page in the list

    memset(newPage,0, PAGE_SIZE);       // Clear data in the page

    return (void *) newPage;
}

/*
void memory_free_page(void *pp)
Inputs:
pp - the physical pointer for the page we are returning to the free page list

Outputs:
None

Effects:
This function puts a page back on the free page list, adding it to the head of the free page

Description:
memory_free_page adds a page back to the free page list, first making sure that the page is aligned. 
It sets the page as the head of the free_list, setting the next page to be the previous list's head.
*/

void memory_free_page(void *pp)
{

    //check if alligned
    if(aligned_ptr(pp,PAGE_SIZE) != 0){
        // panic("page not aligned so we can't add it to free list");
        round_down_addr((uintptr_t)pp,PAGE_SIZE);

    }
    
    //now get the page as a linked page
    union linked_page* new_free_list_head = (union linked_page*) pp;

    //add to the free list
    new_free_list_head->next = free_list;
    free_list = new_free_list_head;

}

/*
void * memory_alloc_and_map_page(uintptr_t vma, uint_fast8_t rwxug_flags)
Inputs:
vma - virtual address to be mapped
rwxug_falgs - flags to be set for the virtual address

Outputs: returns the mapped vma


Effects:
intr_handler calls the correct interrupt handler for the interrupt represented by the given code.

Description:
intr_handler controls when specific interrupt functions are called depending on what the interrupt code was. The extern_intr_handler()
case was already provided for us, but we had to had the timer_intr_handler() case. When mtime >= mtimecap, the MTI bit is set in the MIE reg,
so if we have the case where the interrupt code is set to 7, we call the timer_intr_handler().

*/

void * memory_alloc_and_map_page(uintptr_t vma, uint_fast8_t rwxug_flags)
{
    if(wellformed_vma(vma) == 0){ 
        panic("not well formed because Address bits 63:38 must be all 0 or all 1");
    }

    console_printf("line: %d \n", __LINE__);
    

    void * newPP = memory_alloc_page();

    memset(newPP,0, PAGE_SIZE);

    struct pte * pt2 = active_space_root();

    //Set flags to make valid - "Region must be RW when first mapped, so you can load it" - SET R AND W?

    struct pte* pt0_pte = walk_pt(pt2, vma, 1);

    struct pte leaf = leaf_pte(newPP, rwxug_flags);

    *pt0_pte = leaf;

    return (void *) vma;        // Should be correct return value
}

/*
* memory_alloc_and_map_range
Inputs: vma, size, flags we want to write


Outputs: we return the vma of the mapped range

Effects: it maps a vma accros a large range and mpas various pages

Description: this first founds out how many pages we have to map and then calls
memory malloc and map range that many times for each page


*/

void * memory_alloc_and_map_range(uintptr_t vma, size_t size, uint_fast8_t rwxug_flags)
{
    int numPages = size / PAGE_SIZE;    //get the number of pages needed

    if(size%PAGE_SIZE > 0){ //if there is more space needed that didn't take up a whole page
        numPages++;
    }
    console_printf("line: %d \n", __LINE__);

    for(int x = 0; x< numPages; x++){
        memory_alloc_and_map_page(vma+ (x * PAGE_SIZE) , rwxug_flags);
    }

    return (void *) vma;        // Should be correct return value
}

/*
memory_set_page_flags
Inputs: a vp and the flags we want to set for that vp

Outputs: it does not output anything directly

Effects: the effect is that the flags at that page are getting set

Description: It has to call walk to walk down the page table entries to find the correct leaf pte to set the flags for
then it sets the flags for that corresponding page 

*/

void memory_set_page_flags(const void *vp, uint8_t rwxug_flags)
{
    //Get pte for vp

    // Can we assume valid vp? (When we walk down, we won't have to create anything)

    

    uintptr_t vma = (uintptr_t) vp; 
    struct pte * pte_0 = walk_pt(active_space_root(),vma, 0 );
    if(pte_0 == NULL){
        panic("tryng to set the flag of something that does not have a pte");
    }
    pte_0->flags = rwxug_flags | PTE_A | PTE_D | PTE_V;

}
/*
memory_set_range_flags

Inputs: takes in the vp, size, and flags to set

Outputs: it outputs nothing since it just sets flags

Effects: it sets the flags in a range

Description: we have to find how many pages to set the flags for and call memory_set_page_flags
for each individual page.

*/

void memory_set_range_flags(const void *vp, size_t size, uint_fast8_t rwxug_flags)
{
    int numPages = size / PAGE_SIZE;
    
    if(size%PAGE_SIZE > 0){ //if there is more space needed that didn't take up a whole page
        numPages++;
    }

    // console_printf("file: %s line: %d \n",__FILE__, __LINE__);

    for(int x = 0; x < numPages; x++)
    {
        memory_set_page_flags(vp + (x * PAGE_SIZE), rwxug_flags); // Right way to set each PTE's flags?
    }
}

/*
Inputs: nothing

Outputs: nothing

Effects: this should free the user space by clearing everything including the level 2 root table

Description: this goes down the tree to make clear anything that has the user flag.

*/

void memory_unmap_and_free_user(void)
{
    // Taken from reclaim


    struct pte* pt2 = active_space_root();

    for(int vpn2 = 0; vpn2 < PTE_CNT; vpn2++){


        //get the current pt1 entry
        struct pte curr_pt2_entry = pt2[vpn2];

        //check if it is active
        if((curr_pt2_entry.flags & PTE_V) == 0 ){
            continue;   //valid flag was zero so nothing to see here
        }

        // Want to only unmap pages with U flag set
        if((curr_pt2_entry.flags & PTE_U) == 0)
        {
        continue;
        }

        //at this point it is active/valid


        // lets keep looking down the tree
        struct pte *pt1 = (struct pte*)pagenum_to_pageptr(curr_pt2_entry.ppn);  //pointer to level 1 pt

        //look at every entry in the level 1 pt
        for(int vpn1 = 0; vpn1 < PTE_CNT; vpn1++){
            struct pte curr_pt1_entry = pt1[vpn1];      //curr pt1 entry

            //check if it is active
            if((curr_pt1_entry.flags & PTE_V) == 0 ){
                continue;   //valid flag was zero so nothing to see here
            }

            //at this point it is active/valid
            // Want to only unmap pages with U flag set
               if((curr_pt1_entry.flags & PTE_U) == 0)
               {
                continue;
               }

            // lets keep looking down the tree
            struct pte *pt0 = (struct pte*)pagenum_to_pageptr(curr_pt1_entry.ppn);      //pointer to level 0 pt

            for(int vpn0 = 0; vpn0 < PTE_CNT; vpn0++){
                struct pte curr_pt0_entry = pt0[vpn0];      //curr pt0 entry

                //check if it is active
                if((curr_pt0_entry.flags & PTE_V) == 0 ){
                    continue;   //valid flag was zero so nothing to see here
                }

                //at this point it is active/valid

               // Want to only unmap pages with U flag set
               if((curr_pt0_entry.flags & PTE_U) == 0)
               {
                continue;
               }

                //at this point we are looking at a physical an entry pointing to a physical page we want to free
                void *pp = pagenum_to_pageptr(curr_pt0_entry.ppn);
                console_printf("line: %d \n", __LINE__);
                memory_free_page(pp);       //free physical page
            }
            memory_free_page(pt0);
            console_printf("line: %d \n", __LINE__);

        }
        memory_free_page(pt1);
        console_printf("line: %d \n", __LINE__);
    }
    memory_free_page(pt2);
    console_printf("line: %d \n", __LINE__);

    // Unmaps any page in user range mapped with U flag set and frees underlying physical page 


    // unmap and free all user space pages

    // Go through all pages -> if U bit is set, unmap and free page.
    
    // Start at pt2, go through all entries
    // For all entries in given pt1...,
    // For all entries in given pt0...,
    // If associated file has U bit set, unmap and free page
    // sfence_vma();
    console_printf("line: %d \n", __LINE__);
}

/*
EXTRA CREDIT FUNCTIONS
int memory_validate_vptr_len ( const void * vp, size_t len, uint_fast8_t rwxug_flags);

int memory_validate_vstr (const char * vs, uint_fast8_t ug_flags);
*/

/*

memory_handle_page_fault

Inputs: takes in the vptr where the page fault happens


Outputs: nothing directly

Effects: it maps a page to that virtual pointer

Description: when we get a page fault we memory_alloc_and_map_range and set the user flag

*/

void memory_handle_page_fault(const void * vptr)
{
    void *v_addr = (void *)(uintptr_t)vptr;
    //v_addr = round_down_addr(v_addr, PAGE_SIZE); // maybe round down
    // If v flag == 0 and try to translate, page fault exception



    if((USER_START_VMA <= (uint64_t)vptr) && (USER_END_VMA > (uint64_t)vptr))
    {
        
    
       memory_alloc_and_map_range((uintptr_t)v_addr, PAGE_SIZE, PTE_W |PTE_R|PTE_U);
    }
    else
    {
        // panic("memory_handle_page_fault");
        process_exit();
    }
    // If in S mode and U = 1 and stats.SUM = 0, page fault
    return;
}

// INTERNAL FUNCTION DEFINITIONS
//

// Takes pointer to active root and walks down page table structure using VMA fields of vma. If create is non-zero, then it creates page tables to walk to leaf page table
/*
Inputs: the root table, a vma to map, and if create is 1 we can allocate memory but if it is 0 we cant and return null

Outputs:this return the pointer to the level 0 pte that will point a pp

Effects: this will map return the pt0 entry

Description: this goes do down the tables and either creates them or not and then once we get to the level 0 we return that pt0 entry

*/

struct pte * walk_pt(struct pte * root, uintptr_t vma, int create){
    if (!wellformed_vma(vma)) return NULL;
    

    if ((root[VPN2(vma)].flags & PTE_V) == 0){
        if(create == 0){
            return NULL;
        }

        //not valid so create a level 1 table here
        struct pte * pt1_new = (struct pte *)memory_alloc_page();
        // uint64_t pt1_new_ppn = pageptr_to_pagenum(pt1_new);
        // root[VPN2(vma)].ppn = pt1_new_ppn ;

        // memset(pt1_new,0,PAGE_SIZE);


        root[VPN2(vma)] = ptab_pte(pt1_new, 0);
        

        //set that pte is valid now because there is a table here
        // root[VPN2(vma)].flags |= PTE_V;

    }
    //now the table there is valid
    struct pte * pt1 = pagenum_to_pageptr(root[VPN2(vma)].ppn);

    //now check if pt0 exists
    if((pt1[VPN1(vma)].flags & PTE_V) == 0){
        if(create == 0){
            return NULL;
        }
        //pt0 did not exist so make one

        struct pte * pt0_new = (struct pte *)memory_alloc_page();
        // uint64_t pt0_new_ppn = pageptr_to_pagenum(pt0_new);
        // pt1[VPN1(vma)].ppn = pt0_new_ppn ;
        pt1[VPN1(vma)] = ptab_pte(pt0_new, 0);

        //that pte is valid now because there is a table here
        // pt1[VPN1(vma)].flags |= PTE_V;
    }

    //now there is for sure a pt0
    struct pte * pt0 = pagenum_to_pageptr(pt1[VPN1(vma)].ppn);


    //may need to check if what is at pt0 is valid
    return &pt0[VPN0(vma)];
}


static inline int wellformed_vma(uintptr_t vma) {
    // Address bits 63:38 must be all 0 or all 1
    uintptr_t const bits = (intptr_t)vma >> 38;
    return (!bits || !(bits+1));
}

static inline int wellformed_vptr(const void * vp) {
    return wellformed_vma((uintptr_t)vp);
}

static inline int aligned_addr(uintptr_t vma, size_t blksz) {
    return ((vma % blksz) == 0);
}

static inline int aligned_ptr(const void * p, size_t blksz) {
    return (aligned_addr((uintptr_t)p, blksz));
}

static inline int aligned_size(size_t size, size_t blksz) {
    return ((size % blksz) == 0);
}

static inline uintptr_t active_space_mtag(void) {                   //use to get curr mtag
    return csrr_satp();
}

static inline struct pte * mtag_to_root(uintptr_t mtag) {           //gives the address of the first entry already since ((<<20)>>20)<<12 is just (<<20)>>8
    return (struct pte *)((mtag << 20) >> 8);
}


static inline struct pte * active_space_root(void) {
    return mtag_to_root(active_space_mtag());
}

static inline void * pagenum_to_pageptr(uintptr_t n) {
    return (void*)(n << PAGE_ORDER);
}

static inline uintptr_t pageptr_to_pagenum(const void * p) {
    return (uintptr_t)p >> PAGE_ORDER;
}

static inline void * round_up_ptr(void * p, size_t blksz) {
    return (void*)((uintptr_t)(p + blksz-1) / blksz * blksz);
}

static inline uintptr_t round_up_addr(uintptr_t addr, size_t blksz) {
    return ((addr + blksz-1) / blksz * blksz);
}

static inline size_t round_up_size(size_t n, size_t blksz) {
    return (n + blksz-1) / blksz * blksz;
}

static inline void * round_down_ptr(void * p, size_t blksz) {
    return (void*)((uintptr_t)p / blksz * blksz);
}

static inline size_t round_down_size(size_t n, size_t blksz) {
    return n / blksz * blksz;
}

static inline uintptr_t round_down_addr(uintptr_t addr, size_t blksz) {
    return (addr / blksz * blksz);
}

static inline struct pte leaf_pte (
    const void * pptr, uint_fast8_t rwxug_flags)
{
    return (struct pte) {
        .flags = rwxug_flags | PTE_A | PTE_D | PTE_V,
        .ppn = pageptr_to_pagenum(pptr)
    };
}

static inline struct pte ptab_pte (
    const struct pte * ptab, uint_fast8_t g_flag)
{
    return (struct pte) {
        .flags = g_flag | PTE_V,
        .ppn = pageptr_to_pagenum(ptab)
    };
}

static inline struct pte null_pte(void) {
    return (struct pte) { };
}

static inline void sfence_vma(void) {
    asm inline ("sfence.vma" ::: "memory");
}
