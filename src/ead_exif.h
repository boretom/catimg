#ifndef __EAD_EXIF_H__
#define __EAD_EXIF_H__

#ifdef __cplusplus
#define EXTERNC extern "C"
#else
#define EXTERNC
#endif

EXTERNC int get_exif_rotation(char *file);

/* Return next input byte, or EOF if no more */
#define NEXTBYTE()  getc(f)

/* Error exit handler */
#define ERREXIT(msg)  (exit(0))

#endif // #ifndef __EAD_EXIF_H__

