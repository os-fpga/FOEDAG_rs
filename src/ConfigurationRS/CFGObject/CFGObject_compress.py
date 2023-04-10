import sys
import os
import time

CMP_NONE = 0
CMP_ZERO = 1
CMP_ZERO_VAR = 2
CMP_VAR_ZERO = 3
CMP_HIGH = 4
CMP_HIGH_VAR = 5
CMP_VAR_HIGH = 6
CMP_VAR = 7
CMP_INVALID = 8
MIN_REPEAT_SEARCH = 3
MAX_REPEAT_SEARCH = 50
MAX_REPEAT_SAME_PATTERN = 100

def write_variable_length(data, value) :

  count = 1
  data.append(value & 0x7F)
  value >>= 7
  while value :
    assert count < 10
    data[-1] |= 0x80
    data.append(value & 0x7F)
    value >>= 7
    count += 1
  return count

def read_variable_length(data, index) :
    
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

def analyze_single_chunk(data, index, length) :

    assert length >= 3
    zero_length = 0
    high_length = 0
    for i in range(length) :
        assert index < len(data)
        if data[index] == 0 :
            zero_length += 1
        elif data[index] == 0xFF :
            high_length += 1
        index += 1
    if zero_length == length :
        return CMP_ZERO
    elif high_length == length :
        return CMP_HIGH
    elif zero_length == (length - 1) :
        if data[index] != 0 :
            return CMP_VAR_ZERO
        elif data[index-1] != 0 :
            return CMP_ZERO_VAR
        else :
            return CMP_NONE
    elif high_length == (length - 1) :
        if data[index] != 0xFF :
            return CMP_VAR_HIGH
        elif data[index-1] != 0xFF :
            return CMP_HIGH_VAR
        else :
            return CMP_NONE
    else :
        return CMP_NONE

def analyze_repeat(data, index, debug) :

    total_length = len(data) - index
    if total_length < (2 * MIN_REPEAT_SEARCH) :
        return (0, 0)
    length = MIN_REPEAT_SEARCH
    repeat_length = 0
    repeat = 0
    repeated_check_seq = 0
    repeated_byte = data[index]
    repeated_check_index = index
    repeated_check_size = 0
    while length <= (total_length//2) :
        # if we still cannot find any repeat for consecutive MAX_REPEAT_SEARCH, just terminate
        if repeat_length == 0 and length > MAX_REPEAT_SEARCH :
            break
        chunk0_start = index
        chunk0_end_p1 = chunk0_start + length
        chunk1_start = chunk0_end_p1
        chunk1_end_p1 = chunk1_start + length
        assert chunk0_start < len(data)
        assert chunk0_end_p1 < len(data)
        assert chunk1_start < len(data)
        assert chunk1_end_p1 <= len(data)
        # If too many repeat, we break early otherwise if the file is big, it take a lot of time to process
        if repeated_check_seq == 0 :
            repeated_check_seq = 1
            for i in range(2 * MIN_REPEAT_SEARCH) :
                if data[repeated_check_index] != repeated_byte :
                    repeated_check_seq = 2
                    break
                repeated_check_index += 1
                repeated_check_size += 1
        elif repeated_check_seq == 1 :
            if data[repeated_check_index] == repeated_byte and \
                data[repeated_check_index+1] == repeated_byte :
                repeated_check_index += 2
                repeated_check_size += 2
                if repeated_check_size >= MAX_REPEAT_SAME_PATTERN :
                    repeat_length = 0
                    repeat = 0
                    break
            else :
                repeated_check_seq = 2
        identical = data[chunk0_start:chunk0_end_p1] == data[chunk1_start:chunk1_end_p1]
        if identical :
            # look for next
            repeat_length = length
        else :
            if repeat_length :
                # had match once
                break
        length += 1
    if repeat_length :
        # Double check and find more repeat
        chunk0_start = index
        chunk0_end_p1 = chunk0_start + repeat_length
        chunk1_start = chunk0_end_p1
        chunk1_end_p1 = chunk1_start + repeat_length
        assert chunk0_start < len(data)
        assert chunk0_end_p1 < len(data)
        assert chunk1_start < len(data)
        assert chunk1_end_p1 <= len(data)
        while chunk1_end_p1 <= len(data) :
            identical = data[chunk0_start:chunk0_end_p1] == data[chunk1_start:chunk1_end_p1]
            if identical :
                repeat += 1
                chunk1_start += repeat_length
                chunk1_end_p1 += repeat_length
            else :
                break
        # At least found one repeat
        assert repeat
    if debug and repeat_length > 0 : 
        print("Found repated %d byte(s) of data for %d time(s) at index %d" % (repeat_length, repeat, index))
    return [repeat_length, repeat]

def compress_repeat_chunk(output, input, index, pattern, length, repeat) :

    assert length >= 3
    assert repeat > 0
    assert index < len(input)
    assert (index + length) < len(input)
    if pattern == CMP_NONE :
        flag = (length << 4) | 0x08 | pattern
    else :
        flag = ((length-1) << 4) | 0x08 | pattern
    write_variable_length(output, flag)
    if pattern == CMP_VAR_ZERO or pattern == CMP_VAR_HIGH :
        output.append(input[index])
    elif pattern == CMP_ZERO_VAR or pattern == CMP_HIGH_VAR :
        output.append(input[index+length-1])
    else :
        assert pattern == CMP_NONE
        for i in range(length) :
            output.append(input[index+i])
    write_variable_length(output, repeat)

def compress_pack_data(input, output, index, length, debug) :

    if debug : print("Pack %d byte(s) data at index 0x%08X" % (length, index))
    flag = (length << 4) | CMP_NONE
    write_variable_length(output, flag)
    for i in range(length) :
        assert index < len(input)
        output.append(input[index])
        index += 1

def compress_data(output, compress_byte, length, followup_byte, debug, space = "") :

    assert length > 0
    if followup_byte == -1 :
        if debug : print("%sCompress %d data of 0x%02X" % (space, length, compress_byte))
    else :
        if debug : print("%sCompress %d data of 0x%02X - followed by 0x%02X" % (space, length, compress_byte, followup_byte))
    if compress_byte == 0 :
        if followup_byte == -1 :
            pattern = CMP_ZERO
        else :
            pattern = CMP_ZERO_VAR
    elif compress_byte == 0xFF :
        if followup_byte == -1 :
            pattern = CMP_HIGH
        else :
            pattern = CMP_HIGH_VAR
    else :
        assert compress_byte > 0 and compress_byte < 0xFF
        assert followup_byte == -1
        pattern = CMP_VAR
    flag = (length << 4) | pattern
    write_variable_length(output, flag)
    if pattern == CMP_VAR :
        output.append(compress_byte)
    elif followup_byte != -1 :
        assert followup_byte >= 0 and followup_byte <= 0xFF
        output.append(followup_byte)    

def compress_none_repeat(input, output, index, debug) :

    compress_byte = 0
    compress_size = 0
    uncompress_size = 0
    compress = False
    end_index = 0
    while index < len(input) :
        if compress_size == 0 and uncompress_size == 0 :
            compress_byte = input[index]
            compress_size += 1
        elif compress_byte == input[index] :
            if compress_size :
                compress_size += 1
            else :
                assert uncompress_size >= 2
                assert index >= uncompress_size
                compress_pack_data(input, output, 
                                    index - uncompress_size, uncompress_size - 1, 
                                    debug)
                compress_size = 2
                uncompress_size = 0
                compress = True
                end_index = index + (uncompress_size - 1)
                break
        else :
            if compress_size == 1 :
                if input[index] in [0, 0xFF] and \
                    (index + 1) < len(input) and \
                    input[index] == input[index+1] :
                    end_index = index + 1
                    while end_index < len(input) :
                        if input[index] == input[end_index] :
                            compress_size += 1
                        else :
                            break
                        end_index += 1
                    if debug : print("Compress 0x%02X followed by %ld data of 0x%02X", \
                                        compress_byte, compress_size, input[index])
                    flag = (compress_size << 4)
                    if input[index] == 0 :                        
                        flag |= CMP_VAR_ZERO
                    else :
                        flag |= CMP_VAR_HIGH
                    write_variable_length(output, flag)
                    output.append(compress_byte)
                    compress_size = 0
                    uncompress_size = 0
                    compress = True
                    break
                else : 
                    compress_size = 0
                    uncompress_size = 2
                    compress_byte = input[index]
            elif compress_size :
                assert compress_size >= 2
                assert uncompress_size == 0
                followup_byte = -1
                if compress_byte == 0 or compress_byte == 0xFF :
                    followup_byte = input[index]
                compress_data(output, compress_byte, compress_size, followup_byte, debug)
                if compress_byte == 0 or compress_byte == 0xFF :
                    # restart everything
                    compress_size = 0
                    end_index = index + 1
                else :
                    compress_byte = input[index]
                    compress_size = 1
                    end_index = index
                compress = True
                break
            else :
                assert compress_size == 0
                uncompress_size += 1
                compress_byte = input[index]
        index += 1

    if not compress :
        # If not compress it must be the end of the data
        assert compress_size > 0 or uncompress_size > 0
        if compress_size :
            assert uncompress_size == 0
            compress_data(output, compress_byte, compress_size, -1, debug) 
        else :
            assert uncompress_size >=2
            assert len(input) >= uncompress_size
            compress_pack_data(input, output, 
                                    len(input) - uncompress_size, uncompress_size, 
                                    debug)
        end_index = len(input)
    return end_index

def compress(filepath, debug, support_followup=False) :

    file = open(filepath, "rb")
    input = file.read()
    file.close()

    output = bytearray()
    index = 0
    while index < len(input) :
        # Always give repeat priority
        (length, repeat) = analyze_repeat(input, index, debug)
        if length :
            assert repeat
            chunk_pattern = analyze_single_chunk(input, index, length)
            assert chunk_pattern < CMP_INVALID
            if chunk_pattern == CMP_ZERO or chunk_pattern == CMP_HIGH :
                # There might be more 0 or FFs
                length = (length * (repeat + 1))
                compress_byte = 0 if chunk_pattern == CMP_ZERO else 0xFF
                index += length
                followup_byte = -1
                while index < len(input) :
                    if input[index] == compress_byte :
                        length += 1
                    else :
                        if not support_followup :
                            break
                        followup_byte = input[index]
                    index += 1
                    if followup_byte != -1 :
                        break
                compress_data(output, compress_byte, length, followup_byte, debug, "   ")
            else :
                compress_repeat_chunk(output, input, index, chunk_pattern, length, repeat)
                index += (length * (repeat + 1))
                assert index <= len(input)
                if (debug) : print("   Pattern: %d" % chunk_pattern)
        else :
            assert repeat == 0
            index = compress_none_repeat(input, output, index, debug)
    file = open("%s.cmp" % filepath, "wb")
    file.write(output)
    file.close()
    assert index == len(input)

def uncompress_data(input, debug) :

    assert len(input)
    output = bytearray()
    index = 0
    while index < len(input) :
        (flag, index) = read_variable_length(input, index)
        pattern = flag & 7
        repeat = flag & 8
        length = flag >> 4
        assert length > 0
        if pattern == CMP_NONE :
            assert (index + length) <= len(input)
            chunk = bytearray(input[index:index+length])
            index += length
        elif pattern in [CMP_VAR, CMP_VAR_ZERO, CMP_VAR_HIGH] :
            assert index < len(input)
            chunk = bytearray()
            if pattern == CMP_VAR :
                compress_byte = input[index]
                for i in range(length) :
                    chunk.append(compress_byte)
            else :
                chunk.append(input[index])
                compress_byte = 0 if pattern == CMP_VAR_ZERO else 0xFF
                for i in range(length) :
                    chunk.append(compress_byte)
            index += 1
        else :
            chunk = bytearray()
            if pattern in [CMP_ZERO, CMP_ZERO_VAR] :
                compress_byte = 0
            else :
                assert pattern in [CMP_HIGH, CMP_HIGH_VAR]
                compress_byte = 0xFF
            for i in range(length) :
                chunk.append(compress_byte)
            if pattern in [CMP_ZERO_VAR, CMP_HIGH_VAR] :
                assert index < len(input)
                chunk.append(input[index])
                index += 1
        for c in chunk :
            output.append(c)
        if repeat :
            (repeat_length, index) = read_variable_length(input, index)
            assert repeat_length > 0
            for i in range(repeat_length) :
                for c in chunk :
                    output.append(c)
    assert index == len(input)
    return output

def uncompress(filepath, debug) :

    file = open(filepath, "rb")
    input = file.read()
    file.close()
    output = uncompress_data(input, debug)
    file = open("%s.uncmp" % filepath, "wb")
    file.write(output)
    file.close()

if __name__ == "__main__":

    start_time = time.time()
    assert len(sys.argv) >= 3
    assert sys.argv[1] in ["compress", "uncompress"]
    assert os.path.exists(sys.argv[2])
    debug = int(sys.argv[3]) if (len(sys.argv) >= 4) else 0   
    if sys.argv[1] == "compress" :
        compress(sys.argv[2], debug)
    else :
        uncompress(sys.argv[2], debug)
    end_time = time.time()
    print("Elapsed time: %d second(s)" % (end_time - start_time))
