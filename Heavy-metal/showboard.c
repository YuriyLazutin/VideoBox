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
"<font class=\"rectext\">We recommend you:</font>\n"
"</div>\n"
"</div>\n"
"</header>\n"
"<div class=\"board\">\n";

static char* note_template =
"<div class=\"boardernote\">\n"
" <a href=\"./?play=%s\">\n"
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

ssize_t add_file_name(char* path, ssize_t path_length, char* file_name);
int test_file(char* path);
char* mk_href(char* path, const ssize_t path_length);
char* mk_trumb(char* path, const ssize_t path_length);
char* mk_title(char* path, const ssize_t path_length);
char* mk_descr(char* path, const ssize_t path_length);
int mk_boardernote(char** result, const char* href, const char* trumb_file, const char* trumb_tlt, const char* desc);

int showboard(int conn, const char* params)
{
  write(conn, page_begin, strlen(page_begin));
  fprintf(stdout, "Sended to client:\n");
  fprintf(stdout, "%s", page_begin);

  int rc = EXIT_SUCCESS, rc2;
  ssize_t sig_length, id_length, entry_length;
  char path[PATH_MAX];
  DIR *brd_dir, *id_dir;
  struct dirent *brd_entry, *id_entry;

  if ((brd_dir = opendir(showboard_dir)) != NULL) // Open "${VBX_HOME}/showboard/"
  {
    memcpy(path, showboard_dir, showboard_dir_length); // Without NUL
    while ((brd_entry = readdir(brd_dir)) != NULL)
    {
      entry_length = strlen(brd_entry->d_name);
      sig_length = showboard_dir_length + entry_length + 1;
      if (sig_length + 1 > PATH_MAX) // Path overflow. Just skip such entry
        continue;

      if ( entry_length != SIG_SIZE || strspn(brd_entry->d_name, "0123456789abcdef") != SIG_SIZE)  // This is not vbx signature. Just skip such entry
        continue;

      memcpy(path + showboard_dir_length, brd_entry->d_name, SIG_SIZE); // Without NUL
      path[sig_length - 1] = '/';
      path[sig_length] = '\0';

      if ((id_dir = opendir(path)) != NULL)   // Open "${VBX_HOME}/showboard/<sig32>/"
      {
        while ((id_entry = readdir(id_dir)) != NULL)
        {
          entry_length = strlen(id_entry->d_name);
          id_length = sig_length + entry_length + 1;
          if (id_length + 1 > PATH_MAX) // Path overflow. Just skip such entry
            continue;

          if ( entry_length != ID_SIZE || strspn(id_entry->d_name, "0123456789abcdef") != ID_SIZE)  // This is not vbx id. Just skip such entry
            continue;

          memcpy(path + sig_length, id_entry->d_name, ID_SIZE); // Without NUL
          path[id_length - 1] = '/';
          path[id_length] = '\0';

          char *href, *trumb, *title, *descr, *brdnote = NULL;

          if ( (href = mk_href(path, id_length)) == NULL) // Can't allocate memory or find video.mp4 or video.webm
            continue;
          if ( (trumb = mk_trumb(path, id_length)) == NULL) // Can't allocate memory (in case of missed trumbnail we will use logo)
            continue;
          if ( (title = mk_title(path, id_length)) == NULL) // Can't allocate memory (in case of missed title we will use default video name)
            continue;
          if ( (descr = mk_descr(path, id_length)) == NULL) // Can't allocate memory (in case of missed description we will show "Watch this video")
            continue;

          rc2 = mk_boardernote(&brdnote, href, trumb, title, descr);
          if (rc2 == EXIT_SUCCESS)
          {
            write(conn, brdnote, strlen(brdnote));
            fprintf(stdout, "%s", brdnote);
          }

          if (href)
            free(href);
          if (trumb)
            free(trumb);
          if (title)
            free(title);
          if (descr)
            free(descr);
          if (brdnote)
            free(brdnote);
        }
        closedir(id_dir);
      }
    }
    closedir(brd_dir);
  }

  write(conn, page_end, strlen(page_end));
  fprintf(stdout, "%s", page_end);

  return rc;
}


// In this function we assumes that buffer (targeted by path pointer) has enough space to store PATH_MAX string
// Function return new path_length
ssize_t add_file_name(char* path, ssize_t path_length, char* file_name)
{
  while (path_length < PATH_MAX && *file_name != '\0')
  {
    path[path_length++] = *file_name;
    file_name++;
  }

  if (path_length + 1 == PATH_MAX) // Path overflow. Just skip such entry
    return 0;

  path[path_length] = '\0';
  return path_length;
}

int test_file(char* path)
{
  struct stat st;
  int rc = stat(path, &st);
  if (rc != 0)
    return 0;
  return 1;  // File exists
}

// In this function we assumes that buffer (targeted by path pointer) has enough space to store PATH_MAX string
char* mk_href(char* path, const ssize_t path_length)
{
  ssize_t file_length;
  int found = 0;

  if (!found)
  {
    file_length = add_file_name(path, path_length, "video.mp4");
    if (file_length)
      found = test_file(path);
  }

  if (!found)
  {
    file_length = add_file_name(path, path_length, "video.webm");
    if (file_length)
      found = test_file(path);
  }

  if (!found)
    return NULL;

  ssize_t href_length;
  href_length = SIG_SIZE + ID_SIZE + FLAGS_SIZE + 1;
  char* result = malloc(href_length);
  if (result)
  {
    memcpy(result, path + (path_length - 1 - ID_SIZE - 1 - SIG_SIZE), SIG_SIZE);
    memcpy(result + SIG_SIZE, path + (path_length - 1 - ID_SIZE), ID_SIZE);
    if (strcmp(path + path_length, "video.mp4") == 0)
      memcpy(result + SIG_SIZE + ID_SIZE, "f4xx", FLAGS_SIZE);
    else if (strcmp(path + path_length, "video.webm") == 0)
      memcpy(result + SIG_SIZE + ID_SIZE, "fwxx", FLAGS_SIZE);
    else
      memcpy(result + SIG_SIZE + ID_SIZE, "xxxx", FLAGS_SIZE);
    *(result + href_length - 1) = '\0';
  }
  return result;
}

char* mk_trumb(char* path, const ssize_t path_length)
{
  ssize_t file_length, pref_length, href_length;
  int found = 0;

  if (!found)
  {
    file_length = add_file_name(path, path_length, "trumb.png");
    if (file_length)
      found = test_file(path);
  }

  if (!found)
  {
    file_length = add_file_name(path, path_length, "trumb.jpg");
    if (file_length)
      found = test_file(path);
  }

  if (!found)
  {
    file_length = add_file_name(path, path_length, "trumb.webp");
    if (file_length)
      found = test_file(path);
  }

  if (!found)
    return strdup("https://yuriylazutin.github.io/lazutin.info/videobox/pics/logo.png");

  pref_length = strlen("?pump=");
  href_length = pref_length + SIG_SIZE + ID_SIZE + FLAGS_SIZE + 1;
  char* result = malloc(href_length);
  if (result)
  {
    memcpy(result, "?pump=", pref_length);
    memcpy(result + pref_length, path + (path_length - 1 - ID_SIZE - 1 - SIG_SIZE), SIG_SIZE);
    memcpy(result + pref_length + SIG_SIZE, path + (path_length - 1 - ID_SIZE), ID_SIZE);
    if (strcmp(path + path_length, "trumb.png") == 0)
      memcpy(result + pref_length + SIG_SIZE + ID_SIZE, "ppxx", FLAGS_SIZE);
    else if (strcmp(path + path_length, "trumb.jpg") == 0)
      memcpy(result + pref_length + SIG_SIZE + ID_SIZE, "pjxx", FLAGS_SIZE);
    else if (strcmp(path + path_length, "trumb.webp") == 0)
      memcpy(result + pref_length + SIG_SIZE + ID_SIZE, "pwxx", FLAGS_SIZE);
    else
      memcpy(result + pref_length + SIG_SIZE + ID_SIZE, "xxxx", FLAGS_SIZE);
    *(result + href_length - 1) = '\0';
  }
  return result;
}

char* mk_title(char* path, const ssize_t path_length)
{
  ssize_t file_length;
  int found = 0;

  if (!found)
  {
    file_length = add_file_name(path, path_length, "title.html");
    if (file_length)
      found = test_file(path);
  }

  if (!found)
  {
    file_length = add_file_name(path, path_length, "title.txt");
    if (file_length)
      found = test_file(path);
  }

  if (!found)
    return strdup("Unnamed video");

  int fd = open(path, O_RDONLY);
  if (fd == -1)
    strdup("Unnamed video");

  char* result = malloc(SMALL_BUFFER_SIZE);
  ssize_t rc = read(fd, result, SMALL_BUFFER_SIZE - 1);
  result[rc] = '\0';
  return result;
}

char* mk_descr(char* path, const ssize_t path_length)
{
  ssize_t file_length;
  int found = 0;

  if (!found)
  {
    file_length = add_file_name(path, path_length, "descr.html");
    if (file_length)
      found = test_file(path);
  }

  if (!found)
  {
    file_length = add_file_name(path, path_length, "descr.txt");
    if (file_length)
      found = test_file(path);
  }

  if (!found)
    return strdup("Watch this video...");

  int fd = open(path, O_RDONLY);
  if (fd == -1)
    strdup("Watch this video...");

  char* result = malloc(STANDARD_BUFFER_SIZE);
  ssize_t rc = read(fd, result, STANDARD_BUFFER_SIZE - 1);
  result[rc] = '\0';
  return result;
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
    return EXIT_FAILURE;
  return EXIT_SUCCESS;
}
