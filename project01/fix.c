// Includes
#include <stdio.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/mman.h>
#include <sys/ioctl.h>
#include <termios.h>
#include <linux/fb.h>
#include "iso_font.h"
#include <sys/select.h>
#include <sys/time.h>
#include <sys/types.h>



int main() {

    struct termios terminal;
    ioctl(0, TCGETS, &terminal);
    terminal.c_lflag |= ICANON;
    terminal.c_lflag |= ECHO;
    ioctl(0, TCSETS, &terminal);

}
