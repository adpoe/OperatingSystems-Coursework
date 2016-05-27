//////////////////////////
////// DEBUG MACROS //////
//////////////////////////
# define DBG (x ) printf ( x)
# define ARG (x ) , ( x)
DBG ("z = %d" ARG (z ));


///////////////////////////////
////// LIBRARY FUNCTIONS //////
///////////////////////////////

/*
 * Given a string, make a heap-allocated copy of it
 */
char *strdup (const char *s) {
    char *d = malloc (strlen (s) + 1);   // Allocate memory
    if (d != NULL) strcpy (d,s);         // Copy string if okay
    return d;                            // Return new memory
}


