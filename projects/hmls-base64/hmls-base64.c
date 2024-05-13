// base64 source: https://github.com/elzoughby/Base64
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "base64.h"

char* separator = "      ------------------------------------------";

int encode_text()
{
  printf("\nEnter a string to encode:\n");
  // create variable for the plain string 'decoded_text'
  char decoded_text[100];
  // use the 'gets()' function to write the plain string to the 'decoded_text' variable
  gets(decoded_text);
  // create 'encoded_text' variable and assign the output of the 'base64_encode()' function
  char* encoded_text = base64_encode(decoded_text);
  // print the plain 'decoded_text' and the new 'encoded_text' vars
  printf("\nencoded text: %s \n\n", encoded_text);
  getchar();

}

int decode_text()
{
  printf("\nEnter the encoded string:\n");
  char encoded_text[100];
  gets(encoded_text);
  char* decoded_text = base64_decode(encoded_text);
  printf("\n decoded text: %s\n\n", decoded_text);
  getchar();
}

int main(void)
{
  while (1)
  {
    printf("\n");
    printf(separator);
    char* menu_string = base64_encode("system76");
    printf("\n                     %s\n", menu_string);
    printf(separator);
    printf("\nWould you like to Encode (1), Decode (2), or Quit (3)?\n\n");
    printf("1 - Encode plain text to base64\n");
    printf("2 - Decode base64 to plain text\n");
    printf("q - quit\n\n");
    char* choice;
    gets(choice);
    if (strcmp(choice, "1") == 0)
    {
      encode_text();
    }
    else if (strcmp(choice, "2") == 0)
    {
      decode_text();
    }
    else if (strcmp(choice, "q") == 0)
    {
      break;
    }
  }
  return 0;
}
