#include <stdio.h>
#include <stdlib.h>
#include <sys/fcntl.h>
#include <sys/mman.h>
#include <memory.h>
#include <unistd.h>
#include <assert.h>

#define COLOR_RED     "\x1b[31m"
#define COLOR_GREEN   "\x1b[32m"
#define COLOR_YELLOW  "\x1b[33m"
#define COLOR_RESET   "\x1b[0m"

#define TAG_OK COLOR_GREEN "[+]" COLOR_RESET " "
#define TAG_FAIL COLOR_RED "[-]" COLOR_RESET " "
#define TAG_PROGRESS COLOR_YELLOW "[~]" COLOR_RESET " "


#include "ptedit_header.h"

#define PAGESZ 4096

// do micro benchmark:
//   compare "direct read" vs "check and read" 

void micro_bench() {
  void *va1;
  posix_memalign(&va1, 4096, 4096);
  memset
}

int main(int argc, char *argv[]) {
  size_t address_pfn, target_pfn;
  (void)argc;
  (void)argv;

  if(ptedit_init()) {
    printf(TAG_FAIL "Could not initialize ptedit (did you load the kernel module?)\n");
    return 1;
  }

  ptedit_use_implementation(PTEDIT_IMPL_USER);

  assert(ptedit_get_pagesize()==4096);
  //char page[ptedit_get_pagesize()];


  // 1. build the mmap region; and we don't want kernel to alloc mem.
  void *address = mmap(0, 4096, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
  // 2. print the page table and pte of the first page in the mmap region
  ptedit_entry_t address_ptes = ptedit_resolve(address, 0);
  if (address_ptes.pgd == 0) {
	printf(TAG_FAIL "Could not resolve PTs\n");
	goto error;
  }
  printf(TAG_PROGRESS "====page table walk of mmap (before alloc)====");
  ptedit_print_entry_t(address_ptes);
  printf(TAG_PROGRESS "PTE PFN %zx\n", (size_t)(ptedit_cast(address_ptes.pte, ptedit_pte_t).pfn));
  // 3. walk to the pte of vm; then alloc a free page for it; then build the virt->phys address mapping 
  void *free_page_va = NULL;
  int ret = posix_memalign(&free_page_va, 4096, 4096);
  if (ret != 0) {
	perror("posix_memalign");
	goto error;
  }
  memset(free_page_va, 'A', 4096);
  // get all level ptes of free_page_va
  printf(TAG_PROGRESS "====get ptes start====");
  ptedit_entry_t free_page_ptes = ptedit_resolve(free_page_va, 0);
  printf(TAG_PROGRESS "====get ptes end====");
  // get pfn of free_page_va
  printf(TAG_PROGRESS "====get pfn start====");
  size_t free_pfn = ptedit_get_pfn(free_page_ptes.pte);
  printf(TAG_PROGRESS "====get pfn end====");
  // set pfn of address
  printf(TAG_PROGRESS "====set pfn start====");
  address_ptes.pte = ptedit_set_pfn(address_ptes.pte, free_pfn);
  ptedit_update(address, 0, &address_ptes);
  printf(TAG_PROGRESS "====set pfn end====");
  // set bits of pte
  printf(TAG_PROGRESS "====set pte bits start====");
  ptedit_pte_set_bit(address, 0, PTEDIT_PAGE_BIT_PRESENT);
  ptedit_pte_set_bit(address, 0, PTEDIT_PAGE_BIT_RW);
  ptedit_pte_set_bit(address, 0, PTEDIT_PAGE_BIT_USER);
  printf(TAG_PROGRESS "====set pte bits end====");
  // print and check
  printf(TAG_PROGRESS "====page table walk of mmap (after alloc)====");
  ptedit_entry_t address_ptes_1 = ptedit_resolve(address, 0);
  ptedit_print_entry_t(address_ptes_1);
  printf(TAG_PROGRESS "PTE PFN %zx\n", (size_t)(ptedit_cast(address_ptes_1.pte, ptedit_pte_t).pfn));

  // ======== check pfn ===
  char buf[4096];
  ptedit_read_physical_page(free_pfn, buf);
  printf(TAG_PROGRESS "buf[0] = " COLOR_YELLOW "%c" COLOR_RESET"\n", *(volatile char*)buf);


  // ===print buf ptes to refer===
  ptedit_entry_t buf_ptes = ptedit_resolve(buf, 0);
  ptedit_print_entry_t(buf_ptes);

  printf(TAG_PROGRESS "address[0] = " COLOR_YELLOW "%c" COLOR_RESET"\n", *(volatile char*)address);
  //if(*(volatile char*)address == 'A') {
  //    printf(TAG_OK "OK!\n");
  //} else {
  //    printf(TAG_FAIL "Fail!\n");
  //}

error:
  munmap(address, 4096);
  ptedit_cleanup();

  return 0;
}
