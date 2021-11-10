# Author: Adrian Cinal

import getopt

class BoapEarlyParser:
    class Option:
        def __init__(self, shortopt, longopt, description, requiresArgument = False, validValues = None):
            self.shortopt = shortopt
            self.longopt = longopt
            self.description = description
            self.requiresArgument = requiresArgument
            self.validValues = validValues

    def __init__(self):
        self.options = [
            self.Option('-h', '--help', 'Display this help and exit'),
            self.Option('-p', '--port', 'Port at which the router is connected to the PC', True),
            self.Option('-e', '--early', '[ file | stdout ] Select early logging mechanism (none if no flag provided)', True, ['file', 'stdout']),
            self.Option('-b', '--baud', 'Router baud rate', True),
            self.Option('-d', '--debug', 'Enable debug prints')
        ]

    def Parse(self, argv):
        ret = {}

        # Drop the reference to the program being executed
        args = argv[1:]

        # Prepare the options for getopt
        shortopts = ""
        longopts = []
        for option in self.options:
            shortopts += option.shortopt[1:] + (":" if option.requiresArgument else "")
            longopts.append(option.longopt[2:] + ("=" if option.requiresArgument else ""))

        # Call getopt
        optlist, args = getopt.getopt(args, shortopts, longopts)
        if len(args):
            raise Exception('Unknown trailing arguments: ' + str(args))

        for opt, param in optlist:
            relevantOption = self.FindOptionByName(opt)
            if param:
                if relevantOption.validValues and param not in relevantOption.validValues:
                    raise Exception('Invalid value ' + param + ' for argument ' + opt)
                else:
                    ret[relevantOption.shortopt] = param
            else:
                ret[relevantOption.shortopt] = True

        return ret

    def Help(self):
        print("************** Ball-on-a-plate desktop application **************")
        print("")
        for option in self.options:
            print("\t{} | {:<20}:\t{}".format(option.shortopt, option.longopt, option.description))
        print("")
        print("*************************************************************")

    def FindOptionByName(self, name):
        for option in self.options:
            if name == option.shortopt or name == option.longopt:
                return option
