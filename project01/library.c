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
 * Notes on select system call at: http://www.tutorialspoint.com/unix_system_calls/_newselect.htm
 *
 */


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

// Definitions
typedef short color_t;
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
void draw_single_character(int x, int y, const char character, color_t c);

// Global variables, which more than one function needs to access
file_descriptor_t fd;
void *file_addr;
struct termios backup_terminal_settings;
struct stat sb;
struct fb_var_screeninfo virtual_resolution;
struct fb_fix_screeninfo bit_depth;
int mmap_total_size;


/*
 * Main entry point for program
 */
/*
int main(int argc, char *argv[]) {

    // initialize the settings for our graphics library
    init_graphics();

    // do cool stuff in between

    // exit and cleanup the files and memory mappings, restore ioctl
    exit_graphics();

    // clear the screen itself
    clear_screen();
}
*/

/*
 *  opens a graphics device by getting a handle to the first byte of the framebuffer, stored at '/dev/fb0'
 */
void init_graphics() {
    // definitions needed
    off_t len;

    // the file at /dev/fb0, and get its file descriptor
    fd = open("/dev/fb0", O_RDWR);  /* opens file both for reading and writing */
    printf("file descriptor for fb0: %d\n", fd);

    if (fd == -1) {
         printf("Error On Opening the File\n");
         perror("Error On Opening the File");
         //return;
    }

    if (fstat(fd, &sb) == -1) {
        printf("Error from fstat\n");
        perror("Error from fstat");
        //return;
    }

    if(!S_ISREG(sb.st_mode)) {
        fprintf(stderr, "%s is not a file\n", "/dev/fb0");
        perror("/dev/fb0 is not a file");
        //return;
    }

    // get resolution of the screen, so we can use our memory mapping correctly
    // store these in the fb_ structs named:
    // var_screeninfo -> virtual_resolution, AND...
    // fix_screeninfo -> bit_depth
    int ioctl_return;
    ioctl_return = ioctl(fd, FBIOGET_VSCREENINFO, &virtual_resolution);
    if (ioctl_return == -1) {
        printf("Failure on ioctl, call to FBIOGET_VSCREENINFO\n");
        perror("Failure on ioctl, call to FBIOGET_VSCREENINFO");
        //return;
    }

    printf("VIRTUAL_RESOLUTION.BITS_PER_PIXEL (VAR_SC_INFO)= %d\n", virtual_resolution.bits_per_pixel);

    ioctl_return = ioctl(fd, FBIOGET_FSCREENINFO, &bit_depth);
    if (ioctl_return == -1) {
        printf("Failure on ioctl, call to FBIOGET_FSCREENINFO\n");
        perror("Failure on ioctl, call to FBIOGET_FSCREENINFO");
        //return;
    }
    printf("BIT_DEPTH.LINE_LENGTH (FIX_SC_INFO)= %d(bytes)\n", bit_depth.line_length);

    // get mmap total size
    mmap_total_size = virtual_resolution.yres_virtual * bit_depth.line_length;
    printf("VIRTUAL_RESOLUTION.YRES = %d\n", virtual_resolution.yres_virtual);
    printf("MMAP_TOTAL_SIZE = %d\n", mmap_total_size);

    // use mmap() to map the file we've just opened into memory
    file_addr = mmap(NULL, mmap_total_size, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
    printf("FILE ADDRESS FOR fb0: %p\n", file_addr);

    if (file_addr == MAP_FAILED) {
         perror("Map Failed");
         printf("Map failed\n");
         //return;
    }

    // use the ioctl system call to disable keypress echo and buffering keypresses
    struct termios terminal_settings_old;
    struct termios terminal_settings_new;

    // get the current terminal settings, via ioctl
    ioctl_return = ioctl(0, TCGETS, &terminal_settings_old);
    if (ioctl_return == -1) {
        printf("Failure on ioctl, call to TCGETS\n");
        perror("Failure on ioctl, call to TCGETS");
        //return;
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
    ioctl_return = ioctl(0, TCSETS, &terminal_settings_new);
    if (ioctl_return == -1) {
        printf("Failure on ioctl, call to TCSETS\n");
        perror("Failure on ioctl, call to TCSETS");
        //return;
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
    }
    if (munmap(file_addr, mmap_total_size) == -1) {
        printf("Error at munmap()\n");
        perror("Error at munmap()");
    }

    // set our terminal settings back to what they used to be, via ioctl
    if (ioctl(0, TCSETS, &backup_terminal_settings) == -1) {
        printf("Error restoring old terminal values via ioctl\n");
        perror("Error restoring old terminal values via ioctl");
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

/*
 * Get a keystroke as user input, using select() as a non-blocking call,
 * unless input is present. In which case we then do a read(), which is blocking.
 */
char getkey() {
    // define our variables
    fd_set rfds;
    struct timeval tv;
    int retval;
    char buffer = '\0';   // Initialize to null, so we always have something to return

    // monitor stdin (fd=1), using select(), so we have a non-blocking call unless a key is pressed
    FD_ZERO(&rfds);
    FD_SET(0, &rfds);
    // don't wait at all, just perform one call to check for presence of input
    tv.tv_sec = 0;   // how many seconds to wait
    tv.tv_usec = 0;  // how many microseconds to wait
    retval = select(1, &rfds, NULL, NULL, &tv); // running on fd=1, because first  arg, 'nfds' is the highest num file_descr
                                                // in any of the three values passed in, +1.  We're reading on (0 + 1) = 1
    if (retval == -1) {  /* means we have an error */
         printf("Error on select() syscall\n");
         perror("Error on select() syscall");
    } else if (retval) { /* means we have a data value */
         // we have data, so let's read it, again using '1' as our fd, since 1=stdin
         ssize_t numBytesRead;
         numBytesRead = read(0, &buffer, 1);
         if (numBytesRead < 1) {
            printf("Error Reading Character in getKey(), read() syscall\n");
            perror("read(), in getkey()");
         }
    } //else {  /* just in case */
      //  printf("No data read. No error. Made it to else in select() syscall\n");
      // }

    return buffer;
}

/*
 * Call nanosleep() to make program sleep between frames of graphics being drawn
 * Multiply the value passed in by 1,000,000 to achieve ms precision
 */
void sleep_ms(long ms) {
    // Create a timespec struct to pass in, defined in <time.h>
    struct timespec time_spec;
    time_spec.tv_sec = 0;
    time_spec.tv_nsec = ms * 1000000;

    // call nanosleep(), and check for error
    if (nanosleep(&time_spec, NULL) < 0) {   /* means we have an error */
        printf("Error at nanosleep() system call in sleep_ms\n");
        perror("nanosleep");
    }
}

/*
 * The main drawing code. Set the pixel at coordinates (x,y) to the specified color,
 * taking values from a 16bit int we are using to represent the color.
 * Use the given coords the scale the base address of the memory-mapped buffer, using pointer arithmetic.
 * Frame buffer stored in row-major order. Meaning that first row starts at offset 0, then is followed
 * by the second row of pixels, &c.
 *
 */
void draw_pixel(int x, int y, color_t color) {
    // scale our values to bytes
    int scaled_offset;
    int line_length_in_bytes = bit_depth.line_length;  // already in bytes
    int bits_per_pixel_scaled_to_bytes = (virtual_resolution.bits_per_pixel / 8);

    // use scaled values to find the offset
    scaled_offset = (x * bits_per_pixel_scaled_to_bytes) + (y * line_length_in_bytes);

    // get location of pixel to draw, within our memory map
    color_t *pixel_location;
    pixel_location = file_addr + scaled_offset;
    //printf("file addr = %p\n",file_addr);
    //printf("scaled offset = %d\n", scaled_offset);
    //printf("pixel location = %p\n", pixel_location);

    // transform this pixel location to a different color, as specified by the user
    pixel_location = &color;

}

/*
 * Using draw_pixel(), make a rectangle with corners (x1,y1),
 *                                                   (x1 + width, y1),
 *                                                   (x1+width, y1+height),
 *                                                   (x1, y1+height)
 */
void draw_rect(int x1, int y1, int width, int height, color_t c) {
    // initialize rows and columns
    int row_counter;
    int col_counter;

    // iterate and draw our pixels
    for (row_counter = 0; row_counter < width; row_counter++) {
            // call draw pixel on current values along x-axis
            draw_pixel(x1 + row_counter, y1, c);
            draw_pixel(x1 + row_counter, y1 + height, c);
            //printf("drawing row %d\n", row_counter);
    }
    for (col_counter = 0; col_counter < height; col_counter++) {
            // call draw pixel on current values along y-axis
            draw_pixel(x1, y1 + col_counter, c);
            draw_pixel(x1 + width, y1 + col_counter, c);
            //printf("drawing col %d\n", col_counter);
    }
}

/*
 * Draw the string with the specified color at the starting location (x,y).
 * (X,Y) is the upper left corner of FIRST letter.
 * Each letter ix 8x16 pixels.
 * Each letter is encoded as 16 1-byte integers
 *
 * Index to character by ASCII value * number of rows.
 * So, A, ASCII=65, is found at (65*16 + 0) --> (16*16 + 15)
 *
 * Using shifting and masking, go through each bit of the 16 rows and draw a pixel at
 * the appropriate coordinate if the bit is 1
 *
 *Strategy: Break into 2 functions.
 * - 1. for drawing a single character.
 * - 2. for drawing the whole string
 *
 * Don't use strlen(), just iterate until you find '\0'
 *
 * No need to worry about line breaking.
 *
 *
 */
void draw_text(int x, int y, const char *text, color_t c) {
    // Assuming we get a string of text, with a null terminator,
    // we want to call draw char on every character,
    // until we hit a null terminator

    // define vars we need to keep track of where we are
    int charIndex;
    charIndex = 0;

    int pixelOffset;
    pixelOffset = 0;

    char currentChar;

    // perform the iteration itself
    while(text[charIndex] != '\0') {

        // index into the text array and get our character value, will be ASCII
        currentChar = text[charIndex];

        // call draw pixel with our ASCII value and the pass the (x,y) values
        draw_single_character(x + pixelOffset, y, currentChar, c);

        // update the byte offset and array index
        pixelOffset += 8; // because each character is 8 pixels, and we don't want to overlap/overwrite
                          // no pixel offset is added for y, because prompt says we don't need to
                          // worry about line breaking
        charIndex++;
    } // end-while
}


/*
 *  Helper function to draw a single character
 */
void draw_single_character(int x, int y, const char character, color_t c) {
    // index into the iso_font.h file and get a reference to the character,
    // based on its ASCII value, use the "iso_font" variable, defined in the .h file
    unsigned const char charAddressStart = iso_font[character * 16 + 0];  // this gets me the value itself....

    // iterate through the values assigned to the selected character,
    // AND if the bite is set to 1, then call draw_pixel
    unsigned char valueAtCurrentAddress;

    int intNum = 0; // we'll go from 0->15 on this
    int bitNum = 0; // we'll go from 0->256

    for (intNum = 0; intNum < 16; intNum++) {

        // get the row address, as outlined at: https://people.cs.pitt.edu/~jmisurda/teaching/cs1550/2167/cs1550-2167-project1.htm
        valueAtCurrentAddress = iso_font[character*16 + intNum]; // index directly to the current 1-byte integer that represents the number,
                                                                 // out of 16 total, 0->15

        for (bitNum = 0; bitNum < 8; bitNum++ ) {
            // use shifting and masking within each row to get the pixel at each coord
            int valueOfCurrentBit = 1 << bitNum; // put a 1 at every position from from 2^0 -> 2^7, in order
            if (valueAtCurrentAddress & valueOfCurrentBit) {  /* If there's a '1' at any given bit within                                                       *
                                                                 the one byte 'integer' */
                draw_pixel(x+bitNum, y+intNum, c);
            } // end-if
        } // end inner-for
    } // end outer-for
}


