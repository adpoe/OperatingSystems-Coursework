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


        //if (-1 == subdir_write) {
        //    printf("WRITE->SUBDIR_WRITE::  Failed to write subdir to disk\n");
         //   return -1;
        //}

        // write updated file to disk
        //int file_write = write_to_file_on_disk(file_start_block, &disk_block);
        //if (-1 == file_write) {
        //    printf("WRITE->FILE_WRITE::  Failed to write file to disk\n");
        //    return -1;
        //}

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
    // ELSE WE ARE DOING AN APPEND
    printf("CS1550_WRITE:  FILE INDEX=%d\n", file_index);
	//check that size is > 0
    if (size < 1) {
        printf("WRITE:   SIZE IS < 1\n");
        return -1;
    }

	//check that offset is <= to the file size
    if (offset > subdirectory.files[file_index].fsize) {
        printf("CS1550_WRITE:  OFFSET IS > FILE SIZE\n");
        return -EFBIG;
    }

	//write data
    printf("CS1550_WRITE:: Made it to APPEND SECTION of WRITE\n");

    // WRITE DATA
        cs1550_disk_block disk_block;
        memset(&disk_block, 0, sizeof(cs1550_disk_block)); // because i get weird values sometimes
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


        //if (-1 == subdir_write) {
        //    printf("WRITE->SUBDIR_WRITE::  Failed to write subdir to disk\n");
         //   return -1;
        //}

        // write updated file to disk
        //int file_write = write_to_file_on_disk(file_start_block, &disk_block);
        //if (-1 == file_write) {
        //    printf("WRITE->FILE_WRITE::  Failed to write file to disk\n");
        //    return -1;
        //}

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


	//set size (should be same as input) and return, or error
	return size;
}
