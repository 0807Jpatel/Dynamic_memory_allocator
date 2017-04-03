#include <criterion/criterion.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include "sfmm.h"


#define bp(x) sf_blockprint((char*)x - 8);
#define sp() sf_snapshot(true);
/**
 *  HERE ARE OUR TEST CASES NOT ALL SHOULD BE GIVEN STUDENTS
 *  REMINDER MAX ALLOCATIONS MAY NOT EXCEED 4 * 4096 or 16384 or 128KB
 */

info newInfo;
info testInfo;

Test(sf_memsuite, Malloc_an_Integer, .init = sf_mem_init, .fini = sf_mem_fini) {
  int *x = sf_malloc(sizeof(int));
  *x = 4;
  cr_assert(*x == 4, "Failed to properly sf_malloc space for an integer!");
}

Test(sf_memsuite, Free_block_check_header_footer_values, .init = sf_mem_init, .fini = sf_mem_fini) {
  void *pointer = sf_malloc(sizeof(short));
  sf_free(pointer);
  pointer = (char*)pointer - 8;
  sf_header *sfHeader = (sf_header *) pointer;
  cr_assert(sfHeader->alloc == 0, "Alloc bit in header is not 0!\n");
  sf_footer *sfFooter = (sf_footer *) ((char*)pointer + (sfHeader->block_size << 4) - 8);
  cr_assert(sfFooter->alloc == 0, "Alloc bit in the footer is not 0!\n");
}

Test(sf_memsuite, SplinterSize_Check_char, .init = sf_mem_init, .fini = sf_mem_fini){
  void* x = sf_malloc(32);
  void* y = sf_malloc(32);
  (void)y;

  sf_free(x);

  x = sf_malloc(16);

  sf_header *sfHeader = (sf_header *)((char*)x - 8);
  cr_assert(sfHeader->splinter == 1, "Splinter bit in header is not 1!");
  cr_assert(sfHeader->splinter_size == 16, "Splinter size is not 16");

  sf_footer *sfFooter = (sf_footer *)((char*)sfHeader + (sfHeader->block_size << 4) - 8);
  cr_assert(sfFooter->splinter == 1, "Splinter bit in footer is not 1!");
}

Test(sf_memsuite, Check_next_prev_pointers_of_free_block_at_head_of_list, .init = sf_mem_init, .fini = sf_mem_fini) {
  int *x = sf_malloc(4);
  memset(x, 0, 0);
  cr_assert(freelist_head->next == NULL);
  cr_assert(freelist_head->prev == NULL);
}

Test(sf_memsuite, Coalesce_no_coalescing, .init = sf_mem_init, .fini = sf_mem_fini) {
  int *x = sf_malloc(4);
  int *y = sf_malloc(4);
  memset(y, 0, 0);
  sf_free(x);

  //just simply checking there are more than two things in list
  //and that they point to each other
  cr_assert(freelist_head->next != NULL);
  cr_assert(freelist_head->next->prev != NULL);
}

//#
//STUDENT UNIT TESTS SHOULD BE WRITTEN BELOW
//DO NOT DELETE THESE COMMENTS
//#

Test(single_page, testing_splinters, .init = sf_mem_init, .fini = sf_mem_fini) { //test for basic one page programing
  int *x = sf_malloc(45);
  int *y = sf_malloc(60);
  int *z = sf_malloc(131);
  sf_info(&newInfo);
  cr_assert(newInfo.allocatedBlocks == 3);
  cr_assert(newInfo.splinterBlocks == 0);
  cr_assert(newInfo.padding == 20);
  cr_assert(newInfo.splintering == 0);
  cr_assert(newInfo.coalesces == 0);
  cr_assert((int)(newInfo.peakMemoryUtilization*100000) == 5761);

  memset(x, 0, 0);
  (void)z;
  sf_free(y);
  y = sf_malloc(48);
  sf_free(x);
  cr_assert(freelist_head->header.alloc == 0);
  cr_assert(freelist_head->header.block_size == 4);
  cr_assert(freelist_head->next->header.splinter == 0);
}


Test(single_page, test_coalsece, .init = sf_mem_init, .fini = sf_mem_fini){ //create one block and remove it and test the size of free block
  int *x = sf_malloc(20);
  sf_free(x);
  cr_assert(freelist_head->header.alloc == 0);
  cr_assert(freelist_head->header.block_size == 256);
  cr_assert(((sf_footer*)((char*)freelist_head+(4088)))->alloc == 0);
  cr_assert(((sf_footer*)((char*)freelist_head+(4088)))->block_size == 256);
}


Test(single_page, complex_coal, .init = sf_mem_init, .fini = sf_mem_fini){
  int *x = sf_malloc(20);
  sf_free(x);
  int *y = sf_malloc(20);
  int *z = sf_malloc(30);
  (void)y;
  sf_free(z);
  cr_assert(freelist_head->header.alloc == 0);
  cr_assert(freelist_head->header.block_size == 253);
  cr_assert(((sf_footer*)((char*)freelist_head+(4040)))->alloc == 0);
  cr_assert(((sf_footer*)((char*)freelist_head+(4040)))->block_size == 253);
}

Test(single_page, padding_test, .init = sf_mem_init, .fini = sf_mem_fini){
  int *x = sf_malloc(20);
  int *y = sf_malloc(20);
  (void)y;
  cr_assert(((sf_header*)((char*)x -8))->block_size == 3);
  cr_assert(((sf_header*)((char*)x -8))->padding_size == 12);
  cr_assert(((sf_header*)((char*)y -8))->block_size == 3);
  cr_assert(((sf_header*)((char*)y -8))->padding_size == 12);
  sf_free(x);
  sf_free(y);
  cr_assert(freelist_head->header.block_size == 256);
}

Test(Invalid_free, wrong_address, .init = sf_mem_init, .fini = sf_mem_fini){ //wrong address for free
  int *x = sf_malloc(20);
  sf_free(x + 2);
  cr_assert(errno == EINVAL);
  cr_assert(freelist_head->header.block_size == 253);
  cr_assert(freelist_head->header.alloc == 0);
}

Test(Coalesce, coalease_on_both_side, .init = sf_mem_init, .fini = sf_mem_fini){ //basic three coalease
  int *x = sf_malloc(20);
  int *y = sf_malloc(90);
  int *z = sf_malloc(100);
  sf_free(x);
  sf_free(z);
  sf_free(y);
  cr_assert(freelist_head->header.block_size == 256);
}


Test(Coalesce, coleasemorecomplex, .init = sf_mem_init, .fini = sf_mem_fini){ //complex coalease
  int *x = sf_malloc(20);
  int *y = sf_malloc(90);
  int *z = sf_malloc(100);
  int *a = sf_malloc(20);
  int *b = sf_malloc(90);
  int *c = sf_malloc(100);
  int *d = sf_malloc(100);
  sf_free(x);
  cr_assert(freelist_head->header.block_size == 3);
  cr_assert(freelist_head->next->header.block_size == 212);
  sf_free(z);
  cr_assert(freelist_head->header.block_size == 3);
  cr_assert(freelist_head->next->header.block_size == 8);
  cr_assert(freelist_head->next->next->header.block_size == 212);
  sf_free(b);
  cr_assert(freelist_head->header.block_size == 3);
  cr_assert(freelist_head->next->header.block_size == 8);
  cr_assert(freelist_head->next->next->header.block_size == 7);
  cr_assert(freelist_head->next->next->next->header.block_size == 212);
  sf_free(d);
  cr_assert(freelist_head->header.block_size == 3);
  cr_assert(freelist_head->next->header.block_size == 8);
  cr_assert(freelist_head->next->next->header.block_size == 7);
  cr_assert(freelist_head->next->next->next->header.block_size = 220);
  (void)y;
  (void)c;
  sf_free(a);
  cr_assert(freelist_head->header.block_size == 3);
  cr_assert(freelist_head->next->header.block_size == 18);
  cr_assert(freelist_head->next->next->header.block_size = 220);

}

Test(Coalesce, coalease_on_left_startList, .init = sf_mem_init, .fini = sf_mem_fini){ //left coalease at start of list
  int *x = sf_malloc(20);
  int *y = sf_malloc(90);
  int *z = sf_malloc(100);
  sf_free(x);
  sf_free(y);
  (void)z;
}

Test(Coalesce, coalease_on_left, .init = sf_mem_init, .fini = sf_mem_fini){ //left coalease in middle
  int *a = sf_malloc(20);
  int *x = sf_malloc(20);
  int *y = sf_malloc(90);
  int *z = sf_malloc(100);
  (void)a;
  sf_free(x);
  sf_free(y);
  (void)z;
}

Test(MorePage, mage_more_page, .init = sf_mem_init, .fini = sf_mem_fini){ //morethan malloc for exactly one page size so it requires two pages
  int *x = sf_malloc(4096);
  cr_assert(((sf_header*)((char*)(x) - 8))->block_size == 257);
  cr_assert(((sf_header*)((char*)(x) - 8))->alloc == 1);
  cr_assert(((sf_header*)((char*)(x) - 8))->splinter == 0);
  cr_assert(((sf_header*)((char*)(x) - 8))->padding_size == 0);
  cr_assert(freelist_head->header.block_size == 255);
  cr_assert(freelist_head->header.alloc == 0);
  sf_free(x);
  cr_assert(freelist_head->header.block_size == 512);
  cr_assert(freelist_head->header.alloc == 0);
}

Test(MorePage, splinter_pageone_size, .init = sf_mem_init, .fini = sf_mem_fini){ //maxsize with splinter
  int *x = sf_malloc(4064);
  cr_assert(freelist_head == NULL);
  cr_assert( ((sf_header*)((char*)x - 8))->alloc == 1);
  cr_assert( ((sf_header*)((char*)x - 8))->splinter == 1);
  cr_assert( ((sf_header*)((char*)x - 8))->splinter_size == 16);
  cr_assert( ((sf_header*)((char*)x - 8))->block_size == 256);
  cr_assert( ((sf_header*)((char*)x - 8))->requested_size == 4064);
}


Test(MorePage, splinter_pagetwo_size, .init = sf_mem_init, .fini = sf_mem_fini){ //testing with multi page malloc and free
  int *x = sf_malloc(4000);
  int *y = sf_malloc(1000);
  sf_free(x);
  cr_assert(freelist_head->header.block_size == 251);
  cr_assert(freelist_head->next->header.block_size == 197);
  sf_free(y);
  cr_assert(freelist_head->header.block_size == 512);
}

Test(MorePage, OverFlow, .init = sf_mem_init, .fini = sf_mem_fini){ //check for over malloc and then free with NULL
  int *x = sf_malloc(16700);
  cr_assert(x == NULL);
  cr_assert(errno == ENOMEM);
  (void)x;
  sf_free(x);
  cr_assert(errno == EINVAL);
}

Test(Morepage, maxsize_malloc, .init = sf_mem_init, .fini = sf_mem_fini){ //check for max malloc and free
  int *x = sf_malloc(16368);
  cr_assert(((sf_header*)((char*)x - 8))->block_size == 1024);
  cr_assert(freelist_head == NULL);
  sf_free(x);
  cr_assert(freelist_head->header.block_size == 1024);
}

Test(Morepage, coalease_with_head, .init = sf_mem_init, .fini = sf_mem_fini){
  void* x = sf_malloc(4096);
  void* y = sf_malloc(4060);
  void* z = sf_malloc(1036);
  void* a = sf_malloc(4342);
  cr_assert(freelist_head->header.block_size == 173);
  sf_free(a);
  cr_assert(freelist_head->header.block_size == 446);
  sf_free(y);
  cr_assert(freelist_head->header.block_size == 255);
  sf_free(x);
  cr_assert(freelist_head->header.block_size == 512);
  sf_free(z);
  cr_assert(freelist_head->header.block_size == 1024);
}

Test(Morepage, coalease_with_both_side, .init = sf_mem_init, .fini = sf_mem_fini){
  void* x = sf_malloc(4096);
  void* y = sf_malloc(4060);
  void* z = sf_malloc(1036);
  void* a = sf_malloc(4342);
  cr_assert(freelist_head->header.block_size == 173);
  sf_free(z);
  cr_assert(freelist_head->header.block_size == 66);
  sf_free(x);
  cr_assert(freelist_head->header.block_size == 257);
  sf_free(y);
  cr_assert(freelist_head->header.block_size == 578);
  sf_free(a);
  cr_assert(freelist_head->header.block_size == 1024);
}

Test(SpecialCase, wrongCase, .init = sf_mem_init, .fini = sf_mem_fini){
  void* x = sf_malloc(16384);
  cr_assert(x == NULL);
  cr_assert(errno == ENOMEM);
  void* y = sf_malloc(4096);
  void* z = sf_malloc(4096);
  void* a = sf_malloc(4096);
  void* b = sf_malloc(4096);
  cr_assert(b == NULL);
  cr_assert(errno == ENOMEM);
  b = sf_malloc(4016);
  cr_assert(((sf_header*)(b - 8))->block_size == 253);
  sf_free(b + 8);
  cr_assert(errno == EINVAL);
  sf_free(b);
  cr_assert(freelist_head->header.block_size == 253);
  void* d = a;
  a = sf_realloc(a, 8333);
  cr_assert(a == NULL);
  sf_free(z);
  sf_free(y - 8);
  cr_assert(errno == EINVAL);
  sf_free(y);
  sf_free(d);
  cr_assert((freelist_head->header).block_size == 1024);
  void* q = sf_malloc(16368);
  cr_assert(((sf_header*)(q - 8))->block_size == 1024);
  cr_assert(((sf_header*)(q - 8))->requested_size == 16368);
  cr_assert(((sf_header*)(q - 8))->alloc == 1);
  d = q;
  q = sf_realloc(q+8, 1);
  cr_assert(q == NULL);
  cr_assert(errno == EINVAL);
  sf_realloc(d, 0);
  cr_assert(freelist_head->header.block_size == 1024);
}


Test(Realloc, nextPage, .init = sf_mem_init, .fini = sf_mem_fini){
  void* x = sf_malloc(1040);
  void* y = sf_malloc(100);
  void* a = sf_realloc(y, 4096);
  cr_assert(y == a);
  cr_assert(((sf_header*)(a-8))->block_size == 257);
  cr_assert(((sf_header*)(a-8))->alloc == 1);
  sf_realloc(x, 0);
  sf_realloc(a, 0);
  cr_assert(freelist_head->header.block_size == 512);
}

Test(Realloc, smaller_realloc, .init = sf_mem_init, .fini = sf_mem_fini){
  void* x = sf_malloc(1040);
  void* y = sf_malloc(100);
  void* a = sf_realloc(y, 1);
  cr_assert(y == a);
  cr_assert(((sf_header*)(a-8))->block_size == 2);
  cr_assert(((sf_header*)(a-8))->alloc == 1);
  sf_realloc(x, 0);
  sf_realloc(a, 0);
  cr_assert(freelist_head->header.block_size == 256);
}


Test(Realloc, extreme_smaller, .init = sf_mem_init, .fini = sf_mem_fini){
  void* y = sf_malloc(16368);
  cr_assert(freelist_head == NULL);
  void* a = sf_realloc(y, 1);
  cr_assert(y == a);
  cr_assert(((sf_header*)(a-8))->block_size == 2);
  cr_assert(((sf_header*)(a-8))->alloc == 1);
  cr_assert(freelist_head->header.block_size == 1022);
  sf_realloc(a, 0);
  cr_assert(freelist_head->header.block_size == 1024);
}

Test(Realloc, bigger_realloc, .init = sf_mem_init, .fini = sf_mem_fini){
  void* y = sf_malloc(1);
  cr_assert(((sf_header*)(y - 8))->block_size == 2);
  cr_assert(freelist_head->header.block_size == 254);
  void* a = sf_realloc(y, 16368);
  cr_assert(y == a);
  cr_assert(freelist_head == NULL);
  cr_assert(((sf_header*)(a - 8))->block_size == 1024);
  cr_assert(((sf_header*)(a - 8))->requested_size == 16368);

}

Test(Realloc, bigger_no_colease, .init = sf_mem_init, .fini = sf_mem_fini){
  void* y = sf_malloc(1);
  void* x = sf_malloc(4096);
  void* z = sf_malloc(1);
  cr_assert(freelist_head->header.block_size == 251);
  void* a = sf_realloc(x, 4097);
  cr_assert(a != x);
  cr_assert(((sf_header*)(a - 8))->block_size == 258);
  cr_assert(freelist_head->header.block_size == 257);
  sf_free(y);
  sf_realloc(z, 0);
  cr_assert(freelist_head->header.block_size == 261);
}


Test(Realloc, All_comb, .init = sf_mem_init, .fini = sf_mem_fini){
    void* x = sf_malloc(4048);
    void* y = sf_malloc(16);
    cr_assert(freelist_head == NULL);
    errno = 0;
    void* b = sf_realloc(y + 1, 1);
    cr_assert(b == NULL);
    cr_assert(errno == EINVAL);
    errno = 0;
    sf_free(x + 1);
    cr_assert(errno == EINVAL);
    void* a = sf_realloc(y, 80);
    cr_assert(freelist_head->header.block_size == 252);
    sf_free(x);
    sf_free(a);
    cr_assert(freelist_head->header.block_size == 512);
}

Test(Realloc, paddinig, .init = sf_mem_init, .fini = sf_mem_fini){
    void* x = sf_malloc(1);
    cr_assert(((sf_header*)(x - 8))->block_size == 2);
    cr_assert(((sf_header*)(x - 8))->padding_size == 15);
    cr_assert(freelist_head->header.block_size == 254);
    void* y = sf_realloc(x, 16);
    cr_assert(x == y);
    cr_assert(((sf_header*)(y - 8))->block_size == 2);
    cr_assert(((sf_header*)(y - 8))->padding_size == 0);
    cr_assert(freelist_head->header.block_size == 254);
    void* z = sf_realloc(y, 17);
    sf_free(z);
    cr_assert(freelist_head->header.block_size == 256);
}

Test(Realloc, splinter, .init = sf_mem_init, .fini = sf_mem_fini){
  void* x = sf_malloc(23);
  void* y = sf_malloc(48);
  cr_assert(((sf_header*)(y - 8))->block_size == 4);
  cr_assert(((sf_header*)(y - 8))->splinter == 0);
  cr_assert(((sf_header*)(y - 8))->splinter_size == 0);
  void* z = sf_malloc(1);
  void* a = sf_realloc(y, 32);
  cr_assert(a == y);
  cr_assert(((sf_header*)(a - 8))->block_size == 4);
  cr_assert(((sf_header*)(a - 8))->splinter == 1);
  cr_assert(((sf_header*)(a - 8))->splinter_size == 16);
  y = sf_realloc(a, 48);
  cr_assert(y == a);
  cr_assert(((sf_header*)(y - 8))->block_size == 4);
  cr_assert(((sf_header*)(y - 8))->splinter == 0);
  cr_assert(((sf_header*)(y - 8))->splinter_size == 0);
  sf_free(x);
  sf_realloc(z, 0);
  sf_free(y);
  cr_assert(freelist_head->header.block_size == 256);
}

Test(sf_memsuite, infotest, .init = sf_mem_init, .fini = sf_mem_fini) {
  sf_info(&testInfo);
  int *a = sf_malloc(10);
  int *b = sf_malloc(20);
  int *c = sf_malloc(40);
  sf_info(&testInfo);
  cr_assert(testInfo.padding==26);
  cr_assert(testInfo.allocatedBlocks == 3);
  cr_assert(testInfo.splintering == 0);
  cr_assert(testInfo.splinterBlocks == 0);
  cr_assert(testInfo.coalesces == 0);
  sf_free(a);
  sf_free(b);
  sf_info(&testInfo);
  cr_assert(testInfo.padding==8);
  cr_assert(testInfo.allocatedBlocks == 1);
  cr_assert(testInfo.coalesces == 1);
  cr_assert(testInfo.splintering == 0);
  cr_assert(testInfo.splinterBlocks == 0);
  sf_realloc(c,30);
  sf_info(&testInfo);
  cr_assert(testInfo.padding==2);
  cr_assert(testInfo.allocatedBlocks == 1);
  cr_assert(testInfo.splintering == 0);
  cr_assert(testInfo.splinterBlocks == 0);
  sf_realloc(c,48);
  sf_info(&testInfo);
  cr_assert(testInfo.padding==0);
  cr_assert(testInfo.allocatedBlocks == 1);
  cr_assert(testInfo.splintering == 0);
  cr_assert(testInfo.splinterBlocks == 0);
  sf_free(c);
  sf_info(&testInfo);
  cr_assert(testInfo.padding==0);
  cr_assert(testInfo.allocatedBlocks == 0);
  cr_assert(testInfo.coalesces == 2);
  cr_assert(testInfo.splintering == 0);
  cr_assert(testInfo.splinterBlocks == 0);
  cr_assert((int)(testInfo.peakMemoryUtilization*100000) == 1708);
}
