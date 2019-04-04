#include<cassert>
#include "src/display.h"

using D = WrappedDisplay<FullDisplay<32, 64, 4>>;

int main(int argc, char **argv) {
  assert(D::encode(0, 0, 0).addr == 0);
  assert(D::encode(0, 0, 0).bit == 0);
  assert(D::encode(0, 0, 0).word == 0);

  assert(D::encode(0, 1, 0).addr == 0);
  assert(D::encode(0, 1, 0).bit == 0);
  assert(D::encode(0, 1, 0).word == 1);

  assert(D::encode(1, 0, 0).addr == 1);
  assert(D::encode(1, 0, 0).bit == 0);
  assert(D::encode(1, 0, 0).word == 0);
}
