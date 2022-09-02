static int myintvar = 5;

static int func0 () {
  return ++myintvar;
}

int func1 (int i) {
  return func0() * i;
}
