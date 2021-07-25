#include <pmm.h>
#include <list.h>
#include <string.h>
#include <default_pmm.h>

/* In the first fit algorithm, the allocator keeps a list of free blocks (known as the free list) and,
   on receiving a request for memory, scans along the list for the first block that is large enough to
   satisfy the request. If the chosen block is significantly larger than that requested, then it is 
   usually split, and the remainder added to the list as another free block.
   Please see Page 196~198, Section 8.2 of Yan Wei Ming's chinese book "Data Structure -- C programming language"
*/
// LAB2 EXERCISE 1: YOUR CODE
// you should rewrite functions: default_init,default_init_memmap,default_alloc_pages, default_free_pages.
/*
 * Details of FFMA
 * (1) Prepare: In order to implement the First-Fit Mem Alloc (FFMA), we should manage the free mem block use some list.
 *              The struct free_area_t is used for the management of free mem blocks. At first you should
 *              be familiar to the struct list in list.h. struct list is a simple doubly linked list implementation.
 *              You should know howto USE: list_init, list_add(list_add_after), list_add_before, list_del, list_next, list_prev
 *              Another tricky method is to transform a general list struct to a special struct (such as struct page):
 *              you can find some MACRO: le2page (in memlayout.h), (in future labs: le2vma (in vmm.h), le2proc (in proc.h),etc.)
 * (2) default_init: you can reuse the  demo default_init fun to init the free_list and set nr_free to 0.
 *              free_list is used to record the free mem blocks. nr_free is the total number for free mem blocks.
 * (3) default_init_memmap:  CALL GRAPH: kern_init --> pmm_init-->page_init-->init_memmap--> pmm_manager->init_memmap
 *              This fun is used to init a free block (with parameter: addr_base, page_number).
 *              First you should init each page (in memlayout.h) in this free block, include:
 *                  p->flags should be set bit PG_property (means this page is valid. In pmm_init fun (in pmm.c),
 *                  the bit PG_reserved is setted in p->flags)
 *                  if this page  is free and is not the first page of free block, p->property should be set to 0.
 *                  if this page  is free and is the first page of free block, p->property should be set to total num of block.
 *                  p->ref should be 0, because now p is free and no reference.
 *                  We can use p->page_link to link this page to free_list, (such as: list_add_before(&free_list, &(p->page_link)); )
 *              Finally, we should sum the number of free mem block: nr_free+=n
 * (4) default_alloc_pages: search find a first free block (block size >=n) in free list and reszie the free block, return the addr
 *              of malloced block.
 *              (4.1) So you should search freelist like this:
 *                       list_entry_t le = &free_list;
 *                       while((le=list_next(le)) != &free_list) {
 *                       ....
 *                 (4.1.1) In while loop, get the struct page and check the p->property (record the num of free block) >=n?
 *                       struct Page *p = le2page(le, page_link);
 *                       if(p->property >= n){ ...
 *                 (4.1.2) If we find this p, then it' means we find a free block(block size >=n), and the first n pages can be malloced.
 *                     Some flag bits of this page should be setted: PG_reserved =1, PG_property =0
 *                     unlink the pages from free_list
 *                     (4.1.2.1) If (p->property >n), we should re-caluclate number of the the rest of this free block,
 *                           (such as: le2page(le,page_link))->property = p->property - n;)
 *                 (4.1.3)  re-caluclate nr_free (number of the the rest of all free block)
 *                 (4.1.4)  return p
 *               (4.2) If we can not find a free block (block size >=n), then return NULL
 * (5) default_free_pages: relink the pages into  free list, maybe merge small free blocks into big free blocks.
 *               (5.1) according the base addr of withdrawed blocks, search free list, find the correct position
 *                     (from low to high addr), and insert the pages. (may use list_next, le2page, list_add_before)
 *               (5.2) reset the fields of pages, such as p->ref, p->flags (PageProperty)
 *               (5.3) try to merge low addr or high addr blocks. Notice: should change some pages's p->property correctly.
 */
free_area_t free_area;

#define free_list (free_area.free_list)
#define nr_free (free_area.nr_free)

static void
default_init(void) {
    list_init(&free_list);
    nr_free = 0;
}

static void
default_init_memmap(struct Page *base, size_t n) {
    assert(n > 0);//判断页数是否大于0，若不大于0直接终止程序执行
    struct Page *p = base;//p赋值为页头
    for (; p != base + n; p ++) {
        assert(PageReserved(p));//如果assert括号内的条件语句的值为0，则直接终止程序执行
        //这里是判断p是否为保留页，即flags是否为PG_reserved（是否为0）
        p->flags = p->property = 0;//if this page is free and is not the first page of free block, p->property should be set to 0.
        SetPageProperty(p);//p->flags should be set bit PG_property (means this page is valid)
        set_page_ref(p, 0);//p->ref should be 0, because now p is free and no reference.
        list_add_before(&free_list, &(p->page_link));//We can use p->page_link to link this page to free_list, (such as: list_add_before(&free_list, &(p->page_link)); )
    }
    base->property = n;//if this page is free and is the first page of free block,p->property should be set to total num of block.
    nr_free += n;//Finally, we should sum the number of free mem block: nr_free+=n
}

static struct Page *
default_alloc_pages(size_t n) {
    assert(n > 0);//判断页数是否大于0，若不大于0直接终止程序执行
    if (n > nr_free) {
        return NULL;//若n超过空闲页数总量，则直接返回NULL
    }
    struct Page *page = NULL;//page是最后的返回值，初始化为NULL
    list_entry_t *le = &free_list;//le为空闲块链表头
    list_entry_t *next;//用于保存链表遍历到的当前节点
    int i;//同样用于链表遍历
    while ((le = list_next(le)) != &free_list) {//遍历链表，直到回到链表头free_list
        struct Page *p = le2page(le, page_link);//将块地址转换成页地址
        if (p->property >= n) {//因为是first-fit，所以遇到property>n的块就选中
        //(4.1.2) If we find this p, then it' means we find a free block(block size >=n), and the first n pages can be malloced.
            page = p;//page保存该空闲页地址
            break;
        }
    }
    if (page != NULL) {//若为NULL则证明遍历一遍链表都没有找到property>n的块，此时page仍为NULL，所以直接return page
        struct Page* temp;//保存遍历时的当前空闲页
        for(i=1;i<=n;i++){//将需要分配的n个空闲页从链表中删除
            next=list_next(le);//链表节点向后移动，next存储
            temp=le2page(le,page_link);//块地址转换成页地址
            SetPageReserved(temp);//设置当前页为保留页
            ClearPageProperty(temp);//清空当前页Property
            //Some flag bits of this page should be setted: PG_reserved =1, PG_property =0
            list_del(le);//删除该节点
            //unlink the pages from free_list
            le=next;//节点移动，继续遍历
        }
        if (page->property > n) {//若该页块大小大于n，即分配完之后还有剩余的空闲页
	        (le2page(le,page_link))->property = page->property - n;//因为此时le指向分配过n个页后的那个页
            //所以将该页的property设置为page->property-n（page是分配该块的首地址）
            //(4.1.2.1) If (p->property >n), we should re-caluclate number of the the rest of this free block,
            //(such as: le2page(le,page_link))->property = p->property - n;)
        }
        nr_free -= n;//空闲总页数-n
        //re-caluclate nr_free (number of the the rest of all free block)
        ClearPageProperty(page);//清空当前页property
        SetPageReserved(page);//设置当前页为保留页
    }
    return page;//返回page（值为NULL或p）
}

static void
default_free_pages(struct Page *base, size_t n) {
    assert(n > 0);  
    assert(PageReserved(base));//检查需要释放的页块是否已经被分配
    list_entry_t *le = &free_list;//le赋值为空闲链表头
    struct Page * p;
    while((le=list_next(le))!=&free_list){//找到释放的位置，即第一个大于base的空闲块
        p=le2page(le,page_link);//块地址转换为页地址
        if(p>base){//找到后跳出循环
            break;
        }
    }
    //according the base addr of withdrawed blocks, search free list, find the correct position
    for(p=base;p<base+n;p++){
        p->flags=0;
        set_page_ref(p,0);
        ClearPageReserved(p);
        ClearPageProperty(p);
        //全部重置
        //reset the fields of pages, such as p->ref, p->flags (PageProperty)
        list_add_before(le,&(p->page_link));//将每一个空闲块对应的链表插入空闲链表中
    }
    //(from low to high addr), and insert the pages. (may use list_next, le2page, list_add_before)
    base->property=n;
    SetPageProperty(base);
    //如果是高位，则向高地址合并
    p=le2page(le,page_link) ;
    if(base+n==p){
        base->property+=p->property;
        p->property=0;
    }
    //如果是低位，则向低地址合并
    le=list_prev(&(base->page_link));
    p=le2page(le,page_link);
    if(le!=&free_list&&p==base-1){//满足条件，未分配则合并
        while(le!=&free_list){
            if(p->property!=0){
                p->property+=base->property;
                base->property=0;
                //不断更新p的property值，并将base->property置为0
                break;
            }
            le=list_prev(le);
            p=le2page(le,page_link);
        }
    }
    //try to merge low addr or high addr blocks. Notice: should change some pages's p->property correctly.
    nr_free+=n;//空闲页数量加n
    return;
}
static size_t
default_nr_free_pages(void) {
    return nr_free;
}

static void
basic_check(void) {
    struct Page *p0, *p1, *p2;
    p0 = p1 = p2 = NULL;
    assert((p0 = alloc_page()) != NULL);
    assert((p1 = alloc_page()) != NULL);
    assert((p2 = alloc_page()) != NULL);

    assert(p0 != p1 && p0 != p2 && p1 != p2);
    assert(page_ref(p0) == 0 && page_ref(p1) == 0 && page_ref(p2) == 0);

    assert(page2pa(p0) < npage * PGSIZE);
    assert(page2pa(p1) < npage * PGSIZE);
    assert(page2pa(p2) < npage * PGSIZE);

    list_entry_t free_list_store = free_list;
    list_init(&free_list);
    assert(list_empty(&free_list));

    unsigned int nr_free_store = nr_free;
    nr_free = 0;

    assert(alloc_page() == NULL);

    free_page(p0);
    free_page(p1);
    free_page(p2);
    assert(nr_free == 3);

    assert((p0 = alloc_page()) != NULL);
    assert((p1 = alloc_page()) != NULL);
    assert((p2 = alloc_page()) != NULL);

    assert(alloc_page() == NULL);

    free_page(p0);
    assert(!list_empty(&free_list));

    struct Page *p;
    assert((p = alloc_page()) == p0);
    assert(alloc_page() == NULL);

    assert(nr_free == 0);
    free_list = free_list_store;
    nr_free = nr_free_store;

    free_page(p);
    free_page(p1);
    free_page(p2);
}

// LAB2: below code is used to check the first fit allocation algorithm (your EXERCISE 1) 
// NOTICE: You SHOULD NOT CHANGE basic_check, default_check functions!
static void
default_check(void) {
    int count = 0, total = 0;
    list_entry_t *le = &free_list;
    while ((le = list_next(le)) != &free_list) {
        struct Page *p = le2page(le, page_link);
        assert(PageProperty(p));
        count ++, total += p->property;
    }
    assert(total == nr_free_pages());

    basic_check();

    struct Page *p0 = alloc_pages(5), *p1, *p2;
    assert(p0 != NULL);
    assert(!PageProperty(p0));

    list_entry_t free_list_store = free_list;
    list_init(&free_list);
    assert(list_empty(&free_list));
    assert(alloc_page() == NULL);

    unsigned int nr_free_store = nr_free;
    nr_free = 0;

    free_pages(p0 + 2, 3);
    assert(alloc_pages(4) == NULL);
    assert(PageProperty(p0 + 2) && p0[2].property == 3);
    assert((p1 = alloc_pages(3)) != NULL);
    assert(alloc_page() == NULL);
    assert(p0 + 2 == p1);

    p2 = p0 + 1;
    free_page(p0);
    free_pages(p1, 3);
    assert(PageProperty(p0) && p0->property == 1);
    assert(PageProperty(p1) && p1->property == 3);

    assert((p0 = alloc_page()) == p2 - 1);
    free_page(p0);
    assert((p0 = alloc_pages(2)) == p2 + 1);

    free_pages(p0, 2);
    free_page(p2);

    assert((p0 = alloc_pages(5)) != NULL);
    assert(alloc_page() == NULL);

    assert(nr_free == 0);
    nr_free = nr_free_store;

    free_list = free_list_store;
    free_pages(p0, 5);

    le = &free_list;
    while ((le = list_next(le)) != &free_list) {
        struct Page *p = le2page(le, page_link);
        count --, total -= p->property;
    }
    assert(count == 0);
    assert(total == 0);
}

const struct pmm_manager default_pmm_manager = {
    .name = "default_pmm_manager",
    .init = default_init,
    .init_memmap = default_init_memmap,
    .alloc_pages = default_alloc_pages,
    .free_pages = default_free_pages,
    .nr_free_pages = default_nr_free_pages,
    .check = default_check,
};

