import sys
import os
import CFGObject_compress

SUPPORTED_TYPE = [
    "bool", 
    "u8", 
    "u16", 
    "u32",
    "u64",
    "i32",
    "i64",
    "u8s", 
    "u16s", 
    "u32s",
    "u64s",
    "i32s",
    "i64s",
    "str",
    "class",
    "list"
]

def check_case(name, upper) :

    assert isinstance(upper, bool)
    assert len(name) >= 2, "Name must be at least two characters, but found %s" % name
    for i, c in enumerate(name) :
        assert ((upper and c >= 'A' and c <= 'Z') or \
                (not upper and c >= 'a' and c <= 'z') or \
                (i > 0 and ((c >= '0' and c <= '9') or c == '_'))), \
                "Expect naming (%c) to be alphanumeric in %s case and character '_', first character must be alphabet, but found %s" % (c, "upper" if upper else "lower", name) 

def get_string(data, index, max_size=None, min_size=None, null_check=None) :
    
    string = ""
    current_index = 0
    while True :
        assert index < len(data)
        current_index += 1
        if data[index] :
            string = "%s%c" % (string, data[index])
            index += 1
        else :
            index += 1
            break
        if max_size != None and current_index == max_size :
            break
    assert min_size == None or len(string) >= min_size
    assert null_check == None or current_index <= null_check
    if null_check != None :
        while current_index < null_check :
            assert index < len(data)
            assert data[index] == 0
            index += 1
            current_index += 1
    return [string, index]
    
def get_variable_length(data, index) :
    
    length = 0
    current_index = 0
    while True :
        # maximum is uint64_t (10 bytes which including next bit)
        assert current_index < 10
        assert index < len(data)
        length |= (data[index] & 0x7F) << (current_index * 7)
        index += 1
        current_index += 1   
        if (data[index-1] & 0x80) == 0 :
            break
    return [length, index]
    
def get_value(data, index, length) :
    
    assert length in [1, 2, 4, 8]
    if length == 1 :
        value = 0
        for i in range(length) :
            assert index < len(data)
            value |= (data[index] << (i * 8))
            index += 1
        return [value, index]
    else :
        count = index
        (value, index) = get_variable_length(data, index)
        count = index - count
        assert count > 0
        if length == 8 :
            assert count <= (length + 2)
        else :
            assert count <= (length + 1)
        return [value, index]

def get_values(data, index, length) :
    
    assert length in [1, 2, 4, 8]
    # First thing to get the variable vector length
    (vector_length, index) = get_variable_length(data, index)
    compress = (vector_length & 1) != 0
    vector_length >>= 1
    assert vector_length > 0
    values = []
    if compress :
        (header, index) = get_string(data, index, 7, 7)
        assert header == "CFG_CMP"
        assert index < len(data)
        version = data[index]
        index += 1
        (original_size, index) = get_variable_length(data, index)
        (cmp_size, index) = get_variable_length(data, index)
        assert original_size == (vector_length * length), "Original size: %d vs (%d * %d)" % (original_size, vector_length, length)
        assert (index + cmp_size) <= len(data)
        new_data = CFGObject_compress.uncompress_data(data[index:index+cmp_size], False)
        assert len(new_data) == original_size
        new_index = 0
        for i in range(vector_length) :
            (value, new_index) = get_value(new_data, new_index, length)
            values.append(value)
        index = index + cmp_size
    else :
        for i in range(vector_length) :
            (value, index) = get_value(data, index, length)
            values.append(value)
    return [values, index]
    
def print_values(stdout, values, byte_size, space) :
    
    assert byte_size in [1, 2, 4, 8]
    assert len(values) > 0
    index = 0
    address = 0
    stdout.write("%sAddress   |" % (space * 3 * " "))
    dash = 0
    byte_index = 0
    while byte_index < 16 :
        stdout.write(" %0*X" % (byte_size * 2, byte_index))
        dash += ((byte_size * 2) + 1)
        byte_index += byte_size
    stdout.write("\n%s%s" % (space * 3 * " ", (11 + dash) * "-"))
    for value in values :
        if index % (16//byte_size) == 0 :
            stdout.write("\n%s%08X  |" % (space * 3 * " ", address))
        stdout.write(" %0*X" % (byte_size * 2, value))
        index += 1
        address += byte_size
    stdout.write("\n")
    
def parse_object(stdout, data, index, space) :
    
    assert index < len(data)
    assert data[index] < len(SUPPORTED_TYPE)
    object_type = SUPPORTED_TYPE[data[index]]
    index += 1
    assert index < len(data)
    [object_name, index] = get_string(data, index, 16, 2)
    check_case(object_name, False)
    stdout.write("%s%s (type: %s)" % (space * 3 * " ", object_name, object_type))
    if object_type in ["bool", "u8"] :
        (value, index) = get_value(data, index, 1)
        if object_type == "bool" :
            assert value in [0, 1]
            stdout.write(" - %r\n" % bool(value))
        else :
            stdout.write(" - 0x%02X\n" % value)
    elif object_type in ["u16"] :
        (value, index) = get_value(data, index, 2)
        stdout.write(" - 0x%04X\n" % value)
    elif object_type in ["u32", "i32"] :
        (value, index) = get_value(data, index, 4)
        stdout.write(" - 0x%08X\n" % value)
    elif object_type in ["u64", "i64"] :
        (value, index) = get_value(data, index, 8)
        stdout.write(" - 0x%016X\n" % value)
    elif object_type in ["u8s", "u16s", "u32s", "i32s", "u64s", "i64s"] :
        byte_size = 1 if object_type == "u8s" else \
                    (2 if object_type == "u16s" else \
                    (4 if object_type in ["u32s", "i32s"] else 8))
        (values, index) = get_values(data, index, byte_size)
        stdout.write("\n")
        print_values(stdout, values, byte_size, space+1)
    elif object_type in ["str"] :
        (value, index) = get_string(data, index)
        stdout.write(" - %s\n" % value)
    elif object_type in ["class"] :
        stdout.write("\n")
        index = parse_class_object(stdout, data, index, space+1) 
    elif object_type in ["list"] :
        stdout.write("\n")
        index = parse_list_object(stdout, data, index, space+1)
    else :
        assert False, "Unknown parsing object name %s - type %s" % (object_name, object_type)
    return index  
    
def parse_class_object(stdout, data, index, space) :
    
    assert index < len(data)
    assert data[index] != 0xFF
    while data[index] != 0xFF :
        assert index < len(data)
        index = parse_object(stdout, data, index, space)
    assert index < len(data)
    assert data[index] == 0xFF
    index += 1
    return index
    
def parse_list_object(stdout, data, index, space) :
    
    (list_length, index) = get_variable_length(data, index)
    assert list_length
    for i in range(list_length) :
        stdout.write("%s#%d\n" % (space * 3 * " ", i))
        index = parse_class_object(stdout, data, index, space+1)
    return index

def main() :
    
    assert len(sys.argv) in [2, 3]
    assert os.path.exists(sys.argv[1]), "Input file %s does not exist" % sys.argv[1]
    file = open(sys.argv[1], "rb")
    data = file.read()
    file.close()
    stdout = sys.stdout if len(sys.argv) == 2 else open(sys.argv[2], "w")
    index = 0
    [name, index] = get_string(data, 0, 8, 2, 8)
    check_case(name, True)
    [length, index] = get_variable_length(data, index)
    stdout.write("CFGObject: %s\n" % name)
    stdout.write("   Total Object: %d\n" % length)
    stdout.write("   Objects\n")
    assert index < len(data)
    assert length > 0
    while index < len(data) :
        index = parse_object(stdout, data, index, 2)
    assert index == len(data)
    if len(sys.argv) == 3 :
        stdout.close()

if __name__ == "__main__":
    main()