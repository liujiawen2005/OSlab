# 操作系统实验报告

## Lab2：物理内存管理（page_init 与 Best-Fit 分配器）

> 代码仓库参考：https://github.com/liujiawen2005/OSlab/tree/lab2/code
> 报告格式：Markdown（`.md`），以文字说明为主

---

## 实验基本信息

| 项目 | 内容 |
| ---- | ---- |
| **实验名称** | Lab2：物理内存管理（page_init + Best-Fit） |
| **小组成员** | 2411820-刘家文，2414004-王乐，2413994-陈宇凡 |
| **完成日期** | 2026-10-24 |
| **代码路径** | https://github.com/liujiawen2005/OSlab/tree/lab2/code（Makefile、kern/、libs/、tools/） |
| **报告存放** | https://github.com/liujiawen2005/OSlab/tree/lab2/report |
| **评分结果** | `make grade` → Total Score 20/20（[Test 1] Page Init PASS，[Test 2] Best-Fit PASS） |

### 小组分工

| 成员 | 负责的练习 / 模块 |
| ---- | ----------------- |
| 2411820-刘家文 | page_init() 物理内存探测与页描述符初始化 |
| 2414004-王乐 | Best-Fit 分配器实现与管理器切换 |
| 2413994-陈宇凡 | First-Fit 流程梳理、报告撰写与运行调试 |

---

## 一、实验目的

本实验围绕物理内存的发现、初始化和分配展开，完整链路为：

```
pmm_init() -> init_pmm_manager() -> page_init() -> init_memmap() -> alloc_pages()/free_pages()
```

主要目的：

1. 理解物理地址、虚拟地址、物理页号与 `struct Page` 页描述符之间的关系；
2. 从设备树（DTB）读取可用物理内存范围，实现 `page_init()` 建立页描述符数组；
3. 读懂 First-Fit 连续页分配算法的初始化、分配、回收与合并流程；
4. 参考 `default_pmm.c` 自行设计并实现 Best-Fit 分配器，并在 `init_pmm_manager()` 中切换为默认管理器；
5. 通过 `make grade` 的自动测试验证两项功能均正确。

---

## 二、实验环境

- 硬件模拟：QEMU `qemu-system-riscv64`（virt 机器，128MB RAM）
- 交叉工具链：riscv64-unknown-elf-gcc 16.1.0（MSYS2 ucrt64 环境）
- 固件：OpenSBI v1.9 `fw_jump.bin`（位于 `code/tools/opensbi/`）
- 构建：GNU Make 4.4.1，脚本语言 Python 3.12
- 启动地址：内核 0x80200000，DTB 由 OpenSBI 传入

---

## 三、page_init() 的实现

`page_init()` 在 `kern/mm/pmm.c` 中完成物理内存的探测与组织。

函数首先通过 `get_memory_base()` 与 `get_memory_size()` 从设备树解析出可用物理内存的基址与大小；若设备树中没有合法的 memory 节点，则直接 `panic()` 终止启动。本机 QEMU virt 平台探测到的结果是基址 `0x80000000`、大小 `0x8000000`（128MB，结束于 `0x87ffffff`）。

拿到内存范围后，设置虚拟地址与物理地址的偏移量 `va_pa_offset = PHYSICAL_MEMORY_OFFSET`，并计算总页号 `npage = PPN(mem_base + mem_size)`。这里 `npage` 是从物理地址 0 开始编号的页号上限，因此 `npage - nbase = PPN(mem_base)` 才是这段 128MB 实际拥有的页数。本机上 `npage = 557056`、`nbase = 524288`，二者相减正好 32768 页。

页描述符数组 `pages` 的位置是关键约束：它必须紧跟在内核镜像末尾、按 4KB 对齐，且数组自身占用的物理页绝不能进入空闲区。实现上取链接符号 `end`（bss 段结束的虚拟地址），用 `ROUNDUP((uintptr_t)end, PGSIZE)` 对齐后作为 `pages` 的虚拟地址。本机 `end = 0xffffffffc0206078`，对齐后 `pages` 位于 `0xffffffffc0207000`。随后把全部 `npage - nbase` 个描述符的标志位置为 `PageReserved`，表示这些页默认不参与分配。

接下来计算真正交给分配器管理的空闲内存起点：从 `pages` 数组末尾开始，即 `PADDR(pages + (npage - nbase) * sizeof(struct Page))`，再按 4KB 对齐得到 `freemem_pa`。若该起点之后还有完整可用页，则调用当前 `pmm_manager` 的 `init_memmap(freemem_pa, nr_free)`，把这一段连续空闲页注册进空闲链表。本机空闲区为 `0x80347000 ~ 0x88000000`，共 31929 页。

运行日志关键行：

```
(THU.CST) os is loading ...
memory management: best_fit_pmm_manager
physical memory: base=0x0000000080000000, size=0x0000000008000000 (128 MB)
npage = 557056, nbase = 524288, pages (va) = 0xffffffffc0207000
free memory: 0x0000000080347000 ~ 0x0000000088000000, 31929 pages
check_alloc_page() succeeded!
```

---

## 四、First-Fit 分配器的理解（任务一思考题）

First-Fit 实现在 `kern/mm/default_pmm.c`。内部状态是一个按物理地址升序排列的空闲双向链表 `free_list`，加一个空闲页数计数 `nr_free`；每个 `struct Page` 的 `property` 字段只在"连续空闲块的首页"上有效，记录该块一共连续多少页。

**初始化空闲页**：`default_init` 仅把链表置空、计数清零；`default_init_memmap` 把一段连续物理页的首地址描述符作为块首，设置 `property = n`、清除 `PageReserved`、引用计数清零，再按地址序挂入空闲链表，`nr_free += n`。

**分配连续物理页**：`default_alloc_pages(n)` 从链表头顺序扫描，找第一个 `property >= n` 的块。若该块正好 n 页则摘下；若比 n 大，就从块首切出 n 页分配，剩余部分作为新的块首（`property -= n`）留在链表中。一旦遇到第一个够大的块就立即切分，不继续往后找，因此叫首次适应。

**回收页面并合并相邻空闲块**：`default_free_pages(base, n)` 先把这 n 页的标志位清成空闲、引用计数清零，再按地址序插回空闲链表。插入后做两次相邻合并：往前看，若前一个块的末尾正好挨着回收块开头（`page2pa(prev) + prev->property * PGSIZE == page2pa(base)`），就把 n 并到前块 `property`；往后看，若回收块末尾正好挨着后块，就把后块页数并过来并把后块从链表摘除。最后 `nr_free += n`。

**First-Fit 的改进空间**：每次都从链表头开始扫，低地址区间会被反复切分，时间长了在低地址留下大量无法利用的小碎片，而高地址可能还有整段空闲。改进方向一是按地址维护空闲链表时同时按块大小分桶，分配时直接在对应大小的桶里找；二是切分时尽量从大块尾部切，保留前面大块的连续性；更彻底的做法是改用伙伴系统，按 2 的幂组织空闲块，分配与合并都在固定阶数链表间完成。

---

## 五、Best-Fit 分配器的实现（任务二）

Best-Fit 实现在 `kern/mm/best_fit_pmm.c`，结构与 First-Fit 平行：同样维护地址有序的空闲双向链表和 `nr_free`，`property` 语义相同。差异只在分配策略。

`best_fit_alloc_pages(n)` 遍历整个空闲链表，在所有 `property >= n` 的块中挑选 `property` 最小的那一个。这样切出去后剩余残块最小，不会浪费大段连续空间。选中块后与 First-Fit 一样处理：整 n 页则摘下，多出部分分裂为新块首插回链表。`best_fit_free_pages` 的回收合并流程与 First-Fit 完全一致，都是按地址序插入并向前后两个方向合并。

实现完成后在 `pmm.c` 的 `init_pmm_manager()` 中切换为 `pmm_manager = &best_fit_pmm_manager;`，内核启动日志中的 `memory management: best_fit_pmm_manager` 确认切换生效。

测试函数 `best_fit_check` 故意在场内同时放一块 1 页紧块和一块 2 页松块，先申请 1 页、再申请 2 页。若分配器真的选"最小满足块"，第一次就选中 1 页紧块，第二次才命中 2 页松块——测试通过即证明 Best-Fit 语义正确。

**Best-Fit 的改进空间**：每次都要全表扫描，时间复杂度是 O(链表块数)；且为凑"最小满足"会切出很多很小的残块，小到以后谁都用不上。改进方向一是把空闲块按 `property` 大小组织成排序结构（按大小分桶的数组或树），分配时按 n 二分查找最小满足块；二是设置残块下限，切完后剩余太小的块干脆整块给出，避免制造无用碎片；三是结合伙伴系统，块大小天然对齐到 2 的幂，Best-Fit 的扫描与碎片问题都会被弱化。

---

## 六、扩展思考题：OS 如何获取可用物理内存范围

如果操作系统无法提前知道硬件的可用物理内存范围，通用做法是依赖引导固件提供的硬件描述。本实验中 OpenSBI 作为 M-mode 固件，在跳转到内核之前会把一块设备树（Device Tree Blob, DTB）的物理地址通过 a1 寄存器传给 S-mode 内核；内核启动代码把这个地址保存到 `boot_dtb`，`dtb.c` 提供的 `get_memory_base()` 与 `get_memory_size()` 就是从 DTB 的 `/memory` 节点解析出 `reg` 属性中的基地址和大小。DTB 本身还描述了 RAM 之外保留给固件、设备 MMIO 的区域，内核据此排除不可用区间。

在没有 DTB 的平台上，替代办法包括：编译时硬编码已知机器的内存映射表（RISC-V virt 平台的 128MB@0x80000000 就是约定）、运行时逐段探测（按页向候选地址写入再回读，能通的视为 RAM，但慢且危险）、或者由引导协议（Linux Boot Protocol、EFI）把内存映射表作为启动参数传给内核。本实验采用的就是 OpenSBI + DTB 这条标准路径。

---

## 七、实验结果

`make grade` 自动注入测试并运行：

```
check page init:   OK
check best fit:    OK
Total Score: 20/20
```

运行末尾输出：

```
[Test 1] Page Init: PASS
[Test 2] Best-Fit: PASS
```

---

## 八、实验中遇到的问题

本机使用新版 QEMU 11.1.2，引导阶段出现两个与课程旧环境不同的问题。

第一，`-device loader` 加载内核后 OpenSBI 不再自动设置跳转地址，日志中 OpenSBI 横幅之后没有任何内核输出，CPU 停在 OpenSBI 内部循环。通过 QEMU 的 `-d in_asm,cpu` 指令日志确认内核入口 `0x80200000` 始终未被执行。解决：改用官方 OpenSBI v1.9 的 `fw_jump` 固件，其跳转地址在编译时固定为 `0x80200000`，不依赖 QEMU 的 fw_dynamic 机制。

第二，切换 satp 开启 Sv39 后，由于新版 QEMU 对 satp 写入立即生效，而启动页表原本只有 KERNBASE 一项映射，CPU 在物理地址处继续取指会立刻缺页。解决：在 `boot_page_table_sv39` 中补充 VPN[2]=2 的 1GB 恒等映射（虚拟 `0x80000000-0xbfffffff` 恒等映射到物理 `0x80000000-0xbfffffff`），让切换 satp 后到跳虚拟地址之间的几条指令能正常取指。

两处适配只影响引导流程，不改变 page_init 与 Best-Fit 的实验逻辑。
