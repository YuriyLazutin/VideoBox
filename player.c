#include "player.h"

char* page =
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
"        <video src=\"video.mp4\" controls preload poster=\"https://yuriylazutin.github.io/lazutin.info/videobox/pics/logo.png\" style=\"width: 100%;\"></video>\n"
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

int player_show(int conn, const char* params)
{
  int rc = EXIT_SUCCESS;

  write(conn, page, strlen(page));
  fprintf(stdout, "Sended to client:\n");
  fprintf(stdout, "%s", page);

  return rc;
}
