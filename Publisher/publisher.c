#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
//#include <errno.h>
#include <string.h>
#include <sys/types.h>
#include <sys/wait.h>
//#include <signal.h>
#include <dirent.h> // DIR, struct dirent, PATH_MAX
#include <fcntl.h>
#include <sys/stat.h>
//#include <linux/limits.h> // PATH_MAX
#include "defines.h"

char* server_dir;
ssize_t server_dir_length;
char* showboard_dir;
ssize_t showboard_dir_length;
char* get_self_executable_directory(ssize_t* length);
char* get_showboard_directory(ssize_t* length, char* bse, ssize_t bse_lenght);

int main()
{
  server_dir = get_self_executable_directory(&server_dir_length);
  if (server_dir_length <= 0)
  {
    fprintf(stderr, "Error in function \"get_self_executable_directory\": server_dir_length <= 0\n");
    return EXIT_FAILURE;
  }

  showboard_dir = get_showboard_directory(&showboard_dir_length, server_dir, server_dir_length);
  if (showboard_dir_length <= 0)
  {
    fprintf(stderr, "Error in function \"get_showboard_directory\": showboard_dir_length <= 0\n");
    if (server_dir)
      free(server_dir);
    return EXIT_FAILURE;
  }

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
  struct stat st;

  DIR *cur_dir;
  struct dirent *entry;
  ssize_t entry_length;

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

      entry_length = strlen(entry->d_name);

      if ( !video_candidate && (strcasecmp(entry->d_name, "video.mp4") == 0 || strcasecmp(entry->d_name, "video.webm") == 0) )
      {
        video_candidate = strdup(entry->d_name);
        continue;
      }
      else if ( !video_candidate && (strcasecmp(entry->d_name + entry_length - 4, ".mp4") == 0 || strcasecmp(entry->d_name + entry_length - 5, ".webm") == 0) )
      {
        video_candidate = strdup(entry->d_name);
        continue;
      }
      else if ( video_candidate && strcasecmp(entry->d_name, "video.mp4") == 0 )
      {
        video_candidate = strdup(entry->d_name);
        continue;
      }
      else if ( video_candidate && strcasecmp(entry->d_name, "video.webm") == 0 )
      {
        video_candidate = strdup(entry->d_name);
        continue;
      }

      // Trumbs
      if ( !trumb_candidate && (strcasecmp(entry->d_name, "trumb.png") == 0 || strcasecmp(entry->d_name, "trumb.jpg") == 0 || strcasecmp(entry->d_name, "trumb.jpeg") == 0 || strcasecmp(entry->d_name, "trumb.webp") == 0) )
      {
        trumb_candidate = strdup(entry->d_name);
        continue;
      }
      else if ( !trumb_candidate && (strcasecmp(entry->d_name + entry_length - 4, ".png") == 0 || strcasecmp(entry->d_name + entry_length - 4, ".jpg") == 0 || strcasecmp(entry->d_name + entry_length - 5, ".jpeg") == 0 || strcasecmp(entry->d_name + entry_length - 5, ".webp") == 0) )
      {
        trumb_candidate = strdup(entry->d_name);
        continue;
      }
      else if ( trumb_candidate && strcasecmp(entry->d_name, "trumb.png") == 0 )
      {
        trumb_candidate = strdup(entry->d_name);
        continue;
      }
      else if ( trumb_candidate && strcasecmp(entry->d_name, "trumb.jpg") == 0 )
      {
        trumb_candidate = strdup(entry->d_name);
        continue;
      }
      else if ( trumb_candidate && strcasecmp(entry->d_name, "trumb.jpeg") == 0 )
      {
        trumb_candidate = strdup(entry->d_name);
        continue;
      }
      else if ( trumb_candidate && strcasecmp(entry->d_name, "trumb.webp") == 0 )
      {
        trumb_candidate = strdup(entry->d_name);
        continue;
      }

      // Titles
      if ( !title_candidate && (strcasecmp(entry->d_name, "title.html") == 0 || strcasecmp(entry->d_name, "title.txt") == 0 || strcasecmp(entry->d_name, "title.text") == 0) )
      {
        title_candidate = strdup(entry->d_name);
        continue;
      }
      else if ( !title_candidate && strcasecmp(entry->d_name + entry_length - 4, ".txt") == 0 )
      {
        video_candidate = strdup(entry->d_name);
        continue;
      }
      else if ( !title_candidate && strcasecmp(entry->d_name + entry_length - 5, ".text") == 0 )
      {
        title_candidate = strdup(entry->d_name);
        continue;
      }
      else if ( !title_candidate && strcasecmp(entry->d_name + entry_length - 5, ".html") == 0 )
      {
        title_candidate = strdup(entry->d_name);
        continue;
      }
      else if ( title_candidate && strcasecmp(entry->d_name, "title.txt") == 0 )
      {
        title_candidate = strdup(entry->d_name);
        continue;
      }
      else if ( title_candidate && strcasecmp(entry->d_name, "title.text") == 0 )
      {
        title_candidate = strdup(entry->d_name);
        continue;
      }
      else if ( title_candidate && strcasecmp(entry->d_name, "title.html") == 0 )
      {
        title_candidate = strdup(entry->d_name);
        continue;
      }

      // Descriptions
      if ( !descr_candidate && (strcasecmp(entry->d_name, "descr.html") == 0 || strcasecmp(entry->d_name, "descr.txt") == 0 || strcasecmp(entry->d_name, "descr.text") == 0) )
      {
        descr_candidate = strdup(entry->d_name);
        continue;
      }
      else if ( !descr_candidate && strcasecmp(entry->d_name + entry_length - 5, ".html") == 0 )
      {
        descr_candidate = strdup(entry->d_name);
        continue;
      }
      else if ( !descr_candidate && strcasecmp(entry->d_name + entry_length - 4, ".txt") == 0 )
      {
        descr_candidate = strdup(entry->d_name);
        continue;
      }
      else if ( !descr_candidate && strcasecmp(entry->d_name + entry_length - 5, ".text") == 0 )
      {
        descr_candidate = strdup(entry->d_name);
        continue;
      }
      else if ( descr_candidate && strcasecmp(entry->d_name, "descr.html") == 0 )
      {
        descr_candidate = strdup(entry->d_name);
        continue;
      }
      else if ( descr_candidate && strcasecmp(entry->d_name, "descr.txt") == 0 )
      {
        descr_candidate = strdup(entry->d_name);
        continue;
      }
      else if ( descr_candidate && strcasecmp(entry->d_name, "descr.text") == 0 )
      {
        descr_candidate = strdup(entry->d_name);
        continue;
      }
    }
    closedir(cur_dir);
  }

  char* video_candidate_note = NULL;
  char* trumb_candidate_note = NULL;
  char* title_candidate_note = NULL;
  char* descr_candidate_note = NULL;

  // Analyze file size and notify user if it empty or large then possible
  if (video_candidate)
  {
    stat(video_candidate, &st);
    if (st.st_size == 0)
      video_candidate_note = strdup("Looks like file empty!");
  }

  if (trumb_candidate)
  {
    stat(trumb_candidate, &st);
    if (st.st_size == 0)
      trumb_candidate_note = strdup("Looks like file empty!");
    else if (st.st_size >= 5 * 1024 * 1024)
      trumb_candidate_note = strdup("Looks like file is too large!");
  }

  if (title_candidate)
  {
    stat(title_candidate, &st);
    if (st.st_size == 0)
      title_candidate_note = strdup("Looks like title file empty!");
    else if (st.st_size >= SMALL_BUFFER_SIZE)
      title_candidate_note = strdup("Looks like title is too large!");
  }

  if (descr_candidate)
  {
    stat(descr_candidate, &st);
    if (st.st_size == 0)
      descr_candidate_note = strdup("Looks like description file empty!");
    else if (st.st_size >= STANDARD_BUFFER_SIZE)
      descr_candidate_note = strdup("Looks like description is too large!");
  }

  // Suggest user candidates
  printf("Automaticaly detected:\n");
  if (video_candidate)
  {
    printf("Video file: %s", video_candidate);
    if (video_candidate_note)
      printf(" (%s)", video_candidate_note);
    printf("\n");
  }

  if (trumb_candidate)
  {
    printf("Trumbnail image: %s", trumb_candidate);
    if (trumb_candidate_note)
      printf(" (%s)", trumb_candidate_note);
    printf("\n");
  }

  if (title_candidate)
  {
    printf("Title file: %s", title_candidate);
    if (title_candidate_note)
      printf(" (%s)", title_candidate_note);
    printf("\n");
  }

  if (descr_candidate)
  {
    printf("Description file: %s", descr_candidate);
    if (descr_candidate_note)
      printf(" (%s)", descr_candidate_note);
    printf("\n");
  }

  printf("\n");

  // Accept user choice
  char yes_no, rec_video = 'Y', rec_trumb = 'Y', rec_title = 'Y', rec_descr = 'Y';

  if (video_candidate && trumb_candidate && title_candidate && descr_candidate)
  {
    yes_no = 'X';
    while (yes_no != 'Y' && yes_no != 'y' && yes_no != 'N' && yes_no != 'n')
    {
      printf("Is this information correct (Y/N)?\n");
      yes_no = getchar();
    }
    if (yes_no != 'Y' || yes_no != 'y')
      rec_video = rec_trumb = rec_title = rec_descr = 'N';
  }

  if (!video_candidate || rec_video == 'Y')
  {
    char buf[PATH_MAX];
    while(1)
    {
      if (video_candidate)
        printf("Video file name (Press Enter to use detected %s): ", video_candidate);
      else
        printf("Video file name: ");
      fgets(buf, PATH_MAX, stdin);
      if (stat(buf, &st) != 0)
        printf("File doesn't exists! (Ctrl+C for exit)");
      else
      {
        realloc(video_candidate, strlen(buf) + 1);
        strcpy(video_candidate, buf);
        break;
      }
    }
  }

  if (!trumb_candidate || rec_trumb == 'Y')
  {
    char buf[PATH_MAX];
    while(1)
    {
      if (trumb_candidate)
        printf("Trumbnail file name (Press Enter to use detected %s): ", trumb_candidate);
      else
        printf("Trumbnail file name: ");
      fgets(buf, PATH_MAX, stdin);
      if (stat(buf, &st) != 0)
        printf("File doesn't exists! (Ctrl+C for exit)");
      else
      {
        realloc(trumb_candidate, strlen(buf) + 1);
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
      printf("Would you like to change title (Y/N)?\n");
      yes_no = getchar();
    }
    if (yes_no != 'Y' || yes_no != 'y')
    {
      char buf[SMALL_BUFFER_SIZE];
      fgets(buf, SMALL_BUFFER_SIZE, stdin);

      // create file
      mode_t mode = S_IRUSR | S_IWUSR| S_IRGRP | S_IWGRP | S_IROTH;
      int fd = open("title.txt", O_WRONLY | O_EXCL | O_CREAT, mode);
      if (fd == -1)
        perror("title.txt creation filed!");
      else
      {
        realloc(title_candidate, strlen("title.txt") + 1);
        strcpy(title_candidate, "title.txt");
      }
      close(fd);
    }
  }
  else if (!title_candidate || rec_title == 'Y')
  {
    char buf[SMALL_BUFFER_SIZE];
    while(1)
    {
      printf("Title file name (Press Enter to prompt only it content): ");
      fgets(buf, SMALL_BUFFER_SIZE, stdin);
      if (*buf == '\n' || *buf == '\0')
      {
        fgets(buf, SMALL_BUFFER_SIZE, stdin);

        // create file
        mode_t mode = S_IRUSR | S_IWUSR| S_IRGRP | S_IWGRP | S_IROTH;
        int fd = open("title.txt", O_WRONLY | O_EXCL | O_CREAT, mode);
        if (fd == -1)
          perror("title.txt creation filed!");
        else
        {
          realloc(title_candidate, strlen("title.txt") + 1);
          strcpy(title_candidate, "title.txt");
        }
        close(fd);
      }
      else
      {
        if (stat(buf, &st) != 0)
          printf("File doesn't exists! (Ctrl+C for exit)");
        else
        {
          realloc(title_candidate, strlen(buf) + 1);
          strcpy(title_candidate, buf);
          break;
        }
      }
    }
  }

  if (descr_candidate && rec_descr == 'Y')
  {
    yes_no = 'X';
    while (yes_no != 'Y' && yes_no != 'y' && yes_no != 'N' && yes_no != 'n')
    {
      printf("Would you like to change description (Y/N)?\n");
      yes_no = getchar();
    }
    if (yes_no != 'Y' || yes_no != 'y')
    {
      char buf[STANDARD_BUFFER_SIZE];
      fgets(buf, STANDARD_BUFFER_SIZE, stdin);

      // create file
      mode_t mode = S_IRUSR | S_IWUSR| S_IRGRP | S_IWGRP | S_IROTH;
      int fd = open("descr.html", O_WRONLY | O_EXCL | O_CREAT, mode);
      if (fd == -1)
        perror("descr.html creation filed!");
      else
      {
        realloc(descr_candidate, strlen("descr.html") + 1);
        strcpy(descr_candidate, "descr.html");
      }
      close(fd);
    }
  }
  else if (!descr_candidate || rec_descr == 'Y')
  {
    char buf[STANDARD_BUFFER_SIZE];
    while(1)
    {
      printf("Description file name (Press Enter to prompt only it content): ");
      fgets(buf, STANDARD_BUFFER_SIZE, stdin);
      if (*buf == '\n' || *buf == '\0')
      {
        fgets(buf, STANDARD_BUFFER_SIZE, stdin);

        // create file
        mode_t mode = S_IRUSR | S_IWUSR| S_IRGRP | S_IWGRP | S_IROTH;
        int fd = open("descr.html", O_WRONLY | O_EXCL | O_CREAT, mode);
        if (fd == -1)
          perror("descr.html creation filed!");
        else
        {
          realloc(descr_candidate, strlen("descr.html") + 1);
          strcpy(descr_candidate, "descr.html");
        }
        close(fd);
      }
      else
      {
        if (stat(buf, &st) != 0)
          printf("File doesn't exists! (Ctrl+C for exit)");
        else
        {
          realloc(descr_candidate, strlen(buf) + 1);
          strcpy(descr_candidate, buf);
          break;
        }
      }
    }
  }

  // Calculate sig
  int fds[2];
  pid_t pid;
  pipe(fds);
int rval;

  pid = fork();

  if (pid == (pid_t)0) // child
  {
    char* argv[] = { "/bin/md5sum", "-h", NULL };

    close(fds[0]);

    rval = dup2(fds[1], STDOUT_FILENO);
    if (rval == -1)
      printf("Error dup2!\n");

    rval = dup2(fds[1], STDERR_FILENO);
    if (rval == -1)
      printf("Error dup2!\n");

    execlp("md5sum", "md5sum", 0);
    //execv(argv[0], argv);
    printf("Error exec!\n");
  }
  else // parent
  {
    FILE* stream;
    close(fds[1]);

    stream = fdopen(fds[0], "r");
    char buf[STANDARD_BUFFER_SIZE];
    fgets(buf, STANDARD_BUFFER_SIZE, stream);

    //fflush(stream);
    close(fds[0]);
    rval = waitpid(pid, NULL, 0);
    if (rval == -1)
         printf("Error waitpid!\n");
  }


  // Calculate id
  int dev_random_fd = open("/dev/urandom", O_RDONLY);
  //  assert(dev_random_fd != -1);
  int bytes_to_read;
  unsigned random_value;
  char* next_random_byte = (char*)&random_value;
  bytes_to_read = sizeof(random_value);

  do
  {
    int bytes_read;
    bytes_read = read(dev_random_fd, next_random_byte, bytes_to_read);
    bytes_to_read -= bytes_read;
    next_random_byte += bytes_read;
  } while (bytes_to_read > 0);

  // Move files into showboard
  //int rename(const char *old, const char *new);

  if (server_dir)
    free(server_dir);
  if (showboard_dir)
    free(showboard_dir);
  return EXIT_SUCCESS;
}

char* get_self_executable_directory(ssize_t* length)
{
  char link_target[PATH_MAX];

  // Read symlink /proc/self/exe (readlink does not insert NUL into buf)
  *length = readlink("/proc/self/exe", link_target, sizeof(link_target) - 1);
  if (*length == -1)
    return NULL;

  // Remove file name
  while (*length > 0 && link_target[*length] != '/')
    link_target[(*length)--] = '\0';

  (*length)++;

  return strndup(link_target, PATH_MAX);
}

/*char* get_showboard_directory(ssize_t* length, char* bse, ssize_t bse_lenght)
{
  char* result = strdup("/home/ylazutin/showboard/");
  *length = strlen(result);
  return result;
}*/

char* get_showboard_directory(ssize_t* length, char* bse, ssize_t bse_lenght)
{
  char* result = NULL;

  if ((*length = bse_lenght + strlen("showboard/")) + 1 <= PATH_MAX)
  {
    result = malloc(*length + 1);
    memcpy(result, bse, bse_lenght); // Without NUL
    strcpy(result + bse_lenght, "showboard/"); // Including NUL
  }
  else
    *length = -1;

  return result;
}
