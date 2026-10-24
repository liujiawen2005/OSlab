#include <default_pmm.h>
#include <best_fit_pmm.h>
#include <defs.h>
#include <error.h>
#include <memlayout.h>
#include <mmu.h>
#include <pmm.h>
#include <sbi.h>
#include <stdio.h>
#include <string.h>
#include <riscv.h>
#include <dtb.h>

// virtual address of physical page array
struct Page *pages;
// amount of physical memory (in pages)
size_t npage = 0;
// the kernel image is mapped at VA=KERNBASE and PA=info.base
uint64_t va_pa_offset;
// memory starts at 0x80000000 in RISC-V
// DRAM_BASE defined in riscv.h as 0x80000000
const size_t nbase = DRAM_BASE / PGSIZE;

// virtual address of boot-time page directory
uintptr_t *satp_virtual = NULL;
// physical address of boot-time page directory
uintptr_t satp_physical;

// physical memory management
const struct pmm_manager *pmm_manager;


static void check_alloc_page(void);

// init_pmm_manager - initialize a pmm_manager instance
static void init_pmm_manager(void) {
    pmm_manager = &best_fit_pmm_manager;
    cprintf("memory management: %s\n", pmm_manager->name);
    pmm_manager->init();
}

// init_memmap - call pmm->init_memmap to build Page struct for free memory
static void init_memmap(struct Page *base, size_t n) {
    pmm_manager->init_memmap(base, n);
}

// alloc_pages - call pmm->alloc_pages to allocate a continuous n*PAGESIZE
// memory
struct Page *alloc_pages(size_t n) {
    return pmm_manager->alloc_pages(n);
}

// free_pages - call pmm->free_pages to free a continuous n*PAGESIZE memory
void free_pages(struct Page *base, size_t n) {
    pmm_manager->free_pages(base, n);
}

// nr_free_pages - call pmm->nr_free_pages to get the size (nr*PAGESIZE)
// of current free memory
size_t nr_free_pages(void) {
    return pmm_manager->nr_free_pages();
}

//lab2 function1 YOUR CODE

/* page_init - 根据设备树提供的物理内存信息建立物理页描述符布局，
 * 将排除 OpenSBI、内核镜像和页描述符数组占用后的完整空闲页交给 pmm_manager */
static void page_init(void) {
    uint64_t mem_base = get_memory_base();
    uint64_t mem_size = get_memory_size();

    // 设备树信息无效时终止初始化
    if (mem_base == 0 || mem_size == 0) {
        panic("page_init: invalid physical memory info from device tree");
    }

    // 设置虚实映射偏移量（va = pa + offset）
    va_pa_offset = PHYSICAL_MEMORY_OFFSET;

    // 物理内存总页数 = 物理内存结束地址对应的页号
    npage = PPN(mem_base + mem_size);

    // 页描述符数组起始地址：内核镜像 end 之后按页对齐
    extern char end[];
    pages = (struct Page *)ROUNDUP((uintptr_t)end, PGSIZE);

    // 输出探测到的物理内存范围
    cprintf("physical memory: base=0x%016lx, size=0x%016lx (%lu MB)\n",
            mem_base, mem_size, (unsigned long)(mem_size / (1024 * 1024)));
    cprintf("npage = %lu, nbase = %lu, pages (va) = 0x%016lx\n",
            (unsigned long)npage, (unsigned long)nbase, (uintptr_t)pages);

    // 将所有物理页描述符初始化为 reserved（不可分配）
    size_t i;
    for (i = 0; i < npage - nbase; i++) {
        SetPageReserved(&pages[i]);
    }

    // 可用物理内存起始地址：页描述符数组结束之后按页对齐
    uintptr_t freemem_pa = ROUNDUP(
        PADDR((uintptr_t)pages + (npage - nbase) * sizeof(struct Page)), PGSIZE);
    size_t nr_free = (size_t)((mem_base + mem_size - freemem_pa) / PGSIZE);

    cprintf("free memory: 0x%016lx ~ 0x%016lx, %lu pages\n",
            freemem_pa, mem_base + mem_size, (unsigned long)nr_free);

    // 存在完整空闲页时才注册到 pmm_manager
    if (nr_free > 0) {
        init_memmap(pa2page(freemem_pa), nr_free);
    }
}

/* pmm_init - initialize the physical memory management */
void pmm_init(void) {
    // We need to alloc/free the physical memory (granularity is 4KB or other size).
    // So a framework of physical memory manager (struct pmm_manager)is defined in pmm.h
    // First we should init a physical memory manager(pmm) based on the framework.
    // Then pmm can alloc/free the physical memory.
    // Now the first_fit/best_fit/worst_fit/buddy_system pmm are available.
    init_pmm_manager();

    // detect physical memory space, reserve already used memory,
    // then use pmm->init_memmap to create free page list
    page_init();

    // use pmm->check to verify the correctness of the alloc/free function in a pmm
    check_alloc_page();

    extern char boot_page_table_sv39[];
    satp_virtual = (pte_t*)boot_page_table_sv39;
    satp_physical = PADDR(satp_virtual);
    cprintf("satp virtual address: 0x%016lx\nsatp physical address: 0x%016lx\n", satp_virtual, satp_physical);
}

static void check_alloc_page(void) {
    pmm_manager->check();
    cprintf("check_alloc_page() succeeded!\n");
}
