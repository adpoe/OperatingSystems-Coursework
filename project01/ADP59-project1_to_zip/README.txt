CS1550 - Project01
Summer 2016
Anthony Poerio (adp59)

# Project Notes
- All code for the project is found in two files:
    1.  library.c  ---> all functions, implemented as specified at:
            http://people.cs.pitt.edu/~jmisurda/teaching/cs1550/2167/cs1550-2167-project1.htm
    2.  driver.c   ---> a 'driver' program that shows the required function calls in action,
                        by printing the text from a famous computer science story to the
                        /dev/fb0 frame buffer, with rectangles to mark each line.
                        Also changes the colors every paragraph, to show that the color
                        functionality works as expected

- To run the driver program that I designed, simply type:
    * "gcc driver.c -o driver.exe"
    * "./driver.exe"

- The square.c program also functions as expected, and I can move the square around
the screen, on my testing. To run this program, the same methodology can be used.
    * "gcc square.c -o square.exe"
    * "./square.exe"

- I've included the fix.c program I wrote to help aid development, in case that's of interest,
or can be helpful to you, depending on how you test everything, at least it'll already
be in the folder if needed.

# Dependencies
- I've included a text file which is used by my driver program, called "story_text.txt".
  This file MUST be in the current directory in order to run the driver.
  I also changed the file's structure so that it would work more easily with my program,
  so not *any* text file will work.



