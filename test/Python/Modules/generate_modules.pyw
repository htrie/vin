import glob
import os
import dataclasses


class CppWriter:
    """ Writes the generated content to file """
    def __init__(self, file):
        self.file = file
        self.buffer = ''

    def __enter__(self):
        return self

    def __exit__(self, exc_type, exc_value, exc_traceback):
        # Check if an exception has been thrown before we continue
        if exc_type is not None:
            return False

        # Check if the file already exists
        if os.path.exists(self.file):
            # Read the existing file
            existing_file = open(self.file, 'r').read()
            # Check the contents are not the same before writing
            if existing_file == self.buffer:
                print('  No changes detected')
                return True

        print('  Writing data to ' + self.file)
        # Open the file then write the metadata to it
        with open(self.file, 'w') as f:
            f.write(self.buffer)

        # Success!
        return True

    def ln(self, line=''):
        """ Write a single line to the buffer with a newline character """
        self.buffer += line + '\n'


@dataclasses.dataclass
class Function:
    """ Simple dataclass describing function information """
    line: str
    name: str
    ret: str
    args: str


@dataclasses.dataclass
class Argument:
    """ Simple dataclass describing argument information """
    type: str = None
    name: str = None
    default: str = None

@dataclasses.dataclass
class Enumeration:
    """ Simple dataclass describing enumeration information """
    name: str = None
    values: list = None

class ModuleGenerator:
    """ Class used to generate the module code and write it to file """
    def __init__(self, module):
        # Store the module name
        self.module = module
        # Read the file text
        self.text = open(f'{self.module}.gen', 'r').readlines()
        # Create an empty function list
        self.function_list = []
        # Create an empty enumeration list
        self.enumeration_list = []
        # Search paths
        self.path = {}
        # Preprocessor Defines
        self.defines = []

    def generate_enum(self, enum):
        """ Generate enumeration information from markup """
        for path, file in self.path.items():
            # Find the enum definition
            begin_enum = file.find(f'\nenum {enum}')
            if begin_enum == -1:
                continue

            # Check the enum is valid
            begin_enum = file.find('{', begin_enum)
            if begin_enum == -1:
                raise ValueError('Expected { in generate_enum')

            end_enum = file.find('}', begin_enum)
            if end_enum == -1:
                raise ValueError('Expected } in generate_enum')

            text = file[begin_enum+1:end_enum]

            # Remove preprocessor and comments
            lines = [line.strip() for line in text.split('\n')]
            for i in range(len(lines)):
                line = lines[i]
                if '//' in line: line = line[:line.find('//')]
                if '#' in line: line = line[:line.find('#')]
                line = line.strip()
                lines[i] = line
            text = ''.join(lines)

            # Strip enum values
            lines = [line.strip() for line in text.split(',')]
            for i in range(len(lines)):
                line = lines[i]
                if '=' in line: line = line[:line.find('=')]
                line = line.strip()
                lines[i] = line

            self.enumeration_list.append(Enumeration(enum, lines))
            return

        raise ValueError('Could not find definition for enum: ' + enum)

    def generate_function(self, text, begin):
        """ Generate function information from markup """

        # Grep the actual function definition
        end = text.find('{', begin)
        line = text[begin:end].strip()
        line = line[line.find(' ')+1:]

        # Find the return type
        bracket_iter = line.find('(')
        for ret_iter in range(bracket_iter,0,-1):
            if line[ret_iter] == ' ':
                break

        ret = line[:ret_iter]

        # Find the function name
        func = line[ret_iter+1:bracket_iter]
        func = func[:bracket_iter]

        # Find the argument list
        args = line[bracket_iter+1:-1]

        # Store the function data
        self.function_list.append(Function(line, func, ret, args))

    def get_argument_list(self, args):
        """ Convert a string containing arguments into a parsed argument list """

        # No arguments
        if not args:
            return []

        arg_list = []
        last = 0
        block = 0

        # Scan through the string to find the argument list
        # Splitting by ',' alone would create incorrect results
        for i in range(len(args)):
            c = args[i]
            if c == ',':
                if block == 0:
                    arg_list.append(args[last:i])
                    last = i + 1
            elif c in ['(', '[', '{', '<']:
                block += 1
            elif c in [')', ']', '}', '>']:
                block -= 1

        # Finish and format the argument list
        arg_list.append(args[last:])
        arg_list = [x.strip() for x in arg_list]

        # Generate metadata
        for i in range(len(arg_list)):
            arg = Argument()

            # Find the default argument (if one exists)
            default_split = arg_list[i].split('=')
            type_name = default_split[0].strip()

            # Store the default argument
            if len(default_split) == 2:
                arg.default = default_split[1].strip()

            # Find the argument name
            name_iter = type_name.rfind(' ')
            arg.name = type_name[name_iter+1:].strip()
            arg.type = type_name[:name_iter].strip()

            # Deal with array syntax, move it from the argument name to argument type
            array_iter = arg.name.find('[')
            if array_iter != -1:
                arg.type += arg.name[array_iter:]
                arg.name = arg.name[:array_iter]

            # Change the argument in the array from a 'str' to an 'Argument' in-place
            arg_list[i] = arg

        if 'py::kwargs' in arg_list[-1].type:
            arg_list.pop()
        if 'py::args' in arg_list[-1].type:
            arg_list.pop()

        # Success!
        return arg_list

    def generate_data(self):
        """ Generate metadata from a module file by searching the file for markup """
        # Search for markup through entire file
        for line in self.text:
            word_end = line.find(' ')
            if word_end == -1:
                raise ValueError('Unexpected line in gen file')

            word = line[:word_end].strip()
            arg = line[word_end+1:].strip()

            # Determine what we need to do based on the name of the tag
            if word == 'SOURCE':
                self.source_file(arg)
            elif word == 'INCLUDE':
                self.add_path(arg)
            elif word == 'DEFINE':
                self.defines.append(arg)
            elif word == 'ENUM':
                self.generate_enum(arg)

    def source_file(self, file_path):
        text = open(file_path, 'r').read()
        last = 0

        while True:
        #
            word_begin = text.find('GENERATE ', last)
            last = word_begin + 1
            if word_begin == -1:
                break

            self.generate_function(text, word_begin)
        #

    def add_path(self, path):
        file = open(path, 'r').read()
        last = 0
        while True:
            pre_iter = file.find('#if', last)
            if pre_iter == -1:
                break
            last = pre_iter + 1

            pre_iter2 = file.find('\n', pre_iter)
            pre_line = file[pre_iter:pre_iter2]

            delete = False

            if pre_line.startswith('#ifdef'):
                define = pre_line[7:].strip()
                delete = define not in self.defines
            elif pre_line.startswith('#ifndef'):
                define = pre_line[7:].strip()
                delete = define in self.defines
            else:
                continue

            if delete:
                end_iter = file.find('#endif', pre_iter2)
                if end_iter == -1:
                    raise ValueError('Missing #endif tag in add_path')

                deleting_text = file[pre_iter:end_iter+6]
                file = file[:pre_iter] + file[end_iter+6:]

        self.path[path] = file

    def write_file(self):
        """ Write the generated metadata to file """

        # Open the C++ writer
        with CppWriter(f'{self.module}.inc') as w:

            # We are about to reference this object, so invoke it
            if self.function_list:
                w.ln('mod')

            # Write functions to file
            for func in self.function_list:
                # Get the function name used in Python
                name = func.name
                name = name[:name.find('_')] if '_' in name else name

                # Create the basic definition
                definition = f'\t.def("{name}", &{func.name}'

                # Add any arguments
                for arg in self.get_argument_list(func.args):
                    definition += f', py::arg("{arg.name}")'
                    if arg.default is not None:
                        definition += '=' + arg.default

                # Write the definition
                w.ln(definition + ')')
            w.ln(';')

            # Write enumerations to file
            for enum in self.enumeration_list:
                # Remove excess _
                python_name = enum.name[:-1] if enum.name[-1] == '_' else enum.name

                # Write definition
                w.ln(f'py::enum_<{enum.name}>(mod, "{python_name}", py::arithmetic(), "")')

                # Write values
                for value in enum.values:
                    w.ln(f'\t.value("{value}", {value}, "")')
                w.ln('\t.export_values()')
                w.ln(';')

    def generate(self):
        """ Entry point for module generation """
        print('Generating code for: ' + self.module)
        self.generate_data()
        self.write_file()


if __name__ == '__main__':
    """ Program entry point """
    module_list = [filename[:-4] for filename in glob.glob(f'*.gen')]
    for module in module_list:
        ModuleGenerator(module).generate()
