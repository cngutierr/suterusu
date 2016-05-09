#!/usr/bin/python 
import sys, binascii
from pyparsing import Word, alphanums, Keyword, nums

#type=KERNEL_OTHER msg=audit(1462814101.695:9705): DecMS={/home/krix/dev/suterusu/tests/991080.xls{5}

serial = Word(nums).setResultsName("serial")
type_word = Keyword("type=KERNEL_OTHER").suppress()
timestamp = Word(alphanums + "." + ":")
timestamp_tag =  "msg=audit(" + timestamp.setResultsName("timestamp") + "):"
filename = Word(alphanums + "/" + ".").setResultsName("file")
hexified = Word(nums + "abcdef").setResultsName("hex")
file_offset = Word(nums).setResultsName("offset")
decms_tag = "DecMS={" + filename + "(" + serial + ")" + "(" + file_offset + ")" + "=[" + hexified + "]" + "}"

log_line = type_word + timestamp_tag + decms_tag

file_dict = dict()
#command line args: 
if __name__ == "__main__":
    for line in sys.stdin:
        try:
            result = log_line.parseString(line)
        except:
            continue
        #print "timestamp: %s\nFile: %s\noffset: %s\nHex: %s\n" % (result["timestamp"], result["file"],\
        #                                             result["offset"], result["hex"])
        name = result["file"] + "." + result["serial"]
        if name in file_dict:
            file_dict[name][int(result["offset"])] = result["hex"]
        else:
            file_dict[name] = {int(result["offset"]):result["hex"]}
    for f in file_dict:
        print "recovering %s" % f
        outfile = open("%s.recovered" % f,"w")
        for hex_off in file_dict[f]:
            #for hex_value in file_dict[f][hex_off]:
            print "\trecovering partial file: %d" % hex_off
            outfile.write(binascii.unhexlify(file_dict[f][hex_off]))
        outfile.close()

