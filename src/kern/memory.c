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

union linked_page * not_free_list;

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
    start = RAM_START;      //what this a for?

    // Create page_cnt # of nodes
    for(int x = 0; x < page_cnt; x++)
    {
        // Do we need to set value of padding here??        //pading should never have a value it is only to ensure spacing

        //the space between heap_end and RAM_END should already be left intentionally alone in order for us to use
        //so just create a pointer to each space

        union linked_page * nextPage = (union linked_page *)page + PAGE_SIZE;
        assert(nextPage <= RAM_END); //will let us know if we went past the space
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

void memory_space_reclaim(void)
{
    // Get active memory space
    uintptr_t old_mtag = active_space_mtag();

    //Get the old root page table
    struct pte *old_root_table = mtag_to_root(old_mtag);

    // Switch active memory space to main memory space
    memory_space_switch(main_mtag);
    
    //for all the entries in the level 2 (root) page table
    for(int vpn2 = 0; vpn2 < PTE_CNT; vpn2++){


        //get the current pt1 entry
        struct pte curr_pt2_entry = old_root_table[vpn2];

        //check if it is active
        if((curr_pt2_entry.flags & PTE_V) == 0 ){
            continue;   //valid flag was zero so nothing to see here
        }

        //at this point it is active/valid

        //we want to only reclaim non-global so skip if global
        if(curr_pt2_entry.flags & PTE_G){
            continue;//was global
        }

        //at this point it was valid and non-global so lets keep looking down the tree
        struct pte *pt1 = (struct pte*)pagenum_to_pageptr(curr_pt2_entry.ppn);  //pointer to level 1 pt

        //look at every entry in the level 1 pt
        for(int vpn1 = 0; vpn1 < PTE_CNT; vpn1++){
            struct pte curr_pt1_entry = pt1[vpn1];      //curr pt1 entry

            //check if it is active
            if((curr_pt1_entry.flags & PTE_V) == 0 ){
                continue;   //valid flag was zero so nothing to see here
            }

            //at this point it is active/valid

            //we want to only reclaim non-global so skip if global
            if(curr_pt1_entry.flags & PTE_G){
                continue;//was global
            }

            //at this point it was valid and non-global so lets keep looking down the tree
            struct pte *pt0 = (struct pte*)pagenum_to_pageptr(curr_pt1_entry.ppn);      //pointer to level 0 pt

            for(int vpn0 = 0; vpn0 < PTE_CNT; vpn0++){
                struct pte curr_pt0_entry = pt0[vpn0];      //curr pt0 entry

                //check if it is active
                if((curr_pt0_entry.flags & PTE_V) == 0 ){
                    continue;   //valid flag was zero so nothing to see here
                }

                //at this point it is active/valid

                //we want to only reclaim non-global so skip if global
                if(curr_pt0_entry.flags & PTE_G){
                    continue;//was global
                }

                //at this point we are looking at a physical an entry pointing to a physical page we want to free
                void *pp = pagenum_to_pageptr(curr_pt0_entry.ppn);
                memory_free_page(pp);       //free physical page

                pt0[vpn0] = null_pte();     //set this entry to now point to null but not sure since we might just have to make v flag to 0
            }

            //we looked all the way down from this pt1 entry so now clear this
            // memory_free_page(pt0);          //free all of pt0

            // pt1[vpn1] = null_pte();         //set this entry to now point to null but not sure since we might just have to make v flag to 0

        }

        //we looked all the way down from this pt1 entry so now clear this
        // memory_free_page(pt1);          //free all of pt1

        // old_root_table[vpn2] = null_pte();         //set this entry to now point to null but not sure since we might just have to make v flag to 0

    }

    // memory_free_page(old_root_table);       //free the old root table

}

void *memory_alloc_page(void)
{
    // Panics if no free pages available
    if(free_list == NULL)
    {
        panic("No free pages available");
    }

    union linked_page * newPage = free_list;
    free_list = free_list -> next;

    return (void *) newPage;

    // Need to make pp, make data page pp, virtual adress = pp, return it
    // Returns value in RAM range, so VMA = PMA
    //char pad[PAGE_SIZE];
    // char * pad = (char *) start;

    // for(size_t x = 0; x < (size_t) sizeof(pad); x++)
    // {
    //     newPage -> padding[x] = pad[x];
    // }

    // // Add new page to not free list
    // union linked_page * check = not_free_list;
    // if(check == NULL)
    // {
    //     check = newPage;
    // }
    // else
    // {
    //     while(check -> next != NULL)
    //     {
    //         check = check -> next;
    //     }
    //     check = newPage;
    // }

    // // Move to next physical address after using it for page here

    // // Set page table to pp

    // void * pp = pagenum_to_pageptr((uintptr_t) start);
    // start += 1;
    // return pp;
}

void memory_free_page(void *pp)
{

    //check if alligned
    if(aligned_ptr(pp,PAGE_SIZE) != 0){
        panic("page not aligned so we can't add it to free list");
    }

    // //first access as pte to set the flag as unvalid
    // struct pte * pte = (struct pte *) pp;

    // //set flags to show unavailable
    // pte->flags = pte->flags & !PTE_V;

    //now get the page as a linked page
    union linked_page* new_free_list_head = (union linked_page*) pp;

    //add to the free list
    new_free_list_head->next = free_list;
    free_list = new_free_list_head;
}

void * memory_alloc_and_map_page(uintptr_t vma, uint_fast8_t rwxug_flags)
{
    if(wellformed_vma(vma) == 0){ 
        panic("not well formed because Address bits 63:38 must be all 0 or all 1");
    }

    // Somehow get vma
    void * newPP = memory_alloc_page();
    //struct pte newLeaf = leaf_pte(newPP, rwxug_flags);  // Spawns leaf page table entry for a given pp

    
    // Map vma to newPP somehow

    uintptr_t pt2_pma = (csrr_satp() << 20) >> 8;
    struct pte * pt2 = (struct pte *) pt2_pma;

    uintptr_t pt1_ppn = pt2[VPN2(vma)].ppn;
    uintptr_t pt1_pma = pt1_ppn << 12;
    struct pte * pt1 = (struct pte*)pt1_pma;

    uintptr_t pt0_ppn = pt1[VPN1(vma)].ppn;
    uintptr_t pt0_pma = pt0_ppn << 12;
    struct pte * pt0 = (struct pte*) pt0_pma;

    uintptr_t ppn = pt0[VPN0(vma)].ppn;
    uintptr_t pma = (ppn << 12) | (vma & 0xFFF);

    newPP = pma;        // Using newPP and pma?
    
    return (void *) vma;        // Should be correct
}

void * memory_alloc_and_map_range(uintptr_t vma, size_t size, uint_fast8_t rwxug_flags)
{
    void * firstAdr;
    for(uintptr_t x = 0; x < size; x++)
    {
        if(x == 0)
        {
        firstAdr = memory_alloc_and_map_page(vma, rwxug_flags);
        }
        else
        {
        memory_alloc_and_map_page(vma + x, rwxug_flags);
        }
    }
    return firstAdr;
}

void memory_set_page_flags(const void *vp, uint8_t rwxug_flags)
{
    //Get pte for vp
    struct pte * temp = (struct pte *) vp; 
    temp->flags = rwxug_flags | PTE_A | PTE_D | PTE_V;
}

void memory_set_range_flags(const void *vp, size_t size, uint8_t rwxug_flags)
{
    for(size_t x = 0; x < size; x++)
    {
        memory_set_page_flags(vp + x, rwxug_flags);
    }
}

void memory_unmap_and_free_user(void)
{
    // unmap and free all user space pages

    // Go through all pages -> if U bit is set, unmap and free page.
}

/*
EXTRA CREDIT FUNCTIONS
int memory_validate_vptr_len ( const void * vp, size_t len, uint_fast8_t rwxug_flags);

int memory_validate_vstr (const char * vs, uint_fast8_t ug_flags);
*/

void memory_handle_page_fault(const void * vptr)
{
    // Get pte for vptr
    struct pte temp;

    // If v flag == 0 and try to translate, page fault exception
    if(temp.flags && 0x0 == 0)
    {
        mem_alloc_page();
    }
    // If in U mode and U = 0, translate
    else if(temp.flags && 0x0 << 2 == 0)
    {
        mem_alloc_page();
    }
    // If in S mode and U = 1 and stats.SUM = 0, page fault

}
// INTERNAL FUNCTION DEFINITIONS
//

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
