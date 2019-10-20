#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#include "ft232h.h"

#define ROM_SIZE 0x10000

int main(int argc, char **argv) {

  int ret;
  char *filename = NULL;
  char op = ' ';
  struct ft232h_context *context;

  while((ret = getopt(argc, argv, "hr:w:v:p:")) != -1) {
    switch(ret) {
    case 'r':
      filename = optarg;
      op = 'r';
      break;
    case 'w':
      filename = optarg;
      op = 'w';
      break;
    case 'h':
      printf("Usage ft232h-eeprom -r <file> -w <file>\n");
      printf("    -r <file>: Read eeprom contents to file\n");
      printf("    -w <file>: Write file contents to eeprom\n");
      return EXIT_SUCCESS;
    default:
      return EXIT_FAILURE;
    }
  }

  context = (struct ft232h_context *)malloc(sizeof(struct ft232h_context));
  ft232h_init(context);

  char *buf;
  FILE *f;
  int size;

  switch(op) {
  case 'r':
    printf("Reading from chip to %s...\n", filename);
    buf = (char *)malloc(ROM_SIZE);
    for(int i = 0; i < ROM_SIZE; i++) {
      ft232h_read(context, i);
      buf[i] = context->data;
    }
    f = fopen(filename, "w");
    fwrite(buf, sizeof(char), ROM_SIZE -1, f);
    fclose(f);
    free(buf);
    break;
  case 'w':
    printf("Writing %s to chip...\n", filename);
    f = fopen(filename, "r");
    fseek(f , 0 , SEEK_END);
    size = ftell(f);
    rewind(f);
    buf = (char *)malloc(size);
    fread(buf, 1, size, f);
    fclose(f);
    for(int i = 0; i < size; i++) {
      ft232h_write(context, i, buf[i]);
    }
    free(buf);
    break;
  }
  ft232h_free(context);
  return EXIT_SUCCESS;
}
