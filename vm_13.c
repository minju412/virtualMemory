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
#include <string.h>
#include <assert.h>
#include <getopt.h>
#include <ctype.h>
#include <inttypes.h>
#include <strings.h>

#include "types.h"
#include "parser.h"

#include "list_head.h"
#include "vm.h"


static bool verbose = true;

/**
 * Initial process
 */
static struct process init = {
	.pid = 0,
	.list = LIST_HEAD_INIT(init.list),
	.pagetable = {
		.outer_ptes = { NULL },
	},
};

/**
 * Current process. Should not be listed in the @processes
 */
struct process *current = &init;

/**
 * Ready queue. Put @current process to the tail of this list on
 * switch_process(). Don't forget to remove the switched process from the list.
 */
LIST_HEAD(processes);

/**
 * Page table base register
 */
struct pagetable *ptbr = NULL;

/**
 * Map count for each page frame
 */
unsigned int mapcounts[NR_PAGEFRAMES] = { 0 };


extern unsigned int alloc_page(unsigned int vpn, unsigned int rw);
extern void free_page(unsigned int vpn);
extern bool handle_page_fault(unsigned int vpn, unsigned int rw);
extern void switch_process(unsigned int pid);

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
   
   //min 찾기
    list_for_each_prev(ptr, &stack){ 
		node =list_entry(ptr, struct entry, list);
		if(node->num < min)
            min = node->num;
	}

	list_for_each_safe(ptr, ptrn, &stack){ 
		node =list_entry(ptr, struct entry, list);
		if(node->num == min){
            list_del(&node->list);
            // free(node);
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

    if(rw == 1){ //rw == RW_READ
		pte->valid = true;
		pte->writable = false;
        pte->private = 0; ///추가 - read 였음
	}else if(rw == 3){ //rw == RW_WRITE
		pte->valid = true;
		pte->writable = true;
        pte->private = 1; //추가 - write 였음
	}


    if(list_empty(&stack)){ //스택이 비어있다면	
        cnt++;
        pte->pfn = cnt;
        mapcounts[cnt]++;
        mapcnt_index++;
		// printf("1 : %d\n", cnt);
        return cnt;
    }else{ //free 받은 pfn이 있다면
        int min = find_min();
        pte->pfn = min;
        mapcounts[min]++;
        mapcnt_index++;
		// printf("2 : %d\n", min);
        return min;
    }

	// return -1;
}

void free_page(unsigned int vpn) //맵카운트가 0일때는 free하고 0보다 크면 아예 반환해버리면 안됨
{
	int pd_index = vpn / NR_PTES_PER_PAGE; //outer의 인덱스
	int pte_index = vpn % NR_PTES_PER_PAGE; //ptes의 인덱스

    struct pte *pte = &current->pagetable.outer_ptes[pd_index]->ptes[pte_index];

    // //원래 read였는지 write였는지에 따라 다르게 cow 적용!
    // if(pte->private == 0){ //원래 read만

    // }else{ //원래 read,write

    // }

 

    mapcounts[pte->pfn]--;
	mapcnt_index--;

    int tmp = pte->pfn;
    pte->pfn = 0;
    pte->valid = false;
    pte->writable = false;


    
    if(mapcounts[tmp] == 0){ //혼자 쓰고 있던 것  
		// freed VPN should be denied by MMU!!!!!!!!!!
		printf("alone!\n");
		
    }
	

    
    
    // else{ //두개 이상의 프로세스와 공유하던 page
    //     printf("more two\n");
    // }

	// if(mapcounts[pte->pfn] >= 1){ //다른 프로세스도 같이 쓰고 있던 페이지
    //     push_stack(pte->pfn);  /////////////??
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

        //현재 캡카운트가 1이면 new_pfn 찾지 말고 그냥 자기껄로 만들기! ///////////추가
        if(mapcounts[old_pfn]==1){
            pte->writable = true;
        }else{

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
        }
        return true;
	}

}


//switch 1-> switch 2 했을 때 2번의 show가 다 0으로 출력.......???????????

struct process child;
void switch_process(unsigned int pid) 
//존재하는 프로세스면 context switch(ptbr을 바꿔줘야함), 존재하지 않으면 fork
{
    struct list_head* ptr;
    struct process* prc = NULL;
    int flag=0;
   
    // printf("1. current=%d\n", current->pid);

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
        // printf("2. current=%d\n", current->pid);
        // child.pid = pid;
        // printf("3. current=%d\n", current->pid);
        
        // printf("current=%d child=%d\n", current->pid, child.pid);


        //copy-on-write
        //current의 w를 끄기 (원래 read였으면 pte->private=0 / write였으면 private=1)
        for(int i=0; i<=global_pd_index; i++){
            for(int j=0; j<16; j++){
                current->pagetable.outer_ptes[i]->ptes[j].writable = false;
            }
        } 

        //mapcount 추가 -> 이렇게 하면 안될듯!
        // for(int i=0; i<mapcnt_index; i++)
        //     mapcounts[i]++;

        //valid한 pte를 찾아서
        //mapcounts[pte->pfn]++;



        
        // printf("4. current=%d\n", current->pid);
        for(int i=0; i<=global_pd_index; i++){ //current의 outertable이 몇개까지 있는지!!!
            if(!child.pagetable.outer_ptes[i])
                child.pagetable.outer_ptes[i] = malloc(sizeof(struct pte_directory));
            
            for(int j=0; j<16; j++){                 
                child.pagetable.outer_ptes[i]->ptes[j].writable = false;
                child.pagetable.outer_ptes[i]->ptes[j].valid = current->pagetable.outer_ptes[i]->ptes[j].valid;
                child.pagetable.outer_ptes[i]->ptes[j].pfn = current->pagetable.outer_ptes[i]->ptes[j].pfn; 
                child.pagetable.outer_ptes[i]->ptes[j].private = current->pagetable.outer_ptes[i]->ptes[j].private;      

                //추가
                if(child.pagetable.outer_ptes[i]->ptes[j].valid==true){
                    mapcounts[child.pagetable.outer_ptes[i]->ptes[j].pfn]++;
                }                  
            }
        }
        // printf("5. current=%d\n", current->pid);
        list_add_tail(&current->list, &processes);
        child.pid = pid; ///////////
        ptbr = &child.pagetable;
        current = &child;
        // printf("6. current=%d\n", current->pid);
    }
    
}


int parse_command(char *command, int *nr_tokens, char *tokens[])
{
	char *curr = command;
	int token_started = false;
	*nr_tokens = 0;

	while (*curr != '\0') {
		if (isspace(*curr)) {
			*curr = '\0';
			token_started = false;
		} else {
			if (!token_started) {
				tokens[*nr_tokens] = curr;
				*nr_tokens += 1;
				token_started = true;
			}
		}

		curr++;
	}

	for (int i = 0; i < *nr_tokens; i++) {
		if (strncmp(tokens[i], "#", strlen("#")) == 0) {
			*nr_tokens = i;
			tokens[i] = NULL;
			break;
		}
	}

	return (*nr_tokens > 0);
}




/**
 * __translate()
 *
 * DESCRIPTION
 *   This function simulates the address translation in MMU.
 *   It translates @vpn to @pfn using the page table pointed by @ptbr.
 *
 * RETURN
 *   @true on successful translation
 *   @false if unable to translate. This includes the case when the page access
 *   is for write (indicated in @rw), but the @writable of the pte is @false.
 */
static bool __translate(unsigned int rw, unsigned int vpn, unsigned int *pfn)
{
	int pd_index = vpn / NR_PTES_PER_PAGE;
	int pte_index = vpn % NR_PTES_PER_PAGE;

	struct pagetable *pt = ptbr;
	struct pte_directory *pd;
	struct pte *pte;

	/***
	 * Advanced tasks: Implement TLB hook here
	 */

	/* Page table is invalid */
	if (!pt) return false;

	pd = pt->outer_ptes[pd_index];

	/* Page directory does not exist */
	if (!pd) return false;

	pte = &pd->ptes[pte_index];

	/* PTE is invalid */
	if (!pte->valid) return false;

	/* Unable to handle the write access */
	if (rw == RW_WRITE) {
		if (!pte->writable) return false;
	}
	*pfn = pte->pfn;

	return true;
}

/**
 * __access_memory
 *
 * DESCRIPTION
 *   Simulate the MMU in the processor and call page fault handler
 *   if necessary.
 *
 * RETURN
 *   @true on successful access
 *   @false if unable to access @vpn for @rw
 */
static bool __access_memory(unsigned int vpn, unsigned int rw)
{
	unsigned int pfn;
	int ret;
	int nr_retries = 0;

	/* Cannot read and write at the same time!! */
	assert((rw & RW_READ) ^ (rw & RW_WRITE));

	/**
	 * We have NR_PTES_PER_PAGE entries in the outer table and so do for
	 * inner page table. Thus each process can have up to NR_PTES_PER_PAGE^2
	 * as its VPN
	 */
	assert(vpn < NR_PTES_PER_PAGE * NR_PTES_PER_PAGE);

	do {
		/* Ask MMU to translate VPN */
		if (__translate(rw, vpn, &pfn)) {
			/* Success on address translation */
			fprintf(stderr, "%3u --> %-3u\n", vpn, pfn);
			return true;
		}

		/**
		 * Failed to translate the address. So, call OS through the page fault
		 * and restart the translation if the fault is successfully handled.
		 * Count the number of retries to prevent buggy translation.
		 */
		nr_retries++;
	} while ((ret = handle_page_fault(vpn, rw)) == true && nr_retries < 2);

	if (ret == false) {
		fprintf(stderr, "Unable to access %u\n", vpn);
	}

	return ret;
}

static unsigned int __make_rwflag(const char *rw)
{
	int len = strlen(rw);
	unsigned int rwflag = 0;

	for (int i = 0; i < len; i++) {
		if (rw[i] == 'r' || rw[i] == 'R') {
			rwflag |= RW_READ;
		}
		if (rw[i] == 'w' || rw[i] == 'W') {
			rwflag |= RW_WRITE;
		}
	}
	return rwflag;
}

static bool __alloc_page(unsigned int vpn, unsigned int rw)
{
	unsigned int pfn;

	assert(rw);

	if (__translate(RW_READ, vpn, &pfn)) {
		fprintf(stderr, "%u is already allocated to %u\n", vpn, pfn);
		return false;
	}

	pfn = alloc_page(vpn, rw);
	if (pfn == -1) {
		fprintf(stderr, "memory is full\n");
		return false;
	}
	fprintf(stderr, "alloc %3u --> %-3u\n", vpn, pfn);
	
	return true;
}

static bool __free_page(unsigned int vpn)
{
	unsigned int pfn;

	if (!__translate(RW_READ, vpn, &pfn)) {
		fprintf(stderr, "%u is not allocated\n", vpn);
		return false;
	}
	fprintf(stderr, "free %u (pfn %u)\n", vpn, pfn);
	free_page(vpn);

	return true;
}

static void __init_system(void)
{
	ptbr = &init.pagetable;
}

static void __show_pageframes(void)
{
	for (unsigned int i = 0; i < NR_PAGEFRAMES; i++) {
		if (!mapcounts[i]) continue;
		fprintf(stderr, "%3u: %d\n", i, mapcounts[i]);
	}
	fprintf(stderr, "\n");
}

static void __show_pagetable(void)
{
	fprintf(stderr, "\n*** PID %u ***\n", current->pid);

	for (int i = 0; i < NR_PTES_PER_PAGE; i++) {
		struct pte_directory *pd = current->pagetable.outer_ptes[i];

		if (!pd) continue;

		for (int j = 0; j < NR_PTES_PER_PAGE; j++) {
			struct pte *pte = &pd->ptes[j];

			if (!verbose && !pte->valid) continue;
			fprintf(stderr, "%02d:%02d %c%c | %-3d\n", i, j,
				pte->valid ? 'v' : ' ',
				pte->writable ? 'w' : ' ',
				pte->pfn);
		}
		printf("\n");
	}
}

static void __print_help(void)
{
	printf("  help | ?     : Print out this help message \n");
	printf("  exit         : Exit the simulation\n");
	printf("\n");
	printf("  switch [pid] : Do context switch to pid @pid\n");
	printf("                 Fork @pid if there is no process with the pid\n");
	printf("  show         : Show the page table of the current process\n");
	printf("  pages        : Show the status for each page frame\n");
	printf("\n");
	printf("  alloc [vpn] r|w  : Allocate a page for the rw flag\n");
	printf("  free [vpn]       : Deallocate the page at VPN @vpn\n");
	printf("  access [vpn] r|w : Access VPN @vpn for read or write\n");
	printf("  read [vpn]       : Equivalent to access @vpn r\n");
	printf("  write [vpn]      : Equivalent to access @vpn w\n");
	printf("\n");
}

static bool strmatch(char * const str, const char *expect)
{
	return (strlen(str) == strlen(expect)) &&
			(strncmp(str, expect, strlen(expect)) == 0);
}

static void __do_simulation(FILE *input)
{
	char command[MAX_COMMAND_LEN] = { 0 };

	__init_system();

	while (fgets(command, sizeof(command), input)) {
		char *tokens[MAX_NR_TOKENS] = { NULL };
		int nr_tokens = 0;

		/* Make the command lowercase */
		for (size_t i = 0; i < strlen(command); i++) {
			command[i] = tolower(command[i]);
		}

		if (parse_command(command, &nr_tokens, tokens) < 0) {
			continue;
		}
		if (nr_tokens == 0) continue;

		if (nr_tokens == 1) {
			if (strmatch(tokens[0], "exit")) break;
			if (strmatch(tokens[0], "show")) {
				__show_pagetable();
			} else if (strmatch(tokens[0], "pages")) {
				__show_pageframes();
			} else if (strmatch(tokens[0], "help") || strmatch(tokens[0], "?")) {
				__print_help();
			} else {
				printf("Unknown command %s\n", tokens[0]);
			}
		} else if (nr_tokens == 2) {
			unsigned int arg = strtoimax(tokens[1], NULL, 0);

			if (strmatch(tokens[0], "switch") || strmatch(tokens[0], "s")) {
				switch_process(arg);
			} else if (strmatch(tokens[0], "free") || strmatch(tokens[0], "f")) {
				__free_page(arg);
			} else if (strmatch(tokens[0], "read") || strmatch(tokens[0], "r")) {
				__access_memory(arg, RW_READ);
			} else if (strmatch(tokens[0], "write") || strmatch(tokens[0], "w")) {
				__access_memory(arg, RW_WRITE);
			} else {
				printf("Unknown command %s\n", tokens[0]);
			}
		} else if (nr_tokens == 3) {
			unsigned int vpn = strtoimax(tokens[1], NULL, 0);
			unsigned int rw = __make_rwflag(tokens[2]);

			if (strmatch(tokens[0], "alloc") || strmatch(tokens[0], "a")) {
				if (!__alloc_page(vpn, rw)) break;
			} else if (strmatch(tokens[0], "access")) {
				__access_memory(vpn, rw);
			} else {
				printf("Unknown command %s\n", tokens[0]);
			}
		} else {
			assert(!"Unknown command in trace");
		}

		if (verbose) printf(">> ");
	}
}

static void __print_usage(const char * name)
{
	printf("Usage: %s {-q} {-f [workload file]}\n", name);
	printf("\n");
	printf("  -q: Run quietly\n\n");
}

int main(int argc, char * argv[])
{
	int opt;
	FILE *input = stdin;

	while ((opt = getopt(argc, argv, "qh")) != -1) {
		switch (opt) {
		case 'q':
			verbose = false;
			break;
		case 'h':
		default:
			__print_usage(argv[0]);
			return EXIT_FAILURE;
		}
	}

	if (verbose && !argv[optind]) {
		printf("***************************************************************************\n");
		printf(" __      ____  __     _____ _                 _       _\n");
		printf(" \\ \\    / /  \\/  |   / ____(_)               | |     | |\n");
		printf("  \\ \\  / /| \\  / |  | (___  _ _ __ ___  _   _| | __ _| |_ ___  _ __ \n");
		printf("   \\ \\/ / | |\\/| |   \\___ \\| | '_ ` _ \\| | | | |/ _` | __/ _ \\| '__|\n");
		printf("    \\  /  | |  | |   ____) | | | | | | | |_| | | (_| | || (_) | |   \n");
		printf("     \\/   |_|  |_|  |_____/|_|_| |_| |_|\\__,_|_|\\__,_|\\__\\___/|_|\n");
		printf("\n");
		printf("                                            >> SCE213 2021 Spring <<\n");
		printf("\n");
		printf("***************************************************************************\n");
	}

	if (argv[optind]) {
		if (verbose) printf("Use file \"%s\" for input.\n", argv[optind]);

		input = fopen(argv[optind], "r");
		if (!input) {
			fprintf(stderr, "No input file %s\n", argv[optind]);
			return EXIT_FAILURE;
		}
		verbose = false;
	} else {
		if (verbose) printf("Use stdin for input.\n");
	}

	if (verbose) {
		printf("Enter 'help' or '?' for help.\n\n");
		printf(">> ");
	}

	__do_simulation(input);

	if (input != stdin) fclose(input);

	return EXIT_SUCCESS;
}
