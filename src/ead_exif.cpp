/*

 */

#include <stdio.h>
#include <stdlib.h>

#include "exif.h"
#include "ead_exif.h"

/* Write out Orientation value

      1        2       3      4         5            6           7          8

    888888  888888      88  88      8888888888  88                  88  8888888888
    88          88      88  88      88  88      88  88          88  88      88  88
    8888      8888    8888  8888    88          8888888888  8888888888          88
    88          88      88  88
    88          88  888888  888888 

    How to get from 2...8 to 1 (cwr, hf, vf):

    2 > hf
    3 > vf > hf
    4 > vf
    5 > vf > cwr || cwr > hf
    6 > cwr
    7 > cwr > vf || hf > cwr
    8 > cwr > vf > hf || hf > cwr > hf

 */

int get_exif_rotation(char *file) {

    FILE *fp = fopen(file, "rb");
    if (!fp) {
      printf("Can't open file.\n");
      return -1;
    }
    fseek(fp, 0, SEEK_END);
    unsigned long fsize = ftell(fp);
    rewind(fp);
    unsigned char *buf = new unsigned char[fsize];
    if (fread(buf, 1, fsize, fp) != fsize) {
      printf("Can't read file.\n");
      delete[] buf;
      return -2;
    }
    fclose(fp);

    easyexif::EXIFInfo ei;
    int code = ei.parseFrom(buf, fsize);
    delete[] buf;

    if (code) {
      printf("Error parsing EXIF: code %d\n", code); return -3;
    }

    return ei.Orientation;
}
