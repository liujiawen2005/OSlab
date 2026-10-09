# 操作系统实验 Lab 1：AI 协作提示词

> 实验主题：RISC-V 最小内核启动、初始化及 QEMU/GDB 调试  
> 实验指导书：[2026 操作系统实验手册](http://oslab.mobisys.top/lab2026/_book/)  
> 适用材料：`lab1` 源码、`0_environment_setup.md`、`3_startdash.md`、实验报告模板及实际终端日志。  
> 使用说明：先发送“总控提示词”，再按实验进度依次发送各专题提示词。每次附上相关源码、日志或截图，使 AI 的结论能够核查。

## 一、使用原则与任务边界

本次 Lab 1 以**理解既有最小内核的启动过程、分析两项练习、完成编译运行与调试验证**为主，不预设需要实现后续实验的进程调度、虚拟内存或中断管理功能。提示词采用四段式结构：

- **[PROMPT]**：明确向 AI 提出的任务和预期回答。
- **[RELY]**：列出必须依据的手册、源码、命令和观测证据。
- **[GUARANTEE]**：指定可检查的交付成果。
- **[SPECIFICATION]**：规定前置条件、完成标准和约束。

**证据优先级：** 课程实验手册与提交要求 → 当前源码和 Makefile → 实际运行日志、截图和调试结果 → AI 的一般知识。若材料冲突，应指出冲突并请求补充，不得凭经验替换当前工程的代码事实。下文地址和命令来自参考记录，运行前应根据自己的工具链及源码版本复核。

## 二、总控提示词：实验规划与协作规范

```text
[PROMPT]
你是一名熟悉 RISC-V、操作系统内核启动、GNU 工具链、QEMU 和 GDB 的操作系统实验助教。请基于我提供的 2026 年课程实验手册、Lab1 源码和报告模板，指导我完成本次 Lab 1。

请先核对实验目标、两项练习、需要完成的代码分析与调试任务，以及提交材料；随后给出按依赖顺序排列的执行清单。后续答复采用“实验目标—原理—具体操作—预期现象—实际验证—故障处理”的结构，命令必须说明在哪个目录、哪个终端执行。对新接触内核调试的本科生，解释要准确、易懂且不省略关键步骤。

[RELY]
实验指导书：http://oslab.mobisys.top/lab2026/_book/
现有材料：Lab1 工程源码、0_environment_setup.md、3_startdash.md、实验报告模板、老师的提交要求。
核心对象：code/Makefile、code/tools/kernel.ld、code/kern/init/entry.S、code/kern/init/init.c、code/libs/sbi.c 及控制台输出模块。

[GUARANTEE]
输出一份带完成判据的 Lab1 任务清单；标明练习一与练习二的目标、所需文件、验证方法、截图位置及报告对应章节；明确哪些属于代码分析、哪些属于实际执行验证。

[SPECIFICATION]
Pre-Condition：已提供指导书和当前代码，实验尚未完成或需要核对。
Post-Condition：形成可执行、可追溯且覆盖课程要求的实验计划。
Requirements：优先核实资料；不得虚构已执行命令、截图、性能数据或实验结果；未读取到的手册章节应明确说明；未经确认，不修改与 Lab1 无关的文件；每一步完成后等待我的终端反馈再判断是否成功。
```

## 三、提示词 1：内核项目结构与完整启动链路

```text
[PROMPT]
请阅读 Lab1 的 Makefile、链接脚本、汇编入口、C 初始化函数和输出模块，分析最小内核的组成及控制流。按时间顺序解释“交叉编译—链接—生成镜像—QEMU 加载—复位代码—OpenSBI—内核汇编入口—C 初始化—字符输出”，并指出每一步对应的源码、执行环境与上一阶段的交接条件。

请将功能模块和两项练习建立对应关系，解释 QEMU、OpenSBI、实验内核各自承担的职责。最后给出不超过 10 项的源码阅读顺序。

[RELY]
code/Makefile、code/tools/function.mk、code/tools/kernel.ld。
code/kern/init/entry.S、code/kern/init/init.c。
code/kern/libs/stdio.c、code/kern/driver/console.c、code/libs/sbi.c。

[GUARANTEE]
形成准确的启动阶段清单、文件与功能对应表，以及两项练习的知识点定位；能够解释执行流程如何从固件进入内核。

[SPECIFICATION]
Pre-Condition：已获得当前实验源码。
Post-Condition：能够结合文件路径口头说明最小内核为什么能够启动并输出信息。
Requirements：以源码为准；区分“将镜像装入内存”和“将控制权交给入口”；不得将后续实验的模块视为本次已完成的功能。
```

## 四、提示词 2：编译构建、链接地址与 QEMU 启动

```text
[PROMPT]
请分析当前 Lab1 工程的构建流程，并指导我在已有 Linux/WSL 环境中逐步完成编译和 QEMU 启动。解释交叉编译器、汇编、链接脚本、objcopy 的作用；核对内核入口符号、链接基地址及镜像格式；结合 Makefile 给出可直接执行的检查、构建、运行命令。

如果构建失败、固件正常启动但内核没有输出，或 QEMU 与 Makefile 参数不兼容，请先结合完整错误日志定位原因，再给出最小修复方案和修复前后的差异。未经错误证据支持，不要直接替换整个 Makefile。

[RELY]
code/Makefile、code/tools/function.mk、code/tools/kernel.ld。
参考记录中的链接约定：OUTPUT_ARCH(riscv)、ENTRY(kern_entry)、BASE_ADDRESS = 0x80200000。
参考构建产物：bin/kernel（ELF）和 bin/ucore.img（原始镜像）。
工具：RISC-V 交叉编译工具链、QEMU、objdump、readelf、GDB。

[GUARANTEE]
输出命令、命令含义、预计产物与判断成功的证据；说明 ELF 与原始镜像的用途，解释链接地址、加载地址、执行入口之间的关系。

[SPECIFICATION]
Pre-Condition：工具链已安装或正在检查；源码位于项目的 code/ 目录。
Post-Condition：完成构建并验证内核能否启动，或获得足以定位失败的日志。
Case 1：找不到工具时，先核查 PATH、工具版本和 RISC-V 架构支持。
Case 2：启动后无内核输出时，核查 -kernel/-bios、链接地址和实际固件日志。
Case 3：需要修改构建配置时，先解释必要性，限制修改范围并保留可回退方案。
Requirements：不要将命令的预期输出写成实际结果；不假设所有环境都具有相同版本的 QEMU 或 OpenSBI。
```

## 五、提示词 3：练习一——汇编入口与内核栈初始化

```text
[PROMPT]
针对练习一，请逐条分析 code/kern/init/entry.S 中 kern_entry 的指令，重点解释“la sp, bootstacktop”和“tail kern_init”的语义、作用和顺序。结合内存布局和栈相关宏定义，计算启动栈大小，说明 bootstack、bootstacktop 和 sp 的关系，并解释为什么进入 C 函数前必须设置栈指针。

然后给出使用反汇编和 GDB 验证的详细步骤：如何在 kern_entry 设置断点、查看 sp 和 pc、单步执行、验证控制流进入 kern_init，以及应截取哪些证据。请根据我反馈的实际寄存器值作结论。

[RELY]
code/kern/init/entry.S、code/kern/mm/memlayout.h、code/kern/mm/mmu.h、code/kern/init/init.c。
参考代码：la sp, bootstacktop；tail kern_init。
参考宏：PGSIZE = 4096；KSTACKPAGE = 2；KSTACKSIZE = 8192 字节。
参考入口声明：int kern_init(void) __attribute__((noreturn))。

[GUARANTEE]
形成逐指令解释、启动栈内存布局说明、栈大小计算过程、寄存器验证步骤和可写入报告的练习一结论。

[SPECIFICATION]
Pre-Condition：已能启动内核或通过 GDB 连接到暂停的 QEMU。
Post-Condition：使用实际断点和寄存器读数证明栈指针设置以及到 C 入口的控制流转移。
Requirements：区分预留栈空间与设置 sp；说明 RISC-V 栈通常向低地址增长；区分 tail 与普通 call 的返回地址行为；la 的伪指令展开必须以当前反汇编结果为准。参考值 sp = 0x80203000、pc = 0x8020000a 仅作为既有记录，不可未经复现就冒充本次输出。
```

## 六、提示词 4：C 初始化、BSS 清零与控制台输出

```text
[PROMPT]
请分析 kern_init 从进入函数到持续循环的执行顺序，说明 memset(edata, 0, end - edata) 的目的、edata/end 链接符号的来源，以及为何裸机内核需要确保 BSS 区域为零。随后沿 cprintf 到 SBI 字符输出的实际调用链追踪启动信息如何显示在 QEMU 终端，并说明 ecall 的作用及内核和 OpenSBI 的职责边界。

请给出能证明“内核已输出启动信息”的运行验证方法，以及如果需要验证 BSS 清零本身，应额外观察什么；区分源码可推导的结论与已实际完成的动态测试。

[RELY]
code/kern/init/init.c、code/tools/kernel.ld、code/libs/string.c。
code/kern/libs/stdio.c、code/libs/printfmt.c、code/kern/driver/console.c、code/libs/sbi.c。
参考语句：memset(edata, 0, end - edata)；cprintf("%s\n\n", message)；while (1) ;。
参考接口：cprintf、vcprintf、cons_putc、sbi_console_putchar、sbi_call。

[GUARANTEE]
输出 kern_init 执行顺序、BSS 清零范围说明、按实际源码确定的字符输出调用链，以及“静态分析证据/动态验证证据”对照清单。

[SPECIFICATION]
Pre-Condition：已掌握汇编入口和 C 函数的衔接关系。
Post-Condition：能够解释启动消息产生的全过程，知道哪些现象能支持哪些结论。
Requirements：不可把终端出现启动文字等同于独立证明 BSS 清零；说明本工程使用的 SBI 接口属于何种调用方式，以源码为准，不混用不同版本的 SBI 调用约定；不得将普通用户态 printf 的实现机制套用到裸机内核。
```

## 七、提示词 5：练习二——CPU 复位到内核入口的 GDB 跟踪

```text
[PROMPT]
针对练习二，请指导我在两个终端中启动 QEMU 调试模式并连接 GDB，从模拟 CPU 的初始 PC 开始，逐条解释复位代码最初六条指令，再通过断点确认执行流到达 OpenSBI 和 kern_entry。

请明确每条操作在哪个终端执行、对应 GDB 命令、应观察的寄存器或内存、预期现象和截图时机。重点分析 auipc、addi、csrr、ld、jr 的行为，以及 t0、a0、a1、a2、pc 的作用。每次只给我一组可执行步骤，等我贴出调试结果后再推进。

[RELY]
code/Makefile 中的 debug/gdb 目标；bin/kernel 的 ELF 符号信息。
参考调试模式：QEMU -s -S；GDB 连接 localhost:1234。
参考初始指令：
0x1000: auipc t0,0x0
0x1004: addi  a2,t0,40
0x1008: csrr  a0,mhartid
0x100c: ld    a1,32(t0)
0x1010: ld    t0,24(t0)
0x1014: jr    t0
参考入口：kern_entry = 0x80200000。

[GUARANTEE]
形成“初始 PC—复位代码—固件入口—内核入口”的可复现调试记录；每一阶段都有命令、观察值、含义和截图建议，并能回答练习二相关问题。

[SPECIFICATION]
Pre-Condition：QEMU 已以暂停方式启动，GDB 已加载与运行镜像匹配的符号文件并连接目标。
Post-Condition：依据实际寄存器、单步结果与断点命中情况，验证控制权到达内核入口。
Case 1：查看初始 PC 和 x/6i 反汇编，解释复位阶段指令。
Case 2：执行到 jr t0 前检查 t0，再单步确认跳转目标。
Case 3：在 kern_entry 设置断点并继续，核对 pc、符号及源码位置。
Requirements：将 0x1000、0x80000000、0x80200000 作为本参考环境的待核查地址，不宣称所有 QEMU 平台均相同；区分 QEMU 复位 ROM、OpenSBI 和内核；不要求逐条跟踪整个 OpenSBI；未观察到的加载过程不能写成亲眼验证。
```

## 八、提示词 6：故障诊断与最小修改

```text
[PROMPT]
我将提供构建错误、QEMU 输出、GDB 报错或截图。请使用“现象—证据—可能原因—排查命令—最小修复—复测”的流程帮助我定位问题。先区分编译、链接、镜像加载、固件交接、内核执行和调试器连接中的哪个阶段失败，再按照可能性从高到低给出不超过三项检查。

如果涉及源码或 Makefile 修改，必须给出文件路径、修改位置、最小 diff、修改原因、潜在影响和验证命令；先征求我确认，再处理会影响原有功能的变更。

[RELY]
当前完整命令及工作目录、标准输出/错误输出、工具版本、相关文件片段和最近一次成功操作。
典型问题：找不到 RISC-V 工具、QEMU 内核无输出、-kernel 参数不匹配、GDB 连接失败、符号或断点地址不匹配、终端操作或命令拼写错误。

[GUARANTEE]
给出有证据支持的故障定位建议和可复测的步骤；记录“故障表现—原因—解决方案—最终状态”。

[SPECIFICATION]
Pre-Condition：至少提供一个可核对的错误现象。
Post-Condition：排除错误，或明确下一条最有价值的诊断信息。
Requirements：不得无依据重装全部环境、覆盖工程或删除文件；不得将“可能解决”表述为“已经修复”；不要为了使输出好看而改动内核功能。
```

## 九、提示词 7：实验报告、截图与提交前核查

```text
[PROMPT]
请按照老师给出的实验报告模板和提交要求，检查本次 Lab1 是否完整覆盖两道练习。将已完成的源码分析、构建运行过程、GDB 操作、截图和实验现象组织成内容准确、语言正式且适合本科实验报告的文本。

先生成“实验要求—操作内容—源码依据—运行证据—报告章节”的核对表，再逐项撰写。分别标记已验证、仅从源码分析、尚未验证的结论；对于缺失证据，明确告诉我需要再执行什么命令及截取什么界面，不要编造截图和结果。

[RELY]
实验手册、实验报告模板、提交要求图片、Lab1 源码、Makefile 修改记录、实际终端日志。
参考证据类别：内核成功启动；初始 PC；跳转到 OpenSBI；命中 kern_entry；sp 设置及进入 kern_init。
如实际存在，可核对：report/images/lab1-boot.png、report/images/lab1-gdb-stack-init.png 等截图。

[GUARANTEE]
形成覆盖实验目的、原理、环境、操作、练习分析、测试结果、问题解决和总结的报告提纲与文字；附上缺失证据清单和最终提交检查表。

[SPECIFICATION]
Pre-Condition：已提供实际完成情况和可用证据。
Post-Condition：报告内容能逐项追溯到手册、源码或真实运行结果，并满足模板和提交格式要求。
Requirements：不得把计划步骤写成已完成实验；截图名称以真实文件为准；区分“成功启动内核”和“所有验证通过”；对无法核实的老师要求、版本号、地址、命令和截图明确标注“待确认”。
```

