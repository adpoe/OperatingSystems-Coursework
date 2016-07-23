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
//// PROTOTYPES ////
////////////////////
void get_filepath(const char *filepath, char *DIR_name, char *FILE_name, char *FILE_extension); 
long get_subdirectory_starting_block(char *DIR_name); 
long* get_file_starting_block(const char *subdir_name, const char *extension, long subdir_offset); 

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
        perror("root directory wasn't loaded in currently when trying to parse the .disk file");
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
            starting_block = i; // we return the iteration number, since that is index within our subdirectory where there was a match 
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

	//This line assumes we have no subdirectories, need to change
	if (strcmp(path, "/") != 0)
	return -ENOENT;

	//the filler function allows us to add entries to the listing
	//read the fuse.h file for a description (in the ../include dir)
	filler(buf, ".", NULL, 0);
	filler(buf, "..", NULL, 0);

	/*
	//add the user stuff (subdirs or files)
	//the +1 skips the leading '/' on the filenames
	filler(buf, newpath + 1, NULL, 0);
	*/
	return 0;
}

/* 
 * Creates a directory. We can ignore mode since we're not dealing with
 * permissions, as long as getattr returns appropriate ones for us.
 */
static int cs1550_mkdir(const char *path, mode_t mode)
{
	(void) path;
	(void) mode;

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
	(void) mode;
	(void) dev;
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

	//check to make sure path exists
	//check that size is > 0
	//check that offset is <= to the file size
	//read in data
	//set size and return, or error

	size = 0;

	return size;
}

/* 
 * Write size bytes from buf into file starting from offset
 *
 */
static int cs1550_write(const char *path, const char *buf, size_t size, 
			  off_t offset, struct fuse_file_info *fi)
{
	(void) buf;
	(void) offset;
	(void) fi;
	(void) path;

	//check to make sure path exists
	//check that size is > 0
	//check that offset is <= to the file size
	//write data
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