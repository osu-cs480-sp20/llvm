#include "stdio.h"

extern float foo();

int main() {
  float x = foo();
  printf("Return value from foo(): %f\n", x);
}
