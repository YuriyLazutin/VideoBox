#include "showboard.h"

static char* page_begin =
"HTTP/1.0 200 OK\n"
"Content-type: text/html\n"
"\n"
"<!doctype html>\n"
"<html lang=en>\n"
"<head>\n"
"<meta charset=utf-8>\n"
"<meta http-equiv=\"content-type\" content=\"text/html\">\n"
"<meta name=\"author\" content=\"Yuriy Lazutin\">\n"
"</head>\n"
"<style>\n"
"html, body {\n"
"display: block;\n"
"width: 100%;\n"
"height: 100%;\n"
"margin: 0;\n"
"padding: 0;\n"
"background-color: rgb(160, 200, 210);\n"
"background: radial-gradient(at 50% 50% , rgb(190, 200, 210), rgb(160, 200, 210));\n"
"color: rgb(72, 41, 40);\n"
"}\n"
"header {\n"
"width: 100%;\n"
"height: 10%;\n"
"min-height: 64px;\n"
"position: fixed;\n"
"top: 0px;\n"
"z-index: 1;\n"
"background-color: rgb(130, 170, 180);\n"
"background: radial-gradient(at 50% 50% , rgb(160, 170, 180), rgb(130, 170, 180));\n"
"color: #233D23;\n"
"}\n"
".cont1 {\n"
"width: 70%;\n"
"height: 100%;\n"
"display: flex;\n"
"align-items: center;\n"
"justify-content: center;\n"
"margin-left: auto;\n"
"margin-right: auto;\n"
"}\n"
".logo {\n"
"height: 80%;\n"
"margin: 1%;\n"
"}\n"
".cont2 {\n"
"height: 100%;\n"
"display: block;\n"
"position: relative;\n"
"}\n"
"H1 {\n"
"font-weight: bold;\n"
"font-style: italic;\n"
"font-size: calc(10px + 1.5vw);\n"
"text-align: center;\n"
"margin-left: 3%;\n"
"margin-right: 3%;\n"
"white-space: nowrap;\n"
"}\n"
".rectext {\n"
"font-size: calc(8px + 0.5vw);\n"
"text-align: center;\n"
"white-space: nowrap;\n"
"position: absolute;\n"
"top: 50%;\n"
"margin-left: 50%;\n"
"margin-top: 15%;\n"
"transform: translate(-50%, -50%);\n"
"}\n"
"footer {\n"
"width: 100%;\n"
"height: 5%;\n"
"min-height: 48px;\n"
"position: fixed;\n"
"top: 95%;\n"
"z-index: 1;\n"
"}\n"
".board {\n"
"display: block;\n"
"position: absolute;\n"
"top: 10%;\n"
"margin-top: 1%;\n"
"width: 90%;\n"
"margin-left: 5%;\n"
"margin-right: 5%;\n"
"margin-bottom: 1%;\n"
"bottom: 5%;\n"
"}\n"
".boardernote {\n"
"display: block;\n"
"position: relative;\n"
"float: left;\n"
"width: 18%;\n"
"height: 25%;\n"
"min-width: 240px;\n"
"min-height: 180px;\n"
"margin-right: 0.5%;\n"
"margin-left: 0.5%;\n"
"margin-top: 0.5%;\n"
"margin-bottom: 0.5%;\n"
"}\n"
".trumb {\n"
"width: 100%;\n"
"height: 80%;\n"
"box-shadow: 3px 3px 5px 5px rgba(0,0,0,0.5);\n"
"}\n"
"a {\n"
"text-decoration: none;\n"
"color: rgb(0, 0, 0);\n"
"font-size: calc(8px + 0.5vw);\n"
"}\n"
".viddesc {\n"
"display: block;\n"
"font-size: calc(8px + 0.4vw);\n"
"text-align: justify;\n"
"overflow: hidden;\n"
"}\n"
"</style>\n"
"<body>\n"
"<header>\n"
"<div class=\"cont1\">\n"
"<img src=\"https://yuriylazutin.github.io/lazutin.info/videobox/pics/logo.png\" class=\"logo\"></img>\n"
"<div class=\"cont2\">\n"
"<H1>Videobox</H1>\n"
"<font class=\"rectext\">********************</font>\n"
"</div>\n"
"</div>\n"
"</header>\n"
"<div class=\"board\">\n";

static char* note_template =
"<div class=\"boardernote\">\n"
" <a href=\"?play=%s\">\n"
"   <img class=\"trumb\" src=\"%s\" title=\"%s\"></img>\n"
"   <font class=\"viddesc\"><b>%s</b><br>%s</font>\n"
" </a>\n";

static char* page_end =
"</div>\n"
"<footer>\n"
"<br>\n"
"</footer>\n"
"</body>\n"
"</html>\n"
"\n";

int mk_href(char* result, char* path, const ssize_t path_length);
void mk_trumb(char* result, char* path, const ssize_t path_length);
void mk_title(char* result, char* path, const ssize_t path_length);
void mk_descr(char* result, char* path, const ssize_t path_length);
int mk_boardernote(char** result, const char* href, const char* trumb_file, const char* trumb_tlt, const char* desc);

int showboard(int conn)
{
  int rc = strlen(page_begin);
  rc = write_block(conn, page_begin, rc);
  if (rc < 0)
    return vbx_errno;

  #ifndef NDEBUG
    log_print("Sended to client:\n");
    log_print("%s", page_begin);
  #endif

  ssize_t sig_length, id_length, entry_length;
  char path[PATH_MAX];
  DIR *brd_dir, *id_dir;
  struct dirent *brd_entry, *id_entry;
  char href[SIG_SIZE + ID_SIZE + 1];
  char trumb[SMALL_BUFFER_SIZE];
  char title[SMALL_BUFFER_SIZE];
  char descr[STANDARD_BUFFER_SIZE];
  char* brdnote = NULL;

  if ((brd_dir = opendir(showboard_dir)) != NULL) // Open "${VBX_HOME}/showboard/"
  {
    memcpy(path, showboard_dir, showboard_dir_length); // Without NUL
    while ((brd_entry = readdir(brd_dir)) != NULL)
    {
      entry_length = strlen(brd_entry->d_name);
      if ( entry_length != SIG_SIZE || strspn(brd_entry->d_name, SIG_CHARS) != SIG_SIZE)  // This is not vbx signature. Just skip such entry
        continue;
      sig_length = showboard_dir_length + entry_length + 1;
      if (sig_length + 1 > PATH_MAX) // Path overflow. Just skip such entry
        continue;

      memcpy(path + showboard_dir_length, brd_entry->d_name, SIG_SIZE); // Without NUL
      path[sig_length - 1] = '/';
      path[sig_length] = '\0';

      if ((id_dir = opendir(path)) != NULL)   // Open "${VBX_HOME}/showboard/<sig32>/"
      {
        while ((id_entry = readdir(id_dir)) != NULL)
        {
          entry_length = strlen(id_entry->d_name);
          if ( entry_length != ID_SIZE || strspn(id_entry->d_name, ID_CHARS) != ID_SIZE)  // This is not vbx id. Just skip such entry
            continue;
          id_length = sig_length + entry_length + 1;
          if (id_length + 1 > PATH_MAX) // Path overflow. Just skip such entry
            continue;

          memcpy(path + sig_length, id_entry->d_name, ID_SIZE); // Without NUL
          path[id_length - 1] = '/';
          path[id_length] = '\0';

          if ( (rc = mk_href(href, path, id_length)) != NO_ERRORS) // Can't find video.mp4 or video.webm or path overflow
            continue;
          mk_trumb(trumb, path, id_length);
          mk_title(title, path, id_length);
          mk_descr(descr, path, id_length);

          rc = mk_boardernote(&brdnote, href, trumb, title, descr);
          if (rc == NO_ERRORS)
          {
            rc = strlen(brdnote);
            rc = write_block(conn, brdnote, rc);
            if (rc < 0)
            {
              closedir(id_dir);
              closedir(brd_dir);
              if (brdnote)
                free(brdnote);
              return vbx_errno;
            }

            #ifndef NDEBUG
              log_print("%s", brdnote);
            #endif
          }
        }
        closedir(id_dir);
      }
    }
    closedir(brd_dir);
  }

  if (brdnote)
    free(brdnote);

  rc = strlen(page_end);
  rc = write_block(conn, page_end, rc);
  if (rc < 0)
    return vbx_errno;

  #ifndef NDEBUG
    log_print("%s", page_end);
  #endif

  return NO_ERRORS;
}

// In this function we assumes that:
// 1) buffer (targeted by result pointer) has enough space to store SIG_SIZE + ID_SIZE string and 1 nul byte
// 2) buffer (targeted by path pointer) has enough space to store PATH_MAX string
int mk_href(char* result, char* path, const ssize_t path_length)
{
  int found = 0;
  struct stat st;

  if (path_length + strlen("video.mp4") + 1 <= PATH_MAX)
  {
    strcpy(path + path_length, "video.mp4");
    if (stat(path, &st) == 0 && S_ISREG(st.st_mode))
      found = 1;
  }

  if (!found && path_length + strlen("video.webm") + 1 <= PATH_MAX)
  {
    strcpy(path + path_length, "video.webm");
    if (stat(path, &st) == 0 && S_ISREG(st.st_mode))
      found = 1;
  }

  if (!found)
    return NOT_FOUND;

  memcpy(result, path + (path_length - 1 - ID_SIZE - 1 - SIG_SIZE), SIG_SIZE);
  memcpy(result + SIG_SIZE, path + (path_length - 1 - ID_SIZE), ID_SIZE);
  result[SIG_SIZE + ID_SIZE] = '\0';

  return NO_ERRORS;
}

// In this function we assumes that:
// 1) buffer (targeted by result pointer) has enough space to store https://yuriylazutin.github.io/lazutin.info/videobox/pics/logo.png string and 1 nul byte
// 2) buffer (targeted by path pointer) has enough space to store PATH_MAX string
void mk_trumb(char* result, char* path, const ssize_t path_length)
{
  int found = 0;
  struct stat st;
  char flags[FLAGS_SIZE] = "ppxx";

  if (path_length + strlen("trumb.png") + 1 <= PATH_MAX)
  {
    strcpy(path + path_length, "trumb.png");
    if (stat(path, &st) == 0 && S_ISREG(st.st_mode))
      found = 1;
  }

  if (!found && path_length + strlen("trumb.jpg") + 1 <= PATH_MAX)
  {
    strcpy(path + path_length, "trumb.jpg");
    if (stat(path, &st) == 0 && S_ISREG(st.st_mode))
    {
      found = 1;
      flags[1] = 'j';
    }
  }

  if (!found && path_length + strlen("trumb.webp") + 1 <= PATH_MAX)
  {
    strcpy(path + path_length, "trumb.webp");
    if (stat(path, &st) == 0 && S_ISREG(st.st_mode))
    {
      found = 1;
      flags[1] = 'w';
    }
  }

  if (found)
  {
    ssize_t pref_length = strlen("?pump=");
    memcpy(result, "?pump=", pref_length);
    memcpy(result + pref_length, path + (path_length - 1 - ID_SIZE - 1 - SIG_SIZE), SIG_SIZE);
    memcpy(result + pref_length + SIG_SIZE, path + (path_length - 1 - ID_SIZE), ID_SIZE);
    memcpy(result + pref_length + SIG_SIZE + ID_SIZE, flags, FLAGS_SIZE);
    result[pref_length + SIG_SIZE + ID_SIZE + FLAGS_SIZE] = '\0';
  }
  else
    strncpy(result, "https://yuriylazutin.github.io/lazutin.info/videobox/pics/logo.png", SMALL_BUFFER_SIZE);
}

// In this function we assumes that:
// 1) buffer (targeted by result pointer) has enough space to store string(SMALL_BUFFER_SIZE) and 1 nul byte
// 2) buffer (targeted by path pointer) has enough space to store PATH_MAX string
void mk_title(char* result, char* path, const ssize_t path_length)
{
  int iError = NO_ERRORS;
  int found = 0;
  struct stat st;

  if (path_length + strlen("title.txt") + 1 <= PATH_MAX)
  {
    strcpy(path + path_length, "title.txt");
    if (stat(path, &st) == 0 && S_ISREG(st.st_mode))
      found = 1;
  }

  if (!found && path_length + strlen("title.html") + 1 <= PATH_MAX)
  {
    strcpy(path + path_length, "title.html");
    if (stat(path, &st) == 0 && S_ISREG(st.st_mode))
      found = 1;
  }

  if (found)
  {
    int fd = open(path, O_RDONLY);
    if (fd == -1)
      iError = OPEN_FILE_ERROR;
    else
    {
      ssize_t rc = read(fd, result, SMALL_BUFFER_SIZE - 1);
      if (rc <= 0)
        iError = READ_FILE_ERROR;
      else
      {
        result[rc] = '\0';
        close(fd);
      }
    }
  }
  else
    iError = NOT_FOUND;

  if (iError != NO_ERRORS)
    strncpy(result, "Unnamed video", SMALL_BUFFER_SIZE);
}

void mk_descr(char* result, char* path, const ssize_t path_length)
{
  int iError = NO_ERRORS;
  int found = 0;
  struct stat st;

  if (path_length + strlen("descr.html") + 1 <= PATH_MAX)
  {
    strcpy(path + path_length, "descr.html");
    if (stat(path, &st) == 0 && S_ISREG(st.st_mode))
      found = 1;
  }

  if (!found && path_length + strlen("descr.txt") + 1 <= PATH_MAX)
  {
    strcpy(path + path_length, "descr.txt");
    if (stat(path, &st) == 0 && S_ISREG(st.st_mode))
      found = 1;
  }

  if (found)
  {
    int fd = open(path, O_RDONLY);
    if (fd == -1)
      iError = OPEN_FILE_ERROR;
    else
    {
      ssize_t rc = read(fd, result, STANDARD_BUFFER_SIZE - 1);
      if (rc <= 0)
        iError = READ_FILE_ERROR;
      else
      {
        result[rc] = '\0';
        close(fd);
      }
    }
  }
  else
    iError = NOT_FOUND;

  if (iError != NO_ERRORS)
    strncpy(result, "Watch this video...", SMALL_BUFFER_SIZE);
}

int mk_boardernote(char** result, const char* href, const char* trumb_file, const char* title, const char* descr)
{
  ssize_t sz = strlen(note_template);
  sz += strlen(href);
  sz += strlen(trumb_file);
  sz += strlen(title) * 2;
  sz += strlen(descr);
  *result = realloc(*result, sz);
  if (*result)
    snprintf(*result, sz, note_template, href, trumb_file, title, title, descr);
  else
    return MALLOC_FAILED;
  return NO_ERRORS;
}
