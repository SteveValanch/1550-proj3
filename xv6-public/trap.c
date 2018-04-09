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

  switch(tf->trapno){
  case T_IRQ0 + IRQ_TIMER:
    if(cpuid() == 0){
      acquire(&tickslock);
      ticks++;
      wakeup(&ticks);
      release(&tickslock);
    }
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

  //PAGEBREAK: 13
  default:
    if(myproc() == 0 || (tf->cs&3) == 0){
      // In kernel, it must be our mistake.
      cprintf("unexpected trap %d from cpu %d eip %x (cr2=0x%x)\n",
              tf->trapno, cpuid(), tf->eip, rcr2());
      panic("trap");
    }
    // In user space, assume process misbehaved.
      
  if(tf->trapno == t_PGFLT)//verifying that a page fault has occured
  {//crossroads of 2x2 options: a process cant find its page: if the 
    //page is not in the file, there has been no allocation done.
    //if no allocation done, and below the limit of 15 pages in memory,
    //simply allocate and return as normal. otherwise if at 15 page limit
    //amd not allocated, swap a victim page into file, clear page, and return
    //reference to cleared page. if page IS in file and below 15 page limit,
    //copy the file into a free slot in memory. finally if at 15 page limit,
    //exchange file page for a victim memory page. 
    uint faultingAddress = rcr2();
    pte_t *pte;
    char * mem;
    //case 1: unallocated, <15 pages in memory: we know this is the case because 
    //there is no page struct with a matching address. response is to allocate
    //and return
    if((pte = walkpgdir(myproc()->pgdir, (char *)faultingAddress, 0))==0)//in this case no entry so allocate
    {
      if(myproc()->pgCtMem ==15)//memory full so must swap with file
      {
        myProc()->pageCtTotal++;
        //ADD METHOD 
      }
      else{//else allocate the space and map it
        mem = kalloc();
        if(mem == 0){
        cprintf("lazyalloc out of memory\n");
        return;//return if kalloc unsuccessful
        }
        memset(mem, 0, PGSIZE);//
        uint n = PGROUNDDOWN(rcr2());
        mappages(myproc()->pgdir, (char*)n, PGSIZE, V2P(mem), PTE_W|PTE_U);
        //now add a page struct for this page, and update the proc statistics
        int i = 0;
        for(i = 0; i < 15; i++)//increment through array to find unused page struct
        {
          if(myProc()->pages[i]->swapped == -1)//found one at index i
          {
            myProc()->pages[i]->swapped = 0;
            myProc()->pages[i]->address = mem;
            myProc()->freeInFile[i] = 0;
            myProc()->pageCtMem++;
            myProc()->pageCtTotal++;
          }  
        }  
        return ;
    }
    else
    {
       if( 
    }
    //this far means the page has been created, so pagefault indicates it is in file
    //not in memory. Here, should call a method to determine an appropriate file to swap
      
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
