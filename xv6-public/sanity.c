#include "types.h"
#include "user.h"
#include "syscall.h"
//#include "defs.h"

int main()
{
char *mem[20];

int i;

for (i = 0; i < 20; i++)
{
mem[i] = (char *) malloc(4096);
}

for (i = 0; i < 20; i++)
{
mem[i][0] = 'a';
}

for (i = 19; i >= 0; i--)
{
mem[i][1] = 'b';
}

for (i = 10; i < 20; i++)
{
mem[i][2] = 'c';
}

for (i = 0; i < 10; i++)
{
mem[i][2] = 'c';
}

for (i = 0; i < 20; i++)
{
printf(1, "%c%c%c\n", mem[i][0], mem[i][1], mem[i][2]);
}

exit();
}
