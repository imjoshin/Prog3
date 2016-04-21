#include <limits.h>
#ifndef PROCARRAY
#define PROCARRAY
struct proc* proc_Array[__PID_MAX];
struct lock* proc_Lock;

struct coremap {
	unsigned int size;
	struct coremap_entry* entries;
};

struct coremap* coremap;
int vm_bootstrap_done;
int max_pages;
char printThisPlease[10000];

//masks for page tables
#define UPPERTWENTYM 0xFFFFF000
#define DIRTYM 0x400
#define VALIDM 0x200
#define GLOBALM 0x100
#define EXISTSM 0x80
#endif
