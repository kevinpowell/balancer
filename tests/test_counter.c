#include <iostream>

unsigned ccnt_read ()
{
  volatile unsigned cc;
  asm volatile ("mrc p15, 0, %0, c9, c13, 0" : "=r" (cc));
  return cc;
}

int main() {
  std::cout << ccnt_read() << std::endl;
}
