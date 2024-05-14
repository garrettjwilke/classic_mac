// bip39 library: https://github.com/ciband/bip39
// build library above, then put the compiled 'libbip39.a' file in the root of this project


#include "bip39/bip39.h"

#include <iostream>
#include <string.h>

using namespace std;
int main(int /*argc*/, char* /*argv*/[]) {
  const auto passphrase = BIP39::generate_mnemonic
  (
     BIP39::entropy_bits_t::_256,
     BIP39::language::en
  );
  const auto test_phrase = BIP39::generate_mnemonic();
  BIP39::decode_mnemonic(test_phrase);
  cout << passphrase << endl;
  getchar();
  return 0;
}
