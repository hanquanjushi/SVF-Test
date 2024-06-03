#include <fcntl.h>  /* open() */
#include <stdio.h>  /* fputs(), fprintf(), fwrite(), putc() */
#include <stdlib.h> /* exit(), malloc(), free() */
#include <unistd.h> /* close(), read(), lseek() */

#define local static

/* exit with an error (return a value to allow use in an expression) */
local int bail(char* why1, char* why2)
{
    fprintf(stderr, "gzjoin error: %s%s, output incomplete\n", why1, why2);
    exit(1);
    return 0;
}

/* -- simple buffered file input with access to the buffer -- */

#define CHUNK 32768 /* must be a power of two and fit in unsigned */

/* bin buffered input file type */
typedef struct
{
    char* name;          /* name of file for error messages */
    int fd;              /* file descriptor */
    unsigned left;       /* bytes remaining at next */
    unsigned char* next; /* next byte to read */
    unsigned char* buf;  /* allocated buffer of length CHUNK */
} bin;

/* close a buffered file and free allocated memory */
local void bclose(bin* in)
{
    if (in != NULL)
    {
        if (in->fd != -1)
            close(in->fd);
        if (in->buf != NULL)
            free(in->buf);
        free(in);
    }
}

/* open a buffered file for input, return a pointer to type bin, or NULL on
   failure */
local bin* bopen(char* name)
{
    bin* in;

    in = malloc(sizeof(bin));
    if (in == NULL)
        return NULL;
    in->buf = malloc(CHUNK);
    in->fd = open(name, O_RDONLY, 0);
    if (in->buf == NULL || in->fd == -1)
    {
        bclose(in);
        return NULL;
    }
    in->left = 0;
    in->next = in->buf;
    in->name = name;
    return in;
}

/* load buffer from file, return -1 on read error, 0 or 1 on success, with
   1 indicating that end-of-file was reached */
local int bload(bin* in)
{
    long len;

    if (in == NULL)
        return -1;
    if (in->left != 0)
        return 0;
    in->next = in->buf;
    do
    {
        len = (long)read(in->fd, in->buf + in->left, CHUNK - in->left);
        if (len < 0)
            return -1;
        in->left += (unsigned)len;
    } while (len != 0 && in->left < CHUNK);
    return len == 0 ? 1 : 0;
}

/* get a byte from the file, bail if end of file */
#define bget(in)                                                               \
    (in->left ? 0 : bload(in),                                                 \
     in->left ? (in->left--, *(in->next)++)                                    \
              : bail("unexpected end of file on ", in->name))

/* get a four-byte little-endian unsigned integer from file */
local unsigned long bget4(bin* in)
{
    unsigned long val;

    val = bget(in);
    val += (unsigned long)(bget(in)) << 8;
    val += (unsigned long)(bget(in)) << 16;
    val += (unsigned long)(bget(in)) << 24;
    return val;
}

/* skip bytes in file */
local void bskip(bin* in, unsigned skip)
{
    /* check pointer */
    if (in == NULL)
        return;

    /* easy case -- skip bytes in buffer */
    if (skip <= in->left)
    {
        in->left -= skip;
        in->next += skip;
        return;
    }

    /* skip what's in buffer, discard buffer contents */
    skip -= in->left;
    in->left = 0;

    /* seek past multiples of CHUNK bytes */
    if (skip > CHUNK)
    {
        unsigned left;

        left = skip & (CHUNK - 1);
        if (left == 0)
        {
            /* exact number of chunks: seek all the way minus one byte to check
               for end-of-file with a read */
            lseek(in->fd, skip - 1, SEEK_CUR);
            if (read(in->fd, in->buf, 1) != 1)
                bail("unexpected end of file on ", in->name);
            return;
        }

        /* skip the integral chunks, update skip with remainder */
        lseek(in->fd, skip - left, SEEK_CUR);
        skip = left;
    }

    /* read more input and skip remainder */
    bload(in);
    if (skip > in->left)
        bail("unexpected end of file on ", in->name);
    in->left -= skip;
    in->next += skip;
}

int main(int argc, char** argv)
{
    unsigned long crc, tot;
    argc--;
    argv++;
    bin a;
    bskip(&a, 0);
    return 0;
}