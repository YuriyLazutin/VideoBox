#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <dirent.h> // DIR, struct dirent, PATH_MAX
#include <fcntl.h>
#include <sys/stat.h>
#include <termios.h> // non-canonical mode for terminal, struct termios, tcgetattr, tcsetattr
#include <sys/time.h> // timeval, gettimeofday
#include "defines.h"

struct candidate
{
  char* src;
  char* target;
  char* note;
  char rec;
};

struct application_variables
{
  // paths and targets
  char* server_dir;
  ssize_t server_dir_length;
  char* showboard_dir;
  ssize_t showboard_dir_length;
  char sig[SIG_SIZE];
  // charsets
  char* id_char_set;
  int   id_char_set_len;
  // terminal settings
  struct termios stored_settings;
  // candidates
  struct candidate video;
  struct candidate trumb;
  struct candidate title;
  struct candidate descr;
} *papv;

int init();
int destroy();
int strcmp_ncase(const char* str1, const char* str2);
void set_keypress();
void reset_keypress();
int find_candidates(struct candidate*, struct candidate*, struct candidate*, struct candidate*);
void print_candidate(struct candidate* c, char* label);
void print_candidates(struct candidate*, struct candidate*, struct candidate*, struct candidate*);
int suggest_candidates(struct candidate*, struct candidate*, struct candidate*, struct candidate*);
int check_video_candidate(struct candidate*);
int check_trumb_candidate(struct candidate*);
int check_title_candidate(struct candidate*);
int check_descr_candidate(struct candidate*);
int request_file_name(struct candidate* c, const char* label);
int request_file_name_or_it_contents(struct candidate* c, const char* label, const char* default_file_name, const ssize_t limit);
int make_targets(struct candidate*, struct candidate*, struct candidate*, struct candidate*);
char ask_yes_no(const char*);
int make_signature(const struct candidate*, char*);
int check_directory(const char*);
int make_directory(const char*);
int check_duplicates(const struct candidate*, char*);
int test_file(const char*);
int filecmp(const char*, const char*);
int make_id_directory(char*);
int publish_candidate(const struct candidate*, char*);
char* ext_fgets(char*, int, FILE*);

int main()
{
  struct application_variables apv;
  papv = &apv;

  int rc = init();
  if (rc != NO_ERRORS)
    return EXIT_FAILURE;

  rc = find_candidates(&apv.video, &apv.trumb, &apv.title, &apv.descr);
  if (rc != NO_ERRORS)
    return EXIT_FAILURE;

  // Analyze file size and notify user if it empty or large then possible
  check_video_candidate(&apv.video);
  check_trumb_candidate(&apv.trumb);
  check_title_candidate(&apv.title);
  check_descr_candidate(&apv.descr);

  // Suggest user candidates
  suggest_candidates(&apv.video, &apv.trumb, &apv.title, &apv.descr);

  if (!apv.video.src || apv.video.rec == 'Y')
  {
    rc = request_file_name(&apv.video, "video");
    if (rc != NO_ERRORS)
      return EXIT_FAILURE;
  }
  if (!apv.trumb.src || apv.trumb.rec == 'Y')
  {
    rc = request_file_name(&apv.trumb, "trumbnail");
    if (rc != NO_ERRORS)
      return EXIT_FAILURE;
  }

  if (apv.title.src && apv.title.rec == 'Y')
    apv.title.rec = ask_yes_no("Would you like to change title file (Y/N)?");

  if ( !apv.title.src || (apv.title.src && apv.title.rec == 'Y') )
  {
    rc = request_file_name_or_it_contents(&apv.title, "title", "title.txt", SMALL_BUFFER_SIZE);
    if (rc != NO_ERRORS)
      return EXIT_FAILURE;
  }

  if (apv.descr.src && apv.descr.rec == 'Y')
    apv.descr.rec = ask_yes_no("Would you like to change description file (Y/N)?");

  if ( !apv.descr.src || (apv.descr.src && apv.descr.rec == 'Y') )
  {
    rc = request_file_name_or_it_contents(&apv.descr, "description", "descr.html", STANDARD_BUFFER_SIZE);
    if (rc != NO_ERRORS)
      return EXIT_FAILURE;
  }

  rc = make_targets(&apv.video, &apv.trumb, &apv.title, &apv.descr);
  if (rc != NO_ERRORS)
    return EXIT_FAILURE;

  rc = make_signature(&apv.video, apv.sig);
  if (rc != NO_ERRORS)
    return EXIT_FAILURE;

  rc = check_directory(apv.showboard_dir);
  if (rc == NOT_FOUND)
    rc = make_directory(apv.showboard_dir);
  if (rc != NO_ERRORS)
    return EXIT_FAILURE;

  char buf[PATH_MAX];
  strcpy(buf, apv.showboard_dir);
  memcpy(buf + apv.showboard_dir_length, apv.sig, SIG_SIZE);
  buf[apv.showboard_dir_length + SIG_SIZE] = '\0';
  rc = check_directory(buf);
  if (rc == NOT_FOUND)
    rc = make_directory(buf);
  if (rc != NO_ERRORS)
    return EXIT_FAILURE;

  // Calculate id
  buf[apv.showboard_dir_length + SIG_SIZE] = '/';
  buf[apv.showboard_dir_length + SIG_SIZE + 1] = '\0';

  rc = check_duplicates(&apv.video, buf);
  if (rc != NO_ERRORS)
    return EXIT_FAILURE;

  rc = make_id_directory(buf);
  if (rc != NO_ERRORS)
    return EXIT_FAILURE;

  // Move files into showboard
  buf[apv.showboard_dir_length + SIG_SIZE + 1 + ID_SIZE] = '/';
  buf[apv.showboard_dir_length + SIG_SIZE + 1 + ID_SIZE + 1] = '\0';

  rc = publish_candidate(&apv.video, buf);
  if (rc != NO_ERRORS)
    return EXIT_FAILURE;

  rc = publish_candidate(&apv.trumb, buf);
  if (rc != NO_ERRORS)
    return EXIT_FAILURE;

  rc = publish_candidate(&apv.title, buf);
  if (rc != NO_ERRORS)
    return EXIT_FAILURE;

  rc = publish_candidate(&apv.descr, buf);
  if (rc != NO_ERRORS)
    return EXIT_FAILURE;

  rc = destroy();
  if (rc != NO_ERRORS)
    return EXIT_FAILURE;

  printf("Your files sucessfully published.\n");
  return EXIT_SUCCESS;
}

int init()
{
  // paths
  papv->server_dir = malloc(PATH_MAX);
  if (!papv->server_dir)
    return MALLOC_FAILED;
  papv->server_dir_length = readlink("/proc/self/exe", papv->server_dir, PATH_MAX - 1);
  if (papv->server_dir_length == -1)
    return READ_LINK_ERROR;
  while (papv->server_dir_length > 0 && papv->server_dir[papv->server_dir_length - 1] != '/') papv->server_dir_length--;
  if (!papv->server_dir_length)
    return PATH_INVALID;
  papv->server_dir[papv->server_dir_length] = '\0';
  papv->server_dir = realloc(papv->server_dir, papv->server_dir_length + 1);
  if (!papv->server_dir)
    return MALLOC_FAILED;

  papv->showboard_dir_length = papv->server_dir_length + strlen("showboard/");
  if (papv->showboard_dir_length + 1 > PATH_MAX)
    return PATH_OVERFLOW;
  papv->showboard_dir = malloc(papv->showboard_dir_length + 1);
  if (!papv->showboard_dir)
    return MALLOC_FAILED;
  strcpy(papv->showboard_dir, papv->server_dir);
  strcpy(papv->showboard_dir + papv->server_dir_length, "showboard/");

  papv->video.src = NULL;
  papv->video.target = NULL;
  papv->video.note = NULL;
  papv->video.rec = 'N';
  papv->trumb.src = NULL;
  papv->trumb.target = NULL;
  papv->trumb.note = NULL;
  papv->trumb.rec = 'N';
  papv->title.src = NULL;
  papv->title.target = NULL;
  papv->title.note = NULL;
  papv->title.rec = 'N';
  papv->descr.src = NULL;
  papv->descr.target = NULL;
  papv->descr.note = NULL;
  papv->descr.rec = 'N';

  // store terminal settings
  tcgetattr(0, &papv->stored_settings);

  // charsets
  papv->id_char_set = strdup(ID_CHARS);
  papv->id_char_set_len = strlen(papv->id_char_set);

  // random generator
  struct timeval tv;
  gettimeofday(&tv, NULL);
  srand((unsigned int)tv.tv_usec);

  return NO_ERRORS;
}

int destroy()
{
  // candidates
  if (papv->descr.note)
    free(papv->descr.note);
  if (papv->title.note)
    free(papv->title.note);
  if (papv->trumb.note)
    free(papv->trumb.note);
  if (papv->video.note)
    free(papv->video.note);

  if (papv->descr.target)
    free(papv->descr.target);
  if (papv->title.target)
    free(papv->title.target);
  if (papv->trumb.target)
    free(papv->trumb.target);
  if (papv->video.target)
    free(papv->video.target);

  if (papv->descr.src)
    free(papv->descr.src);
  if (papv->title.src)
    free(papv->title.src);
  if (papv->trumb.src)
    free(papv->trumb.src);
  if (papv->video.src)
    free(papv->video.src);

  // charsets
  if (papv->id_char_set)
    free(papv->id_char_set);

  // paths
  if (papv->server_dir)
    free(papv->server_dir);
  if (papv->showboard_dir)
    free(papv->showboard_dir);
  return NO_ERRORS;
}

int strcmp_ncase(const char* str1, const char* str2)
{
  int mx = 'a' < 'A' ? 'A' : 'a';
  int inc = 'a' < 'A' ? 'A' - 'a' : 'a' - 'A';
  char c1, c2;

  while (*str1 != '\0' && *str2 != '\0')
  {
    c1 = *str1;
    if (c1 >= mx && c1 < mx + inc)
      c1 -= inc;
    c2 = *str2;
    if (c2 >= mx && c2 < mx + inc)
      c2 -= inc;

    if ( c1 == c2)
    {
      str1++;
      str2++;
    }
    else if (c1 > c2)
      return 1;
    else if (c1 < c2)
      return -1;
  }

  if (*str1 == '\0' && *str2 == '\0')
    return 0;
  else if (*str1 == '\0')
    return 1;
  else if (*str2 == '\0')
    return -1;

  return UNKNOWN_ERROR;
}

void set_keypress()
{
  struct termios new_settings = papv->stored_settings;

  // Disable canonical mode, and set buffer size to 1 byte
  new_settings.c_lflag &= (~ICANON);
  new_settings.c_cc[VTIME] = 0;
  new_settings.c_cc[VMIN] = 1;

  tcsetattr(0, TCSANOW, &new_settings);
}

void reset_keypress()
{
  tcsetattr(0, TCSANOW, &papv->stored_settings);
}

/* Scan current folder and try to find:
    a) Video candidate (listed in high priority order)
        1) video.mp4
        2) video.webm
        3) *.mp4, *.webm
    b) Trumbnail candidate (listed in high priority order)
        1) trumb.png
        2) trumb.jpg, trumb.jpeg
        3) trumb.webp
        4) *.png
        5) *.jpg, *.jpeg
        6) *.webp
    c) Title candidate (listed in high priority order)
        1) title.txt, title.text
        2) title.htm, title.html
        3) *.txt, *.text
    d) Description candidate (listed in high priority order)
        1) descr.htm, descr.html
        2) descr.txt, descr.text
        3) *.htm, *.html
*/
int find_candidates(struct candidate* video, struct candidate* trumb, struct candidate* title, struct candidate* descr)
{
  DIR *cur_dir;
  struct dirent *entry;
  struct stat st;
  ssize_t length;

  if ((cur_dir = opendir(".")) != NULL)
  {
    while ((entry = readdir(cur_dir)) != NULL)
    {
      if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0)
        continue;
      if (stat(entry->d_name, &st) != 0)
        continue;
      if (!S_ISREG(st.st_mode))
        continue;

      length = strlen(entry->d_name);

      if (!video->src &&
                         (
                          strcmp_ncase(entry->d_name, "video.mp4") == 0 ||
                          strcmp_ncase(entry->d_name, "video.webm") == 0 ||
                          (length >= 4 && strcmp_ncase(entry->d_name + length - 4, ".mp4") == 0) ||
                          (length >= 5 && strcmp_ncase(entry->d_name + length - 5, ".webm") == 0)
                         )
         )
      {
        video->src = strdup(entry->d_name);
        continue;
      }
      else if ( video->src && strcmp_ncase(entry->d_name, "video.webm") == 0 && !(strcmp_ncase(video->src, "video.mp4") == 0) )
      {
        free(video->src);
        video->src = strdup(entry->d_name);
        continue;
      }
      else if ( video->src && strcmp_ncase(entry->d_name, "video.mp4") == 0 )
      {
        free(video->src);
        video->src = strdup(entry->d_name);
        continue;
      }

      // Trumbs
      if (!trumb->src &&
                         (
                          strcmp_ncase(entry->d_name, "trumb.png") == 0 ||
                          strcmp_ncase(entry->d_name, "trumb.jpg") == 0 ||
                          strcmp_ncase(entry->d_name, "trumb.jpeg") == 0 ||
                          strcmp_ncase(entry->d_name, "trumb.webp") == 0 ||
                          (length >= 4 && strcmp_ncase(entry->d_name + length - 4, ".png") == 0) ||
                          (length >= 4 && strcmp_ncase(entry->d_name + length - 4, ".jpg") == 0) ||
                          (length >= 5 && strcmp_ncase(entry->d_name + length - 5, ".jpeg") == 0) ||
                          (length >= 5 && strcmp_ncase(entry->d_name + length - 5, ".webp") == 0)
                         )
         )
      {
        trumb->src = strdup(entry->d_name);
        continue;
      }
      else if ( trumb->src && strcmp_ncase(entry->d_name, "trumb.jpeg") == 0 && strcmp_ncase(trumb->src, "trumb.webp") == 0 )
      {
        free(trumb->src);
        trumb->src = strdup(entry->d_name);
        continue;
      }
      else if ( trumb->src && strcmp_ncase(entry->d_name, "trumb.jpg") == 0 && strcmp_ncase(trumb->src, "trumb.webp") == 0 )
      {
        free(trumb->src);
        trumb->src = strdup(entry->d_name);
        continue;
      }
      else if ( trumb->src && strcmp_ncase(entry->d_name, "trumb.webp") == 0 && !(strcmp_ncase(trumb->src, "trumb.png") == 0 || strcmp_ncase(trumb->src, "trumb.jpg") == 0) )
      {
        free(trumb->src);
        trumb->src = strdup(entry->d_name);
        continue;
      }
      else if ( trumb->src && strcmp_ncase(entry->d_name, "trumb.png") == 0 )
      {
        free(trumb->src);
        trumb->src = strdup(entry->d_name);
        continue;
      }

      // Titles
      if (!title->src &&
                         (
                          strcmp_ncase(entry->d_name, "title.txt") == 0 ||
                          strcmp_ncase(entry->d_name, "title.text") == 0 ||
                          strcmp_ncase(entry->d_name, "title.html") == 0 ||
                          strcmp_ncase(entry->d_name, "title.htm") == 0 ||
                          (length >= 4 && strcmp_ncase(entry->d_name + length - 4, ".txt") == 0) ||
                          (length >= 5 && strcmp_ncase(entry->d_name + length - 5, ".text") == 0)
                         )
         )
      {
        title->src = strdup(entry->d_name);
        continue;
      }
      else if ( title->src &&
                (strcmp_ncase(entry->d_name, "title.htm") == 0 || strcmp_ncase(entry->d_name, "title.html") == 0) &&
                !(strcmp_ncase(title->src, "title.txt") == 0 || strcmp_ncase(title->src, "title.text") == 0)
              )
      {
        free(title->src);
        title->src = strdup(entry->d_name);
        continue;
      }
      else if ( title->src &&
                (strcmp_ncase(entry->d_name, "title.text") == 0 || strcmp_ncase(entry->d_name, "title.txt") == 0)
              )
      {
        free(title->src);
        title->src = strdup(entry->d_name);
        continue;
      }

      // Descriptions
      if ( !descr->src &&
                          (
                           strcmp_ncase(entry->d_name, "descr.htm") == 0 ||
                           strcmp_ncase(entry->d_name, "descr.html") == 0 ||
                           strcmp_ncase(entry->d_name, "descr.txt") == 0 ||
                           strcmp_ncase(entry->d_name, "descr.text") == 0 ||
                           (length >= 4 && strcmp_ncase(entry->d_name + length - 4, ".htm") == 0) ||
                           (length >= 5 && strcmp_ncase(entry->d_name + length - 5, ".html") == 0) ||
                           (length >= 4 && strcmp_ncase(entry->d_name + length - 4, ".txt") == 0) ||
                           (length >= 5 && strcmp_ncase(entry->d_name + length - 5, ".text") == 0)
                          )
        )
      {
        descr->src = strdup(entry->d_name);
        continue;
      }
      else if ( descr->src &&
                (strcmp_ncase(entry->d_name, "descr.text") == 0 || strcmp_ncase(entry->d_name, "descr.txt") == 0) &&
                !(strcmp_ncase(descr->src, "descr.html") == 0)
              )
      {
        free(descr->src);
        descr->src = strdup(entry->d_name);
        continue;
      }
      else if ( descr->src &&
                (strcmp_ncase(entry->d_name, "descr.html") == 0 || strcmp_ncase(entry->d_name, "descr.htm") == 0)
              )
      {
        free(descr->src);
        descr->src = strdup(entry->d_name);
        continue;
      }
    }
    closedir(cur_dir);
  }
  else
  {
    fprintf(stderr, "find_candidates: Can't open current directory.\n");
    return OPEN_DIR_ERROR;
  }

  return NO_ERRORS;
}

void print_candidate(struct candidate* c, char* label)
{
  if (c->src)
  {
    printf("%s: ", label);
    for (int i = strlen(label) + 2; i < 20; i++)
      printf(" ");
    printf("%s", c->src);
    if (c->note)
      printf(" (%s)", c->note);
    printf("\n");
  }
}

void print_candidates(struct candidate* video, struct candidate* trumb, struct candidate* title, struct candidate* descr)
{
  print_candidate(video, "Video file");
  print_candidate(trumb, "Trumbnail image");
  print_candidate(title, "Title file");
  print_candidate(descr, "Description file");
}

int suggest_candidates(struct candidate* video, struct candidate* trumb, struct candidate* title, struct candidate* descr)
{
  if (video->src || trumb->src || title->src || descr->src)
  {
    printf("Automatically detected\n");
    print_candidates(video, trumb, title, descr);
    printf("\n");

    // Accept user choice
    char yes_no = ask_yes_no("Is this information correct (Y/N)?");
    if (yes_no != 'Y')
      video->rec = trumb->rec = title->rec = descr->rec = 'Y';
  }
  return NO_ERRORS;
}

int check_video_candidate(struct candidate* c)
{
  struct stat st;
  if (c->src)
  {
    if (stat(c->src, &st) == -1)
    {
      fprintf(stderr, "check_video_candidate->src (%s) stat failed (%s).\n", c->src, strerror(errno));
      if (c->note)
        free(c->note);
      c->note = strdup("stat failed!");
      return STAT_FAILED;
    }
    else if (st.st_size == 0)
    {
      if (c->note)
        free(c->note);
      c->note = strdup("Looks like video file empty!");
    }
  }
  return NO_ERRORS;
}

int check_trumb_candidate(struct candidate* c)
{
  struct stat st;
  if (c->src)
  {
    if (stat(c->src, &st) == -1)
    {
      fprintf(stderr, "check_trumb_candidate->src (%s) stat failed (%s).\n", c->src, strerror(errno));
      if (c->note)
        free(c->note);
      c->note = strdup("stat failed!");
      return STAT_FAILED;
    }
    else if (st.st_size == 0)
    {
      if (c->note)
        free(c->note);
      c->note = strdup("Looks like trumbnail file iempty!");
    }
    else if (st.st_size >= 5 * 1024 * 1024)
    {
      if (c->note)
        free(c->note);
      c->note = strdup("Looks like trumbnail file too large!");
    }
  }
  return NO_ERRORS;
}

int check_title_candidate(struct candidate* c)
{
  struct stat st;
  if (c->src)
  {
    if (stat(c->src, &st) == -1)
    {
      fprintf(stderr, "check_title_candidate->src (%s) stat failed (%s).\n", c->src, strerror(errno));
      if (c->note)
        free(c->note);
      c->note = strdup("stat failed!");
      return STAT_FAILED;
    }
    else if (st.st_size == 0)
    {
      if (c->note)
        free(c->note);
      c->note = strdup("Looks like title file empty!");
    }
    else if (st.st_size >= SMALL_BUFFER_SIZE)
    {
      if (c->note)
        free(c->note);
      c->note = strdup("Looks like title too large!");
    }
  }
  return NO_ERRORS;
}

int check_descr_candidate(struct candidate* c)
{
  struct stat st;
  if (c->src)
  {
    if (stat(c->src, &st) == -1)
    {
      fprintf(stderr, "check_descr_candidate->src (%s) stat failed (%s).\n", c->src, strerror(errno));
      if (c->note)
        free(c->note);
      c->note = strdup("stat failed!");
      return STAT_FAILED;
    }
    else if (st.st_size == 0)
    {
      if (c->note)
        free(c->note);
      c->note = strdup("Looks like description file empty!");
    }
    else if (st.st_size >= STANDARD_BUFFER_SIZE)
    {
      if (c->note)
        free(c->note);
      c->note = strdup("Looks like description file too large!");
    }
  }
  return NO_ERRORS;
}

int request_file_name(struct candidate* c, const char* label)
{
  char buf[PATH_MAX];
  char* retstr;
  ssize_t length;
  struct stat st;

  while(1)
  {
    if (c->src)
      printf("Prompt %s file name (Press Enter to use \"%s\" as is): ", label, c->src);
    else
      printf("Prompt %s file name: ", label);

    // Following cases will work
    //   "file_name"
    //   "./file_name"
    //   "/full/path/to/file_name"
    // And this will not work
    //   "~/path/to/file_name"
    retstr = ext_fgets(buf, sizeof(buf), stdin);
    if (retstr == NULL) // error or EOF
    {
      fprintf(stderr, "request_file_name: ext_fgets returned NULL (%s).\n", strerror(errno));
      return UNKNOWN_ERROR;
    }

    length = strlen(buf);
    while (length > 0 && (buf[length-1] == '\n' || buf[length-1] == ' ' || buf[length-1] == '/'))
      buf[--length] = '\0';

    if (c->src && length == 0) // Enter pressed or something like this
      break;

    if (stat(buf, &st) != 0 || !S_ISREG(st.st_mode))
      printf("File doesn't exists! (Ctrl+D for exit)\n");
    else
    {
      c->src = realloc(c->src, length + 1);
      if (!c->src)
      {
        fprintf(stderr, "request_file_name: realloc failed (%s).\n", strerror(errno));
        return MALLOC_FAILED;
      }
      strcpy(c->src, buf);
      break;
    }
  }

  return NO_ERRORS;
}

int request_file_name_or_it_contents(struct candidate* c, const char* label, const char* default_file_name, const ssize_t limit)
{
  char *buf, *retstr;
  ssize_t length;
  struct stat st;

  buf = malloc(limit > PATH_MAX ? limit : PATH_MAX);

  if (!buf)
  {
    fprintf(stderr, "request_file_name_or_it_contents: malloc failed (%s).\n", strerror(errno));
    return MALLOC_FAILED;
  }

  int rc = NO_ERRORS;
  while(1)
  {
    printf("Prompt %s file name (Press Enter to prompt only %s itself): ", label, label);

    // Following cases will work
    //   "file_name"
    //   "./file_name"
    //   "/full/path/to/file_name"
    // And this will not work
    //   "~/path/to/file_name"
    retstr = ext_fgets(buf, PATH_MAX, stdin);
    if (retstr == NULL) // error or EOF
    {
      fprintf(stderr, "request_file_name_or_it_contents: ext_fgets returned NULL (%s).\n", strerror(errno));
      rc = UNKNOWN_ERROR;
      break;
    }

    length = strlen(buf);
    while (length > 0 && (buf[length-1] == '\n' || buf[length-1] == ' ' || buf[length-1] == '/'))
      buf[--length] = '\0';

    if (length == 0) // Enter pressed or something like this
    {
      length = strlen(default_file_name);
      c->src = realloc(c->src, length + 1);
      if (!c->src)
      {
        fprintf(stderr, "request_file_name_or_it_contents: realloc failed (%s).\n", strerror(errno));
        rc = MALLOC_FAILED;
        break;
      }
      strcpy(c->src, default_file_name);

      // create file
      mode_t mode = S_IRUSR | S_IWUSR| S_IRGRP | S_IWGRP | S_IROTH;
      int fd = open(c->src, O_WRONLY | O_CREAT, mode);
      if (fd == -1)
      {
        fprintf(stderr, "request_file_name_or_it_contents: create \"%s\" file error (%s).\n", default_file_name, strerror(errno));
        rc = OPEN_FILE_ERROR;
        break;
      }
      else
      {
        printf("Prompt %s contents (max %ld chars): ", label, limit - 1);
        retstr = ext_fgets(buf, limit, stdin);
        if (retstr == NULL) // error or EOF
        {
          fprintf(stderr, "request_file_name_or_it_contents: ext_fgets returned NULL (%s).\n", strerror(errno));
          close(fd);
          rc = UNKNOWN_ERROR;
          break;
        }
        length = strlen(buf);
        write(fd, buf, length);
        close(fd);
      }
      break;
    }
    else if (stat(buf, &st) != 0 || !S_ISREG(st.st_mode))
      printf("File doesn't exists! (Ctrl+D for exit)\n");
    else
    {
      c->src = realloc(c->src, length + 1);
      if (!c->src)
      {
        fprintf(stderr, "request_file_name_or_it_contents: realloc failed (%s).\n", strerror(errno));
        rc = MALLOC_FAILED;
        break;
      }
      strcpy(c->src, buf);
      break;
    }
  }

  free(buf);
  return rc;
}

char ask_yes_no(const char* question)
{
  char yes_no;
  do
  {
    printf("%s: ", question);
    set_keypress();
    yes_no = fgetc(stdin);
    reset_keypress();
    printf("\n");
  }
  while (yes_no != 'Y' && yes_no != 'y' && yes_no != 'N' && yes_no != 'n');
  if (yes_no == 'y')
    yes_no = 'Y';
  else if (yes_no == 'n')
    yes_no = 'N';

  return yes_no;
}

int make_signature(const struct candidate* c, char* sig)
{
  int fds[2];
  int rc = pipe(fds);
  if (rc == -1)
  {
    fprintf(stderr, "make_signature: pipe creation failed (%s)!\n", strerror(errno));
    return PIPE_FAILED;
  }

  pid_t pid = fork();

  if (pid == (pid_t)-1)
  {
    fprintf(stderr, "make_signature: fork failed (%s)!\n", strerror(errno));
    return FORK_FAILED;
  }

  if (pid == (pid_t)0) // child
  {
    close(fds[0]);

    rc = dup2(fds[1], STDOUT_FILENO);
    if (rc == -1)
    {
      fprintf(stderr, "make_signature: dup2 failed (%s)!\n", strerror(errno));
      return DUP2_FAILED;
    }

    close(fds[1]);

    rc = execlp("md5sum", "md5sum", c->src, NULL);
    if (rc == -1)
    {
      fprintf(stderr, "make_signature: execlp failed (%s)!\n", strerror(errno));
      return EXEC_FAILED;
    }
  }
  else // parent
  {
    close(fds[1]);
    char buf[STANDARD_BUFFER_SIZE];
    ssize_t bytes_read = read(fds[0], buf, STANDARD_BUFFER_SIZE);
    if (bytes_read == 0)
    {
      fprintf(stderr, "make_signature: md5sum failed (readed 0 bytes)!\n");
      return READ_BLOCK_ERROR;
    }
    else if (bytes_read == -1)
    {
      fprintf(stderr, "make_signature: md5sum (read failed) (%s)!\n", strerror(errno));
      return READ_BLOCK_ERROR;
    }
    else if (bytes_read > SIG_SIZE)
      memcpy(sig, buf, SIG_SIZE);
    else
    {
      fprintf(stderr, "make_signature: md5sum (read less bytes than needed)!\n");
      return READ_BLOCK_ERROR;
    }

    rc = waitpid(pid, NULL, 0);
    if (rc == -1)
    {
      fprintf(stderr, "make_signature: waitpid failed (%s)!\n", strerror(errno));
      return WAITPID_FAILED;
    }

    close(fds[0]);
  }

  return NO_ERRORS;
}

int check_directory(const char* path)
{
  struct stat st;
  int rc = stat(path, &st);
  if (rc == 0 && S_ISDIR(st.st_mode))
    return NO_ERRORS;
  else if (rc == -1 && errno == ENOENT)
    return NOT_FOUND;
  else if (rc == -1)
  {
    fprintf(stderr, "check_directory: stat failed on \"%s\" directory (%s)!\n", path, strerror(errno));
    return STAT_FAILED;
  }
  return NO_ERRORS;
}

int make_directory(const char* path)
{
  mode_t dir_mode = S_IRUSR | S_IWUSR | S_IXUSR | S_IRGRP | S_IWGRP | S_IXGRP;
  int rc = mkdir(path, dir_mode);
  if (rc == -1)
  {
    fprintf(stderr, "make_directory: failed to create \"%s\" directory (%s)!\n", path, strerror(errno));
    return OPEN_DIR_ERROR;
  }
  return NO_ERRORS;
}

int check_duplicates(const struct candidate* c, char* path)
{
  int rc = NO_ERRORS;
  DIR *cur_dir;
  struct dirent *entry;
  ssize_t length;
  char yes_no;

  if ((cur_dir = opendir(path)) != NULL)
  {
    length = strlen(path);
    while ((entry = readdir(cur_dir)) != NULL)
    {
      if (strlen(entry->d_name) == ID_SIZE && strspn(entry->d_name, ID_CHARS) == ID_SIZE)
      {
        strcpy(path + length, entry->d_name);
        path[length + ID_SIZE] = '/';

        strcpy(path + length + ID_SIZE + 1, "video.mp4");
        if (test_file(path) == NO_ERRORS)
        {
          if (filecmp(c->src, path) == 0)
          {
            printf("Same video file already published on %s\n", path);
            yes_no = ask_yes_no("Do you want to publish it anyway (Y/N)?");
            if (yes_no == 'N' || yes_no == 'n')
              rc = DUPLICATE_FOUND;
            break;
          }
        }

        strcpy(path + length + ID_SIZE + 1, "video.webm");
        if (test_file(path) == NO_ERRORS)
        {
          if (filecmp(c->src, path) == 0)
          {
            printf("Same video file already published on %s\n", path);
            yes_no = ask_yes_no("Do you want to publish it anyway (Y/N)?");
            if (yes_no == 'N')
              rc = DUPLICATE_FOUND;
            break;
          }
        }

      }
    }
    path[length] = '\0';
    closedir(cur_dir);
  }
  else
  {
    fprintf(stderr, "check_duplicates: Can't open %s directory.\n", path);
    rc = OPEN_DIR_ERROR;
  }

  return rc;
}

int test_file(const char* path)
{
  struct stat st;
  int rc = stat(path, &st);
  if (rc == 0 && S_ISREG(st.st_mode))
    return NO_ERRORS;
  else if (rc == -1 && errno == ENOENT)
    return NOT_FOUND;
  else if (rc == -1)
  {
    fprintf(stderr, "test_file: stat failed on \"%s\" directory (%s)!\n", path, strerror(errno));
    return STAT_FAILED;
  }
  else
    return NOT_FILE;
  return NO_ERRORS;
}

int filecmp(const char* path1, const char* path2)
{
  struct stat st1, st2;
  int rc1, rc2;

  rc1 = stat(path1, &st1);
  rc2 = stat(path2, &st2);

  if (rc1 == 0 && S_ISREG(st1.st_mode) && rc2 == 0 && S_ISREG(st2.st_mode))
  {
    int fd1, fd2;
    char buf1[STANDARD_BUFFER_SIZE], buf2[STANDARD_BUFFER_SIZE];
    ssize_t read_byte1, read_byte2;

    fd1 =  open(path1, O_RDONLY);
    if (fd1 == -1)
      return UNKNOWN_ERROR;
    fd2 =  open(path2, O_RDONLY);
    if (fd2 == -1)
    {
      close(fd1);
      return UNKNOWN_ERROR;
    }

    do
    {
      read_byte1 = read(fd1, buf1, STANDARD_BUFFER_SIZE);
      read_byte2 = read(fd2, buf2, STANDARD_BUFFER_SIZE);
      if (read_byte1 < 0 || read_byte2 < 0)
        break;
      if (read_byte1 > read_byte2)
      {
        close(fd1);
        close(fd2);
        return 1;
      }
      else if (read_byte1 < read_byte2)
      {
        close(fd1);
        close(fd2);
        return -1;
      }
      else if (read_byte1 == read_byte2 && read_byte1 != 0)
      {
        rc1 = memcmp(buf1, buf2, read_byte1);
        if (rc1 != 0)
        {
          close(fd1);
          close(fd2);
          return rc1;
        }
      }
    }
    while (read_byte1 != 0 || read_byte2 != 0);

    close(fd1);
    close(fd2);

    if (rc1 == 0)
      return 0;
  }

  return UNKNOWN_ERROR;
}

int make_id_directory(char* path)
{
  ssize_t length = strlen(path);
  int rc, attempts = 0;

  while (attempts < MAX_CREATE_ID_DIR_ATTEMPTS)
  {
    for (int i = 0; i < ID_SIZE; i++)
      path[length + i] = (char)papv->id_char_set[(int) ( (double)papv->id_char_set_len * rand() / RAND_MAX )];
    path[length + ID_SIZE] = '\0';

    rc = check_directory(path);
    if (rc == NOT_FOUND)
    {
      rc = make_directory(path);
      if (rc == NO_ERRORS)
        break;
    }
    else if (rc == NO_ERRORS) // Dir already exists
    {
      struct stat st;
      path[length + ID_SIZE] = '/';

      strcpy(path + length + ID_SIZE + 1, "video.mp4");
      rc = stat(path, &st);
      if (rc == 0 && S_ISREG(st.st_mode))
      {
        attempts++;
        continue;
      }

      strcpy(path + length + ID_SIZE + 1, "video.webm");
      rc = stat(path, &st);
      if (rc == 0 && S_ISREG(st.st_mode))
      {
        attempts++;
        continue;
      }
      break;
    }
    else
      return rc;
  }
  return NO_ERRORS;
}

int publish_candidate(const struct candidate* c, char* path)
{
  ssize_t length;
  length = strlen(path);
  strcpy(path + length, c->target);
  int rc = rename(c->src, path);
  if (rc == -1)
  {
    fprintf(stderr, "publish_candidate: failed to move %s -> %s (%s)!\n", c->src, path, strerror(errno));
    rc = RENAME_FILE_FAILED;
  }
  else
    rc = NO_ERRORS;

  path[length] = '\0';

  return rc;
}

int make_targets(struct candidate* video, struct candidate* trumb, struct candidate* title, struct candidate* descr)
{
  char* pos = strrchr(video->src, '.');
  if (pos && strcmp_ncase(pos, ".webm") == 0)
    video->target = strdup("video.webm");
  else
    video->target = strdup("video.mp4");

  pos = strrchr(trumb->src, '.');
  if (pos && strcmp_ncase(pos, ".webp") == 0)
    trumb->target = strdup("trumb.webp");
  else if (pos && (strcmp_ncase(pos, ".jpg") == 0 || strcmp_ncase(pos, ".jpeg") == 0))
    trumb->target = strdup("trumb.jpg");
  else
    trumb->target = strdup("trumb.png");

  title->target = strdup("title.txt");

  pos = strrchr(descr->src, '.');
  if (pos && (strcmp_ncase(pos, ".txt") == 0 || strcmp_ncase(pos, ".text") == 0))
    descr->target = strdup("descr.txt");
  else
    descr->target = strdup("descr.html");

  return NO_ERRORS;
}

char* ext_fgets(char *s, int size, FILE* stream)
{
  char *retstr = fgets(s, size, stream);
  if (retstr && (strlen(retstr) == size - 1) && retstr[size - 2] != '\n')
    while (getchar() != '\n');
  return retstr;
}
