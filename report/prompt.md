# 操作系统实验 Lab 4：AI 协作提示词

> 实验主题：Sv39 页映射、内核线程创建与 FIFO/轮转调度  
> 实验指导书：http://oslab.mobisys.top/lab2026/_book/lab4/lab4.html  
> 适用材料：`lab4/code` 源码、`tools/test_lab4.c`、`tools/grade.sh`、实验报告模板。  
> 使用说明：先发送“总控提示词”，再按“页映射 → 线程创建 → 调度切换 → 扩展题 → 评分与报告”的顺序发送。每次附上对应源码和断言，不要只给函数名。

## 一、使用原则与任务边界

本次 Lab 4 要在已有物理内存管理和中断框架上，完成三处标有 `LAB4` / `YOUR CODE` 的代码，并回答扩展题。提示词采用四段式：

- **[PROMPT]**：这次要 AI 写或解释的函数、行为和边界。
- **[RELY]**：必须阅读的指导书页面、源码、宏和测试断言。
- **[GUARANTEE]**：可以对照源码或 `make grade` 检查的结果。
- **[SPECIFICATION]**：前置条件、完成标准，以及不能改动的接口。

**证据优先级：** 当前 `lab4/code` 里的函数签名、断言和 `grade.sh` → 2026 指导书 Lab4 章节 → 已有运行日志 → 一般 ucore 写法。指导书旧片段若与当前 `assert` 冲突，以源码为准并明确指出冲突。

本实验不做缺页按需分配、页面置换、用户地址空间复制和进程回收。`do_exit` 保持 `panic("process exit!!.\n")`。`copy_mm` 只做内核线程占位。不要调用 `libs/riscv.h` 里的 `lsatp`，它写的是 Sv32 的 `satp`。

执行链：

```text
kern_init → pmm_init → pic_init/idt_init → vmm_init → proc_init → cpu_idle
    → schedule → proc_run → switch_to → forkret → forkrets → __trapret
    → kernel_thread_entry → init_main
```

## 二、总控提示词：实验规划与协作规范

```text
[PROMPT]
你是一名熟悉 RISC-V Sv39、ucore 进程管理和 QEMU 的操作系统实验助教。请基于 2026 年 Lab4 指导书和我给出的 lab4/code，帮我完成进程管理实验。

先核对三项目标：页表映射维护、内核线程创建、内核线程调度。列出必须修改的函数、它们的依赖顺序、对应测试和报告章节。后续每次只完成我指定的一个函数组，改完说明不变量和如何用现有 assert 自检。不要提前实现缺页、置换或用户进程。

[RELY]
指导书：http://oslab.mobisys.top/lab2026/_book/lab4/lab4.html
以及 lab4_1_goals、lab4_2_1_exercises、lab4_3_3_multilevel_page_tables、lab4_4_1_pcb、lab4_4_2_3_sched_run_kthread、lab4_5_labs_requirement。
代码：kern/mm/pmm.c、kern/mm/pmm.h、kern/process/proc.c、kern/process/proc.h、kern/process/switch.S、kern/process/entry.S、kern/schedule/sched.c、kern/trap/trapentry.S、kern/sync/sync.h、tools/test_lab4.c、tools/grade.sh、tools/inject.py。
标记：源码中的 “LAB4 function1/2/3 YOUR CODE”。提交时把 YOUR CODE 换成学号，逻辑不变。

[GUARANTEE]
给出按依赖排序的任务清单。每项写明文件、函数、完成判据和 make grade 中对应的检查项。区分“分析已有代码”和“需要填写的函数”。

[SPECIFICATION]
Pre-Condition：已有 Lab2 物理页分配和 Lab3 中断入口，Lab4 工程能进入 kern_init。
Post-Condition：计划覆盖 page_insert/page_remove、alloc_proc 到 proc_init、cpu_idle/schedule/proc_run/switch_to，以及两道扩展题。
Requirements：以当前源码的签名和测试为准；不得虚构运行结果；未经要求不改 Makefile、get_pte、forkrets、kernel_thread_entry；一次只改一个模块。
```

## 三、提示词 1：功能模块一，页映射建立与撤销

```text
[PROMPT]
在 kern/mm/pmm.c 已有 get_pte 之后实现 page_remove_pte、page_remove 和 page_insert。不要重写 get_pte。

page_insert(pgdir, page, la, perm) 把物理页挂到线性地址 la：
- get_pte(pgdir, la, 1) 返回 NULL 时返回 -E_NO_MEM，不留下半成品映射；
- la 尚无有效映射：page_ref_inc(page) 后写入叶子 PTE；
- la 已映射到另一物理页：先增加新页引用，再撤掉旧映射；旧页引用减到 0 才 free_page；
- la 已映射到同一物理页：只更新权限，引用计数和空闲页数都不变。
写入前满足 Sv39 叶子规则：没有 R 也没有 X，或者带了 W，就补上 PTE_R。pte_create 会自己或上 PTE_V。最后 tlb_invalidate(pgdir, la)。
不要给上一级目录项增加 PTE_U、PTE_R、PTE_W、PTE_X。

page_remove(pgdir, la) 必须幂等。中间页表不存在，或槽位存在但 PTE_V 为 0，都直接返回。有映射时清 PTE、按地址 sfence.vma、引用减 1，减到 0 再 free_page。

[RELY]
kern/mm/pmm.c 的 get_pte、check_pgdir；kern/mm/pmm.h 的 pte_create、ptd_create、pte2page、page_ref_inc、page_ref_dec、tlb_invalidate。
权限位：PTE_V、PTE_R、PTE_W、PTE_X、PTE_U。
tools/test_lab4.c 的 test_lab4_pmm_basic。
check_pgdir 中的断言：(boot_pgdir_va[0] & (PTE_V|PTE_U|PTE_R|PTE_W|PTE_X)) == PTE_V。
指导书较旧片段若写 boot_pgdir[0] & PTE_U，以当前 check_pgdir 为准，用户位只出现在叶子项。

[GUARANTEE]
check_pgdir() 打印 succeeded；评分输出 [Test PMM] PASS。
覆盖：首次插入 ref==1；把 PGSIZE 从 p2 换成 p1 后 p1 的 ref==2、p2 的 ref==0，且 U/W 被清掉；同一页只改成 PTE_W 时 ref 仍为 1 且 nr_free_pages 不变；对同一地址连续 page_remove 两次后相关 ref 为 0。

[SPECIFICATION]
Pre-Condition：get_pte 已能分配并回滚中间级页表；alloc_page、free_page 可用。
Post-Condition：只改叶子映射和引用计数，启动阶段的 1GB 大页映射保持不动。
Requirements：中间页表分配失败由 get_pte 清理，page_insert 不要再次释放同一张页表页。不要用 lsatp。替换映射时必须先增加新页引用，再移除旧页，避免新旧碰巧相关时把仍需使用的页释放掉。
```

## 四、提示词 2：功能模块二，内核线程创建与初始化

```text
[PROMPT]
在 kern/process/proc.c 完成 alloc_proc、copy_mm、copy_thread、do_fork、kernel_thread、proc_init。复用文件中已有的 get_pid、hash_proc、setup_kstack、put_kstack、wakeup_proc、forkret，不要改它们的语义。

alloc_proc：kmalloc 一块 proc_struct，失败返回 NULL。成功则 memset 为 0，再设置 state=PROC_UNINIT、pid=-1、pgdir=boot_pgdir_pa，并对 list_link、hash_link 做 list_init。mm、tf、parent、context 保持为空或 0。

proc_init：
- 初始化 proc_list 和全部哈希桶；
- idleproc 不是 fork 出来的，它就是当前启动现场：pid=0，PROC_RUNNABLE，kstack=(uintptr_t)bootstack，need_resched=1，名字 "idle"，parent、mm、tf 为空；
- idleproc 不要插入 proc_list；nr_process 加 1；current=idleproc；
- kernel_thread(init_main, "Hello world!!", 0) 创建 1 号线程，失败则 panic；find_proc 得到 initproc，名字设为 "init"。

kernel_thread：在栈上构造 trapframe。从当前线程抄 gp、tp；s0=fn，s1=arg；status 在当前 sstatus 上置 SSTATUS_SPP 和 SSTATUS_SPIE，并清除 SSTATUS_SIE；epc=kernel_thread_entry。然后 do_fork(clone_flags|CLONE_VM, 0, &tf)。参数放在 s1，不要依赖 a0 传递。

do_fork：nr_process 达到 MAX_PROCESS 时返回 -E_NO_FREE_PROC。否则 alloc_proc，parent=current，setup_kstack，copy_mm，copy_thread。关中断后再次检查进程数，分配 pid，hash_proc，插入 proc_list 尾部，nr_process++，wakeup_proc。失败时只释放已经拿到的资源：有内核栈才 put_kstack，然后 kfree PCB。wakeup_proc 要求状态还不是 PROC_RUNNABLE，所以新线程在唤醒前必须保持 PROC_UNINIT。

copy_thread：trapframe 放在内核栈顶部 stack_top-sizeof(trapframe)。复制传入的 tf，把 a0 置 0。esp==0 时 tf->gpr.sp=stack_top，否则用 esp。context.ra=(uintptr_t)forkret，context.sp 指向这块 trapframe，而不是 stack_top。这样 switch_to 的 ret 进入 forkret 时，C 函数先减小 sp，栈帧落在 trapframe 下方。

copy_mm：本实验不复制用户地址空间。current->mm==NULL 时返回 0，否则返回 -E_INVAL。不要给 proc 分配 mm。

[RELY]
kern/process/proc.h 的 proc_struct、context、PROC_UNINIT、PROC_RUNNABLE。
kern/process/entry.S：move a0, s1；jalr s0；jal do_exit。
kern/trap/trapentry.S：forkrets 把 sp 设为 a0 后跳 __trapret。
kern/mm/memlayout.h：KSTACKPAGE=2，KSTACKSIZE=8192。
kern/mm/pmm.h：CLONE_VM、bootstack。
tools/test_lab4.c 的 test_lab4_process_create。该测试插在 proc_init 末尾、第一次 schedule 之前。

[GUARANTEE]
proc_init 返回前满足：current==idleproc，idle 的 pid 为 0、状态可运行、kstack 等于 bootstack、parent 和 mm 和 tf 为空、runs 为 0、flags 为 0、context.ra 为 0。
initproc 的 pid 为 1、状态可运行、parent 是 idleproc、mm 为空，context.ra 和 context.sp 非 0。kernel_thread 返回正的 pid。

[SPECIFICATION]
Pre-Condition：kmalloc、alloc_pages、local_intr_save、list_add_before、set_proc_name、find_proc 可用。
Post-Condition：系统中至少有 idleproc 和 initproc。initproc 第一次被切换进去时从 forkret 进入，而不是从 switch_to 的调用点返回。
Requirements：不要把 idleproc 也走 copy_thread。不要在 alloc_proc 里把状态设成 RUNNABLE。a0 清零是 fork 返回值约定，线程参数仍从 s1 恢复。不要在 copy_mm 里实现用户页表复制。
```

## 五、提示词 3：功能模块三，调度与上下文切换

```text
[PROMPT]
实现 cpu_idle、schedule、proc_run，以及 kern/process/switch.S 中的 switch_to。

cpu_idle 不返回。循环中若 current->need_resched 为真就调用 schedule。proc_init 已把 idle 的该标志置 1，所以进入 cpu_idle 后不必等时钟中断就会让出 CPU。

schedule 全程用 local_intr_save/restore 包住：
- 清除 current->need_resched；
- current 是 idleproc 时，从 proc_list 表头向后找；否则从 current->list_link 的下一个开始找，走到表尾再绕回；
- 只选择 state==PROC_RUNNABLE 的线程；
- 扫完没有其他就绪线程时，next 保持为 idleproc；
- next->runs++；仅当 next!=current 时调用 proc_run。不要对当前线程 switch_to 到自己。

proc_run 也关中断。目标就是 current 时直接返回。否则保存旧线程，把 current 改为目标，按 enable_paging 的方式写 satp 后 flush_tlb，再 switch_to(&prev->context, &proc->context)。
satp 必须是 Sv39：((uintptr_t)SATP_MODE_SV39 << 60) | (proc->pgdir >> RISCV_PGSHIFT)。
这与 pmm.c 里 enable_paging 的 0x8000000000000000 | (boot_pgdir_pa >> RISCV_PGSHIFT) 一致。禁止调用 lsatp。
local_intr_restore 放在 switch_to 之后：切走的线程要等被切回来才恢复自己栈上的中断标志。新线程第一次不从 proc_run 返回，它的中断开关由 trapframe 里的 SPIE 经 sret 完成。

switch_to：a0 指向旧 context，a1 指向新 context。按 proc.h 中 struct context 的顺序保存并恢复 ra、sp、s0 到 s11，每项 REGBYTES 字节，最后 ret。只保存被调用者保存寄存器。switch_to 不判断调度策略，也不写 satp。

[RELY]
kern/schedule/sched.c、kern/process/proc.c、kern/process/proc.h 的 struct context、kern/process/switch.S。
kern/mm/pmm.c 的 enable_paging。kern/sync/sync.h。libs/riscv.h 中 SATP_MODE_SV39 为 8，以及 lsatp 使用 SATP32_MODE 的事实。
kern/process/entry.S、kern/trap/trapentry.S 的 forkrets 和 __trapret。
tools/test_lab4.c 的 test_lab4_schedule：在 init_main 开头再创建两个线程，主动 need_resched 并 schedule，要求两个线程都运行过。
grade.sh 还要匹配：this initproc, pid = 1, name = "init"；To U: "Hello world!!".；To U: "en.., Bye, Bye. :)"。

[GUARANTEE]
make grade 中 check Schedule 为 OK，并出现上述三行输出。[Test Schedule] PASS 由注入的测试打印。
init_main 返回后 do_exit panic 是预期现象，评分只检查字符串是否出现。

[SPECIFICATION]
Pre-Condition：proc_init 已建立 idleproc 与 initproc，copy_thread 已把新线程的 context.ra 设为 forkret。
Post-Condition：idleproc 能通过 schedule 把 CPU 交给 initproc；测试线程之间按链表顺序轮转。
Requirements：idleproc 不在 proc_list 上，查找时要单独从链表头开始。临界区内不能依赖时钟中断来改 current。不要在 switch_to 里恢复 a0–a7 或 t 寄存器。不要把调度策略改成优先级队列。
```

## 六、提示词 4：扩展题，关中断与多级页表

```text
[PROMPT]
不要改代码。根据当前源码回答 Lab4 扩展题，答案要能直接放进实验报告。

第一题：说明 local_intr_save(intr_flag) 与 local_intr_restore(intr_flag) 如何开关中断。解释它们为什么能嵌套：内层 restore 不会把外层已经关掉的中断打开。说明这组宏和 kernel_thread 写进 trapframe 的 SIE、SPIE 不是同一层。

第二题：get_pte 中处理 PDX1 和 PDX0 的两段代码为什么相像。结合 Sv32、Sv39、Sv48 的虚拟地址划分和页表级数说明异同。指出两段并不完全相同的地方，尤其是新分配的上一级页何时才写入父项。

第三题：get_pte 把查找和分配放在一个函数里是否合适。给出保持现状的理由，以及以后缺页路径变多时拆成“只查找”和“按需建表”的理由。本实验不要真的拆开。

[RELY]
kern/sync/sync.h 的 __intr_save、__intr_restore。
kern/driver/intr.c：intr_enable 置 SSTATUS_SIE，intr_disable 清除 SSTATUS_SIE。
kern/mm/pmm.c 的 get_pte。kern/mm/mmu.h 的 PDX1、PDX0、PTX 和 PTE_*。
libs/riscv.h：SATP_MODE_SV32=1，SATP_MODE_SV39=8，SATP_MODE_SV48=9。
指导书 lab4_2_1_exercises 的 Challenge，以及 lab4_3_2_page_table_entry、lab4_3_3_multilevel_page_tables。

[GUARANTEE]
每题都有源码依据：指出文件和关键语句，并说明 Sv32 为 10+10+12 共 2 级、Sv39 为 9+9+9+12 共 3 级、Sv48 再多一级。明确当前 get_pte 只服务 Sv39。

[SPECIFICATION]
Pre-Condition：已阅读 get_pte 和 sync.h，不需要重新实现。
Post-Condition：答案能区分“目录项指向下一级”和“叶子项带 R/W/X”。
Requirements：不要把 Sv32 的 lsatp 写成 Sv39 的正确写法。不要声称本实验已经实现缺页处理；vmm_init 里的 check_pgfault 是注释掉的。
```

## 七、提示词 5：编译、QEMU 与 make grade

```text
[PROMPT]
请根据 lab4/code/Makefile 和 tools/grade.sh，告诉我在实验目录中如何编译、启动和评分。说明 make qemu 与 make grade 的差别：grade 会定义 DEBUG_GRADE，并用 tools/inject.py 把 test_lab4_pmm_basic 插到 pmm_init 末尾、把 test_lab4_process_create 插到 proc_init 末尾、把 test_lab4_schedule 插到 init_main 开头。

请列出三项检查的分值、必须出现的字符串，以及 init_main 返回后 process exit!! panic 为什么仍然可以得分。若编译或运行失败，先根据完整日志判断是工具链、链接、断言失败还是调度死循环，再给最小修改。

[RELY]
code/Makefile、code/tools/grade.sh、code/tools/inject.py、code/tools/test_lab4.c、code/kern/init/init.c。
评分项各 10 分：check PMM、check Process Create、check Schedule。满分 30。
make qemu 的普通日志应包含 check_pgdir() succeeded!、check_vmm() succeeded.、this initproc, pid = 1，以及 kernel panic process exit!!。

[GUARANTEE]
给出在 code 目录执行的命令：make、make qemu、make grade。说明成功时的分数行 Total Score: 30/30，并指出哪些输出只在 grade 注入测试后出现。

[SPECIFICATION]
Pre-Condition：已安装 riscv64-unknown-elf-gcc 和 qemu-system-riscv64，当前目录是 lab4/code。
Post-Condition：有一次真实的 grade 输出，或有足够定位失败的日志。
Requirements：不要把预期字符串写成已经跑出来的结果。普通 make qemu 没有 [Test PMM] PASS 是正常的。未经错误日志不要改链接脚本或启动页表。
```

## 八、提示词 6：实验报告与提交前核查

```text
[PROMPT]
请按实验报告模板，把 Lab4 整理成 Markdown 报告。章节包括实验目的、环境、整体逻辑、三个功能模块、扩展题、测试、知识点对照和总结。每个模块写清函数在系统中的作用、最终提示词，以及这些实现如何满足 test_lab4.c 和 check_pgdir 的断言。

报告中的运行结果只能使用我提供的截图和日志。已有证据是：make qemu 打印 initproc 三行问候后在 do_exit panic；make grade 为 PMM、Process Create、Schedule 均为 OK，Total Score: 30/30。截图放在 lab4/images/lab4-qemu.png 和 lab4/images/lab4-grade.png。

[RELY]
实验报告模板、lab4/report.md 若已存在、lab4/prompt.md、指导书 lab4_5_labs_requirement。
源码标记仍可能是 YOUR CODE；提交前改为对应学号，不要为此改函数体。
知识点对照至少包括：页表与地址变换、引用计数、TLB、PCB、context 与 trapframe、调度点、关中断临界区、内核线程共享 boot_pgdir。
未覆盖原理至少包括：缺页、页面置换、用户进程 fork/exec/wait、僵尸回收、抢占时间片、多核运行队列。

[GUARANTEE]
报告能逐项对应三个功能模块和两道扩展题。测试一节嵌入上述两张图，并解释 panic 是 do_exit 的预期行为。提示词使用本文件中的四段式，不另写一套互相矛盾的接口约定。

[SPECIFICATION]
Pre-Condition：代码、grade 输出和截图已经提供。
Post-Condition：报告语句可以追溯到源码、指导书或这次运行结果。
Requirements：不得编造迭代次数、编译器报错或未出现的分数。指导书旧示例与 check_pgdir 冲突时，写明以当前断言为准。不要把本实验写成已经完成用户态进程或页面置换。
```
