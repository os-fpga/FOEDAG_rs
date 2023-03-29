import sys
import os
import json

ALIGNS = ["@align8", "@align16", "@align32", "@align64", "@align128", "@align256"]
RESERVED = 0

def check_case(name, upper) :

    assert isinstance(upper, bool)
    assert len(name) >= 2, "Name must be at least two characters, but found %s" % name
    for i, c in enumerate(name) :
        assert ((upper and c >= 'A' and c <= 'Z') or \
                (not upper and c >= 'a' and c <= 'z') or \
                (i > 0 and ((c >= '0' and c <= '9') or c == '_'))), \
                "Expect naming (%c) to be alphanumeric in %s case and character '_', first character must be alphabet, but found %s" % (c, "upper" if upper else "lower", name) 

class RULE :

    def __init__(self, field) : 

        global RESERVED
        name = field["name"]
        size = field["size"]
        data = field["data"]
        sdata_size = field["sdata_size"]
        sdata = field["sdata"]
        print_info = field["print_info"]
        dependency = field["dependency"]
        assert isinstance(name, str)
        assert isinstance(size, (int, str))
        assert isinstance(data, (int, str))
        assert sdata_size == None or isinstance(sdata_size, str)
        assert sdata == None or isinstance(sdata, str)
        assert print_info == None or isinstance(print_info, str)
        assert dependency == None or isinstance(dependency, str)
        size_type = 0
        rule_size = size
        rule_data = data
        if name in ALIGNS :
            size_type = ALIGNS.index(name) + 8
            name = "auto_aligned_reserve%d" % RESERVED
            RESERVED += 1
            assert size == 0
        elif isinstance(size, str) :
            size_type = 1
            rule_size = 0
        if isinstance(data, str) :
            rule_data = 0
        self.name = name
        self.size = size
        self.size_type = size_type
        self.rule_size = rule_size
        self.data = data
        self.rule_data = rule_data
        self.sdata_size = sdata_size
        self.sdata = sdata
        self.print_info = print_info
        self.dependency = dependency

def write_rule(cfile, rule) :

    cfile.write("    BitGen_DATA_RULE(\"%s\", %d, %d, %d)" % (rule.name, \
                                                                rule.rule_size, \
                                                                rule.rule_data, \
                                                                rule.size_type))

def match_dependency(dependency, rules) :

    found_rule = False
    for rule in rules :
        if rule.name == dependency :
            found_rule = True
            break
    assert found_rule, "Cannot find rule %s" % (dependency)

def check_dependency(equation, rules, current_rule_name, keywords = { "@get_rule_size(" : ")",
                                                                      "@get_rule_value(" : ")",
                                                                      "@get_rule_data(" : ")"}) :
    assert isinstance(equation, str) and len(equation) > 0
    dependencies = []
    for keyword in keywords :
        assert isinstance(keyword, str) and len(keyword) > 0
        assert isinstance(keywords[keyword], str) and len(keywords[keyword]) > 0
        # put name into quotation
        index = equation.find(keyword)
        end_index = -1
        if index != -1 :
            end_index = equation.find(keywords[keyword], index + len(keyword))
        while index != -1 and end_index != -1 :
            start_insert = index + len(keyword)
            string = equation[start_insert:end_index]
            check_case(string, False)
            assert string != current_rule_name
            match_dependency(string, rules)
            dependencies.append(string)
            index = equation.find(keyword, end_index+len(keywords[keyword]))
            end_index = -1
            if index != -1 :
                end_index = equation.find(keywords[keyword], index + len(keyword))
    return dependencies 

def replace_equation_string(equation, rule_name, keywords = {"@get_src_value(" : ")",
                                                             "@get_src_data(" : ")", 
                                                             "@get_src_data_size(" : ")", 
                                                             "@get_rule_size(" : ")",
                                                             "@get_rule_value(" : ")",
                                                             "@get_rule_data_ptr(" : ")",
                                                             "@get_rule_data(" : ")",
                                                             "@get_defined_value(" : ")"} ) :
    INC_RULE_NAME_KEYWORDS = [ "@calc_crc(" ]
    assert isinstance(equation, str) and len(equation) > 0
    for keyword in keywords :
        assert isinstance(keyword, str) and len(keyword) > 0
        assert isinstance(keywords[keyword], str) and len(keywords[keyword]) > 0
        # put name into quotation
        index = equation.find(keyword)
        end_index = -1
        if index != -1 :
            end_index = equation.find(keywords[keyword], index + len(keyword))
        while index != -1 and end_index != -1 :
            start_insert = index + len(keyword)
            string = equation[start_insert:end_index]
            check_case(string, False)
            equation = equation[:start_insert] + "\"" + equation[start_insert:]
            equation = equation[:end_index+1] + "\"" + equation[end_index+1:]
            index = equation.find(keyword, end_index+len(keywords[keyword])+2)
            end_index = -1
            if index != -1 :
                end_index = equation.find(keywords[keyword], index + len(keyword))
    for keyword in INC_RULE_NAME_KEYWORDS :
        assert isinstance(keyword, str) and len(keyword) > 0
        index = equation.find(keyword)
        while index != -1 :
            print(index, equation, keyword)
            assert len(rule_name)
            index += len(keyword)
            equation = equation[:index] + ("\"%s\", " % rule_name) + equation[index:]
            index += (4 + len(rule_name))
            index = equation.find(keyword, index)
    return equation

def replace_equation_function(equation, keywords = { "size8" : "convert_to8",
                                                     "size16" : "convert_to16",
                                                     "size32" : "convert_to32",
                                                     "size64" : "convert_to64"}) :

    index = equation.find("@")
    while index != -1 :
        end_index = equation.find("(", index + 1)
        assert end_index != -1
        string = equation[index+1:end_index]
        strip_string = string.strip()
        if strip_string in keywords :
            equation = equation.replace(string, keywords[strip_string])
            # end_index might be mess up since replacement had happened
            end_index = equation.find("(", index + 1)
            assert end_index != -1
        index = equation.find("@", end_index+1)
    # lastly all remaining @, replace with empty string
    equation = equation.replace("@", "")
    return equation

def get_explicite_dependency(data_name, rule_name, dependency, rules) :

    dependency_string = None
    if dependency != None :
        dependencies = dependency.strip().split(",")
        assert len(dependencies) >= 1
        dependency = int(dependencies[0].strip())
        assert dependency in [0, 1] # only support two function
        ds = []
        for d in dependencies[1:] :
            d = d.strip()
            check_case(d, False)
            if dependency == 0 :
                assert d != rule_name
            match_dependency(d, rules)
            ds.append(d)
        if dependency == 1 : 
            if rule_name not in ds :
                ds.append(rule_name)
        assert len(ds)
        dependency_string = ",".join([("%s_%%s" % data_name) % d.upper() for d in ds])
        if dependency == 0 :
            dependency_string = "check_rules_readiness({%s})" % dependency_string
        else :
            dependency_string = "check_all_rules_readiness_but({%s})" % dependency_string
    return dependency_string;

def main() :

    print("****************************************")
    print("*")
    print("* Auto generate CFG BitGen Data")
    print("*")
    assert len(sys.argv) == 4, "BitGen_data.py need input json file and output auto.h/auto.cpp arguments"
    assert os.path.exists(sys.argv[1]), "Input json file %s does not exist" % sys.argv[1]
    print("*    Input:      %s" % sys.argv[1])
    print("*    Output H:   %s" % sys.argv[2])
    print("*    Output CPP: %s" % sys.argv[3])
    print("*")
    print("****************************************")

    # Read the JSON file into datas
    file = open(sys.argv[1], 'r')
    datas = json.loads(file.read())
    file.close()

    # Open output file for writting
    file = open(sys.argv[2], 'w')
    cfile = open(sys.argv[3], 'w')
    file.write("#ifndef BITGEN_DATA_AUTO_H\n")
    file.write("#define BITGEN_DATA_AUTO_H\n\n")
    file.write("#include \"BitGen_data.h\"\n")
    cfile.write("#include \"BitGen_data_auto.h\"\n")
    file.write("\n/*\n")
    file.write("    This file is auto-generated by ConfigurationRS/BitGenerator/BitGen_data.py\n")
    file.write("        Input: %s\n" % os.path.basename(sys.argv[1]))
    file.write("*/\n\n")
    cfile.write("\n/*\n")
    cfile.write("    This file is auto-generated by ConfigurationRS/BitGenerator/BitGen_data.py\n")
    cfile.write("        Input: %s\n" % os.path.basename(sys.argv[1]))
    cfile.write("*/\n\n")

    global RESERVED
    for data in datas :
        data_name = data.upper()
        assert "fields" in datas[data]
        fields = datas[data]["fields"]
        size_alignment = 0
        defined = None
        if "size_alignment" in datas[data] :
            size_alignment = datas[data]["size_alignment"]
        if "defined" in datas[data] :
            defined = datas[data]["defined"]
        assert isinstance(fields, list)
        check_case(data, False)
        file.write("class BitGen_%s_DATA : public BitGen_DATA\n" % data_name)
        file.write("{\n")
        file.write("public:\n")
        file.write("  enum FIELD\n")
        file.write("  {\n")
        cfile.write("BitGen_%s_DATA::BitGen_%s_DATA() :\n" % (data_name, data_name))
        cfile.write("  BitGen_DATA(\"%s\", %d, {\n" % (data, size_alignment))
        names = []
        rules = []
        funcs = []
        RESERVED = 0
        for i, field in enumerate(fields) :
            assert isinstance(field, dict)
            assert "name" in field
            name = field["name"]
            if name not in ALIGNS :
                assert name not in names
                names.append(name)
                check_case(name, False)
                assert name.find("auto_aligned_reserve") != 0
            if "size" not in field :
                field["size"] = 0
            if "data" not in field :
                field["data"] = 0
            if "sdata_size" not in field :
                field["sdata_size"] = None
            if "sdata" not in field :
                field["sdata"] = None
            if "print_info" not in field :
                field["print_info"] = None
            if "dependency" not in field :
                field["dependency"] = None
            assert len(field) == 7
            rules.append(RULE(field))
            write_rule(cfile, rules[-1])
            file.write("    %s_%s = %d" % (data_name, rules[-1].name.upper(), i))
            if i == (len(fields)-1) :
                cfile.write("})\n")
                file.write("\n")
            else :
                cfile.write(",\n")
                file.write(",\n")
        file.write("  };\n\n")
        file.write("  BitGen_%s_DATA();\n" % data_name)
        cfile.write("{\n")
        cfile.write("}\n\n")

        # Set size
        cfile.write("uint8_t BitGen_%s_DATA::set_sizes(const std::string& field)\n" % data_name)
        cfile.write("{\n")
        cfile.write("  // return 0 if it is not supported, 1 if it is set, 2 if it is not ready\n")
        index = 0
        for rule in rules :
            if rule.size_type == 1 :
                rule_enum = "%s_%s" % (data_name, rule.name.upper())
                assert isinstance(rule.size, str)
                equation = replace_equation_string(rule.size, rule.name)
                equation = replace_equation_function(equation)
                dependencies = check_dependency(rule.size, rules, rule.name)
                if index == 0 :
                    cfile.write("  uint8_t set = 1;\n")
                    cfile.write("  if (field == \"%s\")\n" % rule.name)
                else :
                    cfile.write("  else if (field == \"%s\")\n" % rule.name)
                cfile.write("  {\n")
                if len(dependencies) :
                    dependency_string = ",".join([("%s_%%s" % data_name) % d.upper() for d in dependencies])
                    cfile.write("    if (!check_rules_readiness({%s})) { return 2; }\n" % dependency_string)    
                cfile.write("    set_size(%s, %s);\n" % (rule_enum, equation))
                cfile.write("  }\n")
                index += 1
        if index :
            cfile.write("  else\n")
            cfile.write("  {\n")
            cfile.write("    set = 0;\n")
            cfile.write("  }\n")
        else :
            cfile.write("  uint8_t set = 0;\n")
        cfile.write("  return set;\n")
        cfile.write("}\n\n")
        
        # Set data
        cfile.write("uint8_t BitGen_%s_DATA::set_datas(const std::string& field)\n" % data_name)
        cfile.write("{\n")
        cfile.write("  // return 0 if it is not supported, 1 if it is set, 2 if it is not ready\n")
        index = 0
        for rule in rules :
            if isinstance(rule.data, str) :
                rule_enum = "%s_%s" % (data_name, rule.name.upper())
                rule_ptr = "rules[%s]" % (rule_enum)
                explicite_dependency = get_explicite_dependency(data_name, rule.name, rule.dependency, rules)
                if index == 0 :
                    cfile.write("  uint8_t set = 1;\n")
                    cfile.write("  if (field == \"%s\")\n" % rule.name)
                else :
                    cfile.write("  else if (field == \"%s\")\n" % rule.name)
                cfile.write("  {\n")
                cfile.write("    if (!check_rule_size_readiness(%s)) { return 2; }\n" % (rule_enum))
                if rule.data.find("@func(") == 0 and rule.data[-1] == ')' :
                    func_name = rule.data[6:-1]
                    cfile.write("    %s(&%s);\n" % (func_name, rule_ptr))
                else :
                    equation = replace_equation_string(rule.data, rule.name)
                    equation = replace_equation_function(equation)
                    dependencies = check_dependency(rule.data, rules, rule.name)
                    if len(dependencies) :
                        dependency_string = ",".join([("%s_%%s" % data_name) % d.upper() for d in dependencies])
                        cfile.write("    if (!check_rules_readiness({%s})) { return 2; }\n" % dependency_string)
                    cfile.write("    set_data(%s, %s);\n" % (rule_enum, equation))
                cfile.write("  }\n")
                index += 1
        if index :
            cfile.write("  else\n")
            cfile.write("  {\n")
            cfile.write("    set = 0;\n")
            cfile.write("  }\n")
        else :
            cfile.write("  uint8_t set = 0;\n")
        cfile.write("  return set;\n")
        cfile.write("}\n\n")

        # defined
        cfile.write("void BitGen_%s_DATA::auto_defined()\n" % data_name)
        cfile.write("{\n")
        if defined != None :
            for define in defined :
                assert isinstance(define, str)
                assert isinstance(defined[define], str)
                equation = replace_equation_string(defined[define], "")
                equation = replace_equation_function(equation)
                cfile.write("    defineds[\"%s\"] = BitGEN_SRC_DATA(%s);\n" % (define, equation))
        cfile.write("}\n\n")        

        # Source data Size
        cfile.write("void BitGen_%s_DATA::get_source_data_size(std::vector<uint8_t>&data, const std::string& field)\n" % data_name)
        cfile.write("{\n")
        cfile.write("  // return zero size data if not supported, otherwise return resize data accordingly with ZEROs\n")
        cfile.write("  data.resize(0);\n")
        cfile.write("  uint64_t data_size = 0;\n") 
        index = 0
        for rule in rules :
            if isinstance(rule.sdata_size, str) :
                if index == 0 :
                    cfile.write("  if (field == \"%s\")\n" % rule.name)
                else :
                    cfile.write("  else if (field == \"%s\")\n" % rule.name)
                cfile.write("  {\n")
                equation = replace_equation_string(rule.sdata_size, rule.name)
                equation = replace_equation_function(equation)
                cfile.write("    data_size = %s;\n" % (equation))
                cfile.write("  }\n")
                index += 1
        cfile.write("  if (data_size)\n")
        cfile.write("  {\n")
        cfile.write("    data.resize(data_size);\n");
        cfile.write("    memset(&data[0], 0, data.size());\n");
        cfile.write("  }\n")
        cfile.write("}\n\n")

        # Source data
        cfile.write("void BitGen_%s_DATA::get_source_data(std::vector<uint8_t>&data, const std::string& field)\n" % data_name)
        cfile.write("{\n")
        cfile.write("  // Assign to data if supported\n")
        index = 0
        for rule in rules :
            if isinstance(rule.sdata, str) :
                if index == 0 :
                    cfile.write("  if (field == \"%s\")\n" % rule.name)
                else :
                    cfile.write("  else if (field == \"%s\")\n" % rule.name)
                cfile.write("  {\n")
                equation = replace_equation_string(rule.sdata, rule.name)
                equation = replace_equation_function(equation)
                cfile.write("    %s;\n" % (equation))
                cfile.write("  }\n")
                index += 1
        cfile.write("}\n\n")

        # Print
        cfile.write("uint64_t BitGen_%s_DATA::get_print_info(const std::string& field)\n" % data_name)
        cfile.write("{\n")
        cfile.write("  // return 0 if it does support special print, otherwise special print info\n")
        cfile.write("  uint64_t info = 0;\n")
        index = 0
        for rule in rules :
            if isinstance(rule.print_info, str) :
                if index == 0 :
                    cfile.write("  if (field == \"%s\")\n" % rule.name)
                else :
                    cfile.write("  else if (field == \"%s\")\n" % rule.name)
                cfile.write("  {\n")
                equation = replace_equation_string(rule.print_info, rule.name)
                equation = replace_equation_function(equation)
                cfile.write("    info = (%s);\n" % (equation))
                cfile.write("  }\n")
                index += 1
        cfile.write("  return info;\n")
        cfile.write("}\n\n")

        # Write to function and virtual
        file.write("protected:\n")
        file.write("  uint8_t set_sizes(const std::string& field);\n")
        file.write("  uint8_t set_datas(const std::string& field);\n")
        file.write("  void auto_defined();\n")
        file.write("  void get_source_data_size(std::vector<uint8_t>&data, const std::string& field);\n")
        file.write("  void get_source_data(std::vector<uint8_t>&data, const std::string& field);\n")
        file.write("  uint64_t get_print_info(const std::string& field);\n")
        for func in funcs :
            file.write("  virtual void %s(BitGen_DATA_RULE* rule);\n" % func)
            cfile.write("void BitGen_%s_DATA::%s(BitGen_DATA_RULE* rule)\n" % (data_name, func))
            cfile.write("{\n")
            cfile.write("  CFG_INTERNAL_ERROR(\"Unimplemented virtual function\");\n")
            cfile.write("}\n")
        file.write("};\n\n")



    # Close output file
    file.write("#endif\n")
    file.close()
    cfile.close()

if __name__ == "__main__":
    main()
