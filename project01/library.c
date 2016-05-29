/*
 * Anthony Poerio
 * CS1550 - Summer 2016
 * University of Pittsburgh
 * Project01 - Graphics Library
 *
 * Create a graphics library using only using Linux system calls.
 *
 * Notes:
 * Tutorial for mmap():  https://www.safaribooksonline.com/library/view/linux-system-programming/0596009585/ch04s03.html
 * Notes on disabling keypress echo:  http://www.glue.umd.edu/afs/glue.umd.edu/system/info/olh/Programming/Answers_to_Common_Questions_about_C/c_terminal_echo
 * Notes on disabling cannonical mode, generally messing around with termnios: http://blog.eduardofleury.com/archives/2007/11/16
 *
 */


// Includes
#include<stdio.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/mman.h>
#include <sys/ioctl.h>
#include <termios.h>
#include <linux/fb.h>
#include "iso_font.h"

// Definitions
typedef int16_t color_t;
typedef int file_descriptor_t;

// Function Prototypes
void init_graphics();
void exit_graphics();
void clear_screen();
char getkey();
void sleep_ms(long ms);
void draw_pixel(int x, int y, color_t color);
void draw_rect(int x1, int y1, int width, int height, color_t c);
void draw_text(int x, int y, const char *text, color_t c);

// Global variables, which more than one function needs to access
file_descriptor_t fd;
void *file_addr;
struct termios backup_terminal_settings;

/*
 * Main entry point for program
 */
int main(int argc, char *argv[]) {

    // initialize the settings for our graphics library
    init_graphics();

    // do cool stuff in between

    // exit and cleanup the files and memory mappings, restore iotl
    exit_graphics();

    // clear the screen itself
    clear_screen();
}

/*
 *  Opens a graphics device by getting a handle to the first byte of the framebuffer, stored at '/dev/fb0'
 */
void init_graphics() {
    // definitions needed
    struct stat sb;
    off_t len;

    // the file at /dev/fb0, and get its file descriptor
    fd = open("/dev/fb0", O_RDWR);  /* opens file BOTH for reading and writing */
    printf("FILE DESCRIPTOR FOR fb0: %d\n", fd);

    if (fd == -1) {
         printf("Error On Opening the File\n");
         perror("Error On Opening the File");
         return;
    }

    if (fstat(fd, &sb) == -1) {
        printf("Error from fstat\n");
        perror("Error from fstat");
        return;
    }

    if(!S_ISREG(sb.st_mode)) {
        fprintf(stderr, "%s is not a file\n", "/dev/fb0");
        return;
    }


    // use mmap() to map the file we've just opened into memory
    file_addr = mmap(0, sb.st_size, PROT_READ, MAP_SHARED, fd, 0);

    if (file_addr == MAP_FAILED) {
         perror("Map Failed");
         return;
    }

    // get resolution of the screen, so we can use our memory mapping correctly
    struct fb_var_screeninfo virtual_resolution;
    struct fb_fix_screeninfo bit_depth;
    int ioctl_return;
    ioctl_return = ioctl(fd, FBIOGET_VSCREENINFO, &virtual_resolution);
    if (ioctl_return == -1) {
        printf("Failure on ioctl, call to FBIOGET_VSCREENINFO\n");
        perror("Failure on ioctl, call to FBIOGET_VSCREENINFO");
        return;
    }

    ioctl_return = ioctl(fd, FBIOGET_FSCREENINFO, &virtual_resolution);
    if (ioctl_return == -1) {
        printf("Failure on ioctl, call to FBIOGET_FSCREENINFO\n");
        perror("Failure on ioctl, call to FBIOGET_FSCREENINFO");
        return;
    }

    // get mmap total size
    int mmap_total_size = virtual_resolution.yres_virtual * bit_depth.line_length;

    // use the ioctl system call to disable keypress echo and buffering keypresses
    struct termios terminal_settings_old;
    struct termios terminal_settings_new;

    // get the current terminal settings, via ioctl
    ioctl_return = ioctl(0, TCGETS, &terminal_settings_old);
    if (ioctl_return == -1) {
        printf("Failure on ioctl, call to TCGETS\n");
        perror("Failure on ioctl, call to TCGETS");
        return;
    }

    // backup our terminal settings, before we change anything
    backup_terminal_settings = terminal_settings_old;

    // disable canonical mode, which is a c_lflag constant
    terminal_settings_old.c_lflag &= ~ICANON;

    // disable ECHO, also a c_lflag constant
    terminal_settings_old.c_lflag &= ~ECHO;

    // copy these changes to our new settings, so we can write them via ioctl()
    terminal_settings_new = terminal_settings_old;

    // set the new terminal settings, via ioctl
    ioctl_return = ioctl(0, TCGETS, &terminal_settings_new);
    if (ioctl_return == -1) {
        printf("Failure on ioctl, call to TCSETS\n");
        perror("Failure on ioctl, call to TCSETS");
        return;
    }

}

/*
 * Function to undo whatever is necessary before the program exits
 * Use for cleanup
 */
void exit_graphics() {
    // close the file and cleanup memory mapping
    if (close(fd) == -1) {
        printf("Error closing the file at /dev/fb0\n");
        perror("Error closing the file at /dev/fb0");
        return;
    }

    if (munmap(file_addr, sb.st_size) == -1) {
        printf("Error at munmap()\n");
        perror("Error at munmap()");
        return;
    }

    // set our terminal settings back to what they used to be, via ioctl
    if (ioctl(0, TCSETS, &backup_terminal_settings) == -1) {
        printf("Error restoring old terminal values via ioctl\n");
        perror("Error restoring old terminal values via ioctl");
        return;
    }

}

/*
 * Clear the screen with ans ASCII escape code, sent to STDOUT
 */
void clear_screen() {
// clear screen with an ANSI escape code
    // seven bytes, since 7 chars
    // writing to file descriptor 1 for stdout (2 is stderr, if we need it)
    if(write(1, "\033[2j", 7) == -1) {
        printf("Error clearing STDOUT\n");
        perror("Error clearing STDOUT");
        return;
    };
}


