#!/usr/bin/python 
import sys, binascii
from pyparsing import Word, alphanums, Keyword, nums

# define logfile parser here
serial = Word(nums).setResultsName("serial")
type_word = Keyword("type=KERNEL_OTHER").suppress()
timestamp = Word(alphanums + "." + ":")
timestamp_tag =  "msg=audit(" + timestamp.setResultsName("timestamp") + "):"
filename = Word(alphanums + "/" + "." + "_").setResultsName("file")
hexified = Word(nums + "abcdef").setResultsName("hex")
file_offset = Word(nums).setResultsName("offset")
decms_tag = "DecMS={" + filename + "(" + serial + ")" + "(" + file_offset + ")" + "=[" + hexified + "]" + "}"


log_entry_parser = type_word + timestamp_tag + decms_tag

file_dict = dict()
#command line args: 
if __name__ == "__main__":
    for line in sys.stdin:
        try:
            result = log_entry_parser.parseString(line)
        except:
            #parser failed, check the next line
            continue
        # appened the serial number to the file name
        name = result["file"] + "." + result["serial"]
        
        #if we have file.serial is in the dictionary, appened the hex
        if name in file_dict:
            file_dict[name][int(result["offset"])] = result["hex"]
        #else create a new dictionary with hex
        else:
            file_dict[name] = {int(result["offset"]):result["hex"]}
    
    #for each recovered file
    for f in file_dict:
        print "recovering %s" % f
        try:
            outfile = open("%s.recovered" % f,"w")
        except:
            print "Failed to write to %s" % file_dict[f]
            continue

        #for each partial file
        for hex_off in file_dict[f]:
            #for hex_value in file_dict[f][hex_off]:
            print "\trecovering partial file: %d" % hex_off
            #decode the partil file and write it out
            outfile.write(binascii.unhexlify(file_dict[f][hex_off]))
        
        outfile.close()

