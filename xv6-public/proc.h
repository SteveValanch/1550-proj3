
// Per-CPU state
struct cpu {
  uchar apicid;                // Local APIC ID
  struct context *scheduler;   // swtch() here to enter scheduler
  struct taskstate ts;         // Used by x86 to find stack for interrupt
  struct segdesc gdt[NSEGS];   // x86 global descriptor table
  volatile uint started;       // Has the CPU started?
  int ncli;                    // Depth of pushcli nesting.
  int intena;                  // Were interrupts enabled before pushcli?
  struct proc *proc;           // The process running on this cpu or null
};

extern struct cpu cpus[NCPU];
extern int ncpu;

//PAGEBREAK: 17
// Saved registers for kernel context switches.
// Don't need to save all the segment registers (%cs, etc),
// because they are constant across kernel contexts.
// Don't need to save %eax, %ecx, %edx, because the
// x86 convention is that the caller has saved them.
// Contexts are stored at the bottom of the stack they
// describe; the stack pointer is the address of the context.
// The layout of the context matches the layout of the stack in swtch.S
// at the "Switch stacks" comment. Switch doesn't save eip explicitly,
// but it is on the stack and allocproc() manipulates it.
struct context {
  uint edi;
  uint esi;
  uint ebx;
  uint ebp;
  uint eip;
};

//Page information
struct page {
  uint address;       //Virtual address of the page
  uint file_index;    //Offset into the file
  int swapped;        //1 if in file, 0 if in physical memory, -1 if uninitialized
};

#ifdef LRU
struct node {
  struct node *nextNode;  //Node below in the stack.
  struct node *previousNode;  //Node above in the stack.
  struct page *page;  //Page structure of that node.
  int inuse;  //0 if not in use, 1 if in use.
};
#endif

enum procstate { UNUSED, EMBRYO, SLEEPING, RUNNABLE, RUNNING, ZOMBIE };

// Per-process state
struct proc {
  uint sz;                     // Size of process memory (bytes)
  pde_t* pgdir;                // Page table
  char *kstack;                // Bottom of kernel stack for this process
  enum procstate state;        // Process state
  int pid;                     // Process ID
  struct proc *parent;         // Parent process
  struct trapframe *tf;        // Trap frame for current syscall
  struct context *context;     // swtch() here to run process
  void *chan;                  // If non-zero, sleeping on chan
  int killed;                  // If non-zero, have been killed
  struct file *ofile[NOFILE];  // Open files
  struct inode *cwd;           // Current directory
  char name[16];               // Process name (debugging)
  struct file *swapFile;       // Page/swap file.  Must initiate with createSwapFile.
  struct page *pages[MAX_TOTAL_PAGES];  //Pages within the process
  int freeInFile[15];          //1 if page in file, 0 if free
  int SwapIndex;               //this SwapIndex int stores the file index last paged in, for removal
  int pageCtTotal;             //under our simple test paging strategy which will be lifo
  int pageCtFile;              //Number of pages in the swap file
  int size;
  //Select which page replacement algorithm to use.
  #ifdef FIFO  //First in First Out
  struct page *queue[15];  //Queue of pages to swap out.
  #endif
  #ifdef RAND  //Random
  struct page *randpages[15];  //Array of pages to be randomly selected.
  #endif
  #ifdef LRU  //Least Recently Used
  struct node *stack[15];  //Array of nodes that represent a stack.
  struct node *head;  //Head of the linked list and the top of the stack.
  struct node *tail;  //Tail of the linked list and the bottom of the stack.
  #endif
};

// Process memory is laid out contiguously, low addresses first:
//   text
//   original data and bss
//   fixed-size stack
//   expandable heap
