#include "galois/Timer.h"
#include "galois/runtime/Context.h"

#include <cstdlib>
#include <iostream>

int main(int argc, char** argv) {
  galois::runtime::SimpleRuntimeContext S;
  galois::runtime::Lockable L;

  int numAcquires = 1;
  if (argc > 1)
    numAcquires = atoi(argv[1]);
  if (numAcquires <= 0)
    numAcquires = 1024*1024*1024;

  galois::Timer t;
  t.start();
 
  for (int x = 0; x < numAcquires; ++x)
    galois::runtime::acquire(&L, galois::MethodFlag::WRITE);
  
  t.stop();
  std::cout << "Locking time: " << t.get() << " ms after " << numAcquires << "\n";
  
  return 0;
}
