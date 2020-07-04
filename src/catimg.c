#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "sh_image.h"
#include "sh_utils.h"
#include <unistd.h>
#include <signal.h>

#define USAGE "Usage: catimg [-hct] [-w width] [-l loops] [-r resolution] image-file\n\n" \
  "  -h: Displays this message\n"                                      \
  "  -w: Terminal width by default\n"                           \
  "  -l: Loops are only useful with GIF files. A value of 1 means that the GIF will " \
  "be displayed twice because it loops once. A negative value means infinite " \
  "looping\n"                                                           \
  "  -o: Honor EXIF orientation " \
  "  -r: Resolution must be 1 or 2. By default catimg checks for unicode support to " \
  "use higher resolution\n" \
  "  -c: Convert colors to a restricted palette\n" \
  "  -t: Disables true color (24-bit) support, falling back to 256 color\n"

// Transparency threshold -- all pixels with alpha below 25%.
#define TRANSP_ALPHA 64

extern char *optarg;
extern int optind;
extern int optopt;
extern int opterr;
extern int optreset;

// For <C-C>
volatile int loops = -1, loop = -1;
volatile char stop = 0;

void intHandler() {
    loops = loop;
    stop = 1;
}

int getopt(int argc, char * const argv[], const char *optstring);

uint32_t pixelToInt(const color_t *pixel) {
    if (pixel->a == 0)
        return 0xffff;
    else if (pixel->r == pixel->g && pixel->g == pixel->b)
        return 232 + (pixel->r * 23)/255;
    else
        return (16 + ((pixel->r*5)/255)*36
                + ((pixel->g*5)/255)*6
                + (pixel->b*5)/255);
}

char supportsUTF8() {
    const char* LC_ALL = getenv("LC_ALL");
    const char* LANG = getenv("LANG");
    const char* LC_CTYPE = getenv("LC_CTYPE");
    const char* UTF = "UTF-8";
    return (LC_ALL && strstr(LC_ALL, UTF))
        || (LANG && strstr(LANG, UTF))
        || (LC_CTYPE && strstr(LC_CTYPE, UTF));
}
/////////////////////////////////////
FILE *f = NULL;

/* Return next input byte, or EOF if no more */
#define NEXTBYTE()  getc(f)

/* Error exit handler */
#define ERREXIT(msg)  (exit(0))

/* Read one byte, testing for EOF */
static int
read_1_byte (void)
{
  int c;

  c = NEXTBYTE();
  if (c == EOF)
      ERREXIT("Premature EOF in JPEG file");

  return c;
}

/* Read 2 bytes, convert to unsigned int */
/* All 2-byte quantities in JPEG markers are MSB first */
static unsigned int
read_2_bytes (void)
{
    int c1, c2;

    c1 = NEXTBYTE();
    if (c1 == EOF)
        ERREXIT("Premature EOF in JPEG file");
    c2 = NEXTBYTE();
    if (c2 == EOF)
        ERREXIT("Premature EOF in JPEG file");

    return (((unsigned int) c1) << 8) + ((unsigned int) c2);
}

int get_exif_rotation(char *file) {

    unsigned char exif_data[65536L];

    int set_flag;
    unsigned int length, i;
    int is_motorola; /* Flag for byte order */
    unsigned int offset, number_of_tags, tagnum;


    set_flag = 0;

    if ( (f = fopen(file, "rb")) == NULL ) {
        printf("[ERROR] Oops, couldn\'t open file \'%s\'!\n", file);
    }

    /* Read File head, check for JPEG SOI + Exif APP1 */
    for (i = 0; i < 4; i++)
        exif_data[i] = (unsigned char) read_1_byte();
    if (exif_data[0] != 0xFF ||
        exif_data[1] != 0xD8 ||
        exif_data[2] != 0xFF ||
        exif_data[3] != 0xE1)
        return 0;

    /* Get the marker parameter length count */
    length = read_2_bytes();
    /* Length includes itself, so must be at least 2 */
    /* Following Exif data length must be at least 6 */
    if (length < 8)
        return 0;
    length -= 8;
    /* Read Exif head, check for "Exif" */
    for (i = 0; i < 6; i++)
        exif_data[i] = (unsigned char) read_1_byte();

    if (exif_data[0] != 0x45 ||
        exif_data[1] != 0x78 ||
        exif_data[2] != 0x69 || exif_data[3] != 0x66 ||
        exif_data[4] != 0 ||
        exif_data[5] != 0)
        return 0;

    /* Read Exif body */
    for (i = 0; i < length; i++)
        exif_data[i] = (unsigned char) read_1_byte();

    if (length < 12) return 0; /* Length of an IFD entry */

    /* Discover byte order */
    if (exif_data[0] == 0x49 && exif_data[1] == 0x49)
        is_motorola = 0;
    else if (exif_data[0] == 0x4D && exif_data[1] == 0x4D)
        is_motorola = 1;
    else
        return 0;

    /* Check Tag Mark */
    if (is_motorola) {
        if (exif_data[2] != 0) return 0;
        if (exif_data[3] != 0x2A) return 0;
    } else {
        if (exif_data[3] != 0) return 0;
        if (exif_data[2] != 0x2A) return 0;
    }

    /* Get first IFD offset (offset to IFD0) */
    if (is_motorola) {
        if (exif_data[4] != 0) return 0;
        if (exif_data[5] != 0) return 0;
        offset = exif_data[6];
        offset <<= 8;
        offset += exif_data[7];
    } else {
        if (exif_data[7] != 0) return 0;
        if (exif_data[6] != 0) return 0;
        offset = exif_data[5];
        offset <<= 8;
        offset += exif_data[4];
    }
    if (offset > length - 2) return 0; /* check end of data segment */

    /* Get the number of directory entries contained in this IFD */
    if (is_motorola) {
        number_of_tags = exif_data[offset];
        number_of_tags <<= 8;
        number_of_tags += exif_data[offset+1];
    } else {
        number_of_tags = exif_data[offset+1];
        number_of_tags <<= 8;
        number_of_tags += exif_data[offset];
    }
    if (number_of_tags == 0) return 0;
    offset += 2;

    /* Search for Orientation Tag in IFD0 */
    for (;;) {
        if (offset > length - 12) return 0; /* check end of data segment */
        /* Get Tag number */
        if (is_motorola) {
            tagnum = exif_data[offset];
            tagnum <<= 8;
            tagnum += exif_data[offset+1];
        } else {
            tagnum = exif_data[offset+1];
            tagnum <<= 8;
            tagnum += exif_data[offset];
        }
        if (tagnum == 0x0112) break; /* found Orientation Tag */
        if (--number_of_tags == 0) return 0;
        offset += 12;
    }
    /* Get the Orientation value */
    if (is_motorola) {
        if (exif_data[offset+8] != 0) return 0;
        set_flag = exif_data[offset+9];
    } else {
        if (exif_data[offset+9] != 0) return 0;
        set_flag = exif_data[offset+8];
    }
    if (set_flag > 8) return 0;

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

    printf("EXIF orientation %c\n", '0' + set_flag);

    return set_flag;
}

int main(int argc, char *argv[])
{
    init_hash_colors();
    image_t img;
    char *file;
    char *num;
    int c;
    opterr = 0;

    uint32_t cols = 0, precision = 0;
    uint8_t convert = 0;
    uint8_t true_color = 1, orientation = 0;
    while ((c = getopt (argc, argv, "w:l:r:hcot")) != -1)
        switch (c) {
            case 'w':
                cols = strtol(optarg, &num, 0) >> 1;
                break;
            case 'l':
                loops = strtol(optarg, &num, 0);
                break;
            case 'r':
                precision = strtol(optarg, &num, 0);
                break;
            case 'h':
                printf(USAGE);
                exit(0);
                break;
            case 'c':
                convert = 1;
                break;
            case 'o':
                orientation = 1;
                break;
             case 't':
                true_color = 0;
                break;
            default:
                printf(USAGE);
                exit(1);
                break;
        }

    if (argc > 1)
        file = argv[argc-1];
    else {
        printf(USAGE);
        exit(1);
    }

    if (precision == 0 || precision > 2) {
        if (supportsUTF8())
            precision = 2;
        else
            precision = 1;
    }

    // get EXIF rotation (only for certain JPG right now)
    if (orientation) orientation = get_exif_rotation(file);

    if (cols < 1) // if precision is 2 we can use the terminal full width. Otherwise we can only use half
        cols = terminal_columns() / (2 / precision);

    if (strcmp(file, "-") == 0) {
        img_load_from_stdin(&img);
    } else {
        img_load_from_file(&img, file);
    }
    if (cols < img.width) {
        float sc = cols/(float)img.width;
        img_resize(&img, sc, sc, orientation);
    }
    if (convert)
        img_convert_colors(&img);
    /*printf("Loaded %s: %ux%u. Console width: %u\n", file, img.width, img.height, cols);*/
    // For GIF
    if (img.frames > 1) {
        system("clear");
        signal(SIGINT, intHandler);
    } else {
        loops = 0;
    }
    // Save the cursor position and hide it
    printf("\e[s\e[?25l");
    while (loop++ < loops || loops < 0) {
        uint32_t offset = 0;
        for (uint32_t frame = 0; frame < img.frames; frame++) {
            if (frame > 0 || loop > 0) {
                if (frame > 0)
                    usleep(img.delays[frame - 1] * 10000);
                else
                    usleep(img.delays[img.frames - 1] * 10000);
                printf("\e[u");
            }
            uint32_t index, x, y;
            for (y = 0; y < img.height; y += precision) {
                for (x = 0; x < img.width; x++) {
                    index = y * img.width + x + offset;
                    const color_t* upperPixel = &img.pixels[index];
                    uint32_t fgCol = pixelToInt(upperPixel);
                    if (precision == 2) {
                        if (y < img.height - 1) {
                            const color_t* lowerPixel = &img.pixels[index + img.width];
                            uint32_t bgCol = pixelToInt(&img.pixels[index + img.width]);
                            if (upperPixel->a < TRANSP_ALPHA) { // first pixel is transparent
                                if (lowerPixel->a < TRANSP_ALPHA)
                                    printf("\e[m ");
                                else
                                    if (true_color)
                                        printf("\x1b[0;38;2;%d;%d;%dm\u2584",
                                               lowerPixel->r, lowerPixel->g, lowerPixel->b);
                                    else
                                        printf("\e[0;38;5;%um\u2584", bgCol);
                            } else {
                                if (lowerPixel->a < TRANSP_ALPHA)
                                    if (true_color)
                                        printf("\x1b[0;38;2;%d;%d;%dm\u2580",
                                               upperPixel->r, upperPixel->g, upperPixel->b);
                                    else
                                        printf("\e[0;38;5;%um\u2580", fgCol);
                                else
                                    if (true_color)
                                        printf("\x1b[48;2;%d;%d;%dm\x1b[38;2;%d;%d;%dm\u2584",
                                               upperPixel->r, upperPixel->g, upperPixel->b,
                                               lowerPixel->r, lowerPixel->g, lowerPixel->b);
                                    else
                                         printf("\e[38;5;%u;48;5;%um\u2580", fgCol, bgCol);
                            }
                        } else { // this is the last line
                            if (upperPixel->a < TRANSP_ALPHA)
                                printf("\e[m ");
                            else
                                if (true_color)
                                    printf("\x1b[0;38;2;%d;%d;%dm\u2580",
                                           upperPixel->r, upperPixel->g, upperPixel->b);
                                else
                                    printf("\e[38;5;%um\u2580", fgCol);
                        }
                    } else {
                        if (img.pixels[index].a < TRANSP_ALPHA)
                            printf("\e[m  ");
                        else
                            if (true_color)
                                printf("\x1b[0;48;2;%d;%d;%dm  ",
                                       img.pixels[index].r, img.pixels[index].g, img.pixels[index].b);
                            else
                                printf("\e[48;5;%um  ", fgCol);
                    }
                }
                printf("\e[m\n");
            }
            offset += img.width * img.height;
            if (stop) frame = img.frames;
        }
    }
    // Display the cursor again
    printf("\e[?25h");

    img_free(&img);
    free_hash_colors();
    return 0;
}
