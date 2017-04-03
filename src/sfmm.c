/**
 * All functions you make for the assignment must be implemented in this file.
 * Do not submit your assignment with a main function in this file.
 * If you submit with a main function in this file, you will get a zero.
 */
#include "sfmm.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>

#define error_mem() errno = ENOMEM; return NULL;
#define free_err_mem() errno = EINVAL; return;
#define err_invalid() errno = EINVAL; return NULL;
#define HF_SIZE 8
#define MAX_PAGES 4
#define make_freeblock(block, i_alloc , i_splinter, i_block_size, i_requested_size, i_splinter_size, i_padding_size, i_next, i_prev) \
			makeHeaderFooter(block, i_alloc , i_splinter, i_block_size, i_requested_size, i_splinter_size, i_padding_size);\
		 	((sf_free_header*)block)->next = i_next; ((sf_free_header*)block)->prev = i_prev;\

/**
 * You should store the head of your free list in this variable.
 * Doing so will make it accessible via the extern statement in sfmm.h
 * which will allow you to pass the address to sf_snapshot in a different file.
 */
sf_free_header* freelist_head = NULL;
int curr_page = 0;
void* LOWERBOUND = 0;
int lowerbound = 0;
static int allocatedBlock = 0, splinterBlock = 0, paddingTotal = 0, splinterTotal = 0, coalescesTotal = 0, peakMemoryUtil = 0, currentMemoryUtil = 0;

// this method is used to "create" block if the block is craeted then it updates the data
void* makeHeaderFooter(sf_header* block, int i_alloc , int i_splinter, int i_block_size, int i_requested_size, int i_splinter_size, int i_padding_size) {
	block->alloc = i_alloc;
	block->splinter = i_splinter;
	block->block_size = i_block_size >> 4;
	block->requested_size = i_requested_size;
	block->splinter_size = i_splinter_size;
	block->padding_size = i_padding_size;

	sf_footer* footadd = (sf_footer*)(((char*)block) + i_block_size - HF_SIZE);
	footadd->alloc = i_alloc;
	footadd->splinter = i_splinter;
	footadd->block_size = i_block_size >> 4;
	return block;
}

//coaleascestwo blocks together adds second block to first block, unless inv is 1 (special case for coaleasing new page)
int coalescestwo(sf_free_header* first, sf_free_header* second) {
	sf_free_header* prev = first->prev, *next = second->next;
	if (!prev && first != freelist_head) { prev = second->prev; }
	if (!next && first->next != second) { next = first->next; }
	make_freeblock((sf_header*)first, 0, 0, ((first->header).block_size << 4) + ((second->header).block_size << 4), 0, 0, 0, next, prev); //new block with new info
	prev ? (prev->next = first) :  (freelist_head = first); // if prev is null meaning this is new first block in linked list
	if (next) {next->prev = first;}
	return 1;
}
//checks for free block on left and right of currect block and calls coaleaces if needed.
int coalesces(sf_free_header* block) {
	int x = 0;
	if (!lowerbound) {LOWERBOUND = block; lowerbound++;}
	if (freelist_head == NULL) {
		freelist_head = block;
		return 1;
	}
	sf_header* nextblock = (sf_header*)((char*)block + (block->header.block_size << 4)), *prevblock = (sf_header*)((char*)block - HF_SIZE);
	if (!nextblock->alloc && (nextblock < (sf_header*)sf_sbrk(0))) //if allocated value is not 1 then we can coaleaces sf_sbrk(O) is UPPER BOUND so for freeing last block;
		x = coalescestwo(block, (sf_free_header*)nextblock);
	if (!prevblock->alloc && (prevblock >= (sf_header*)LOWERBOUND)) //if value of the footer says its not allocated we can coaleaces LOWER BOUND for freeign first block
		x = coalescestwo((sf_free_header*)(((char*)prevblock) - (prevblock->block_size << 4) + HF_SIZE), block); //getting address of previous block by footer - blocksize + HF_size
	if (x) {coalescesTotal++;}
	return x;
}
//adds this block to the freelist according to address.
void addtolist(sf_free_header* block) {
	if (!freelist_head || block < freelist_head) {
		block->next = freelist_head;
		if (freelist_head) {freelist_head->prev = block;}
		freelist_head = block;
		return;
	}
	sf_free_header* cursor = freelist_head, *pre = NULL;
	while (cursor != NULL && ((void*)block > ((void*)cursor))) {
		pre = cursor; cursor = cursor->next;
	}
	block->next = pre->next;
	if (block->next) {block->next->prev = block;}
	block->prev = pre;
	pre->next = block;
}

void removefromlist(sf_free_header* block) {
	if (block == freelist_head) {freelist_head = block->next;}
	if (block->next) {block->next->prev = block->prev;}
	if (block->prev) {block->prev->next = block->next;}
}

void* mallocthis(sf_free_header* placetoadd, int blocksize, int size, int padding, sf_free_header** freeblockafter, int remove) {
	int splinter_size = 0;
	if (((placetoadd->header.block_size << 4) - blocksize) < 31) { //is the size is less than 31 there is a splinter else no splinter
		splinter_size = (placetoadd->header.block_size << 4) - blocksize; //blocksize - actual size is size of splinter
		blocksize = placetoadd->header.block_size << 4;
		if(splinter_size) {splinterBlock++; splinterTotal += splinter_size;}
		if (remove) {removefromlist(placetoadd);}//case where calling this method from realloc where there is no need to remove the block just make the block with splinter
	} else {														//if splinter is found then there is no free block created
		sf_free_header* next = placetoadd->next, *prev = placetoadd->prev; //creating next and prev pointer to current free block
		sf_header* x = (sf_header*)((char*)placetoadd + blocksize); //location of new free block's header
		make_freeblock(x, 0, 0, (placetoadd->header.block_size << 4) - blocksize, 0, 0, 0, next, prev); //updating free block header
		if (freeblockafter) {
			*freeblockafter = (sf_free_header*)x; //set free block address so it can be used in realloc to coalease
		} else {
			prev ? (prev->next = (sf_free_header*)x) : (freelist_head = (sf_free_header*)x); //if previous is not null then set previou's next else set freelist_head
		}
	}
	paddingTotal += padding;
	return ((char*)makeHeaderFooter((sf_header*)placetoadd, 1, (splinter_size > 0), blocksize, size, splinter_size, padding)) + HF_SIZE; //return the new block's header address + 8
}

void* findPlaceFreelist(int blocksize) {
	sf_free_header* cursor = freelist_head, *placetoadd = NULL;
	int min = 17000;
	while (cursor) { //going through freelist and finding best fit
		int cursorblocksize = (cursor->header.block_size) << 4;
		if (blocksize == cursorblocksize) { //if found place with perfect size break;
			placetoadd = cursor;
			break;
		} else if ((blocksize < cursorblocksize) && (min > cursorblocksize)) { //look for the next block with minimum splinter / spliting require
			placetoadd = cursor;
			min = cursorblocksize;
		}
		cursor = cursor->next;
	}
	return placetoadd;
}

void *sf_malloc(size_t size) {
	if (size == 0) { err_invalid(); }
	int padding = (16 - size) % 16, blocksize = size + padding + (2 * HF_SIZE), pageAdded = 0;
	do { //do while loop for looking for place and if no place found then request for a page.
		sf_free_header* placetoadd = (sf_free_header*)findPlaceFreelist(blocksize);
		if (placetoadd) { //if placetoadd is not NULL meaning found a place to add our current size block
			allocatedBlock++; currentMemoryUtil += size;
			if(currentMemoryUtil > peakMemoryUtil) { peakMemoryUtil = currentMemoryUtil; }
			return mallocthis(placetoadd, blocksize, size, padding, NULL, 1);
		}
		if (curr_page >= MAX_PAGES) { error_mem();}
		void* rt = sf_sbrk(1);//if it didn't return that means there wasn't enough space for the size request more page.
		if (rt == ((void*) - 1)) {
			error_mem();
		} else {
			make_freeblock(rt, 0, 0, 4096, 0, 0, 0, NULL, NULL); //turn a page into free block
			int x = coalesces(rt); //coaleaces the page with current memory.
			if (!x) //if it returns 0 means no coaleasion
				addtolist(rt);
			pageAdded++; curr_page++;
		}
	} while (pageAdded);
	error_mem(); //if it gets to this point size was bigger than what we have.
}

void *sf_realloc(void *ptr, size_t size) {
	if (!ptr) {err_invalid();}
	sf_free_header* x = (sf_free_header*)((char*)ptr - HF_SIZE);
	sf_free_header* y = (sf_free_header*)((char*)x + (x->header.block_size << 4) - HF_SIZE);
	int currentblocksize = (x->header).block_size << 4;
	if (LOWERBOUND > (void*)x || (void*)y > sf_sbrk(0) || !x->header.alloc || (x->header.block_size != y->header.block_size) || (x->header.alloc != y->header.alloc) || (x->header.splinter != y->header.splinter) ) { //if alloc bit is not 1 then it is not valid address.
		err_invalid();
	}
	if (!size) {sf_free(ptr); return NULL;} //if the size is 0 then call free on this pointer
	int padding = (16 - size) % 16, blocksize = size + padding + (2 * HF_SIZE), sizediff = size - x->header.requested_size, paddingori = x->header.padding_size, spli = x->header.splinter_size;
	if (!((sf_header*)((char*)y + HF_SIZE))->alloc && ((blocksize) <= currentblocksize + (((sf_header*)((char*)y + HF_SIZE))->block_size << 4))) {
		removefromlist((sf_free_header*)((char*)y + HF_SIZE));
		currentblocksize = currentblocksize + (((sf_header*)((char*)y + HF_SIZE))->block_size << 4);
		makeHeaderFooter((sf_header*)x, 1 , 0, currentblocksize, size, 0, padding);
	}
	if (currentblocksize >= blocksize) {
		sf_free_header* freeblock = NULL;
		sf_free_header** freeblockafter = &freeblock;
		if(spli) { splinterBlock--; splinterTotal-= spli; }
		void* payload = mallocthis(x, blocksize, size, padding, freeblockafter, 0);
		if (*freeblockafter) {
			(*freeblockafter)->header.alloc = 01; ((sf_header*)((sf_free_header*)((char*)(*freeblockafter) + ((*freeblockafter)->header.block_size << 4) - HF_SIZE)))->alloc = 01;
			sf_free(((char*)*freeblockafter) + 8); allocatedBlock++;
		}
		currentMemoryUtil += sizediff;
		if(currentMemoryUtil > peakMemoryUtil) { peakMemoryUtil = currentMemoryUtil; }
		paddingTotal -= paddingori;
		return payload;
	} else {
		sf_free_header* placetoadd = findPlaceFreelist(blocksize);
		if (placetoadd) {
			if(spli) { splinterBlock--; splinterTotal-= spli; }
			void* payload = mallocthis(placetoadd, blocksize, size, padding, NULL, 1);
			if (!payload) { error_mem(); }
			memcpy(payload, ptr, (currentblocksize - 2 * HF_SIZE));
			sf_free(ptr);
			allocatedBlock++; currentMemoryUtil += sizediff; paddingTotal -= paddingori;
			if(currentMemoryUtil > peakMemoryUtil) { peakMemoryUtil = currentMemoryUtil; }
			return payload;
		} else {
			if (curr_page >= MAX_PAGES) { error_mem();}
			void* rt = sf_sbrk(1);//if it didn't return that means there wasn't enough space for the size request more page.
			if (rt == ((void*) - 1)) {
				error_mem();
			} else {
				make_freeblock(rt, 0, 0, 4096, 0, 0, 0, NULL, NULL); //turn a page into free block
				int x = coalesces(rt); //coaleaces the page with current memory.
				if (!x) {addtolist(rt);}
				curr_page++;
				return sf_realloc(ptr, size);
			}
		}
	}
	error_mem();
}

void sf_free(void* ptr) {
	if (!ptr) { free_err_mem(); }
	sf_free_header* x = (sf_free_header*)((char*)ptr - HF_SIZE);
	sf_free_header* y = (sf_free_header*)((char*)x + (x->header.block_size << 4) - HF_SIZE); //subtract the size of word to get back to header from payload address
	if (LOWERBOUND > (void*)x || (void*)y > sf_sbrk(0) || !x->header.alloc || (x->header.block_size != y->header.block_size) || (x->header.alloc != y->header.alloc) || (x->header.splinter != y->header.splinter) ) { //if alloc bit is not 1 then it is not valid address.
		free_err_mem();
	}
	paddingTotal -= x->header.padding_size;
	currentMemoryUtil -= x->header.requested_size;
	if (x->header.splinter) {splinterBlock--; splinterTotal -= x->header.splinter_size;} //decrementing splinterblock
	make_freeblock((sf_header*)x, 0, 0, (((sf_free_header*)x)->header).block_size << 4, 0, 0, 0, NULL, NULL);
	int add = coalesces(x); //check of coaleaces if possible execute it
	if (!add) { addtolist(x); }		//if no coaleaces then add the newly made free block into the linked list
	allocatedBlock--; return;
}

int sf_info(info* ptr) {
	if(!ptr) { return -1; }
	ptr->allocatedBlocks = allocatedBlock;
	ptr->splinterBlocks = splinterBlock;
	ptr->padding = paddingTotal;
	ptr->splintering = splinterTotal;
	ptr->coalesces = coalescesTotal;
	if(curr_page){ //no pages are allocated
		ptr->peakMemoryUtilization = ((float)peakMemoryUtil / (curr_page * 4096));
		return 1;
	}
	return -1;
}
