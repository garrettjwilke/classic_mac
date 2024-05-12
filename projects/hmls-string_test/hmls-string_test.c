#include <stdio.h>

void printString(char *str)
{
  if (*str == '\0')
  {
    return;
  }
  puts(str);
}

char getInput()
{
  char str2[100];
  printf("Enter a string:\n");
  gets(str2);
  printf("\nyou typed:\n");
  puts(str2);
}

int main(void)
{
  printString("hmls - 4382\n:)");
  getInput();
  char testvar[] = " bloop";
  printf("test string with var:%s", testvar);
  printf("\npress any key to exit\n\n");
  //puts("\npress any key to exit\n");
  getchar();
  return 0;
}
