#include "types.h"
#include "defs.h"
#include "param.h"
#include "memlayout.h"
#include "mmu.h"
#include "proc.h"
#include "x86.h"
#include "traps.h"
#include "spinlock.h"

// Interrupt descriptor table (shared by all CPUs).
struct gatedesc idt[256];
extern uint vectors[];  // in vectors.S: array of 256 entry pointers
struct spinlock tickslock;
uint ticks;

#ifdef RAND
unsigned long randstate = 1;
#endif


void
tvinit(void)
{
  int i;

  for(i = 0; i < 256; i++)
    SETGATE(idt[i], 0, SEG_KCODE<<3, vectors[i], 0);
  SETGATE(idt[T_SYSCALL], 1, SEG_KCODE<<3, vectors[T_SYSCALL], DPL_USER);

  initlock(&tickslock, "time");
}

void
idtinit(void)
{
  lidt(idt, sizeof(idt));
}

//PAGEBREAK: 41
void
trap(struct trapframe *tf)
{
  if(tf->trapno == T_SYSCALL){
    if(myproc()->killed)
      exit();
    myproc()->tf = tf;
    syscall();
    if(myproc()->killed)
      exit();
    return;
  }

    uint faultingAddress = rcr2();
    struct proc* p = myproc();
    pte_t * pte;

  switch(tf->trapno){
  case T_IRQ0 + IRQ_TIMER:
    if(cpuid() == 0){
      acquire(&tickslock);
      ticks++;
      wakeup(&ticks);
      release(&tickslock);
    }
    
  #ifdef LRU
  //struct proc* p = myproc();
  //pte_t *pte;
  int i;
  for(i = 0; i < 15; i++)
  {
    if (p->stack[i]->inuse)
    {
      pte = walkpgdir(p->pgdir, (char *)p->stack[i]->page->address, 0);

      if (*pte & PTE_A)  //check PTE_A
      {
        //Put the node on top of the stack.
        struct node * curr, * next, * previous;
        curr = p->stack[i];

        if ((curr->previousNode != 0) || (curr->nextNode != 0))
        {
          if ((curr->previousNode != 0) && (curr->nextNode != 0))  //Move the node to the top of the stack.
          {
            previous = curr->previousNode;
            next = curr->nextNode;
            previous->nextNode = next;
            next->previousNode = previous;
            curr->previousNode = 0;
            curr->nextNode = p->head;
            p->head->previousNode = curr;
            p->head = curr;
          }
          else if (curr->nextNode == 0)  //Move the bottom node to the top of the stack.
          {
            previous = curr->previousNode;
            previous->nextNode = 0;
            curr->previousNode = 0;
            curr->nextNode = p->head;
            p->head->previousNode = curr;
            p->head = curr;
          }
        }
      }
      
      *pte &= ~PTE_A; //reset PTE_A
      
    }
  }
  #endif
      
    lapiceoi();
    break;
  case T_IRQ0 + IRQ_IDE:
    ideintr();
    lapiceoi();
    break;
  case T_IRQ0 + IRQ_IDE+1:
    // Bochs generates spurious IDE1 interrupts.
    break;
  case T_IRQ0 + IRQ_KBD:
    kbdintr();
    lapiceoi();
    break;
  case T_IRQ0 + IRQ_COM1:
    uartintr();
    lapiceoi();
    break;
  case T_IRQ0 + 7:
  case T_IRQ0 + IRQ_SPURIOUS:
    cprintf("cpu%d: spurious interrupt at %x:%x\n",
            cpuid(), tf->cs, tf->eip);
    lapiceoi();
    break;

  
    // In user space, assume process misbehaved.
      
  case T_PGFLT://verifying that a page fault has occured
   //crossroads of 2x2 options: a process cant find its page: if the 
    //page is not in the file, there has been no allocation done.
    //if no allocation done, and below the limit of 15 pages in memory,
    //simply allocate and return as normal. otherwise if at 15 page limit
    //amd not allocated, swap a victim page into file, clear page, and return
    //reference to cleared page. if page IS in file and below 15 page limit,
    //copy the file into a free slot in memory. finally if at 15 page limit,
    //exchange file page for a victim memory page. 

    if(p->pageCtTotal >= MAX_TOTAL_PAGES)
      kill(p->pid);
    //pte_t *pte;
    char * mem;
    mem = kalloc();
    //case 1: unallocated, <15 pages in memory: we know this is the case because 
    //there is no page struct with a matching address. response is to allocate
    //and return
    if((pte = walkpgdir(p->pgdir, (char *)faultingAddress, 0))==0)//in this case no entry so allocate
    {
       memset(mem, 0, PGSIZE);
    }
    else{
      struct page * swap;
      int j = 0;
      for(j = 0; j < 30; j++)
      {
         if(p->pages[j]->address == faultingAddress);
            swap = p->pages[j];
            break;
      }
      readFromSwapFile(p, mem, swap->file_index*4096,4096); 
    }
      if(p->pageCtTotal - p->pageCtFile == 15)//memory full so must swap with file
      {
        struct page *victim;
        
        //Select a victim using a page replacement algorithm.
        #ifdef FIFO    
        victim = queue[0];  //Remove the first item in the queue.
        
        int index;
        for (index = 0; index < 14; index++)  //Shift the entire queue by one space to the left.
        {
          queue[index] = queue[index+1];
        }
	#endif
        #ifdef RAND
        int replaceindex;  //Index of page to swap out.
        
        randstate = randstate * 1664525 + 1013904223;  
        replaceindex = randstate % 15  //Generate a random number between 0 and 14.
        victim = randpages[replaceindex];
	#endif
        #ifdef LRU
        victim = p->tail->page;  //Remove the item on the bottom of the stack.
        p->tail->inuse = 0;  //Remove the node from the stack.
        p->tail = p->tail->previousNode;  //The new tail is the next item on the bottom of the stack.
        p->tail->nextNode = 0;
        #endif
        
        swapOut(victim);
        
        //mem = kalloc();
        
        //if(mem == 0){
        //cprintf("lazyalloc out of memory\n");
        //return;//return if kalloc unsuccessful
        //}
        
        //memset(mem, 0, PGSIZE);
        
        uint n = PGROUNDDOWN(faultingAddress);
        
        mappages(p->pgdir, (char*)n, PGSIZE, V2P(mem), PTE_W|PTE_U);
        
        //now add a page struct for this page, and update the proc statistics
        int i = 0;
        for(i = 0; i < MAX_TOTAL_PAGES; i++)//increment through array to find unused page struct
        {
          if(p->pages[i]->swapped == -1)//found one at index i
          {
            p->pages[i]->swapped = 0;
            p->pages[i]->address =faultingAddress;
            p->freeInFile[i] = 0;
            p->pageCtFile++;
            
            break;
          }  
        }  
        
        //Add p->pages[i] to data structure.
        #ifdef FIFO    
        queue[14] = p->pages[i];  //Add p->pages[i] to the end of the queue.
	#endif
        #ifdef RAND
        randpages[replaceindex] = p->pages[i];  //Add p->pages[i] to randpages[replaceindex].
	#endif
        #ifdef LRU
        int index;
        
        for (index = 0; index < 15; index++)  //Look for a node that is not in use.
        {
          if(p->stack[index]->inuse == 0)
          {
            break;
          }
        }
        
        struct node *newNode;
        newNode = p->stack[index];
        newNode->nextNode = p->head;  //Add the new node to the top of the stack.
        newNode->previousNode = 0;
        newNode->page = p->pages[i];  //Add p->pages[i] to the new node.
        newNode->inuse = 1;
        p->head->previousNode = newNode;
        p->head = newNode;  //Make the new node the head.
        #endif
      
        return;
      }
      else{//else allocate the space and map it
       /* mem = kalloc();
        if(mem == 0){
        cprintf("lazyalloc out of memory\n");
        return;//return if kalloc unsuccessful
        }
        memset(mem, 0, PGSIZE);*/
        uint n = PGROUNDDOWN(faultingAddress);
        mappages(p->pgdir, (char*)n, PGSIZE, V2P(mem), PTE_W|PTE_U);
        //now add a page struct for this page, and update the proc statistics
        int i = 0;
        for(i = 0; i < MAX_TOTAL_PAGES; i++)//increment through array to find unused page struct
        {
          if(p->pages[i]->swapped == -1)//found one at index i
          {
            p->pages[i]->swapped = 0;
            p->pages[i]->address = faultingAddress;
            p->freeInFile[i] = 0;
            p->pageCtFile++;
            
            break;
          }
	}
          
        //Add p->pages[i] to data structure.
	#ifdef FIFO    
        p->queue[p->size] = p->pages[i];  //Add p->pages[i] to the end of the queue.
        p->size++;
	#endif
        #ifdef RAND
        p->randpages[p->size] = p->pages[i];  //Add p->pages[i] to randpages[replaceindex].
        p->size++;
        #endif
	#ifdef LRU
        struct node *newNode;
        newNode = p->stack[p->size];
        newNode->nextNode = p->head;  //Add the new node to the top of the stack.
        newNode->previousNode = 0;
        newNode->page = p->pages[i];  //Add p->pages[i] to the new node.
        newNode->inuse = 1;
        p->head->previousNode = newNode;
        p->head = newNode;  //Make the new node the head.
        p->size++;
        #endif
           
    break;
    //this far means the page has been created, so pagefault indicates it is in file
    //not in memory. Here, should call a method to determine an appropriate file to swap

  //PAGEBREAK: 13
  default:
    if(myproc() == 0 || (tf->cs&3) == 0){
      // In kernel, it must be our mistake.
      cprintf("unexpected trap %d from cpu %d eip %x (cr2=0x%x)\n",
              tf->trapno, cpuid(), tf->eip, rcr2());
      panic("trap");
    }
    cprintf("pid %d %s: trap %d err %d on cpu %d "
            "eip 0x%x addr 0x%x--kill proc\n",
            myproc()->pid, myproc()->name, tf->trapno,
            tf->err, cpuid(), tf->eip, rcr2());
    myproc()->killed = 1;
  }

  // Force process exit if it has been killed and is in user space.
  // (If it is still executing in the kernel, let it keep running
  // until it gets to the regular system call return.)
  if(myproc() && myproc()->killed && (tf->cs&3) == DPL_USER)
    exit();

  // Force process to give up CPU on clock tick.
  // If interrupts were on while locks held, would need to check nlock.
  if(myproc() && myproc()->state == RUNNING &&
     tf->trapno == T_IRQ0+IRQ_TIMER)
    yield();

  // Check if the process has been killed since we yielded
  if(myproc() && myproc()->killed && (tf->cs&3) == DPL_USER)
    exit();
}
}
