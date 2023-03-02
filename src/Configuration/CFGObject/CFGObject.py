import sys
import os
import json

SUPPORTED_TYPES = {
                    "bool"  :  ["bool", "false"],
                    "u8"    : ["uint8_t", "0"], 
                    "u16"   : ["uint16_t", "0"],
                    "u32"   : ["uint32_t", "0"],
                    "u64"   : ["uint64_t", "0"],
                    "i32"   : ["int32_t", "0"],
                    "i64"   : ["int64_t", "0"],
                    "u8s"   : ["std::vector<uint8_t>", "{}"],
                    "u16s"   : ["std::vector<uint16_t>", "{}"],
                    "u32s"   : ["std::vector<uint32_t>", "{}"],
                    "u64s"   : ["std::vector<uint64_t>", "{}"],
                    "i32s"   : ["std::vector<int32_t>", "{}"],
                    "i64s"   : ["std::vector<int64_t>", "{}"],
                    "str"    : ["std::string", "\"\""],
                    None :  ["class", ""]
                }

SUPPORTED_MAX_LELVE = 3
MAX_LEVEL = 0
CLASS_CREATOR = []

def check_case(name, upper) :

    assert isinstance(upper, bool)
    assert len(name) >= 2, "Name must be at least two characters, but found %s" % name
    for i, c in enumerate(name) :
        assert ((upper and c >= 'A' and c <= 'Z') or \
                (not upper and c >= 'a' and c <= 'z') or \
                (i > 0 and ((c >= '0' and c <= '9') or c == '_'))), \
                "Expect naming (%c) to be alphanumeric in %s case and character '_', first character must be alphabet, but found %s" % (c, "upper" if upper else "lower", name) 

def get_c_type(type) :

    assert type in SUPPORTED_TYPES
    type_map = SUPPORTED_TYPES[type]
    return type_map[0]

def get_c_type_default(type) :

    assert type in SUPPORTED_TYPES
    type_map = SUPPORTED_TYPES[type]
    if len(type_map[1]) :
        return " = %s" % type_map[1]
    else :
        return type_map[1]

class element :

    def __init__(self, n, t, e, l) :

        self.name = n
        self.type = None if isinstance(t, list) else t
        self.class_type = None
        self.exist = e
        self.list = l
        self.childs = []
        assert len(self.name) <= 16, "Element name must not more than 16 characters, but found %s" % self.name
        check_case(self.name, False)
        assert isinstance(self.exist, bool), "Element %s exist paramter must be boolean, but found %s" % type(self.exist)
        assert isinstance(self.list, bool), "Element %s list paramter must be boolean, but found %s" % type(self.list)
        assert self.type in SUPPORTED_TYPES, "Element %s type %s is not supported (%s)" % (self.name, self.type, SUPPORTED_TYPES)
        if self.type != None :
            assert not self.list, "None-clasee element %s (%s) does not support list parameter" % (self.name, self.type)

def check_element(values, name, siblings, level=0) :

    global MAX_LEVEL
    assert level >= 0 and level < SUPPORTED_MAX_LELVE, "Currently only support %d layer of element" % SUPPORTED_MAX_LELVE
    assert isinstance(values, dict), "Object %s element must be list, but found %s" % (name, type(values))
    additional = 0
    if "exist" not in values :
        values["exist"] = True
        additional += 1
    if "list" not in values :
        values["list"] = False
        additional += 1
    assert "name" in values, "Object %s element must have have a \"name\" key" % name
    assert "type" in values, "Object %s element must have have a \"type\" key" % name
    assert len(values) == 4, "Object %s element %s should only has %d entries, but found %d" % (name, values["name"], 4 - additional, len(element) - additional)
    e = element(values["name"], values["type"], values["exist"], values["list"])
    for sibling in siblings :
        assert sibling.name != e.name, "Object %s found duplicate element name %s" % (name, e.name)
    siblings.append(e)
    if e.type == None :
        for ge in values["type"] :
            check_element(ge, "%s.%s" % (name, e.name), e.childs, level+1)
    assert (e.type == None and len(e.childs)) or (e.type != None and len(e.childs) == 0)
    if level > MAX_LEVEL :
        MAX_LEVEL = level

def write_rule(file, element, parent_name, space, last) :

    if len(parent_name) :
        name = "%s.%s" % (parent_name, element.name)
    else :
        name = element.name
    if len(element.childs) :
        if element.list :
            element_type = "list"
        else :
            element_type = "class"
    else :
        element_type = element.type
    file.write("%sCFGObject_RULE(\"%s\", %s, &%s, \"%s\")" % \
                (space, name, "true" if element.exist else "false", name, element_type))
    if not last :
        file.write(",")
    file.write("\n")

def write_class(file, elements, class_name, current_level, target_level) :

    global CLASS_CREATOR
    assert current_level <= target_level, "%d %d %s" % (current_level, target_level, class_name)
    if current_level == target_level :
        assert len(class_name) > 10 and class_name[:10] == "CFGObject_"
        file.write("class %s : public CFGObject\n" % class_name)
        file.write("{\n")
        file.write("public:\n")
        # Constructor
        file.write("    %s() :\n" % (class_name))
        file.write("        CFGObject(\"%s\", {\n" % class_name[10:])
        for i, element in enumerate(elements) :
            write_rule(file, element, "", "                     ", i == (len(elements) - 1))
        file.write("                 })\n")
        file.write("        {\n")
        for element in elements :
            if element.type == None :
                assert element.class_type == None
                if not element.list :
                    file.write("            %s.set_parent_ptr(this);\n" % (element.name))
        file.write("        }\n\n")
        # Destructor
        file.write("    ~%s()\n" % (class_name))
        file.write("    {\n")
        for element in elements :
            if element.type == None and element.list :
                file.write("        while (%s.size()) { delete %s.back(); %s.pop_back(); }\n" % (element.name, element.name, element.name))
        file.write("    }\n\n")
        # Create child
        file.write("    void create_child(const std::string& name)\n")
        file.write("    {\n")
        for element in elements :
            if element.type == None and element.list :
                file.write("        if (name == \"%s\") { %s.push_back(new %s_%s); this->update_exist(name); return; }\n" % (element.name, element.name, class_name, element.name.upper()))
                if class_name[10:] not in CLASS_CREATOR :
                    CLASS_CREATOR.append(class_name[10:])
        file.write("        CFG_INTERNAL_ERROR(\"%s does not support child %%s\", name.c_str());\n" % (class_name))
        file.write("    }\n\n")    
        # Members
        for element in elements :
            if element.type == None :
                assert element.class_type == None
                element.class_type = "%s_%s" % (class_name, element.name.upper())
                if element.list :
                    file.write("    std::vector<%s *> %s;\n" % (element.class_type, element.name))
                else :
                    file.write("    %s %s;\n" % (element.class_type, element.name))
            else :
                assert element.class_type == None
                file.write("    %s %s%s;\n" % (get_c_type(element.type), element.name, get_c_type_default(element.type)))
        file.write("};\n\n")
    elif current_level < target_level :
        for element in elements :
            if len(element.childs) :
                write_class(file, element.childs, "%s_%s" % (class_name, element.name.upper()), current_level+1, target_level)

def main() :

    print("****************************************")
    print("*")
    print("* Auto generate CFB Objects")
    print("*")
    assert len(sys.argv) == 3, "CFGObject.py need input json file and output auto.h arguments"
    assert os.path.exists(sys.argv[1]), "Input json file %s does not exist" % sys.argv[1]
    print("*    Input : %s" % sys.argv[1])
    print("*    Output: %s" % sys.argv[2])
    print("*")
    print("****************************************")

    # Read the JSON file into objs
    file = open(sys.argv[1], 'r')
    objs = json.loads(file.read())
    file.close()

    # Open output file for writting
    file = open(sys.argv[2], 'w')
    file.write("#ifndef CFG_OBJECTS_AUTO_H\n")
    file.write("#define CFG_OBJECTS_AUTO_H\n\n")
    file.write("#include <string>\n")
    file.write("#include <vector>\n")
    file.write("#include \"CFGObject/CFGObject.h\"\n")
    file.write("\n/*\n")
    file.write("    This file is auto-generated by Configuration/CFGObject/CFGObject.py\n")
    file.write("        Input: %s\n" % os.path.basename(sys.argv[1]))
    file.write("*/\n\n")
    
    # Loop through the objs
    global MAX_LEVEL
    assert isinstance(objs, dict), "JSON top level must be a dictionary, but found %s" % type(objs)
    for obj in objs :
        MAX_LEVEL = 0
        assert isinstance(obj, str), "JSON top level dictionary key must be str, but found %s" % type(obj)
        assert len(obj) <= 8, "JSON top level dictionary key string must not more than 8 characters, but found %s" % obj
        check_case(obj, True)
        elements = objs[obj]
        assert isinstance(elements, list), "JSON top level dictionary value must be list, but found %s" % type(elements)
        childs = []
        for element in elements :
            check_element(element, obj, childs)
        file.write("/*\n")
        file.write("    This is for CFGObject_%s\n" % obj)
        file.write("*/\n\n")
        # Write out all the class
        class_name = "CFGObject_%s" % obj
        for child in childs :
            if len(child.childs) :
                for level in reversed(list(range(MAX_LEVEL))) :
                    write_class(file, child.childs, "%s_%s" % (class_name, child.name.upper()), 0, level)
        write_class(file, childs, class_name, 0, 0)
    # Global class creater
    file.write("static void CFGObject_create_child_from_names(const std::string& parent, const std::string& child, void * ptr)\n")
    file.write("{\n")
    for creator in CLASS_CREATOR :
        file.write("    if (parent == \"%s\") { CFGObject_%s * class_ptr = reinterpret_cast<CFGObject_%s *>(ptr); class_ptr->create_child(child); return; }\n" % (creator, creator, creator))
    file.write("    CFG_INTERNAL_ERROR(\"Invalid parent %s CFGObject\", parent.c_str());\n")
    file.write("}\n\n")
    # Close output file
    file.write("#endif\n")
    file.close()

if __name__ == "__main__":
    main()
