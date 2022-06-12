#include "player.h"

static char* page_begin =
"HTTP/1.0 200 OK\n"
"Content-type: text/html\n"
"\n"
"<!doctype html>\n"
"<html lang=en>\n"
"\n"
"  <head>\n"
"    <meta charset=utf-8>\n"
"    <meta http-equiv=\"content-type\" content=\"text/html\">\n"
"    <meta name=\"author\" content=\"Yuriy Lazutin\" >\n"
"  </head>\n"
"\n"
"  <body>\n"
"    <table style=\"width: 90%; margin-left: 5%; margin-right: 5%;\" border=\"0px\" cellpadding=\"0\" cellspacing=\"0\">\n"
"      <tr style=\"height: 40px;\">\n"
"        <td style=\"width: 3%; background-image: url(https://yuriylazutin.github.io/lazutin.info/videobox/pics/11.png); background-size: 100% 100%;\">\n"
"        </td>\n"
"        <td style=\"background-image: url(https://yuriylazutin.github.io/lazutin.info/videobox/pics/12.png); background-size: 100% 100%;\">\n"
"        </td>\n"
"        <td style=\"width: 3%; background-image: url(https://yuriylazutin.github.io/lazutin.info/videobox/pics/13.png); background-size: 100% 100%;\">\n"
"        </td>\n"
"      </tr>\n"
"\n"
"      <tr>\n"
"        <td style=\"text-align: right; background-image: url(https://yuriylazutin.github.io/lazutin.info/videobox/pics/21.png); background-size: 100% 100%;\">\n"
"        </td>\n"
"        <td style=\"background: radial-gradient(circle at 90% 150%, rgb(0, 0, 0), rgb(70, 70, 70));\">\n"
;

static char* video_template =
"        <video src=\"?pump=%s\" controls preload poster=\"?pump=%s\" style=\"width: 100%;\"></video>\n"
;

static char* page_end =
"        </td>\n"
"        <td style=\"text-align: left; background-image: url(https://yuriylazutin.github.io/lazutin.info/videobox/pics/23.png); background-size: 100% 100%;\">\n"
"        </td>\n"
"      </tr>\n"
"\n"
"      <tr style=\"vertical-align: top;\">\n"
"       <td style=\"background-image: url(https://yuriylazutin.github.io/lazutin.info/videobox/pics/31.png); background-size: 100% 100%;\">\n"
"        </td>\n"
"        <td style=\"background-image: url(https://yuriylazutin.github.io/lazutin.info/videobox/pics/32.png); background-size: 100% 100%; text-align: center;\">\n"
"          <a href=\"https://github.com/YuriyLazutin/VideoBox\"><img src=\"https://yuriylazutin.github.io/lazutin.info/videobox/pics/logo.png\" style=\"width: 4%; margin-top: 1%;\"></img></a>\n"
"        </td>\n"
"        <td style=\"background-image: url(https://yuriylazutin.github.io/lazutin.info/videobox/pics/33.png); background-size: 100% 100%;\">\n"
"        </td>\n"
"      </tr>\n"
"\n"
"    </table>\n"
"  </body>\n"
"\n"
"</html>\n";

extern int showboard(int conn);

int player_show(int conn, const char* params)
{
  int iError = NO_ERRORS;
  char video_href[SIG_SIZE + ID_SIZE + FLAGS_SIZE + 1];
  char trumb_href[SMALL_BUFFER_SIZE];
  char path[PATH_MAX];
  ssize_t path_length;
  int found = 0;
  struct stat st;

  char* ppar = strstr(params, "?play=");
  if (ppar == NULL)
    iError = BAD_REQUEST;
  else
    ppar += strlen("?play=");

  if (iError == NO_ERRORS)
  {
    if (strlen(ppar) < SIG_SIZE + ID_SIZE)
      iError = BAD_REQUEST;
  }

  if (iError == NO_ERRORS)
  {
    char sig[SIG_SIZE + 1];
    memcpy(sig, ppar, SIG_SIZE);
    *(sig + SIG_SIZE) = '\0';

    if ( strspn(sig, "0123456789abcdef") != SIG_SIZE )
      iError = BAD_REQUEST;
  }

  if (iError == NO_ERRORS)
  {
    char id[ID_SIZE + 1];
    memcpy(id, ppar + SIG_SIZE, ID_SIZE);
    *(id + ID_SIZE) = '\0';
    if ( strspn(id, "0123456789abcdef") != ID_SIZE )
      iError = BAD_REQUEST;
  }

  if (iError == NO_ERRORS)
  {
    memcpy(video_href, ppar, SIG_SIZE + ID_SIZE);
    memcpy(trumb_href, ppar, SIG_SIZE + SIG_SIZE);

    path_length = showboard_dir_length + SIG_SIZE + 1 + ID_SIZE + 1;
    if (path_length <= PATH_MAX)
    {
      memcpy(path, showboard_dir, showboard_dir_length);
      memcpy(path + showboard_dir_length, ppar, SIG_SIZE);
      *(path + showboard_dir_length + SIG_SIZE) = '/';
      memcpy(path + showboard_dir_length + SIG_SIZE + 1, ppar + SIG_SIZE, ID_SIZE);
      *(path + path_length - 1) = '/';
    }
    else
      iError = PATH_OVERFLOW;
  }

  if (iError == NO_ERRORS)
  {
    found = 0;

    if (!found)
    {
      if (path_length + strlen("video.mp4") + 1 <= PATH_MAX)
      {
        strcpy(path + path_length, "video.mp4");
        if (stat(path, &st) == 0 && S_ISREG(st.st_mode))
        {
          found = 1;
          strcpy(video_href + SIG_SIZE + ID_SIZE, "f4xx");
        }
      }
    }

    if (!found)
    {
      if (path_length + strlen("video.webm") + 1 <= PATH_MAX)
      {
        strcpy(path + path_length, "video.webm");
        if (stat(path, &st) == 0 && S_ISREG(st.st_mode))
        {
          found = 1;
          strcpy(video_href + SIG_SIZE + ID_SIZE, "fwxx");
        }
      }
    }

    if (!found)
      iError = NOT_FOUND;
  }

  if (iError == NO_ERRORS)
  {
    found = 0;

    if (!found)
    {
      if (path_length + strlen("trumb.png") + 1 <= PATH_MAX)
      {
        strcpy(path + path_length, "trumb.png");
        if (stat(path, &st) == 0 && S_ISREG(st.st_mode))
        {
          found = 1;
          strcpy(trumb_href + SIG_SIZE + ID_SIZE, "ppxx");
        }
      }
    }

    if (!found)
    {
      if (path_length + strlen("trumb.jpg") + 1 <= PATH_MAX)
      {
        strcpy(path + path_length, "trumb.jpg");
        if (stat(path, &st) == 0 && S_ISREG(st.st_mode))
        {
          found = 1;
          strcpy(trumb_href + SIG_SIZE + ID_SIZE, "pjxx");
        }
      }
    }

    if (!found)
    {
      if (path_length + strlen("trumb.webp") + 1 <= PATH_MAX)
      {
        strcpy(path + path_length, "trumb.webp");
        if (stat(path, &st) == 0 && S_ISREG(st.st_mode))
        {
          found = 1;
          strcpy(trumb_href + SIG_SIZE + ID_SIZE, "pwxx");
        }
      }
    }

    if (!found)
      strncpy(trumb_href, "https://yuriylazutin.github.io/lazutin.info/videobox/pics/logo.png", SMALL_BUFFER_SIZE);
  }

  if (iError == NO_ERRORS)
  {
    ssize_t length = strlen(video_template) + strlen(video_href) + strlen(trumb_href);
    char* itm = malloc(length);
    if (itm)
    {
      snprintf(itm, length, video_template, video_href, trumb_href);

      write(conn, page_begin, strlen(page_begin));
      write(conn, itm, strlen(itm));
      write(conn, page_end, strlen(page_end));
      free(itm);

      #ifndef NDEBUG
        log_print("Sended to client:\n");
        log_print("%s", page_begin);
        log_print("%s", itm);
        log_print("%s", page_end);
      #endif
    }
    else
      iError = MALLOC_FAILED;
  }

  if (iError == NO_ERRORS)
    return EXIT_SUCCESS;

  showboard(conn);
  return EXIT_FAILURE;
}
