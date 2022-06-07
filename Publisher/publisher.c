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
#include <sys/time.h>
#include "defines.h"

char* server_dir;
ssize_t server_dir_length;
char* showboard_dir;
ssize_t showboard_dir_length;
int strcmp_ncase(const char* str1, const char* str2);

static struct termios stored_settings;
void set_keypress();
void reset_keypress();
void random_init();
char random_char();

int main()
{
  server_dir = malloc(PATH_MAX);
  if (!server_dir)
    return EXIT_FAILURE;
  server_dir_length = readlink("/proc/self/exe", server_dir, PATH_MAX - 1);
  if (server_dir_length == -1)
    return EXIT_FAILURE;
  while (server_dir_length > 0 && server_dir[server_dir_length - 1] != '/') server_dir_length--;
  if (!server_dir_length)
    return EXIT_FAILURE;
  server_dir[server_dir_length] = '\0';
  server_dir = realloc(server_dir, server_dir_length + 1);
  if (!server_dir)
    return EXIT_FAILURE;

  showboard_dir_length = server_dir_length + strlen("showboard/");
  if (showboard_dir_length + 1 > PATH_MAX)
    return EXIT_FAILURE;
  showboard_dir = malloc(showboard_dir_length + 1);
  if (!showboard_dir)
    return EXIT_FAILURE;
  strcpy(showboard_dir, server_dir);
  strcpy(showboard_dir + server_dir_length, "showboard/");

  // store terminal settings
  tcgetattr(0, &stored_settings);


  // Scan current folder and try to find
  // 1) video.mp4, "video.webm"
  // 2) *.mp4, *.webm
  // 3) trumb.png, trumb.jpg, trumb.jpeg, trumb.webp
  // 4) *.png, *.jpg, *.webp
  // 5) title.html, title.txt, title.text
  // 6) descr.html, descr.txt
  // 7) *.txt (title)
  // 8) *.html (description)
  char* video_candidate = NULL;
  char* trumb_candidate = NULL;
  char* title_candidate = NULL;
  char* descr_candidate = NULL;
  char* video_candidate_note = NULL;
  char* trumb_candidate_note = NULL;
  char* title_candidate_note = NULL;
  char* descr_candidate_note = NULL;
  char* retstr;
  char buf[PATH_MAX], sig[SIG_SIZE], id[ID_SIZE], yes_no, rec_video, rec_trumb, rec_title, rec_descr;

  struct stat st;
  DIR *cur_dir;
  struct dirent *entry;
  ssize_t length;
  int rc;

  if ((cur_dir = opendir(".")) != NULL)
  {
    while ((entry = readdir(cur_dir)) != NULL)
    {
      if (strcmp(entry->d_name, ".") == 0)
        continue;
      if (strcmp(entry->d_name, "..") == 0)
        continue;
      if ( stat(entry->d_name, &st) != 0)
        continue;
      if (!S_ISREG(st.st_mode))
        continue;

      length = strlen(entry->d_name);

      if ( !video_candidate && (strcmp_ncase(entry->d_name, "video.mp4") == 0 || strcmp_ncase(entry->d_name, "video.webm") == 0) )
      {
        video_candidate = strdup(entry->d_name);
        continue;
      }
      else if ( !video_candidate && (strcmp_ncase(entry->d_name + length - 4, ".mp4") == 0 || strcmp_ncase(entry->d_name + length - 5, ".webm") == 0) )
      {
        video_candidate = strdup(entry->d_name);
        continue;
      }
      else if ( video_candidate && strcmp_ncase(entry->d_name, "video.webm") == 0 )
      {
        free(video_candidate);
        video_candidate = strdup(entry->d_name);
        continue;
      }
      else if ( video_candidate && strcmp_ncase(entry->d_name, "video.mp4") == 0 )
      {
        free(video_candidate);
        video_candidate = strdup(entry->d_name);
        continue;
      }

      // Trumbs
      if ( !trumb_candidate && (strcmp_ncase(entry->d_name, "trumb.png") == 0 || strcmp_ncase(entry->d_name, "trumb.jpg") == 0 || strcmp_ncase(entry->d_name, "trumb.jpeg") == 0 || strcmp_ncase(entry->d_name, "trumb.webp") == 0) )
      {
        trumb_candidate = strdup(entry->d_name);
        continue;
      }
      else if ( !trumb_candidate && (strcmp_ncase(entry->d_name + length - 4, ".png") == 0 || strcmp_ncase(entry->d_name + length - 4, ".jpg") == 0 || strcmp_ncase(entry->d_name + length - 5, ".jpeg") == 0 || strcmp_ncase(entry->d_name + length - 5, ".webp") == 0) )
      {
        trumb_candidate = strdup(entry->d_name);
        continue;
      }
      else if ( trumb_candidate && strcmp_ncase(entry->d_name, "trumb.jpeg") == 0 )
      {
        free(trumb_candidate);
        trumb_candidate = strdup(entry->d_name);
        continue;
      }
      else if ( trumb_candidate && strcmp_ncase(entry->d_name, "trumb.jpg") == 0 )
      {
        free(trumb_candidate);
        trumb_candidate = strdup(entry->d_name);
        continue;
      }
      else if ( trumb_candidate && strcmp_ncase(entry->d_name, "trumb.webp") == 0 )
      {
        free(trumb_candidate);
        trumb_candidate = strdup(entry->d_name);
        continue;
      }
      else if ( trumb_candidate && strcmp_ncase(entry->d_name, "trumb.png") == 0 )
      {
        free(trumb_candidate);
        trumb_candidate = strdup(entry->d_name);
        continue;
      }

      // Titles
      if ( !title_candidate && (strcmp_ncase(entry->d_name, "title.html") == 0 || strcmp_ncase(entry->d_name, "title.txt") == 0 || strcmp_ncase(entry->d_name, "title.text") == 0) )
      {
        title_candidate = strdup(entry->d_name);
        continue;
      }
      else if ( !title_candidate && strcmp_ncase(entry->d_name + length - 4, ".txt") == 0 )
        video_candidate = strdup(entry->d_name);
      else if ( !title_candidate && strcmp_ncase(entry->d_name + length - 5, ".text") == 0 )
        title_candidate = strdup(entry->d_name);
      else if ( !title_candidate && strcmp_ncase(entry->d_name + length - 5, ".html") == 0 )
        title_candidate = strdup(entry->d_name);
      else if ( title_candidate && strcmp_ncase(entry->d_name, "title.html") == 0 )
      {
        free(title_candidate);
        title_candidate = strdup(entry->d_name);
        continue;
      }
      else if ( title_candidate && strcmp_ncase(entry->d_name, "title.text") == 0 )
      {
        free(title_candidate);
        title_candidate = strdup(entry->d_name);
        continue;
      }
      else if ( title_candidate && strcmp_ncase(entry->d_name, "title.txt") == 0 )
      {
        free(title_candidate);
        title_candidate = strdup(entry->d_name);
        continue;
      }

      // Descriptions
      if ( !descr_candidate && (strcmp_ncase(entry->d_name, "descr.html") == 0 || strcmp_ncase(entry->d_name, "descr.txt") == 0 || strcmp_ncase(entry->d_name, "descr.text") == 0) )
      {
        descr_candidate = strdup(entry->d_name);
        continue;
      }
      else if ( !descr_candidate && strcmp_ncase(entry->d_name + length - 5, ".html") == 0 )
        descr_candidate = strdup(entry->d_name);
      else if ( !descr_candidate && strcmp_ncase(entry->d_name + length - 4, ".txt") == 0 )
        descr_candidate = strdup(entry->d_name);
      else if ( !descr_candidate && strcmp_ncase(entry->d_name + length - 5, ".text") == 0 )
        descr_candidate = strdup(entry->d_name);
      else if ( descr_candidate && strcmp_ncase(entry->d_name, "descr.text") == 0 )
      {
        free(descr_candidate);
        descr_candidate = strdup(entry->d_name);
        continue;
      }
      else if ( descr_candidate && strcmp_ncase(entry->d_name, "descr.txt") == 0 )
      {
        free(descr_candidate);
        descr_candidate = strdup(entry->d_name);
        continue;
      }
      else if ( descr_candidate && strcmp_ncase(entry->d_name, "descr.html") == 0 )
      {
        free(descr_candidate);
        descr_candidate = strdup(entry->d_name);
        continue;
      }
    }
    closedir(cur_dir);
  }
  else
    fprintf(stderr, "Can't open current directory.\n");

  // Analyze file size and notify user if it empty or large then possible
  if (video_candidate)
  {
    if ( stat(video_candidate, &st) == -1)
    {
      fprintf(stderr, "video_candidate (%s) stat failed (%s).\n", video_candidate, strerror(errno));
      video_candidate_note = strdup("stat failed!");
    }
    else if (st.st_size == 0)
      video_candidate_note = strdup("Looks like file empty!");
  }

  if (trumb_candidate)
  {
    if ( stat(trumb_candidate, &st) == -1)
    {
      fprintf(stderr, "trumb_candidate (%s) stat failed (%s).\n", trumb_candidate, strerror(errno));
      trumb_candidate_note = strdup("stat failed!");
    }
    else if (st.st_size == 0)
      trumb_candidate_note = strdup("Looks like file empty!");
    else if (st.st_size >= 5 * 1024 * 1024)
      trumb_candidate_note = strdup("Looks like file is too large!");
  }

  if (title_candidate)
  {
    if ( stat(title_candidate, &st) == -1)
    {
      fprintf(stderr, "title_candidate (%s) stat failed (%s).\n", title_candidate, strerror(errno));
      title_candidate_note = strdup("stat failed!");
    }
    else if (st.st_size == 0)
      title_candidate_note = strdup("Looks like title file empty!");
    else if (st.st_size >= SMALL_BUFFER_SIZE)
      title_candidate_note = strdup("Looks like title is too large!");
  }

  if (descr_candidate)
  {
    if ( stat(descr_candidate, &st) == -1)
    {
      fprintf(stderr, "descr_candidate (%s) stat failed (%s).\n", descr_candidate, strerror(errno));
      descr_candidate_note = strdup("stat failed!");
    }
    else if (st.st_size == 0)
      descr_candidate_note = strdup("Looks like description file empty!");
    else if (st.st_size >= STANDARD_BUFFER_SIZE)
      descr_candidate_note = strdup("Looks like description is too large!");
  }

  // Suggest user candidates
  if (video_candidate || trumb_candidate || title_candidate || descr_candidate)
  {
    printf("Automatically detected\n");
    if (video_candidate)
    {
      printf("Video file:         %s", video_candidate);
      if (video_candidate_note)
        printf(" (%s)", video_candidate_note);
      printf("\n");
    }

    if (trumb_candidate)
    {
      printf("Trumbnail image:    %s", trumb_candidate);
      if (trumb_candidate_note)
        printf(" (%s)", trumb_candidate_note);
      printf("\n");
    }

    if (title_candidate)
    {
      printf("Title file:         %s", title_candidate);
      if (title_candidate_note)
        printf(" (%s)", title_candidate_note);
      printf("\n");
    }

    if (descr_candidate)
    {
      printf("Description file:   %s", descr_candidate);
      if (descr_candidate_note)
        printf(" (%s)", descr_candidate_note);
      printf("\n");
    }

    printf("\n");

    // Accept user choice
    yes_no = 'X';
    rec_video = rec_trumb = rec_title = rec_descr = 'N';
    while (yes_no != 'Y' && yes_no != 'y' && yes_no != 'N' && yes_no != 'n')
    {
      printf("Is this information correct (Y/N)?: ");
      set_keypress();
      yes_no = fgetc(stdin);
      reset_keypress();
      printf("\n");
    }
    if (yes_no != 'Y' && yes_no != 'y')
      rec_video = rec_trumb = rec_title = rec_descr = 'Y';
  }

  if (!video_candidate || rec_video == 'Y')
  {
    while(1)
    {
      if (video_candidate)
        printf("Video file name (Press Enter to use detected %s): ", video_candidate);
      else
        printf("Video file name: ");

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

      if (video_candidate && length == 0)
        break;

      if (stat(buf, &st) != 0 || !S_ISREG(st.st_mode))
        printf("File doesn't exists! (Ctrl+D for exit)\n");
      else
      {
        video_candidate = realloc(video_candidate, length + 1);
        strcpy(video_candidate, buf);
        break;
      }
    }
  }

  if (!trumb_candidate || rec_trumb == 'Y')
  {
    while(1)
    {
      if (trumb_candidate)
        printf("Trumbnail file name (Press Enter to use detected %s): ", trumb_candidate);
      else
        printf("Trumbnail file name: ");

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

      if (trumb_candidate && length == 0)
        break;

      if (stat(buf, &st) != 0 || !S_ISREG(st.st_mode))
        printf("File doesn't exists! (Ctrl+D for exit)\n");
      else
      {
        trumb_candidate = realloc(trumb_candidate, length + 1);
        strcpy(trumb_candidate, buf);
        break;
      }
    }
  }

  if (title_candidate && rec_title == 'Y')
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

  if ( !title_candidate || (title_candidate && rec_title == 'Y' && (yes_no == 'Y' || yes_no == 'y')) )
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
        title_candidate = realloc(title_candidate, length + 1);
        strcpy(title_candidate, "title.txt");

        // create file
        mode_t mode = S_IRUSR | S_IWUSR| S_IRGRP | S_IWGRP | S_IROTH;
        int fd = open(title_candidate, O_WRONLY | O_EXCL | O_CREAT, mode);
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
        title_candidate = realloc(title_candidate, length + 1);
        strcpy(title_candidate, buf);
        break;
      }
    }
  }

  if (descr_candidate && rec_descr == 'Y')
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

  if ( !descr_candidate || (descr_candidate && rec_descr == 'Y' && (yes_no == 'Y' || yes_no == 'y')) )
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
        descr_candidate = realloc(descr_candidate, length + 1);
        strcpy(descr_candidate, "descr.html");

        // create file
        mode_t mode = S_IRUSR | S_IWUSR| S_IRGRP | S_IWGRP | S_IROTH;
        int fd = open(descr_candidate, O_WRONLY | O_EXCL | O_CREAT, mode);
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
        descr_candidate = realloc(descr_candidate, length + 1);
        strcpy(descr_candidate, buf);
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

    rc = execlp("md5sum", "md5sum", video_candidate, NULL);
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
  random_init();
  buf[showboard_dir_length + SIG_SIZE] = '/';

  while (1)
  {
    for (int i = 0; i < ID_SIZE; i++)
      id[i] = random_char();
    memcpy(buf + showboard_dir_length + SIG_SIZE + 1, id, ID_SIZE);
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

  strcpy(buf + showboard_dir_length + SIG_SIZE + 1 + ID_SIZE + 1, video_candidate);
  rc = rename(video_candidate, buf);
  if (rc == -1)
  {
    fprintf(stderr, "Failed to move %s -> %s (%s)!\n", video_candidate, buf, strerror(errno));
    return EXIT_FAILURE;
  }

  strcpy(buf + showboard_dir_length + SIG_SIZE + 1 + ID_SIZE + 1, trumb_candidate);
  rc = rename(trumb_candidate, buf);
  if (rc == -1)
  {
    fprintf(stderr, "Failed to move %s -> %s (%s)!\n", trumb_candidate, buf, strerror(errno));
    return EXIT_FAILURE;
  }

  strcpy(buf + showboard_dir_length + SIG_SIZE + 1 + ID_SIZE + 1, title_candidate);
  rc = rename(title_candidate, buf);
  if (rc == -1)
  {
    fprintf(stderr, "Failed to move %s -> %s (%s)!\n", title_candidate, buf, strerror(errno));
    return EXIT_FAILURE;
  }

  strcpy(buf + showboard_dir_length + SIG_SIZE + 1 + ID_SIZE + 1, descr_candidate);
  rc = rename(descr_candidate, buf);
  if (rc == -1)
  {
    fprintf(stderr, "Failed to move %s -> %s (%s)!\n", descr_candidate, buf, strerror(errno));
    return EXIT_FAILURE;
  }

  if (server_dir)
    free(server_dir);
  if (showboard_dir)
    free(showboard_dir);

  printf("Your files sucessfully published.\n");
  return EXIT_SUCCESS;
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

void random_init()
{
  struct timeval tv;
  gettimeofday(&tv, NULL);
  srand((unsigned int)tv.tv_usec);
}

char random_char()
{
  char* mas = "0123456789abcdefghijklmnopqrstuvwxy", c;
  int cur, min = 0, max = strlen(mas);
  if (max < RAND_MAX / 2)
    cur = min + (int) ( (double)max * rand() / (RAND_MAX + (double)min));
  else
    cur = min + (rand() % (max - min + 1));
  c = mas[cur];
  return c;
}
