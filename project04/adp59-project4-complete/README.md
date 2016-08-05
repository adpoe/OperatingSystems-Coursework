# CS1550 Project #4 - Anthony Poerio (adp59@pitt.edu)

## PROGRAM ARCHITECTURE
- Structure:  Data stored in .disk file
    * Block 0 = Root
    * Blocks 1 -> 17 = Subdirectories
    * Blocks 18 -> End = Files

- This makes finding free space somewhat simpler, since the the last section of .disk is files, and the first section is directories.


## WORKS
- Make directories, with **mkdir**
- List Directories and Files, with **ls**
- Write to and create files with **echo** "my data" > example.txt
- Read from files with **cat** myfile.txt
- Open and view files with **nano** myfile.txt
- Append to a file with **echo** >> " append data" > example.txt


## DOES NOT WORK
- Write to a file with nano
- Write to a file which would take up more than 1 block on .disk
- Read from a file which would take up more than 1 block on .disk


## ISSUES AND NOTES
- Not entirely sure how READ is supposed to work. It calls itself recursively, but it is not entirely clear how it works in fuse.
    * I hacked through this function as best I could, and it works on my testing, but I had to to do some tricks to pull it off.
    * For instance, I know that NANO is opening a file when: 1) offset > size; 2) strlen(buf) == 1; 3) buf[0] == 'x'
        + Why is this?  I do not know. But after staring at traces of the program for hours, it works.
        + In the future, would be great to have some more clear direction about what FUSE is doing under the hood. I don't think that'd make the project easier,
        but it would help students understand what the end goal is more clearly, so we can work towards it. Therefore, I believe it would be both justifiable and beneficial.

- MKNOD is almost never being called on my traces? Again, it is implemented and I can make, write to and read files succesfully, but I would love to gain more understanding
of the specific use-case for this function in FUSE.

- WRITE has two paths. One for appending, and one for when the file does not yet exist. There is code duplication between these two paths, and therefore it is not ideal,
but it works.

- I have found an edge case where sometimes: 1) append does not work on files of a certain size. Please try at least 2 files if the first append tells you file is too large

- Sometimes the **very first** cat you to to READ a file doesn't work. Please just do a second cat if this happens. Thank you.


### Thanks for your time
And good luck in the future here at Pitt.
-Tony
