//base64 source: https://github.com/elzoughby/Base64
#include <stdio.h>
#include "base64.h"

int main(void)
{
  char decoded_text[100];
  printf("Enter a string:\n");
  gets(decoded_text);
  printf("\n  plain text: %s", decoded_text);
  printf("\nencoded text: %s", base64_encode(decoded_text));
  printf("\n\npress ENTER to exit\n\n");
  getchar();
  return 0;
}
