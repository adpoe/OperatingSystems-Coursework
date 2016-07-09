"""
@author Anthony (Tony) Poerio - adp59@pitt.edu
CS1550 - Operating Systems
Project #3:   VM Simulator for Page Replacement Algorithms
Summer 2016

Test and report on algorithms for page replacement in an Operating System
"""
import sys
import parseInput as parse
import pageTable as pt
import optV3 as opt
import time

####################################
### PARSE INPUT FROM TRACE FILES ###
####################################



####################
### CONTROL FLOW ###
####################
def main():
    # get user input
    #cmdLineArgs = getUserInputgetUserInput()

    # SELECTED VARIABLES
    num_frames = 8
    algorithm = "opt"
    refresh = None
    traceFile = "gcc.trace"

    # parse the input and store it
    memory_addresses = parse.parse_trace_file(traceFile)

    # build the model for our page table, 32bit address space
    # initialize our table
    pageTable = pt.PageTable(num_frames)

    print str(len(pageTable.frame_table))


    # write opt algorithm
    t0 = time.time()
    OptAlgorithm = opt.Opt(pageTable, memory_addresses)
    OptAlgorithm.run_algorithm()
    t1 = time.time()
    total = t1-t0
    print "TOTAL RUNNING TIME IN MINUTES: " + str(total*0.0166667)
    # write clock algorithm

    # write aging algorithm

    # write lru algorithm


    # write statistical tests and generate graphs


    return


# NOTE:  Need to change this later... because once we start using -n, etc., the arg values will change
def getUserInput():
    """ Gets user input and saves as class level variables
        :return A list of arguments passed in by the user
    """
    # create a list of argumenst to return
    arglist = []

    # get the num frames and algorithm selection
    num_frames = sys.argv[1]
    algorithm = sys.argv[2]
    # and append them to the list
    arglist.append(num_frames)
    arglist.append(algorithm)

    # if algorithm is aging
    if algorithm == "aging":
        # then we need a refresh rate
        refresh = sys.argv[3]
        tracefile = sys.argv[4]
        # append refresh rate and tracefile to our list
        arglist.append(refresh)
        arglist.append(tracefile)

    # otherwise, we just need the tracefile
    else:
        # and apend that to the list, with a None in place of our refresh rate
        tracefile = sys.argv[3]
        arglist.append(None)
        arglist.append(tracefile)

    # return the list we've built
    return arglist

def outputResults():
    """  Outputs results for user after our algorithm runs
    """
    return


###################
### ENTRY POINT ###
###################

if __name__ == "__main__":
    main()

