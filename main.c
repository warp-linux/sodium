#include "stdio.h"
#include "string.h"
#include "stdlib.h"
#include "unistd.h"
#include "sys/types.h"
#include "sys/stat.h"
#include "fcntl.h"
#include "errno.h"
#include "limits.h"
#include "libtar.h"
#include "libcurl/curl.h"

#ifdef HAVE_GETOPT_H
# include "getopt.h"
#endif

#define BLOCKSIZE (1024 * 64 - 1)
#define MAX_BLOCKSIZE BLOCKSIZE

#define FORCE_OPT 'f'
#define VERBOSE_OPT 'v'
#define HELP_OPT 'h'
#define SYNC_OPT 's'
#define GET_OPT 'g'

typedef unsigned char u8;

static off_t nr_read, nr_written;

static const char *imagename;
static enum { compress, uncompress, lzcat } mode = compress;
static int verbose = 0;
static int force = 0;
static long blocksize = BLOCKSIZE;

#ifdef HAVE_GETOPT_LONG

  struct option longopts[] = {
    {"force", 0, 0, FORCE_OPT},
    {"help", 0, 0, HELP_OPT},
    {"verbose", 0, 0, VERBOSE_OPT},
    {"sync", 0, 0, SYNC_OPT},
    {"get", 1, 0, GET_OPT},


    {0, 0, 0, 0}
  };

  static const char *opt =
    "-f --force       force overwrite of output file\n"
    "-h --help        give this help\n" "-v --verbose     verbose mode\n" "-b # --blocksize # set blocksize\n" "\n"
    "-s --sync        sync a sodium package\n"
    "-g --get         get a package from the internet without installing it\n";

#else

  static const char *opt =
    "-f   force overwrite of output file\n"
    "-h   give this help\n"
    "-v   verbose mode\n"
    "-s   sync package\n"
    "-g   download package\n"
    "\n";

#endif

static void usage (int rc) {

  fprintf (stderr, "\n"
           "sodium, the distro-agnostic linux (and other *NIX) package manager. \n"
           "uses liblzf as it's compression library. \n"
           "written by the WarpOS team under the GPLv3 \n"
           "\n"
           "usage: sodium [-dufhvbsg] [file ...] \n"
           "       unpack [file ...] \n"
           "\n%s",
           opt);

  exit (rc);

}

static inline ssize_t rread (int fd, void *buf, size_t len) {

  ssize_t rc = 0, offset = 0;
  char *p = buf;

  while (len && (rc = read (fd, &p[offset], len)) > 0)
    {
      offset += rc;
      len -= rc;
    }

  nr_read += offset;

  if (rc < 0)
    return rc;

  return offset;

}

/* returns 0 if all written else -1 */
static inline ssize_t wwrite (int fd, void *buf, size_t len) {

  ssize_t rc;
  char *b = buf;
  size_t l = len;

  while (l)
    {
      rc = write (fd, b, l);
      if (rc < 0)
        {
          fprintf (stderr, "%s: write error: ", imagename);
          perror ("");
          return -1;
        }

      l -= rc;
      b += rc;
    }

  nr_written += len;
  return 0;

}

/*
 * Anatomy: an lzf file consists of any number of blocks in the following format:
 *
 * \x00   EOF (optional)
 * "ZV\0" 2-byte-usize <uncompressed data>
 * "ZV\1" 2-byte-csize 2-byte-usize <compressed data>
 * "ZV\2" 4-byte-crc32-0xdebb20e3 (NYI)
 */

size_t curl_wwrite (void *ptr, size_t size, size_t nmemb, FILE *stream) {
    size_t written = fwrite(ptr, size, nmemb, stream);
    return written;
}

int download(char *url) {

    CURL *curl;
    FILE *fp;
    CURLcode res;
    /* TO CHANGE */
    char outfilename[FILENAME_MAX] = "/tmp/tmp.tar.gz";
    /* TO CHANGE */
    curl = curl_easy_init();
    if (curl) {
        fp = fopen(outfilename,"wb");
        curl_easy_setopt(curl, CURLOPT_URL, url);
        curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, curl_wwrite);
        curl_easy_setopt(curl, CURLOPT_WRITEDATA, fp);
        res = curl_easy_perform(curl);
        /* always cleanup */
        curl_easy_cleanup(curl);
        fclose(fp);
    }
    return 0;

}

#define TYPE0_HDR_SIZE 5
#define TYPE1_HDR_SIZE 7
#define MAX_HDR_SIZE 7
#define MIN_HDR_SIZE 5

static int open_out (const char *name) {

  int fd;
  int m = O_EXCL;

  if (force)
    m = 0;

  fd = open (name, O_CREAT | O_WRONLY | O_TRUNC | m, 600);
#if defined(__MINGW32__)
  _setmode(fd, _O_BINARY);
#endif
  return fd;
}

int psync(char *pkgname) {

  const char repotemplate[128] = "https://github.com/warp-linux/";
  const char prefix[12]        = ".tar.gz";

  char       workstr[256]; 

  strcpy(workstr, repotemplate);
  strcat(workstr, pkgname);
  strcat(workstr, prefix);
  printf("%s", workstr);

  download(workstr);
  chdir ("/tmp/");
  popen("tar xzpf tmp.tar.gz", "r");

  return 0;

}

int plocalsync(char *tar) {

    char       workstr[256]; 

    strcpy(workstr, "cp ");
    strcat(workstr, tar);
    strcat(workstr, " tmp.tar.gz");
    printf("%s", workstr);
    printf("\n");
    printf("\n");

    popen(workstr, "r");
    popen("./sodium-install.sh", "r");

  return 0;

}

int main (int argc, char *argv[]) {

  char *p = argv[0];
  int optc;
  int rc = 0;

  errno = 0;

#ifdef HAVE_GETOPT_LONG
  while ((optc = getopt_long (argc, argv, "fhvbsg:", longopts, 0)) != -1)
#else
  while ((optc = getopt (argc, argv, "fhvbsg:")) != -1)
#endif
    {
      switch (optc)
        {
          case FORCE_OPT:
            force = 1;
            break;
          case HELP_OPT:
            usage (0);
            break;
          case VERBOSE_OPT:
            verbose = 1;
            break;
	  case SYNC_OPT:
	    plocalsync(argv[2]);
	    break;
	  case GET_OPT:
	    download(argv[2]);
            break;
          default:
            usage (1);
            break;
        }
    }

  return 0;

}
