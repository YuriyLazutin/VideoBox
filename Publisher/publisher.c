#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <sys/types.h>
#include <sys/wait.h>
//#include <signal.h>
#include <dirent.h> // DIR, struct dirent, PATH_MAX
#include <fcntl.h>
#include <sys/stat.h>
//#include <linux/limits.h> // PATH_MAX
#include <termios.h> // non-canonical mode for terminal, struct termios, tcgetattr, tcsetattr
#include <sys/time.h> // timeval, gettimeofday
#include "defines.h"

// paths
char* server_dir;
ssize_t server_dir_length;
char* showboard_dir;
ssize_t showboard_dir_length;
// charsets
char* id_char_set;
int   id_char_set_len;
// terminal settings
static struct termios stored_settings;

struct candidate
{
  char* src;
  char* target;
  char* note;
  char rec;
};

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
int request_file_name(struct candidate* c, char* label);

int main()
{
  int rc = init();
  if (rc != NO_ERRORS)
    return EXIT_FAILURE;

  struct candidate video, trumb, title, descr;

  rc = find_candidates(&video, &trumb, &title, &descr);
  if (rc != NO_ERRORS)
    return EXIT_FAILURE;

  // Analyze file size and notify user if it empty or large then possible
  check_video_candidate(&video);
  check_trumb_candidate(&trumb);
  check_title_candidate(&title);
  check_descr_candidate(&descr);

  // Suggest user candidates
  suggest_candidates(&video, &trumb, &title, &descr);

  if (!video.src || video.rec == 'Y')
  {
    rc = request_file_name(&video, "video");
    if (rc != NO_ERRORS)
      return EXIT_FAILURE;
  }
  if (!trumb.src || trumb.rec == 'Y')
  {
    rc = request_file_name(&trumb, "trumbnail");
    if (rc != NO_ERRORS)
      return EXIT_FAILURE;
  }


  char* retstr;
  char buf[PATH_MAX], sig[SIG_SIZE], yes_no;

  struct stat st;
  ssize_t length;



  if (title.src && title.rec == 'Y')
  {
    yes_no = 'X';
    while (yes_no != 'Y' && yes_no != 'y' && yes_no != 'N' && yes_no != 'n')
    {
      printf("Would you like to change title file (Y/N)?: ");
      set_keypress();
      yes_no = fgetc(stdin);
      reset_keypress();
      printf("\n");
    }
  }

  if ( !title.src || (title.src && title.rec == 'Y' && (yes_no == 'Y' || yes_no == 'y')) )
  {
    while (1)
    {
      printf("Prompt new title file name (Press Enter to prompt only title itself): ");

      // Following cases will work
      //   "file_name"
      //   "./file_name"
      //   "/full/path/to/file_name"
      // And this will not work
      //   "~/path/to/file_name"
      retstr = fgets(buf, sizeof(buf), stdin);
      if (retstr == NULL)
        return EXIT_FAILURE;
      length = strlen(buf);
      while (length > 0 && (buf[length-1] == '\n' || buf[length-1] == ' ' || buf[length-1] == '/'))
        buf[--length] = '\0';

      if (length == 0)
      {
        length = strlen("title.txt");
        title.src = realloc(title.src, length + 1);
        strcpy(title.src, "title.txt");

        // create file
        mode_t mode = S_IRUSR | S_IWUSR| S_IRGRP | S_IWGRP | S_IROTH;
        int fd = open(title.src, O_WRONLY | O_EXCL | O_CREAT, mode);
        if (fd == -1)
          perror("title.txt creation failed!");
        else
        {
          printf("Prompt title contents (max %d chars): ", SMALL_BUFFER_SIZE - 1);
          retstr = fgets(buf, SMALL_BUFFER_SIZE - 1, stdin);
          if (retstr == NULL)
            return EXIT_FAILURE;
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
        title.src = realloc(title.src, length + 1);
        strcpy(title.src, buf);
        break;
      }
    }
  }

  if (descr.src && descr.rec == 'Y')
  {
    yes_no = 'X';
    while (yes_no != 'Y' && yes_no != 'y' && yes_no != 'N' && yes_no != 'n')
    {
      printf("Would you like to change description file (Y/N)?: ");
      set_keypress();
      yes_no = fgetc(stdin);
      reset_keypress();
      printf("\n");
    }
  }

  if ( !descr.src || (descr.src && descr.rec == 'Y' && (yes_no == 'Y' || yes_no == 'y')) )
  {
    while (1)
    {
      printf("Prompt new description file name (Press Enter to prompt only description itself): ");

      // Following cases will work
      //   "file_name"
      //   "./file_name"
      //   "/full/path/to/file_name"
      // And this will not work
      //   "~/path/to/file_name"
      retstr = fgets(buf, sizeof(buf), stdin);
      if (retstr == NULL)
        return EXIT_FAILURE;
      length = strlen(buf);
      while (length > 0 && (buf[length-1] == '\n' || buf[length-1] == ' ' || buf[length-1] == '/'))
        buf[--length] = '\0';

      if (length == 0)
      {
        length = strlen("descr.html");
        descr.src = realloc(descr.src, length + 1);
        strcpy(descr.src, "descr.html");

        // create file
        mode_t mode = S_IRUSR | S_IWUSR| S_IRGRP | S_IWGRP | S_IROTH;
        int fd = open(descr.src, O_WRONLY | O_EXCL | O_CREAT, mode);
        if (fd == -1)
          perror("descr.html creation failed!");
        else
        {
          printf("Prompt description contents (max %d chars): ", STANDARD_BUFFER_SIZE - 1);
          retstr = fgets(buf, STANDARD_BUFFER_SIZE - 1, stdin);
          if (retstr == NULL)
            return EXIT_FAILURE;
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
        descr.src = realloc(descr.src, length + 1);
        strcpy(descr.src, buf);
        break;
      }
    }
  }

  // Calculate sig
  int fds[2];
  rc = pipe(fds);
  if (rc == -1)
  {
    fprintf(stderr, "Pipe creation failed (%s)!\n", strerror(errno));
    return EXIT_FAILURE;
  }

  pid_t pid = fork();

  if (pid == (pid_t)-1)
  {
    fprintf(stderr, "Fork failed (%s)!\n", strerror(errno));
    return EXIT_FAILURE;
  }
  else if (pid == (pid_t)0) // child
  {

    close(fds[0]);

    rc = dup2(fds[1], STDOUT_FILENO);
    if (rc == -1)
    {
      fprintf(stderr, "dup2 failed (%s)!\n", strerror(errno));
      return EXIT_FAILURE;
    }

    close(fds[1]);

    rc = execlp("md5sum", "md5sum", video.src, NULL);
    if (rc == -1)
    {
      fprintf(stderr, "execlp failed (%s)!\n", strerror(errno));
      return EXIT_FAILURE;
    }
  }
  else // parent
  {
    close(fds[1]);

    ssize_t bytes_read = read(fds[0], buf, STANDARD_BUFFER_SIZE);
    if (bytes_read == 0)
    {
      fprintf(stderr, "md5sum: readed 0 bytes!\n");
      return EXIT_FAILURE;
    }
    else if (bytes_read == -1)
    {
      fprintf(stderr, "md5sum: read failed (%s)!\n", strerror(errno));
      return EXIT_FAILURE;
    }
    else if (bytes_read > SIG_SIZE)
      memcpy(sig, buf, SIG_SIZE);

    rc = waitpid(pid, NULL, 0);
    if (rc == -1)
    {
      fprintf(stderr, "waitpid failed (%s)!\n", strerror(errno));
      return EXIT_FAILURE;
    }

    close(fds[0]);
  }

  //strcpy(buf, "28badc69dc9e82a51ee885122552ad1c");
  //memcpy(sig, buf, SIG_SIZE);

  mode_t dir_mode = S_IRUSR | S_IWUSR | S_IXUSR | S_IRGRP | S_IWGRP | S_IXGRP;
  rc = stat(showboard_dir, &st);
  if (rc == 0 && S_ISDIR(st.st_mode))
    NULL;
  else if (rc == -1 && errno == ENOENT)
  {
    rc = mkdir(showboard_dir, dir_mode);
    if (rc == -1)
    {
      fprintf(stderr, "Failed to create %s directory (%s)!\n", showboard_dir, strerror(errno));
      return EXIT_FAILURE;
    }
  }
  else if (rc == -1)
  {
    fprintf(stderr, "stat failed on %s directory (%s)!\n", showboard_dir, strerror(errno));
    return EXIT_FAILURE;
  }

  strcpy(buf, showboard_dir);
  memcpy(buf + showboard_dir_length, sig, SIG_SIZE);
  buf[showboard_dir_length + SIG_SIZE] = '\0';
  rc = stat(buf, &st);
  if (rc == 0 && S_ISDIR(st.st_mode))
    NULL;
  else if (rc == -1 && errno == ENOENT)
  {
    rc = mkdir(buf, dir_mode);
    if (rc == -1)
    {
      fprintf(stderr, "Failed to create %s directory (%s)!\n", buf, strerror(errno));
      return EXIT_FAILURE;
    }
  }
  else if (rc == -1)
  {
    fprintf(stderr, "stat failed on %s directory (%s)!\n", buf, strerror(errno));
    return EXIT_FAILURE;
  }

  // Calculate id
  buf[showboard_dir_length + SIG_SIZE] = '/';

  while (1)
  {
    for (int i = 0; i < ID_SIZE; i++)
      buf[showboard_dir_length + SIG_SIZE + 1 + i] = (char)id_char_set[(int) ( (double)id_char_set_len * rand() / RAND_MAX )];
    buf[showboard_dir_length + SIG_SIZE + 1 + ID_SIZE] = '\0';

    rc = stat(buf, &st);
    if (rc == 0 && S_ISDIR(st.st_mode))
      continue;
    else if (rc == -1 && errno == ENOENT)
    {
      rc = mkdir(buf, dir_mode);
      if (rc == -1)
      {
        fprintf(stderr, "Failed to create %s directory (%s)!\n", buf, strerror(errno));
        return EXIT_FAILURE;
      }
      break;
    }
    else if (rc == -1)
    {
      fprintf(stderr, "stat failed on %s directory (%s)!\n", buf, strerror(errno));
      return EXIT_FAILURE;
    }
  }

  // Move files into showboard
  buf[showboard_dir_length + SIG_SIZE + 1 + ID_SIZE] = '/';

  strcpy(buf + showboard_dir_length + SIG_SIZE + 1 + ID_SIZE + 1, video.src);
  rc = rename(video.src, buf);
  if (rc == -1)
  {
    fprintf(stderr, "Failed to move %s -> %s (%s)!\n", video.src, buf, strerror(errno));
    return EXIT_FAILURE;
  }

  strcpy(buf + showboard_dir_length + SIG_SIZE + 1 + ID_SIZE + 1, trumb.src);
  rc = rename(trumb.src, buf);
  if (rc == -1)
  {
    fprintf(stderr, "Failed to move %s -> %s (%s)!\n", trumb.src, buf, strerror(errno));
    return EXIT_FAILURE;
  }

  strcpy(buf + showboard_dir_length + SIG_SIZE + 1 + ID_SIZE + 1, title.src);
  rc = rename(title.src, buf);
  if (rc == -1)
  {
    fprintf(stderr, "Failed to move %s -> %s (%s)!\n", title.src, buf, strerror(errno));
    return EXIT_FAILURE;
  }

  strcpy(buf + showboard_dir_length + SIG_SIZE + 1 + ID_SIZE + 1, descr.src);
  rc = rename(descr.src, buf);
  if (rc == -1)
  {
    fprintf(stderr, "Failed to move %s -> %s (%s)!\n", descr.src, buf, strerror(errno));
    return EXIT_FAILURE;
  }

  // Local destroy
  if (descr.note)
    free(descr.note);
  if (title.note)
    free(title.note);
  if (trumb.note)
    free(trumb.note);
  if (video.note)
    free(video.note);
  if (descr.target)
    free(descr.target);
  if (title.target)
    free(title.target);
  if (trumb.target)
    free(trumb.target);
  if (video.target)
    free(video.target);
  if (descr.src)
    free(descr.src);
  if (title.src)
    free(title.src);
  if (trumb.src)
    free(trumb.src);
  if (video.src)
    free(video.src);

  // Global destroy
  rc = destroy();
  if (rc != NO_ERRORS)
    return EXIT_FAILURE;

  printf("Your files sucessfully published.\n");
  return EXIT_SUCCESS;
}

int init()
{
  // paths
  server_dir = malloc(PATH_MAX);
  if (!server_dir)
    return MALLOC_FAILED;
  server_dir_length = readlink("/proc/self/exe", server_dir, PATH_MAX - 1);
  if (server_dir_length == -1)
    return READ_LINK_ERROR;
  while (server_dir_length > 0 && server_dir[server_dir_length - 1] != '/') server_dir_length--;
  if (!server_dir_length)
    return PATH_INVALID;
  server_dir[server_dir_length] = '\0';
  server_dir = realloc(server_dir, server_dir_length + 1);
  if (!server_dir)
    return MALLOC_FAILED;

  showboard_dir_length = server_dir_length + strlen("showboard/");
  if (showboard_dir_length + 1 > PATH_MAX)
    return PATH_OVERFLOW;
  showboard_dir = malloc(showboard_dir_length + 1);
  if (!showboard_dir)
    return MALLOC_FAILED;
  strcpy(showboard_dir, server_dir);
  strcpy(showboard_dir + server_dir_length, "showboard/");

  // store terminal settings
  tcgetattr(0, &stored_settings);

  // charsets
  id_char_set = strdup(ID_CHARS);
  id_char_set_len = strlen(id_char_set);

  // random generator
  struct timeval tv;
  gettimeofday(&tv, NULL);
  srand((unsigned int)tv.tv_usec);

  return NO_ERRORS;
}

int destroy()
{
  // charsets
  if (id_char_set)
    free(id_char_set);

  // paths
  if (server_dir)
    free(server_dir);
  if (showboard_dir)
    free(showboard_dir);
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

  return -5;
}

void set_keypress()
{
  struct termios new_settings = stored_settings;

  // Disable canonical mode, and set buffer size to 1 byte
  new_settings.c_lflag &= (~ICANON);
  new_settings.c_cc[VTIME] = 0;
  new_settings.c_cc[VMIN] = 1;

  tcsetattr(0, TCSANOW, &new_settings);
}

void reset_keypress()
{
  tcsetattr(0, TCSANOW, &stored_settings);
}

/* Scan current folder and try to find:
    a) Video candidate (in high priority order)
        1) video.mp4
        2) video.webm
        3) *.mp4, *.webm
    b) Trumbnail candidate (in high priority order)
        1) trumb.png
        2) trumb.jpg, trumb.jpeg
        3) trumb.webp
        4) *.png
        5) *.jpg, *.jpeg
        6) *.webp
    c) Title candidate (in high priority order)
        1) title.txt, title.text
        2) title.htm, title.html
        3) *.txt, *.text
    d) Description candidate (in high priority order)
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
                         (  strcmp_ncase(entry->d_name, "video.mp4") == 0 ||
                            strcmp_ncase(entry->d_name, "video.webm") == 0 ||
                            (length >= 4 && strcmp_ncase(entry->d_name + length - 4, ".mp4") == 0) ||
                            (length >= 5 && strcmp_ncase(entry->d_name + length - 5, ".webm") == 0)
                         )
         )
      {
        video->src = strdup(entry->d_name);
        if (strcmp_ncase(entry->d_name + length - 4, ".mp4") == 0)
          video->target = strdup("video.mp4");
        else
          video->target = strdup("video.webm");
        continue;
      }
      else if ( video->src && strcmp_ncase(entry->d_name, "video.webm") == 0 && !(strcmp(video->target, "video.mp4") == 0) )
      {
        free(video->src);
        free(video->target);
        video->src = strdup(entry->d_name);
        video->target = strdup("video.webm");
        continue;
      }
      else if ( video->src && strcmp_ncase(entry->d_name, "video.mp4") == 0 )
      {
        free(video->src);
        free(video->target);
        video->src = strdup(entry->d_name);
        video->target = strdup("video.mp4");
        continue;
      }

      // Trumbs
      if (!trumb->src &&
                         (  strcmp_ncase(entry->d_name, "trumb.png") == 0 ||
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
        if (strcmp_ncase(entry->d_name + length - 4, ".png") == 0)
          trumb->target = strdup("trumb.png");
        else if (strcmp_ncase(entry->d_name + length - 4, ".jpg") == 0 || (length >= 5 && strcmp_ncase(entry->d_name + length - 5, ".jpeg") == 0))
          trumb->target = strdup("trumb.jpg");
        else
          trumb->target = strdup("trumb.webp");
        continue;
      }
      else if ( trumb->src && strcmp_ncase(entry->d_name, "trumb.jpeg") == 0 && strcmp(trumb->target, "trumb.webp") == 0 )
      {
        free(trumb->src);
        free(trumb->target);
        trumb->src = strdup(entry->d_name);
        trumb->target = strdup("trumb.jpg");
        continue;
      }
      else if ( trumb->src && strcmp_ncase(entry->d_name, "trumb.jpg") == 0 && strcmp(trumb->target, "trumb.webp") == 0 )
      {
        free(trumb->src);
        free(trumb->target);
        trumb->src = strdup(entry->d_name);
        trumb->target = strdup("trumb.jpg");
        continue;
      }
      else if ( trumb->src && strcmp_ncase(entry->d_name, "trumb.webp") == 0 && !(strcmp(trumb->target, "trumb.png") == 0 || strcmp(trumb->target, "trumb.jpg") == 0) )
      {
        free(trumb->src);
        free(trumb->target);
        trumb->src = strdup(entry->d_name);
        trumb->target = strdup("trumb.webp");
        continue;
      }
      else if ( trumb->src && strcmp_ncase(entry->d_name, "trumb.png") == 0 )
      {
        free(trumb->src);
        free(trumb->target);
        trumb->src = strdup(entry->d_name);
        trumb->target = strdup("trumb.png");
        continue;
      }

      // Titles
      if (!title->src &&
                         (  strcmp_ncase(entry->d_name, "title.txt") == 0 ||
                            strcmp_ncase(entry->d_name, "title.text") == 0 ||
                            strcmp_ncase(entry->d_name, "title.html") == 0 ||
                            strcmp_ncase(entry->d_name, "title.htm") == 0 ||
                            (length >= 4 && strcmp_ncase(entry->d_name + length - 4, ".txt") == 0) ||
                            (length >= 5 && strcmp_ncase(entry->d_name + length - 5, ".text") == 0)
                         )
         )
      {
        title->src = strdup(entry->d_name);
        title->target = strdup("title.txt");
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
                          (  strcmp_ncase(entry->d_name, "descr.htm") == 0 ||
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
        if (strcmp_ncase(entry->d_name + length - 4, ".htm") == 0 || (length >= 5 && strcmp_ncase(entry->d_name + length - 5, ".html") == 0))
          descr->target = strdup("descr.html");
        else
          descr->target = strdup("descr.txt");
        continue;
      }
      else if ( descr->src &&
                (strcmp_ncase(entry->d_name, "descr.text") == 0 || strcmp_ncase(entry->d_name, "descr.txt") == 0) &&
                !(strcmp(descr->target, "descr.html") == 0)
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
        free(descr->target);
        descr->src = strdup(entry->d_name);
        descr->target = strdup("descr.html");
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
    char yes_no = 'X';
    video->rec = trumb->rec = title->rec = descr->rec = 'N';
    while (yes_no != 'Y' && yes_no != 'y' && yes_no != 'N' && yes_no != 'n')
    {
      printf("Is this information correct (Y/N)?: ");
      set_keypress();
      yes_no = fgetc(stdin);
      reset_keypress();
      printf("\n");
    }
    if (yes_no != 'Y' && yes_no != 'y')
      video->rec = trumb->rec = title->rec = descr->rec = 'Y';
  }
  return NO_ERRORS;
}

int check_video_candidate(struct candidate* c)
{
  struct stat st;
  if (c->src)
  {
    if ( stat(c->src, &st) == -1)
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
    if ( stat(c->src, &st) == -1)
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
    if ( stat(c->src, &st) == -1)
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
    if ( stat(c->src, &st) == -1)
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

int request_file_name(struct candidate* c, char* label)
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
    retstr = fgets(buf, sizeof(buf), stdin);
    if (retstr == NULL) // error or EOF
    {
      fprintf(stderr, "request_file_name: fgets returned NULL (%s).\n", strerror(errno));
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
      strcpy(c->src, buf);
      break;
    }
  }

  return NO_ERRORS;
}
