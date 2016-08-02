/*
	FUSE: Filesystem in Userspace
	Copyright (C) 2001-2007  Miklos Szeredi <miklos@szeredi.hu>

	This program can be distributed under the terms of the GNU GPL.
	See the file COPYING.
*/

#define	FUSE_USE_VERSION 26

#include <fuse.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <fcntl.h>

//size of a disk block
#define	BLOCK_SIZE 512

//we'll use 8.3 filenames
#define	MAX_FILENAME 8
#define	MAX_EXTENSION 3

//How many files can there be in one directory?
#define MAX_FILES_IN_DIR (BLOCK_SIZE - sizeof(int)) / ((MAX_FILENAME + 1) + (MAX_EXTENSION + 1) + sizeof(size_t) + sizeof(long))

////////////////////
///// STRUCTS //////
////////////////////

//The attribute packed means to not align these things
struct cs1550_directory_entry
{
	int nFiles;	//How many files are in this directory.
				//Needs to be less than MAX_FILES_IN_DIR

	struct cs1550_file_directory
	{
		char fname[MAX_FILENAME + 1];	//filename (plus space for nul)
		char fext[MAX_EXTENSION + 1];	//extension (plus space for nul)
		size_t fsize;					//file size
		long nStartBlock;				//where the first block is on disk
	} __attribute__((packed)) files[MAX_FILES_IN_DIR];	//There is an array of these

	//This is some space to get this to be exactly the size of the disk block.
	//Don't use it for anything.
	char padding[BLOCK_SIZE - MAX_FILES_IN_DIR * sizeof(struct cs1550_file_directory) - sizeof(int)];
} ;

typedef struct cs1550_root_directory cs1550_root_directory;

#define MAX_DIRS_IN_ROOT (BLOCK_SIZE - sizeof(int)) / ((MAX_FILENAME + 1) + sizeof(long))

struct cs1550_root_directory
{
	int nDirectories;	//How many subdirectories are in the root
						//Needs to be less than MAX_DIRS_IN_ROOT
	struct cs1550_directory
	{
		char dname[MAX_FILENAME + 1];	//directory name (plus space for nul)
		long nStartBlock;				//where the directory block is on disk
	} __attribute__((packed)) directories[MAX_DIRS_IN_ROOT];	//There is an array of these

	//This is some space to get this to be exactly the size of the disk block.
	//Don't use it for anything.
	char padding[BLOCK_SIZE - MAX_DIRS_IN_ROOT * sizeof(struct cs1550_directory) - sizeof(int)];
};


typedef struct cs1550_directory_entry cs1550_directory_entry;

//How much data can one block hold?
#define	MAX_DATA_IN_BLOCK (BLOCK_SIZE)

struct cs1550_disk_block
{
	//All of the space in the block can be used for actual data
	//storage.
	char data[MAX_DATA_IN_BLOCK];
};

typedef struct cs1550_disk_block cs1550_disk_block;


// Data Map
struct cs1550_bitmap {
    char blocks[MAX_DATA_IN_BLOCK];
    // block 0 = root
    // blocks 1 -> 17 = directories
    // blocks 18 -> end-1 = files
    // LAST Block - the data map
};

typedef struct cs1550_data_map cs1550_data_map;

////////////////////
//// PROTOTYPES ////
////////////////////
void get_filepath(const char *filepath, char *DIR_name, char *FILE_name, char *FILE_extension);
long get_subdirectory_starting_block(char *DIR_name);
long* get_file_starting_block(const char *subdir_name, const char *extension, long subdir_offset);
int create_directory(char *DIR_name);
long find_next_free_directory_starting_block(void);
cs1550_directory_entry* get_subdirectory_struct(long subdir_block_offset);
cs1550_root_directory* get_root_directory_struct(void);
cs1550_disk_block* get_disk_block(long block_offset);
int write_to_root_directory_on_disk(cs1550_root_directory *updated_root);
int write_to_subdirectory_on_disk(long block_offset, cs1550_directory_entry *updated_directory);
int write_to_file_on_disk(long disk_offset, cs1550_disk_block *updated_disk_block);
long find_next_free_file_block(void);

/////////////////////////////////
/////// HELPER FUNCTIONS ////////
/////////////////////////////////

/*
 * Take a filepath as a string, and three strings initialized to be the length,
 * and fill those strings with the proper tokens in the filepath: DIR_name, FILE_name, FILE_extension
 * We modify the other strings in place, pulling them out of the filepath with the sscanf regular expression
 * provided in the assignment prompt. Making this its own function, in order to abstract away that process and simplify
 */
void get_filepath(const char *filepath, char *DIR_name, char *FILE_name, char *FILE_extension) {
    // Need to ensure all strings have nothing but zeros in them to start
    memset(DIR_name, 0, (MAX_FILENAME + 1) );
    memset(FILE_name, 0, (MAX_FILENAME + 1) );
    memset(FILE_extension, 0, (MAX_EXTENSION + 1) );

    // grab the input and fill each string, using our sscanf
    sscanf(filepath, "/%[^/]/%[^.].%s", DIR_name, FILE_name, FILE_extension);

    // ensure that the last possible value in the string is still a null terminator
    // prevent overrun
    DIR_name[ (MAX_FILENAME + 1) ] = '\0';
    FILE_name[ (MAX_FILENAME + 1) ] = '\0';
    FILE_extension[ (MAX_EXTENSION + 1) ] = '\0';
}

/*
 * Open .disk and return the first byte of the subdirectory, if it is found, based on filename
 * If not found, return -1, to indicate ERROR
 */
long get_subdirectory_starting_block(char *DIR_name) {
    // Open the .disk file, get a pointer to it
    FILE *disk_file_ptr = fopen(".disk", "rb");
    // handle error
    if (disk_file_ptr == NULL) {
        perror("Could not open .disk file. Please ensure it is in the current directory");
    }

    /* search through root directory for the directory we want... */

    // First, read in the contents of the root directory struct from its binary form
    fseek(disk_file_ptr, SEEK_SET, 0); // make sure we read from BEGINNING, since that's where the root is
    cs1550_root_directory ROOT_dir;    // define a root directory struct

    // handle error, if the root directory isn't one full block size for any reason
   if (BLOCK_SIZE != fread(&ROOT_dir, 1, BLOCK_SIZE, disk_file_ptr)) {
        perror("GET_SUBDIR_STARTING_BLOCK: root directory wasn't loaded in currently when trying to parse the .disk file");
    }

    // now, iterate through and compare the directory found in .disk, to the one we're looking for
    long starting_block = -1;
    int i;
    for (i=0; i < ROOT_dir.nDirectories; i++) {
        // first, perform the comparison
        int comparison_result = strcmp(DIR_name, ROOT_dir.directories[i].dname);
        // then check for a match
        if (0 == comparison_result)  {
            // and if found, de-reference the starting block so we can return it,
            // then break out of for-loop
            starting_block = ROOT_dir.directories[i].nStartBlock;
            break;
        }
    }

    // close the .disk file, once we're done
    fclose(disk_file_ptr);

    // return what we've found
    return starting_block;

}

/*
 *  Get file starting block within a subdirectory
 */
long* get_file_starting_block(const char *subdir_name, const char *extension, long subdir_offset) {
    // declare our return value array
    long *return_vals_array = malloc(2*sizeof(long));

    // Open the .disk file, get a pointer to it
    FILE *disk_file_ptr = fopen(".disk", "rb");
    // handle error
    if (disk_file_ptr == NULL) {
        perror("Could not open .disk file. Please ensure it is in the current directory");
    }

    // seek to the location in .disk where our directory is stored, using the subdir_offset
    long offset = BLOCK_SIZE * subdir_offset; // how many blocks to offfset by
    fseek(disk_file_ptr, offset, SEEK_SET);

    // read in the directory's data from our opened file, and handle error if needed
    cs1550_directory_entry SUB_directory;
    // check that it equals block size to ensure there wasn't an error reading the data,
    // there was actually something there, and we got all we expected
    if (BLOCK_SIZE != fread(&SUB_directory, 1, BLOCK_SIZE, disk_file_ptr)) {
        perror("Could not read in subdirectory entry from the .disk file");
    }

    // now, iterate through and compare the directory found in .disk, to the one we're looking for
    long starting_block = -1;
    int i;
    for (i=0; i < SUB_directory.nFiles; i++) {
        // first, perform the comparison
        int FILENAME_comparison_result = strcmp(subdir_name, SUB_directory.files[i].fname);
        int EXTENSION_comparison_result = strcmp(extension, SUB_directory.files[i].fext);
        // then check for a match
        if (0 == FILENAME_comparison_result && 0 == EXTENSION_comparison_result)  {
            // and if found, de-reference the starting block so we can return it,
            // then break out of for-loop
            starting_block = SUB_directory.files[i].nStartBlock; // we return the iteration number, since that is index within our subdirectory where there was a match
            return_vals_array[0] = starting_block;
            return_vals_array[1] = SUB_directory.files[i].fsize;
            break;
        }
    }

    // close the .disk file, once we're done
    fclose(disk_file_ptr);

    // return the starting block we've found, and the filesize
    return return_vals_array;
}

/*
 * Find next directory block in root struct which is free to use
 */
long find_next_free_directory_starting_block(){
    // might need to initalize the root directory and memset everything to 0's, in dName, so I know what I'm working with a priori
    // open a file, load in root dir... iteratore through and fine where dName[0] = 0
    // this will be our offset, once we find out many blocks from start this location is...
    // iterate by block
    // also check if nDirectories > maxdirsinroot

    printf("FIND_NEXT_FREE_DIR: Inside find next free dir.\n");
    // open the disk file for reading
    // open the .disk file, get a pointer to it
    FILE *disk_file_ptr = fopen(".disk", "rb");
    // handle error
    if (disk_file_ptr == NULL) {
        perror("Could not open .disk file. Please ensure it is in the current directory");
    }

    // seek to the location in .disk where our directory is stored, using the subdir_offset
    // seek to start, which is beginning of root
    fseek(disk_file_ptr, 0, SEEK_SET);

    // define a root directory struct for us to use
    cs1550_root_directory ROOT_dir;

    // handle error, if the root directory isn't one full block size for any reason
    if (BLOCK_SIZE != fread(&ROOT_dir, 1, BLOCK_SIZE, disk_file_ptr)) {
        perror("FIND_NEXT_FREE_DIR: root directory wasn't loaded in currently when trying to parse the .disk file");
        return -1;
    }

    if (ROOT_dir.nDirectories >= MAX_DIRS_IN_ROOT) {
        perror("We already have the maximum number of directories in the root. So we cannot create another.");
        return -1;
    }
    // now, iterate through and find the first subdirectory with an empty name
    long starting_block = -1;
    int i;
    for (i=0; i < MAX_DIRS_IN_ROOT; i++) {
        // find the first directory name that starts with a 0, meaning it is un-initialized
        printf("FIND_NEXT_FREE_DIR: Iterating, and at index %d, name is %s \n", i, ROOT_dir.directories[i].dname);
        if (ROOT_dir.directories[i].dname[0] == 0)  {
            // and if found, de-reference the starting block so we can return it,
            starting_block = i; //ROOT_dir.directories[i].nStartBlock; /* >>> this probably needs to just be the index.. it wouldn't be set a priori */
            // starting block is index + 1, because block 0 is the root
            // break out of loop
            break;
        }
    }

    // close the .disk file, once we're done
    fclose(disk_file_ptr);

    // return what we've found
    return starting_block;
}

/*
 * Function to create a new directory in first empty space
 */
int create_directory(char *DIR_name) {
    // need how many bytes to seek to?
    // it's size of block * how many blocks into the root we've iterated
    // and at this location, we create the DIRECTORY struct
    printf("CREATE_DIRECTORY:  Attempting to create\n");
    long subdir_starting_block = find_next_free_directory_starting_block();
    printf("CREATE_DIRECTORY: Result of find_next_free_directory_starting_block() == %ld\n", subdir_starting_block);
    // handle error, if there are no free directories, return -1
    if (subdir_starting_block == -1)
        return -1;

    // open the disk file for writing
    // Open the .disk file, get a pointer to it
    FILE *disk_file_ptr = fopen(".disk", "r+b"); // maybe make this r+?
    // handle error
    if (disk_file_ptr == NULL) {
        perror("CREATE_DIR:  Could not open .disk file. Please ensure it is in the current directory");
    }

    // seek to the location in .disk where our directory is stored, using the subdir_offset
    // seek to start, which is beginning of root
    fseek(disk_file_ptr, 0, SEEK_SET);

    // define a root directory struct for us to use
    cs1550_root_directory ROOT_dir;
    /* >>> DON'T NEED TO READ IN ROOT HERE;; WE'RE **WRITING** TO IT
    // handle error, if the root directory isn't one full block size for any reason
    */
    if (BLOCK_SIZE != fread(&ROOT_dir, 1, BLOCK_SIZE, disk_file_ptr)) {
        perror("CREATE_DIR: root directory wasn't loaded in currently when trying to parse the .disk file");
        return -1;
    }



    // instead, we need to fwrite into the binary on the disk, in the proper location....

    // TODO:   Update data in the root, in block zero, offset by whatever comes before our array...
    //         Update data in the block designated by our index... need to do +1 to get to it, when we multiply by block size, since 0 is the root


    // now index into the proper directory entry in root, and set it's name
    // and it's starting block, for future reference
    // Keeping a 1-to-1 mapping of directory index and starting blocks, this should work out no matter what logic
    int index;
    for (index=0; index < (MAX_FILENAME + 1); index++)
        ROOT_dir.directories[subdir_starting_block].dname[index] = DIR_name[index];
    // need to copy ^^^ with a for-loop because the dname is of different type, it must be char[9]
    ROOT_dir.directories[subdir_starting_block].nStartBlock = subdir_starting_block + 1; // +1 since 0 is root directory
    // also keep track of how many directories we have. add one, since we just created a new dir
    ROOT_dir.nDirectories++;

    // NOW, WRITE OUR UPDATE ROOT DIR BACK TO THE DISK
    fseek(disk_file_ptr, 0, SEEK_SET);
    // write back and handle error
    if (BLOCK_SIZE != fwrite(&ROOT_dir, 1, BLOCK_SIZE, disk_file_ptr)) {
        printf("CREATE_DIR: Error writing ROOT_DIR's data back to the binary .disk file\n");
        return -1;
    }
    // close the .disk file, once we're done
    fclose(disk_file_ptr);

    // if we make it this, far, everything is okay. return 0 to signify this.
    return 0;
}

/*
 * Get a copy of data in the subdirectory struct, so we can see what's in it
 */
cs1550_directory_entry* get_subdirectory_struct(long subdir_block_offset){
    // Open the .disk file, get a pointer to it
    FILE *disk_file_ptr = fopen(".disk", "rb");
    // handle error
    if (disk_file_ptr == NULL) {
        perror("Could not open .disk file. Please ensure it is in the current directory");
    }

    // seek to the location in .disk where our directory is stored, using the subdir_offset
    long offset = BLOCK_SIZE * subdir_block_offset; // how many blocks to offfset by
    fseek(disk_file_ptr, offset, SEEK_SET);

    // read in the directory's data from our opened file, and handle error if needed
    cs1550_directory_entry SUB_directory;
    // check that it equals block size to ensure there wasn't an error reading the data,
    // there was actually something there, and we got all we expected
    if (BLOCK_SIZE != fread(&SUB_directory, 1, BLOCK_SIZE, disk_file_ptr)) {
        perror("Could not read in subdirectory entry from the .disk file");
    }

    // get our ptr to return
    cs1550_directory_entry *subdirectory_ptr = malloc(sizeof(SUB_directory));
    subdirectory_ptr = &SUB_directory;

    // close the file
    fclose(disk_file_ptr);

    return subdirectory_ptr;
}


/*
 * Get a copy of the root directory struct, for us to use
 */
cs1550_root_directory* get_root_directory_struct(){
     // Open the .disk file, get a pointer to it
    FILE *disk_file_ptr = fopen(".disk", "rb");
    // handle error
    if (disk_file_ptr == NULL) {
        perror("Could not open .disk file. Please ensure it is in the current directory");
    }

    /* search through root directory for the directory we want... */

    // First, read in the contents of the root directory struct from its binary form
    fseek(disk_file_ptr, SEEK_SET, 0); // make sure we read from BEGINNING, since that's where the root is
    cs1550_root_directory ROOT_dir;    // define a root directory struct

    // handle error, if the root directory isn't one full block size for any reason
    if (BLOCK_SIZE != fread(&ROOT_dir, 1, BLOCK_SIZE, disk_file_ptr)) {
        perror("root directory wasn't loaded in currently when trying to parse the .disk file");
    }

    // get our ptr to return
    cs1550_root_directory *root_directory_ptr = malloc(sizeof(ROOT_dir));
    root_directory_ptr = &ROOT_dir;

    // close the file
    fclose(disk_file_ptr);

    // return our ptr
    return root_directory_ptr;

}

/*
 * Get a copy of the a disk block, so we can use it's data
 */
cs1550_disk_block* get_disk_block(long disk_offset) {

    // Open the .disk file, get a pointer to it
    FILE *disk_file_ptr = fopen(".disk", "rb");
    // handle error
    if (disk_file_ptr == NULL) {
        perror("GET DISK BLOCK: Could not open .disk file. Please ensure it is in the current directory");
    }

    // seek to the location in .disk where our directory is stored, using the subdir_offset
    long offset = BLOCK_SIZE * disk_offset; // how many blocks to offfset by
    fseek(disk_file_ptr, offset, SEEK_SET);

    // read in the directory's data from our opened file, and handle error if needed
    cs1550_disk_block DISK_block;
    // check that it equals block size to ensure there wasn't an error reading the data,
    // there was actually something there, and we got all we expected
    if (BLOCK_SIZE != fread(&DISK_block, 1, BLOCK_SIZE, disk_file_ptr)) {
        perror("DISK BLOCK: Could not read in DISK BLOCK entry from the .disk file");
    }

    // get our ptr to return
    cs1550_disk_block *disk_block_ptr = malloc(sizeof(DISK_block));
    disk_block_ptr = &DISK_block;

    // close the file
    fclose(disk_file_ptr);

    return disk_block_ptr;
}


/*
 * Write data the root directory on our disk
 * Take all possible values, and if NOT null, use the to write
 * pass in an updated root directory -- and overwrite
 */
int write_to_root_directory_on_disk(cs1550_root_directory *updated_root){
    /* Process */
    // open the .disk file
    // write root at offset 0

    /* Code */
    // open the disk file for writing
    // Open the .disk file, get a pointer to it
    FILE *disk_file_ptr = fopen(".disk", "r+b"); // maybe make this r+?
    // handle error
    if (disk_file_ptr == NULL) {
        perror("CREATE_DIR:  Could not open .disk file. Please ensure it is in the current directory");
    }
    // NOW, WRITE OUR UPDATE ROOT DIR BACK TO THE DISK
    fseek(disk_file_ptr, 0, SEEK_SET);
    // write back and handle error
    if (BLOCK_SIZE != fwrite(&updated_root, 1, BLOCK_SIZE, disk_file_ptr)) {
        printf("WRITE_ROOT_DIR: Error writing ROOT_DIR's data back to the binary .disk file\n");
        return -1;
    }
    // close the .disk file, once we're done
    fclose(disk_file_ptr);

    // return 0 to indicate all is well
    return 0;
}

/*
 * Write data to a subdirectory, need its block index and the data to write
 * pass in an updated subdirectory
 */
int write_to_subdirectory_on_disk(long disk_offset, cs1550_directory_entry *updated_directory){
    /* Process */
    // open .disk file
    // seek to disk offset
    // write the updated directory

    /* Code */
    FILE *disk_file_ptr = fopen(".disk", "r+b"); // r+ to read AND write
    // handle error
    if (disk_file_ptr == NULL) {
        perror("CREATE_DIR:  Could not open .disk file. Please ensure it is in the current directory");
    }

    // seek to the location in .disk where our directory is stored, using the subdir_offset
    long offset = BLOCK_SIZE * disk_offset; // how many blocks to offfset by
    fseek(disk_file_ptr, offset, SEEK_SET);

    // write back and handle error
    if (BLOCK_SIZE != fwrite(&updated_directory, 1, BLOCK_SIZE, disk_file_ptr)) {
        printf("WRITE_SUB_DIR: Error writing SUB_DIR's data back to the binary .disk file\n");
        return -1;
    }
    // close the .disk file, once we're done
    fclose(disk_file_ptr);

    // return 0 to indicate all is well
    return 0;

}

/*
 * Write data to a file on disk. Need a block index and the data to write
 * May also want to support an append. Can do that that with 0/1 variable if necessary.
 */
int write_to_file_on_disk(long disk_offset, cs1550_disk_block *updated_disk_block){
    /* Process */
    // open .disk file
    // seek to disk offset
    // write the updated disk block

    /* Code */
    FILE *disk_file_ptr = fopen(".disk", "r+b"); // r+ to read AND write
    // handle error
    if (disk_file_ptr == NULL) {
        perror("CREATE_DIR:  Could not open .disk file. Please ensure it is in the current directory");
    }

    // seek to the location in .disk where our directory is stored, using the subdir_offset
    long offset = BLOCK_SIZE * disk_offset; // how many blocks to offfset by
    fseek(disk_file_ptr, offset, SEEK_SET);

    // write back and handle error
    if (BLOCK_SIZE != fwrite(&updated_disk_block, 1, BLOCK_SIZE, disk_file_ptr)) {
        printf("WRITE_FILE: Error writing SUB_DIR's data back to the binary .disk file\n");
        return -1;
    }
    // close the .disk file, once we're done
    fclose(disk_file_ptr);

    // return 0 to indicate all is well
    return 0;
}

/*
 * Find the next available location on disk at which we can store a file
 */
long find_next_free_file_block() {
    // print where we are, to trace
    printf("FIND_NEXT_FREE_FILE_BLOCK:  Inside next free file.\n");

    // set the return value to -1, as default, meaning nothing was found
    int next_free_block_offset = -1;

    // open the disk file for reading
    // open the .disk file, get a pointer to it
    FILE *disk_file_ptr = fopen(".disk", "rb");
    // handle error
    if (disk_file_ptr == NULL) {
        perror("Could not open .disk file. Please ensure it is in the current directory");
    }
    // get the file's size
    int file_size_in_bytes;
    fseek(disk_file_ptr, 0, SEEK_END); // seek to end of file
    file_size_in_bytes = ftell(disk_file_ptr);
    fseek(disk_file_ptr, 0, SEEK_SET); // seek back to the beginning of file

    // seek to the location in .disk where our files are stored
    int file_section_start_byte = (1 + MAX_FILES_IN_DIR) * BLOCK_SIZE;
    fseek(disk_file_ptr, file_section_start_byte, SEEK_SET);

    // determine how many file blocks TOTAL, we have to test
    int total_file_blocks = (file_size_in_bytes - file_section_start_byte) / BLOCK_SIZE;

    // define a disk block, "File Entry" for us to use
    cs1550_disk_block DISK_block;

    // SEEK UNTIL WE REACH FILE END, LOADING EACH FILE BLOCK
    int file_index;
    for (file_index=0; file_index < total_file_blocks; file_index++)
    {
        printf("FIND_NEXT_FREE_FILE_LOCATION: Iterating through file section on .disk at iteration %d\n", file_index);

        // LOAD IN THE BLOCK, CHECK IT'S OKAY
        // handle error, if the file block read in isn't one full block size for any reason
        if (BLOCK_SIZE != fread(&DISK_block, 1, BLOCK_SIZE, disk_file_ptr)) {
            perror("FIND_NEXT_FREE_FILE_LOCATION: disk blck was't loaded in correctly when trying to parse the .disk file");
            return -1;
        }

        // THEN CHECK IF THE 0th byte of our data == 0
        if (DISK_block.data[0] == '\0')  {
            // IF YES --> it's free

            printf("FIND_NEXT_FREE_FILE_LOCATION: Found free block at: %ld, strlen of disk.data =%d\n, disk.data string=%s\n", file_index + (1 + MAX_FILES_IN_DIR), (int)strlen(DISK_block.data), DISK_block.data );
            // get our return value, and break out of the loop
            next_free_block_offset = file_index + /*1 +*/ (1 + MAX_FILES_IN_DIR) ;
            break;
        }
        else
        {
            printf("FIND_NEXT_FREE_FILE_LOCATION: MATCH NOT FOUND block at: %ld, strlen of disk.data =%d\n, disk.data string=%s\n", file_index + (1 + MAX_FILES_IN_DIR), (int)strlen(DISK_block.data), DISK_block.data );
        }
        // IF NO --> keep going, it's being used

        // fseek 512 more bytes, so we can test in the next loop iteration
        //fseek(disk_file_ptr, BLOCK_SIZE, SEEK_CUR);
    }
    // ONCE WE REACH END OF FILE, RETURN ERROR, NOTHING IS FREE
    // and if we reach this far, return value will still be -1,
    // it should never have been set in the loop

    // close the .disk file, once we're done
    fclose(disk_file_ptr);

    // return what we've found
    return next_free_block_offset;
}

//////////////////////////////////
///// FILE-SYSTEM OPERATIONS /////
//////////////////////////////////
/*
 * Called whenever the system wants to know the file attributes, including
 * simply whether the file exists or not.
 *
 * man -s 2 stat will show the fields of a stat structure
 */
static int cs1550_getattr(const char *path, struct stat *stbuf)
{
	int res = 0;

	memset(stbuf, 0, sizeof(struct stat));

	//is path the root dir?
	if (strcmp(path, "/") == 0) {
        // if yes, we're good
		stbuf->st_mode = S_IFDIR | 0755;
		stbuf->st_nlink = 2;
	} else {
	    //Check if name is subdirectory

        // allocate strings to hold our file paths
        char DIR_name[MAX_FILENAME + 1];          // +1 for null terminators
        char FILE_name[MAX_FILENAME + 1];
        char FILE_extension[MAX_EXTENSION + 1];

        // fill our strings with the proper values, using the sscanf provided by Dr. Misurda
        get_filepath(path, DIR_name, FILE_name, FILE_extension);

        // coming out of the last function call, our three strings will now be tokenized to hold
        // their respective elements of the file's path
        // So, next, we we need to determine if these tokens correspond to a valid subdirectory
        long subdir_starting_block = get_subdirectory_starting_block(DIR_name);
        // if the starting block isn't -1, then we've found the directory we're looking for
        // SUBDIRECTORY FOUND
        if (subdir_starting_block != -1) {
            // Check if we are looking for a file name WITHIN that directory, or the directory itself
            if ( '\0' == FILE_name[0] ) {
                // if the first value in our filename is a null, then there's no filename, return the directory
            	stbuf->st_mode = S_IFDIR | 0755;
	    	    stbuf->st_nlink = 2;
     	    	res = 0; //no error
            } else {
                // otherwise, we need to find a file within this directory
                long *return_info = get_file_starting_block(DIR_name, FILE_extension, subdir_starting_block);

                long file_offset = return_info[0];
                long file_size = return_info[1];
                // if file is found, then file_offset will NOT be a -1
                if (file_offset != -1) {
                    //regular file, probably want to be read and write
	    	        stbuf->st_mode = S_IFREG | 0666;
	    	        stbuf->st_nlink = 1; //file links
	    	        stbuf->st_size = file_size;  //file size - make sure you replace with real size!
	    	        res = 0; // no error
                } else {
                    // otherwise, there's an error
                    res = -ENOENT;
                } // end-if for checking if we found a file INSIDE the directory, when FILENAME was expected
            } // end-if for SUB_DIRECTORY FOUND, SEARCH FILENAME
        } // end-if for SUB_DIRECTORY FOUND
        else {
            // If we hit this else, then SUB_DIRECTORY was NOT found
		    //Else return that path doesn't exist
		    res = -ENOENT;
        }
    }
	return res;
}

/*
 * Called whenever the contents of a directory are desired. Could be from an 'ls'
 * or could even be when a user hits TAB to do autocompletion
 */
static int cs1550_readdir(const char *path, void *buf, fuse_fill_dir_t filler,
			 off_t offset, struct fuse_file_info *fi)
{
	//Since we're building with -Wall (all warnings reported) we need
	//to "use" every parameter, so let's just cast them to void to
	//satisfy the compiler
	(void) offset;
	(void) fi;
          /* TEST IF FILLER ACTUALLY WORKS
            filler(buf, ".", NULL, 0);
            filler(buf, "..", NULL, 0);
            return 0;

            END TEST */

    printf("READDIR CALLED\n");
	// First, check if we are in the root.
    // In this case, we need to list all subdirectories, under root
    // LIST SUB_DIRS UNDER ROOT
    if (strcmp(path, "/") == 0)
    {
        filler(buf, ".", NULL, 0);
	    filler(buf, "..", NULL, 0);

        // next, get all subdirectories that are valid in the root data structure,
        // and pass their names to the filler function
        FILE *disk_file_ptr = fopen(".disk", "rb");
        // handle read error on fopen
        if (NULL == disk_file_ptr) {
            perror("Could not open .disk file, in READDIR syscall");
            return -ENOENT;
        }

        // get the data we've stored on disk in root dir, put it in our root_directory struct, so we can work with it
        cs1550_root_directory ROOT_dir;
        // handle fread error, if data is corrupted or non-existent
        if ( BLOCK_SIZE != fread(&ROOT_dir, 1, BLOCK_SIZE, disk_file_ptr) ) {
           perror("Disk file exists, but could not read in data for the root directory. In READDIR syscall.");
           return -ENOENT;
        }

        printf("READDIR:  ROOT SELECTED AND VALID\n");
        // iterate through the root and grab all valid names, fill them in the buffer
        int directory_index;
        for (directory_index=0; directory_index<MAX_DIRS_IN_ROOT; directory_index++) {
            printf("ITERATING THROUGH DIRS IN READDIR\n");
            // check if name is valid. if it's valid, first letter will not be a 0
            if (ROOT_dir.directories[directory_index].dname[0] != 0) {
                printf("READDIR:  Found a valid directory name");
                // if we're here, name is valid, so put it in the filler, buffer
                filler(buf, ROOT_dir.directories[directory_index].dname, NULL, 0);
            } // end-if, checking if names are valid
        } // end for-loop, iterating through directories in the root

        // close the file
        fclose(disk_file_ptr);
    } // end-if, if where user has asked to list directories under root

    // ELSE: WE ARE TRYING TO LIST **FILES** IN A SUBDIRECTORY
    else
    {
        printf("READDIR: Subdirectory selected\n");
        // get our directory name, all the elements of the ath
        char DIR_name[MAX_FILENAME + 1];
        char FILE_name[MAX_FILENAME + 1];
        char FILE_extension[MAX_EXTENSION + 1];

        // fill our strings with proper values
        get_filepath(path, DIR_name, FILE_name, FILE_extension);

        // CHECK IF A FILE EXTENSION WAS PASSED IN
        // if so, we aren't looking for a directory to list we need to return an error
        if (FILE_extension[0] != 0){
            perror("File extension passed into READDIR, which is for listing contents of a DIR, not contents of a file.");
            return -ENOENT;
        }

        // CHECK IF FILE NAME WAS PASSED IN
        if (FILE_name[0] != 0) {
            perror("File NAME  passed into READDIR, which is for listing contents of a DIR, not contents of a file.");
            return -ENOENT;
        }

        // ELSE, we are looking for a directory, as expected.
        // And, we know it's not the root. So let's look for the dir name, and see if it's valid
        long subdir_starting_block = get_subdirectory_starting_block(DIR_name);
        // handle error; if our subdirectory wasn't found, the previous function call
        // will return a -1
        if (-1 == subdir_starting_block) {
            perror("READDIR: Subdirectory not found");
            return -ENOENT;
        }

        // if we make it here, the directory is valid, so let's find it's block on the disk and grab all the file names it is storing
//        cs1550_directory_entry *subdirectory = get_subdirectory_struct(subdir_starting_block);
//        int num_files_in_directory = subdirectory->nFiles;
        // at a minimum, we'll show . and .. as valid files, since they are in EVERY directory
        filler(buf, ".", NULL, 0);
	    filler(buf, "..", NULL, 0);
// open directory as a struct pointer, if no error
    //cs1550_directory_entry subdirectory = get_subdirectory_struct(subdir_starting_block);
    // Open the .disk file, get a pointer to it
    FILE *disk_file_ptr = fopen(".disk", "rb");
    // handle error
    if (disk_file_ptr == NULL) {
        perror("Could not open .disk file. Please ensure it is in the current directory");
    }

    // seek to the location in .disk where our directory is stored, using the subdir_offset
    long offset_byte = BLOCK_SIZE * subdir_starting_block; // how many blocks to offfset by
    fseek(disk_file_ptr, offset_byte, SEEK_SET);

    // read in the directory's data from our opened file, and handle error if needed
    cs1550_directory_entry SUB_directory;
    // check that it equals block size to ensure there wasn't an error reading the data,
    // there was actually something there, and we got all we expected
    if (BLOCK_SIZE != fread(&SUB_directory, 1, BLOCK_SIZE, disk_file_ptr)) {
        perror("Could not read in subdirectory entry from the .disk file");
    }

    // get our ptr to return
    cs1550_directory_entry *subdirectory_ptr = malloc(sizeof(SUB_directory));
    subdirectory_ptr = &SUB_directory;

    // close the file
    fclose(disk_file_ptr);

   // return subdirectory_ptr;
    printf("CS1550_WRITE: SUBDIRECTORY LOADED --> StartBlock is: %ld \n", subdir_starting_block);
    cs1550_directory_entry subdirectory;
    memset(&subdirectory, 0, sizeof(cs1550_directory_entry));
    subdirectory = SUB_directory;
    int num_files_in_directory = subdirectory.nFiles;
        // check if we have any files to list in the chosen directory
        if (num_files_in_directory == 0) {
            // if no other files to list, just return
            printf("READDIR: 0 Files found in the sub_directory\n");
            return 0;
        }

        // othewrwise, we need to fill the buffer with our remaining files
        int file_index;
        for (file_index=0; file_index < MAX_FILES_IN_DIR; file_index++) {
            printf("ITERATING THROUGH SUB_DIR FILES IN READDIR\n");
            // check if each file is valid
            //cs1550_file_directory current_file = subdirectory->files[file_index];
            // if the first letter of fname is NOT 0, the file has a name
            if (subdirectory.files[file_index].fname[0] != 0
                && subdirectory.files[file_index].fsize > 0
                && subdirectory.files[file_index].fsize <= 512 /*nStartBlock > MAX_DIRS_IN_ROOT*/) {
                printf("READDIR:  File found in the directory, fname: %s;; fext: %s, fsize: %d, nStartBlock: %ld\n", subdirectory.files[file_index].fname, subdirectory.files[file_index].fext, (int)subdirectory.files[file_index].fsize, subdirectory.files[file_index].nStartBlock);
                // concat the file name, and add it to our buffer
                char file_name[MAX_FILENAME + MAX_EXTENSION + 2];
                strcpy(file_name, subdirectory.files[file_index].fname);
                strcat(file_name, ".");
                strcat(file_name, subdirectory.files[file_index].fext);
                filler(buf, file_name, NULL, 0);
            } // end-if, for checking if file name is present
        } // end-for, for iterating through the list of all possible files in the subdirectory

    } // end-else, for trying to list files in a SUB_DIRECTORY
	return 0;
}

/*
 * Creates a directory. We can ignore mode since we're not dealing with
 * permissions, as long as getattr returns appropriate ones for us.
 */
static int cs1550_mkdir(const char *path, mode_t mode)
{
    printf("MKDIR:  call to cs1550_mkdir, path is %s\n", path);
    // cast the path an mode to void...
	(void) path;
	(void) mode;

    // get our file path, to start
    // allocate strings to hold our file paths
    char DIR_name[MAX_FILENAME + 1];          // +1 for null terminators
    char FILE_name[MAX_FILENAME + 1];
    char FILE_extension[MAX_EXTENSION + 1];

    // fill our strings with the proper values, using the sscanf provided by Dr. Misurda
    get_filepath(path, DIR_name, FILE_name, FILE_extension);

    // next, handle errors
    // CHECK IF NAME TOO LONG
    int dir_name_length = strlen(DIR_name);
    if (dir_name_length > MAX_FILENAME) {
        perror("Directory name too long. Max length is 8 characters");
        return -ENAMETOOLONG;
    }

    // CHECK IF DIRECTORY ALREADY EXISTS
    if (get_subdirectory_starting_block(DIR_name) != -1) {
        // if this return value isn't -1, then we've found the directory,
        // and therefore it already exists, so we cannot create it
        perror("Directory name already exists under root");
        return -EEXIST;
    }

    // CHECK IF DIRECTORY IS NOT DIRECTLY UNDER ROOT
    int pathLen = strlen(path);
    int index;
    int count = 0;
    // count how many /'s we have
    for (index=0; index < pathLen; index++) {
        if (path[index] == '/') {
            count++;
        }
    }
    // count must be 1, otherwise we aren't making this directory under the root!
    if (count != 1) {
        perror("New directory name must be made under root.");
        return -EPERM;
    }

    printf("MKDIR: TRYING TO CREATE DIRECTORY\n");
    // IF WE MAKE IT THIS FAR, CREATE THE DIRECTORY
    // handle error
    int directory_return_value = create_directory(DIR_name);
    if (directory_return_value != 0) {
        printf("MKDIR ERROR: for: dir_name: %s,   return_val: %d\n", DIR_name, directory_return_value);
        perror("Could not create directory");
        return -1;
    }

    // if we make it this far, SUCCCESS. Return 0.
	return 0;
}

/*
 * Removes a directory.
 */
static int cs1550_rmdir(const char *path)
{
	(void) path;
    return 0;
}

/*
 * Does the actual creation of a file. Mode and dev can be ignored.
 *
 */
static int cs1550_mknod(const char *path, mode_t mode, dev_t dev)
{
    printf("CS1550_MKNOD: MKNOD CALLED\n");
    //(void) path; // for now, cast to avoid error
	(void) mode;
	(void) dev;

    // Parse the path
    char DIR_name[MAX_FILENAME + 1];
    char FILE_name[MAX_FILENAME + 1];
    char FILE_extension[MAX_EXTENSION + 1];
        // fill all declared strings above with proper values
    get_filepath(path, DIR_name, FILE_name, FILE_extension);

    printf("CS1550_MKNOD: FILE PATH PARSED\n");
    // CHECK FOR ERRORS
    // Is name length beyond 8.3 chars?
    if (strlen(DIR_name) > 8 || strlen(FILE_extension) > 3) {
        return -ENAMETOOLONG;
    }

    // Are we creating file in the root directory?
    int char_index;
    int slash_counter = 0;
    // check how many slashes we have in our path
    for (char_index = 0; char_index < strlen(path); char_index++) {
        if (path[char_index] == '/')
            slash_counter++;
    }
    printf("MKNOD: SLASH COUNT = %d\n", slash_counter);
    // if slash_counter is < 2, then we're in root
    if (slash_counter < 2)
        return -EPERM;

    // Open the directory entry
    long subdir_starting_block = get_subdirectory_starting_block(DIR_name);
    // Make sure entry is valid, and open it if so
    // handle error
    if (subdir_starting_block == -1) {
        printf("MKNOD: SUB_DIRECTORY NAME IS INVALID  \n");
        return -1;
    }
    // open directory as a struct pointer, if no error
    cs1550_directory_entry *subdirectory = get_subdirectory_struct(subdir_starting_block);
    printf("MKNOD: SUBDIRECTORY LOADED \n");

    // DOES FILE NAME EXIST IN CURRENT DIR?
    int file_index;
    int file_exists = 0;
    // iterate through the file array and check each name
    for (file_index = 0; file_index < subdirectory->nFiles; file_index++)
    {
        if (strcmp(subdirectory->files[file_index].fname, FILE_name) == 0)
        {
            file_exists = 1;
            break;
        }
    }

    // if there was a match, return an error
    if (file_exists == 1)
        return -EEXIST;

    // else, keep on keepin' on.



    // Find the next available file, get the block offset number
    long next_free_file_block = find_next_free_file_block();
        // if NOTHING left, error
    if (next_free_file_block == -1) {
        // error meaning that there's no room left
        printf("MKNOD: No room left for file entries in .disk\n");
        return -1;
    }
    // Updated the directory entry to hold the new block offset number as a file index
        // Increment # of files in the directory entry
    subdirectory->nFiles++;
    int nFiles = subdirectory->nFiles;
        // Add the element to the directories array
        // And set all the values for the directory entry
    // need to make files that are assignable

    // First, assign the file name, char by char
    int i;
    for (i=0; i<9; i++) {
        subdirectory->files[nFiles].fname[i] = FILE_name[i];
    }

    // Then, assign the file ext, char by char
    int j;
    for (j=0; j<9; j++) {
        subdirectory->files[nFiles].fext[j] = FILE_extension[j];
    }

    // now assign size and start block
    subdirectory->files[nFiles].fsize = 0;
    subdirectory->files[nFiles].nStartBlock = next_free_file_block;

    // Finally, WRITE the Subdirectory entry we've updated back to the .disk
    int write_return_value = write_to_subdirectory_on_disk(subdir_starting_block, subdirectory);
    // handle error
    if (write_return_value == -1) {
        printf("MKNOD: Error writing our subdirectory back to disk\n");
        return -1;
    }

    // if we made it this far, everything worked, return 0 to indicate success
    return 0;

}

/*
 * Deletes a file
 */
static int cs1550_unlink(const char *path)
{
    (void) path;

    return 0;
}

/*
 * Read size bytes from file into buf starting from offset
 *
 */
static int cs1550_read(const char *path, char *buf, size_t size, off_t offset,
			  struct fuse_file_info *fi)
{
	(void) buf;
	(void) offset;
	(void) fi;
	(void) path;

    int file_size;
    memset(&buf, '\0', strlen(buf));

	//check to make sure path exists
    char DIR_name[MAX_FILENAME + 1];
    char FILE_name[MAX_FILENAME + 1];
    char FILE_extension[MAX_EXTENSION + 1];
    get_filepath(path, DIR_name, FILE_name, FILE_extension);

    // check for root
    if (strcmp(path, "/") == 0) return -EISDIR;

    // check if path is a directory, not a file
    if (strlen(FILE_name) < 1) return -EISDIR;

    // check if file exists
    int directory_offset = get_subdirectory_starting_block(DIR_name);
    long *file_info = get_file_starting_block(FILE_name, FILE_extension, directory_offset);
    // check if file not found
    if (file_info[0] == -1) {
       printf("CS1550_READ:: FILE NOT FOUND, file size will be 0");
       file_size = 0;
    } else {
        file_size = file_info[1];
    }

	//check that size is > 0
    if ( size < 1) {
        return -EISDIR;

    }
	//check that offset is <= to the file size
    // NOTE:  Looks like this call is used recursively, in some way, and this is the end condition.
    //        It must ALWAYS be true at the END of a READ
    if (offset > file_size) {
        printf("CS1550_READ:: Offset is > file size. Offset=%d, FileSize=%d\n\n", (int)offset, file_size );
        return 0;
    }

	//read in data
    // open the directory up, read in the file's data
    //cs1550_disk_block *disk_block = get_disk_block(file_info[0]);

    cs1550_disk_block disk_block;
    memset(&disk_block, '\0', sizeof(cs1550_disk_block));


    /* OPEN DISK FILE AND GET ITS DATA */
    // Open the .disk file, get a pointer to it
    FILE *disk_file_ptr = fopen(".disk", "rb");
    // handle error
    if (disk_file_ptr == NULL) {
        perror("GET DISK BLOCK: Could not open .disk file. Please ensure it is in the current directory");
    }

    // seek to the location in .disk where our directory is stored, using the subdir_offset
    long offset_index = BLOCK_SIZE * file_info[0]; // how many blocks to offfset by
    fseek(disk_file_ptr, offset_index, SEEK_SET);

    // read in the directory's data from our opened file, and handle error if needed
    cs1550_disk_block DISK_block;
    // check that it equals block size to ensure there wasn't an error reading the data,
    // there was actually something there, and we got all we expected
    /*
    if (BLOCK_SIZE != fread(&DISK_block, 1, BLOCK_SIZE, disk_file_ptr)) {
        perror("DISK BLOCK: Could not read in DISK BLOCK entry from the .disk file");
    }
    */

    if (BLOCK_SIZE != fread(&DISK_block, 1, BLOCK_SIZE, disk_file_ptr)) {
        perror("DISK BLOCK: Could not read in DISK BLOCK entry from the .disk file");
    }
    // get our ptr to return
    //cs1550_disk_block *disk_block_ptr = malloc(sizeof(DISK_block));
    //disk_block_ptr = &DISK_block;

    disk_block = DISK_block;
    // close the file
    fclose(disk_file_ptr);
    /* END GRAB DATA FROM .DISK */

    int index;
    printf("CS1550_READ:: Iterating and filling buffer. Size=%d, File_Size=%d\n, offset=%d", (int)size, file_size, (int)offset);
    for (index=0; index < file_size; index++) {
        buf[index] = disk_block.data[index];
        printf("CS1550_READ: buf[%d]=%c\n", index, buf[index]);
    }

    // null terminate our buffer
    buf[index] = '\0';
    printf("CS1550_READ:: BUFFER IS FILLED AND SAYS: %s\n", buf);
    printf("CS1550_READ:: LENGTH OF BUFFER IS: %d \n", (int)strlen(buf));


    //set size and return, or error
	//size = file_info[1];

	return size;
}

/*
 * Write size bytes from buf into file starting from offset
 * REFACTOR TO MAKE THIS SIMPLER.
 * Use the part that works for the append section as well.
 * Control flow needs to be re-worked. Everything gets opened already.
 * Don't re-invent the wheel. Just get it working. Then you're pretty much done.
 */
static int cs1550_write(const char *path, const char *buf, size_t size,
			  off_t offset, struct fuse_file_info *fi)
{
	(void) buf;
	(void) offset;
	(void) fi;
	(void) path;

		//check to make sure path exists
    // get values for all parts of path
    char DIR_name[MAX_FILENAME + 1];
    char FILE_name[MAX_FILENAME + 1];
    char FILE_extension[MAX_EXTENSION + 1];
    get_filepath(path, DIR_name, FILE_name, FILE_extension);

    printf("WRITE: CHECKING IF PATH EXISTS\n");
    // Open the directory entry
    long subdir_starting_block = get_subdirectory_starting_block(DIR_name);
    // Make sure entry is valid, and open it if so
    // handle error
    if (subdir_starting_block == -1) {
        printf("CS1550_WRITE: SUB_DIRECTORY NAME IS INVALID  \n");
        return -1;
    }
    // open directory as a struct pointer, if no error
    //cs1550_directory_entry subdirectory = get_subdirectory_struct(subdir_starting_block);
    // Open the .disk file, get a pointer to it
    FILE *disk_file_ptr = fopen(".disk", "rb");
    // handle error
    if (disk_file_ptr == NULL) {
        perror("Could not open .disk file. Please ensure it is in the current directory");
    }

    // seek to the location in .disk where our directory is stored, using the subdir_offset
    long offset_byte = BLOCK_SIZE * subdir_starting_block; // how many blocks to offfset by
    fseek(disk_file_ptr, offset_byte, SEEK_SET);

    // read in the directory's data from our opened file, and handle error if needed
    cs1550_directory_entry SUB_directory;
    // check that it equals block size to ensure there wasn't an error reading the data,
    // there was actually something there, and we got all we expected
    if (BLOCK_SIZE != fread(&SUB_directory, 1, BLOCK_SIZE, disk_file_ptr)) {
        perror("Could not read in subdirectory entry from the .disk file");
    }

    // get our ptr to return
    cs1550_directory_entry *subdirectory_ptr = malloc(sizeof(SUB_directory));
    subdirectory_ptr = &SUB_directory;

    // close the file
    fclose(disk_file_ptr);

   // return subdirectory_ptr;

    printf("CS1550_WRITE: SUBDIRECTORY LOADED --> StartBlock is: %ld \n", subdir_starting_block);
    cs1550_directory_entry subdirectory;
    memset(&subdirectory, 0, sizeof(cs1550_directory_entry));
    subdirectory = SUB_directory;

    // DOES FILE NAME EXIST IN CURRENT DIR?
    int file_index;
    int file_found = -1;
    // iterate through the file array and check each name
    for (file_index = 0; file_index < MAX_FILES_IN_DIR; file_index++)
    {

        printf("CS1550_WRITE:  ITERATING THROUGH FILES IN DIRECTORY TO FIND IF IT EXISTS\n");
        printf("\tDATA:: \tFNAME = %s\n \tFEXT = %s\n \tfsize = %d\n, \tnStartBlock=%ld\n", subdirectory.files[file_index].fname, subdirectory.files[file_index].fext, (int)subdirectory.files[file_index].fsize, subdirectory.files[file_index].nStartBlock);

        if (strcmp(subdirectory.files[file_index].fname, FILE_name) == 0
                && 0 == strcmp(subdirectory.files[file_index].fext, FILE_extension))
        {
            printf("CS1550_WRITE:  MATCH FOUND\n");
            // we break and keep the file index where we had a match
            file_found = 1;
            break;
        }
    }

    printf("CS1550_WRITE:  FILE INDEX=%d\n", file_index);
    // NO MATCH, MAKE A NEW FILE
    if (file_found == -1)
    {
        // if we didn't find anything, set the file index back to 0
        file_index = 0;
        printf("CS1550_WRITE:  FILE DOES NOT EXIST!!!\n");

        // if the file isn't found, put it at location nFiles++ in the directory;
        // if nFiles = 0, put it at 0
        // get the index first
        // then, increment the number of files
        if (subdirectory.nFiles < 0 || subdirectory.nFiles > 17)
        {
            // if the nFiles in our subdirectory wasn't initalized for any reason, then we need to make sure we don't get a CRAZY number
            // and if we have a crazy number, it really should be 0 at this point
            subdirectory.nFiles = 0;
        }
        subdirectory.nFiles++;
        int new_file_index = subdirectory.nFiles - 1;
        file_index = new_file_index;
        printf("CS1550_WRITE:  Increment Number of Files by 1, file index=%d\n", new_file_index);
        // get a reference to a file struct
        struct cs1550_file_directory new_file;
        printf("CS1550_WRITE:  Allocated a New struct\n");
        // change its attributes to hold the values for the NEW file
        strcpy(new_file.fname, FILE_name);
        printf("CS1550_WRITE:  STRCPY'd File name, and it is %s\n", new_file.fname);
        strcpy(new_file.fext, FILE_extension);
        printf("CS1550_WRITE:  STRCPY'd File ext, and it is: %s\n", new_file.fext);
        new_file.fsize = 0;
        printf("CS1550_WRITE:  set file size, and it is %d\n", 0);
        new_file.nStartBlock = find_next_free_file_block();
        printf("CS1550_WRITE:  Set nStartBlock and it = %ld\n", new_file.nStartBlock);
        // write our updated values BACK to the subdirectory struct, so we can use them later
        subdirectory.files[new_file_index] = new_file;


        printf("CS1550_WRITE:  Allocated ALL info for NEW FILE\n");

        // SET EVERYTHING MANUALLY....
        // copy fname
        int i=0;
        for (i=0; i < MAX_FILENAME; i++) {
            subdirectory.files[new_file_index].fname[i] = new_file.fname[i];
        }
        subdirectory.files[new_file_index].fname[i] = '\0';

        // copy fext
        int j=0;
        for (j=0; j < MAX_EXTENSION; j++) {
            subdirectory.files[new_file_index].fname[j] = new_file.fname[j];
        }
        subdirectory.files[new_file_index].fname[j] = '\0';

        //strcpy(subdirectory.files[new_file_index].fname,  new_file.fname);
        //strcpy(subdirectory.files[new_file_index].fext, new_file.fext);
        subdirectory.files[new_file_index].fsize = new_file.fsize;
        subdirectory.files[new_file_index].nStartBlock = new_file.nStartBlock;
        // DOES THIS EVER GET SET?

        /*
         * PRINT THE FILE INFORMATION, SO WE KNOW WHAT'S GOING ON
         */
        size_t file_size_check = subdirectory.files[file_index].fsize;
        long file_start_block = subdirectory.files[file_index].nStartBlock;
        printf("CS1550_WRITE: \tFNAME = %s\n \tFEXT = %s\n \tfsize = %d\n, \tnStartBlock=%ld\n", subdirectory.files[file_index].fname, subdirectory.files[file_index].fext, (int)file_size_check, file_start_block);
        /* END PRINTINT FILE INFO */


        // DUPLICATED TO SEE IF IT WORKS
        cs1550_disk_block disk_block;
        memset(&disk_block, 0, sizeof(cs1550_disk_block)); // becasue i get weird values sometimes
        int file_size = subdirectory.files[file_index].fsize;
        int overage = file_size - offset;
        printf("CS1550_WRITE: FILE OFFSET = %d\n", (int)offset);

        // if overage < 0 ---> REJECT
        if (overage <= 0)
        {
            // okay
            // go get a reference to the file struct so we can access its data
            printf("CS1550_WRITE:: Getting starting block from %ld\n", file_start_block);
            //disk_block = get_disk_block(file_start_block);

    // Open the .disk file, get a pointer to it
    FILE *disk_file_ptr = fopen(".disk", "rb");
    // handle error
    if (disk_file_ptr == NULL) {
        perror("GET DISK BLOCK: Could not open .disk file. Please ensure it is in the current directory");
    }

    // seek to the location in .disk where our directory is stored, using the subdir_offset
    long offset_index = BLOCK_SIZE * file_start_block; // how many blocks to offfset by
    fseek(disk_file_ptr, offset_index, SEEK_SET);

    // read in the directory's data from our opened file, and handle error if needed
    cs1550_disk_block DISK_block;
    // check that it equals block size to ensure there wasn't an error reading the data,
    // there was actually something there, and we got all we expected
    if (BLOCK_SIZE != fread(&DISK_block, 1, BLOCK_SIZE, disk_file_ptr)) {
        perror("DISK BLOCK: Could not read in DISK BLOCK entry from the .disk file");
    }

    // get our ptr to return
    //cs1550_disk_block *disk_block_ptr = malloc(sizeof(DISK_block));
    //disk_block_ptr = &DISK_block;

    disk_block = DISK_block;
    // close the file
    fclose(disk_file_ptr);

            // make sure we have a value for the disk block
            //if (NULL == disk_block)
            //{
            //    printf("CS1550_WRITE:: DISK BLOCK IS NULL\n");
            //    return -1;
            //}
            //index into our data segment at this disk block and write however many bytes we need to from the buffer
            int buffer_index;
            for (buffer_index=0; buffer_index < size; buffer_index++)
            {
                disk_block.data[offset + buffer_index] = buf[buffer_index];
                printf("Written to disk block index #%d: %c\n", (int)(offset+buffer_index), disk_block.data[offset + buffer_index]  );
            }
            // make sure data is null terminated
            disk_block.data[offset + buffer_index + 1] = '\0';
            printf("CS1550_WRITE: data_written to disk block: %s\n", disk_block.data);

        // WRITE DATA BACK TO .DISK
        // want to do:  filesize += size - overage

        // SET EVERYTHING MANUALLY....
        strcpy(subdirectory.files[new_file_index].fname,  new_file.fname);
        strcpy(subdirectory.files[new_file_index].fext, new_file.fext);
        subdirectory.files[new_file_index].fsize = new_file.fsize + size - overage;
        subdirectory.files[new_file_index].nStartBlock = new_file.nStartBlock;
        // DOES THIS EVER GET SET?



        // write sub_dir with new file_size back to disk
        //int subdir_write = write_to_subdirectory_on_disk(subdir_starting_block, &subdirectory);


    /* Code */
    disk_file_ptr = fopen(".disk", "r+b"); // r+ to read AND write
    // handle error
    if (disk_file_ptr == NULL) {
        perror("CREATE_DIR:  Could not open .disk file. Please ensure it is in the current directory");
    }

    // seek to the location in .disk where our directory is stored, using the subdir_offset
    long offset_write_subdir = BLOCK_SIZE * subdir_starting_block; // how many blocks to offfset by
    fseek(disk_file_ptr, offset_write_subdir, SEEK_SET);
     // PRINT OUT SUBDIR INFO BEFORE WE WRITE IT
        /*
         * PRINT THE FILE INFORMATION, SO WE KNOW WHAT'S GOING ON
         */
        file_size_check = subdirectory.files[file_index].fsize;
        file_start_block = subdirectory.files[file_index].nStartBlock;
        printf("CS1550_WRITE: file_index=%d, \tFNAME = %s\n \tFEXT = %s\n \tfsize = %d\n, \tnStartBlock=%ld\n, directoryBlock=%ld\n", file_index, subdirectory.files[file_index].fname, subdirectory.files[file_index].fext, (int)file_size_check, file_start_block, subdir_starting_block);
        /* END PRINTINT FILE INFO */

    // write back and handle error
    if (BLOCK_SIZE != fwrite(&subdirectory, 1, BLOCK_SIZE, disk_file_ptr)) {
        printf("WRITE_SUB_DIR: Error writing SUB_DIR's data back to the binary .disk file\n");
        return -1;
    }
    // close the .disk file, once we're done
    fclose(disk_file_ptr);

    /* Code */
    disk_file_ptr = fopen(".disk", "r+b"); // r+ to read AND write
    // handle error
    if (disk_file_ptr == NULL) {
        perror("CREATE_DIR:  Could not open .disk file. Please ensure it is in the current directory");
    }

    // seek to the location in .disk where our directory is stored, using the subdir_offset
    long offset_file_write = BLOCK_SIZE * file_start_block; // how many blocks to offfset by
    fseek(disk_file_ptr, offset_file_write, SEEK_SET);

    // write back and handle error
    if (BLOCK_SIZE != fwrite(&disk_block, 1, BLOCK_SIZE, disk_file_ptr)) {
        printf("WRITE_FILE: Error writing SUB_DIR's data back to the binary .disk file\n");
        return -1;
    }
    // close the .disk file, once we're done
    fclose(disk_file_ptr);


        //set size (should be same as input) and return, or error
        return size;

        // END DUPLICATION

        }
        else
        {
            // bad
            printf("CS1550_WRITE: FILE TOO LARGE FROM CS1550\n");
            return -EFBIG;
        }

    }

    /*
     * APPEND
     */
    // We have: file_index, subdir_starting_bloc
    // ELSE WE ARE DOING AN APPEND
    printf("CS1550_WRITE:  FILE INDEX=%d\n", file_index);
	//check that size is > 0
    if (size < 1) {
        printf("WRITE:   SIZE IS < 1\n");
        return -1;
    }

	//check that offset is <= to the file size
    if (offset > subdirectory.files[file_index].fsize) {
        printf("CS1550_WRITE - Line:%d:  OFFSET IS > FILE SIZE. Offset=%d, FileSize=%d , input size=%d\n", __LINE__, (int)offset, (int)subdirectory.files[file_index].fsize, (int)size);
        offset = subdirectory.files[file_index].fsize - 1;
        //return -EFBIG;
    }



	//write data
    printf("CS1550_WRITE:: Made it to APPEND SECTION of WRITE\n");

    // WRITE DATA
        cs1550_disk_block disk_block;
        memset(&disk_block, 0, sizeof(cs1550_disk_block)); // because i get weird values sometimes
        int file_size = subdirectory.files[file_index].fsize;
        long file_start_block = subdirectory.files[file_index].nStartBlock;
        int overage = file_size - offset;
        printf("CS1550_WRITE: FILE OFFSET = %d\n", (int)offset);

        // if overage < 0 ---> REJECT
        if (overage > 0)
        {
            // okay
            // go get a reference to the file struct so we can access its data
            printf("CS1550_WRITE:: Getting starting block from %ld\n", file_start_block);
            //disk_block = get_disk_block(file_start_block);

    // Open the .disk file, get a pointer to it
    FILE *disk_file_ptr = fopen(".disk", "rb");
    // handle error
    if (disk_file_ptr == NULL) {
        perror("GET DISK BLOCK: Could not open .disk file. Please ensure it is in the current directory");
    }

    // seek to the location in .disk where our directory is stored, using the subdir_offset
    long offset_index = BLOCK_SIZE * file_start_block; // how many blocks to offfset by
    fseek(disk_file_ptr, offset_index, SEEK_SET);

    // read in the directory's data from our opened file, and handle error if needed
    cs1550_disk_block DISK_block;
    // check that it equals block size to ensure there wasn't an error reading the data,
    // there was actually something there, and we got all we expected
    if (BLOCK_SIZE != fread(&DISK_block, 1, BLOCK_SIZE, disk_file_ptr)) {
        perror("DISK BLOCK: Could not read in DISK BLOCK entry from the .disk file");
    }

    disk_block = DISK_block;
    // close the file
    fclose(disk_file_ptr);
            int buffer_index;
            for (buffer_index=0; buffer_index < size; buffer_index++)
            {
                disk_block.data[offset + buffer_index] = buf[buffer_index];
                printf("Written to disk block index #%d: %c\n", (int)(offset+buffer_index), disk_block.data[offset + buffer_index]  );
            }
            // make sure data is null terminated
            disk_block.data[offset + buffer_index + 1] = '\0';
            printf("CS1550_WRITE: data_written to disk block: %s\n", disk_block.data);

        // WRITE DATA BACK TO .DISK
        // want to do:  filesize += size - overage

    /* Code */
    disk_file_ptr = fopen(".disk", "r+b"); // r+ to read AND write
    // handle error
    if (disk_file_ptr == NULL) {
        perror("CREATE_DIR:  Could not open .disk file. Please ensure it is in the current directory");
    }

    // seek to the location in .disk where our directory is stored, using the subdir_offset
    long offset_write_subdir = BLOCK_SIZE * subdir_starting_block; // how many blocks to offfset by
    fseek(disk_file_ptr, offset_write_subdir, SEEK_SET);
     // PRINT OUT SUBDIR INFO BEFORE WE WRITE IT
        /*
         * PRINT THE FILE INFORMATION, SO WE KNOW WHAT'S GOING ON
         */
        int file_size_check = subdirectory.files[file_index].fsize += (size - overage);
        long file_start_block = subdirectory.files[file_index].nStartBlock;
        printf("CS1550_WRITE: file_index=%d, \tFNAME = %s\n \tFEXT = %s\n \tfsize = %d\n, \tnStartBlock=%ld\n, directoryBlock=%ld\n", file_index, subdirectory.files[file_index].fname, subdirectory.files[file_index].fext, (int)file_size_check, file_start_block, subdir_starting_block);
        /* END PRINTINT FILE INFO */

    // write back and handle error
    if (BLOCK_SIZE != fwrite(&subdirectory, 1, BLOCK_SIZE, disk_file_ptr)) {
        printf("WRITE_SUB_DIR: Error writing SUB_DIR's data back to the binary .disk file\n");
        return -1;
    }
    // close the .disk file, once we're done
    fclose(disk_file_ptr);

    /* Code */
    disk_file_ptr = fopen(".disk", "r+b"); // r+ to read AND write
    // handle error
    if (disk_file_ptr == NULL) {
        perror("CREATE_DIR:  Could not open .disk file. Please ensure it is in the current directory");
    }

    // seek to the location in .disk where our directory is stored, using the subdir_offset
    long offset_file_write = BLOCK_SIZE * file_start_block; // how many blocks to offfset by
    fseek(disk_file_ptr, offset_file_write, SEEK_SET);

    // write back and handle error
    if (BLOCK_SIZE != fwrite(&disk_block, 1, BLOCK_SIZE, disk_file_ptr)) {
        printf("WRITE_FILE: Error writing SUB_DIR's data back to the binary .disk file\n");
        return -1;
    }
    // close the .disk file, once we're done
    fclose(disk_file_ptr);

    // END DUPLICATION

    } 
    // ELSE WE HAVE AN OVERAGE
    else {
        printf("CS1550_WRITE_%d: FILE TOO LARGE. FileSize: %d, Offset:%d\n = Overage of: %d", __LINE__, file_size, (int)offset, overage);
        return -EFBIG;
    }
	//set size (should be same as input) and return, or error
	return size;

}

/******************************************************************************
 *
 *  DO NOT MODIFY ANYTHING BELOW THIS LINE
 *
 *****************************************************************************/

/*
 * truncate is called when a new file is created (with a 0 size) or when an
 * existing file is made shorter. We're not handling deleting files or
 * truncating existing ones, so all we need to do here is to initialize
 * the appropriate directory entry.
 *
 */
static int cs1550_truncate(const char *path, off_t size)
{
	(void) path;
	(void) size;

    return 0;
}


/*
 * Called when we open a file
 *
 */
static int cs1550_open(const char *path, struct fuse_file_info *fi)
{
	(void) path;
	(void) fi;
    /*
        //if we can't find the desired file, return an error
        return -ENOENT;
    */

    //It's not really necessary for this project to anything in open

    /* We're not going to worry about permissions for this project, but
	   if we were and we don't have them to the file we should return an error

        return -EACCES;
    */

    return 0; //success!
}

/*
 * Called when close is called on a file descriptor, but because it might
 * have been dup'ed, this isn't a guarantee we won't ever need the file
 * again. For us, return success simply to avoid the unimplemented error
 * in the debug log.
 */
static int cs1550_flush (const char *path , struct fuse_file_info *fi)
{
	(void) path;
	(void) fi;

	return 0; //success!
}


//register our new functions as the implementations of the syscalls
static struct fuse_operations hello_oper = {
    .getattr	= cs1550_getattr,
    .readdir	= cs1550_readdir,
    .mkdir	= cs1550_mkdir,
	.rmdir = cs1550_rmdir,
    .read	= cs1550_read,
    .write	= cs1550_write,
	.mknod	= cs1550_mknod,
	.unlink = cs1550_unlink,
	.truncate = cs1550_truncate,
	.flush = cs1550_flush,
	.open	= cs1550_open,
};

//Don't change this.
int main(int argc, char *argv[])
{
	return fuse_main(argc, argv, &hello_oper, NULL);
}
