#include <stdio.h>

int test(char* str) {
  char res = str[0] - '0';

  if (res > 5) {
    printf("This is branch I\n");
    res = 3;
  }
  else {
    printf("This is branch II\n");
    res = 0;
  }
  
  printf("%d\n", res);
  return res;
}


int main(int argc, char** argv) {
  return test(argv[1]);
}
