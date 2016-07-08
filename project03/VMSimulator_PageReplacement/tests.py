import parseInput as parse

class Tests():
    # parse input data traces ---> OK
    trace_data_array_SWIM = parse.parse_trace_file("swim.trace")
    for elem in trace_data_array_SWIM:
        print str(elem)
        print parse.hex_string_to_binary_int(elem[0])

    trace_data_array_GCC = parse.parse_trace_file("gcc.trace")
    for elem in trace_data_array_GCC:
        print str(elem)