/**********************************************************************
 * Copyright (c) 2020-2021
 *  Sang-Hoon Kim <sanghoonkim@ajou.ac.kr>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTIABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 **********************************************************************/

#include <stdio.h>
#include <stdlib.h>
#include <strings.h>

#include "types.h"
#include "list_head.h"
#include "vm.h"


/**
 * Ready queue of the system
 */
extern struct list_head processes;

/**
 * Currently running process
 */
extern struct process *current;

/**
 * Page Table Base Register that MMU will walk through for address translation
 */
extern struct pagetable *ptbr;


/**
 * The number of mappings for each page frame. Can be used to determine how
 * many processes are using the page frames.
 */
extern unsigned int mapcounts[];


/**
 * alloc_page(@vpn, @rw)
 *
 * DESCRIPTION
 *   Allocate a page frame that is not allocated to any process, and map it
 *   to @vpn. When the system has multiple free pages, this function should
 *   allocate the page frame with the **smallest pfn**.
 *   You may construct the page table of the @current process. When the page
 *   is allocated with RW_WRITE flag, the page may be later accessed for writes.
 *   However, the pages populated with RW_READ only should not be accessed with
 *   RW_WRITE accesses.
 *
 * RETURN
 *   Return allocated page frame number.
 *   Return -1 if all page frames are allocated.
 */
 //추가
LIST_HEAD(stack);
struct list_head stack;
struct entry {
	struct list_head list;
	int num; //free된 인덱스들
};
void push_stack(int num)
{
	struct entry *node;
	node = (struct entry*)malloc(sizeof(struct entry));
	node->num = num;
	INIT_LIST_HEAD(&node->list);
	list_add(&node->list, &stack);
}
int find_min(void){
    //min을 찾아서 리턴하고 삭제
    struct list_head *ptr, *ptrn;
	struct entry *node;
    int min=1000;
   
    list_for_each_prev(ptr, &stack){ 
		node =list_entry(ptr, struct entry, list);
		if(node->num < min)
            min = node->num;
	}

	list_for_each_safe(ptr, ptrn, &stack){ 
		node =list_entry(ptr, struct entry, list);
		if(node->num == min){
            list_del(&node->list);
            free(node);
            return min;
        }
	}
}

int cnt=-1;
int global_pd_index=0;
int mapcnt_index=0; ////////////////////////////추가
unsigned int alloc_page(unsigned int vpn, unsigned int rw) //vpn을 index로 사용?, 0 ~ 15
{ 
    int pd_index = vpn / NR_PTES_PER_PAGE; //outer의 인덱스
	int pte_index = vpn % NR_PTES_PER_PAGE; //ptes의 인덱스

    if(global_pd_index < pd_index)
        global_pd_index = pd_index;

   struct pte_directory *pd = current->pagetable.outer_ptes[pd_index];

   if(!pd){
       current->pagetable.outer_ptes[pd_index] = malloc(sizeof(struct pte_directory));
   }

    struct pte *pte = &current->pagetable.outer_ptes[pd_index]->ptes[pte_index];

    if(rw == 1){ //액세스 할 수 없도록
		pte->valid = true;
		pte->writable = false;
        pte->private = 0; //원래 read 였음
	}else if(rw == 3){ //rw == RW_WRITE이면 나중에 쓰기 위해 액세스 가능하도록
		pte->valid = true;
		pte->writable = true;
        pte->private = 1; //원래 read,write 였음
	}

    if(list_empty(&stack)){
        cnt++;
        pte->pfn = cnt;
        mapcounts[cnt]++;
        mapcnt_index++;
        return cnt;
    }else{ //free 받은 pfn이 있다면
        int min = find_min();
        pte->pfn = min;
        mapcounts[min]++;
        mapcnt_index++;
        return min;
    }

	// return -1;
}


/**
 * free_page(@vpn)
 *
 * DESCRIPTION
 *   Deallocate the page from the current processor. Make sure that the fields
 *   for the corresponding PTE (valid, writable, pfn) is set @false or 0.
 *   Also, consider carefully for the case when a page is shared by two processes,
 *   and one process is to free the page.
 */
void free_page(unsigned int vpn) //맵카운트가 0일때는 free하고 0보다 크면 아예 반환해버리면 안됨
{
	int pd_index = vpn / NR_PTES_PER_PAGE; //outer의 인덱스
	int pte_index = vpn % NR_PTES_PER_PAGE; //ptes의 인덱스

    struct pte *pte = &current->pagetable.outer_ptes[pd_index]->ptes[pte_index];

    pte->valid = false;
    pte->writable = false;

    mapcounts[pte->pfn]--;
	mapcnt_index--;


    int tmp = pte->pfn;
    pte->pfn = 0;
    pte->valid = false;
    pte->writable = false;

    if(mapcounts[tmp] == 0){ //혼자 쓰고 있던 것
       pte = NULL;
    }


	// if(mapcounts[pte->pfn] >= 1){ //다른 프로세스도 같이 쓰고 있던 페이지
    //     push_stack(pte->pfn); 
    // } else{ //나 혼자 쓰고 있었다면 스택에서 찾아서 지우기?
	// 	struct list_head *ptr, *ptrn;
	// 	struct entry *node;
	// 	int flag=0;
	
	// 	list_for_each_prev(ptr, &stack){ 
	// 		node =list_entry(ptr, struct entry, list);			
	// 		if(node->num == pte->pfn){ //존재하면 삭제
	// 			flag=1;
	// 		}
	// 	}
	// 	if(flag==1){
	// 		list_for_each_safe(ptr, ptrn, &stack){ 
	// 			node =list_entry(ptr, struct entry, list);
	// 			if(node->num == pte->pfn){
	// 				list_del(&node->list);					
	// 			}
	// 		}
	// 	}
		
	// }

    // // push_stack(pte->pfn);
    // pte->pfn = 0;
    // pte->valid = false;
	// pte->writable = false;
}



/**
 * handle_page_fault()
 *
 * DESCRIPTION
 *   Handle the page fault for accessing @vpn for @rw. This function is called
 *   by the framework when the __translate() for @vpn fails. This implies;
 *   0. page directory is invalid
 *   1. pte is invalid
 *   2. pte is not writable but @rw is for write
 *   This function should identify the situation, and do the copy-on-write if
 *   necessary.
 *
 * RETURN
 *   @true on successful fault handling
 *   @false otherwise
 */
bool handle_page_fault(unsigned int vpn, unsigned int rw) //상태가 Invalid일 때(page fault) MMU가 운영체제에게 부탁, copy-on-write 구현
{
    // printf("handling start!\n");
    int pd_index = vpn / NR_PTES_PER_PAGE; //outer의 인덱스
	int pte_index = vpn % NR_PTES_PER_PAGE; //ptes의 인덱스
    struct pte_directory *pd = current->pagetable.outer_ptes[pd_index];
    struct pte *pte = &current->pagetable.outer_ptes[pd_index]->ptes[pte_index];

    int old_pfn=pte->pfn;
    
    int new_pfn=0;

    if(pte->private == 0){ //rw == RW_READ
		return false;
	}else if(pte->private == 1){ //rw == RW_WRITE
        //내용을 다른 page frame에 copy한 뒤에

        //new_pfn 찾기
        if(list_empty(&stack)){
            cnt++;
            new_pfn = cnt;         
        }else{ //free 받은 pfn이 있다면
            int min = find_min();
            new_pfn = min;       
        }
        
        //원래 매핑을 끊고 (free 아닌듯!) 
        pte->pfn = new_pfn;
        //copy한 frame에 연결하고 
        // current->pagetable.outer_ptes[pd_index]->ptes[new_pfn].pfn = new_pfn;
        //w를 켜주고 (PTE update)
        pte->writable = true;
        // current->pagetable.outer_ptes[pd_index]->ptes[new_pfn].writable = true;
        
        mapcounts[old_pfn]--;
        mapcounts[new_pfn]++;
        return true;
	}

}


/**
 * switch_process()
 *
 * DESCRIPTION
 *   If there is a process with @pid in @processes, switch to the process.
 *   The @current process at the moment should be put into the @processes
 *   list, and @current should be replaced to the requested process.
 *   Make sure that the next process is unlinked from the @processes, and
 *   @ptbr is set properly.
 *
 *   If there is no process with @pid in the @processes list, fork a process
 *   from the @current. This implies the forked child process should have
 *   the identical page table entry 'values' to its parent's (i.e., @current)
 *   page table. 
 *   To implement the copy-on-write feature, you should manipulate the writable
 *   bit in PTE and mapcounts for shared pages. You may use pte->private for 
 *   storing some useful information :-)
 */
struct process child;
void switch_process(unsigned int pid) 
//존재하는 프로세스면 context switch(ptbr을 바꿔줘야함), 존재하지 않으면 fork
{
    struct list_head* ptr;
    struct process* prc = NULL;
    int flag=0;
    

    list_for_each(ptr, &processes){
        prc = list_entry(ptr, struct process, list);

        if(prc->pid == pid){ //존재하는 프로세스로 switch
            // printf("find!\n");
            flag=1;
            
            list_del(&prc->list);
                     
            ptbr = &prc->pagetable; 
            list_add_tail(&current->list, &processes);
            current = prc; 
            goto here;   
        }      
    }

here:
    if(flag==0){ //존재하지 않는 프로세스로 switch
        // printf("forked!\n");
        //깊은 복사 -> 포인터를 복사하는게 아니라 하나하나 내용 복사!

        child.pid = pid;

        //copy-on-write
        //current의 w를 끄기 (원래 read였으면 pte->private=0 / write였으면 private=1)
        for(int i=0; i<=global_pd_index; i++){
            for(int j=0; j<16; j++){
                current->pagetable.outer_ptes[i]->ptes[j].writable = false;
            }
        } 

        //mapcount 추가
        for(int i=0; i<mapcnt_index; i++)
            mapcounts[i]++;
        
        for(int i=0; i<=global_pd_index; i++){ //current의 outertable이 몇개까지 있는지!!!
            child.pagetable.outer_ptes[i] = malloc(sizeof(struct pte_directory));
            
            for(int j=0; j<16; j++){ 
                // struct pte *pte = &current->pagetable.outer_ptes[i]->ptes[j];  
                child.pagetable.outer_ptes[i]->ptes[j].writable = false;
                child.pagetable.outer_ptes[i]->ptes[j].valid = current->pagetable.outer_ptes[i]->ptes[j].valid;
                child.pagetable.outer_ptes[i]->ptes[j].pfn = current->pagetable.outer_ptes[i]->ptes[j].pfn; 
                child.pagetable.outer_ptes[i]->ptes[j].private = current->pagetable.outer_ptes[i]->ptes[j].private;              
             
            }
        }

        list_add_tail(&current->list, &processes);
        ptbr = &child.pagetable;
        current = &child;
        
    }
    
}