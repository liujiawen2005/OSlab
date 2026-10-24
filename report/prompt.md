# Lab2 任务二：Best-Fit 分配器实现使用的 Prompt

## 任务说明
参考 `kern/mm/default_pmm.c` 中的 First-Fit 完整实现，在 `kern/mm/best_fit_pmm.c` 中标有 `lab2` 和 `YOUR CODE` 的位置实现完整的 Best-Fit 物理内存管理模块。

## 提交给 AI 的 Prompt

```
阅读 pmm.h 中的 struct pmm_manager 定义，以及 default_pmm.c 中 First-Fit 分配器
（default_init / default_init_memmap / default_alloc_pages / default_free_pages /
default_nr_free_pages）的完整实现。

在 kern/mm/best_fit_pmm.c 标有 "lab2 function2-2 YOUR CODE" 的位置，实现完整的
Best-Fit 物理内存管理模块：

1. 内部状态：一个按物理地址升序排列的空闲双向链表 free_list，以及空闲页数
   nr_free。每个 struct Page 的 property 字段只在连续空闲块的首页有效，记录
   该块连续页数。
2. best_fit_init：把 free_list 置空、nr_free 清零。
3. best_fit_init_memmap：把一段连续物理页首地址作为块首，设置 property=n，
   清除 PageReserved，引用计数清零，按地址序挂入 free_list，nr_free += n。
4. best_fit_alloc_pages(n)：遍历整个 free_list，在所有 property >= n 的块中
   挑选 property 最小的那一个（Best-Fit 语义）。选中后若块正好 n 页则摘下；
   若大于 n，从块首切出 n 页分配，剩余部分作为新块首（property -= n）插回链表。
   找不到满足块时返回 NULL。
5. best_fit_free_pages(base, n)：把这 n 页标志位清成空闲、引用计数清零，
   按地址序插回 free_list。插入后做两次相邻合并：与前面物理相邻的前块合并
   （前块 property += n），与后面物理相邻的后块合并（本块 property += 后块
   property 并把后块从链表摘除）。最后 nr_free += n。
6. best_fit_nr_free_pages：直接返回 nr_free。

不得修改文件底部已有的 basic_check() 和 best_fit_check() 两个测试函数。
文件末尾注册：

    const struct pmm_manager best_fit_pmm_manager = {
        .name = "best_fit_pmm_manager",
        .init = best_fit_init,
        .init_memmap = best_fit_init_memmap,
        .alloc_pages = best_fit_alloc_pages,
        .free_pages = best_fit_free_pages,
        .nr_free_pages = best_fit_nr_free_pages,
        .check = best_fit_check,
    };

并在 kern/mm/pmm.c 的 init_pmm_manager() 中把 pmm_manager 切换为
&best_fit_pmm_manager。
```

## 设计规则与限制说明
- 分配页数 n 必须 >= 1，否则 panic（防御非法请求）；
- property 只在块首页有效，避免块内页被误判为独立块；
- 链表必须按物理地址有序，否则前后合并无法判断相邻；
- 释放只认物理相邻（地址连续），跨块不能合并；
- PageReserved 页绝不进入空闲链表，也绝不分配出去；
- nr_free 必须随分配/释放准确增减（守恒）；
- 不得修改 basic_check / best_fit_check，保证评分用例不变。

## 测试覆盖
- best_fit_check 在场内同时放 1 页紧块和 2 页松块：先申请 1 页必须选紧块
  （验证"最小满足"而非"第一个满足"），再申请 2 页命中松块；
- check_alloc_page() succeeded 验证基本分配-释放后 nr_free 守恒；
- make grade 注入的 test_best_fit() 验证 pmm_manager 确实指向
  &best_fit_pmm_manager。
