#include <iostream>
#include <string>
using namespace std;

char string_function(string input_string)
{
  cout << "\nyou input the following string:\n";
  cout << input_string << endl;
  return 0;
}

int main()
{
  string get_text;
  cout << "input some text:\n";
  getline (cin, get_text);
  string_function(get_text);
  cout << "\npress ENTER to exit\n";
  getchar();
  return 0; 
}
