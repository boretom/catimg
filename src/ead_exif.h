#ifndef __EAD_EXIF_H__
#define __EAD_EXIF_H__


/* Return next input byte, or EOF if no more */
#define NEXTBYTE()  getc(f)

/* Error exit handler */
#define ERREXIT(msg)  (exit(0))

int get_exif_rotation(char *file);

#endif // #ifndef __EAD_EXIF_H__

