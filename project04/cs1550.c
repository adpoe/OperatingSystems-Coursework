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
int write_to_root_directory_on_disk(void);
int write_to_subdirectory_on_disk(long block_offset);
int write_to_file_on_disk(void);

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
    // Open the .disk file, get a pointer to it 
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
    printf("CREATE_DIRECTORY: Result of find_next_free_directory_starting_block() == %d\n", subdir_starting_block);
    // handle error, if there are no free directories, return -1
    if (subdir_starting_block == -1) 
        return -1;
    
    // open the disk file for writing
    // Open the .disk file, get a pointer to it 
    FILE *disk_file_ptr = fopen(".disk", "wb");
    // handle error
    if (disk_file_ptr == NULL) {
        perror("Could not open .disk file. Please ensure it is in the current directory");
    }

    // seek to the location in .disk where our directory is stored, using the subdir_offset
    // seek to start, which is beginning of root
    fseek(disk_file_ptr, 0, SEEK_SET); 

    // define a root directory struct for us to use
    cs1550_root_directory ROOT_dir;
    /* >>> DON'T NEED TO READ IN ROOT HERE;; WE'RE **WRITING** TO IT 
    // handle error, if the root directory isn't one full block size for any reason
    if (BLOCK_SIZE != fread(&ROOT_dir, 1, BLOCK_SIZE, disk_file_ptr)) {
        perror("CREATE_DIRECTORY: root directory wasn't loaded in currently when trying to parse the .disk file");
        return -1;
    }
    */


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
    ROOT_dir.directories[subdir_starting_block].nStartBlock = subdir_starting_block;
    // also keep track of how many directories we have. add one, since we just created a new dir 
    ROOT_dir.nDirectories++;
   
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
    // code here
    (void) disk_offset;
    return NULL;
}


/*
 * Write data the root directory on our disk
 * Take all possible values, and if NOT null, use the to write
 */
int write_to_root_directory_on_disk(){
    // code here
    return -1;
}

/*
 * Write data to a subdirectory, need its block index and the data to write
 */
int write_to_subdirectory_on_disk(long disk_offset){
    // code here
    (void) disk_offset;
    return -1;
}

/*
 * Write data to a file on disk. Need a block index and the data to write
 * May also want to support an append. Can do that that with 0/1 variable if necessary.
 */
int write_to_file_on_disk(){
    // code here
    return -1;
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
        cs1550_directory_entry *subdirectory = get_subdirectory_struct(subdir_starting_block); 
        int num_files_in_directory = subdirectory->nFiles; 
        // at a minimum, we'll show . and .. as valid files, since they are in EVERY directory
        filler(buf, ".", NULL, 0);
	    filler(buf, "..", NULL, 0);

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
            if (subdirectory->files[file_index].fname[0] != 0) {
                printf("READDIR:  File found in the directory\n");
                // concat the file name, and add it to our buffer
                char file_name[MAX_FILENAME + MAX_EXTENSION + 2];
                strcpy(file_name, subdirectory->files[file_index].fname);
                strcat(file_name, ".");
                strcat(file_name, subdirectory->files[file_index].fext);
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
    (void) path; // for now, cast to avoid error
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
