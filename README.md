# icli
Interactive cli/shell library for C

A library that adds the ability to implement CISCO-like interactive shells from C code.
Please see examples directory for sample code and API usage.

## Example cli session:

```
$ ./cli 
my_cli> help 
Available commands:
    quit       : Quit interactive shell
    help       : Show available commands or show help of a specific command
    ?          : Synonym for `help'
    history    : Show a list of previously run commands
    containers : Containers
    show       : Print info
my_cli> show containers
Container: 1
Container: 2
Container: 3
Container: 4
my_cli> containers 
my_cli(containers)> list
args:

Container: 1
Container: 2
Container: 3
Container: 4
my_cli(containers)> end
my_cli> sd
sd: No such command
my_cli> show
Command show accepts exactly 1 arguments. 0 were provided
my_cli> show cont
Command show 0 argument invalid: cont
my_cli> quit
```
