#include "icli.h"
#include <stdlib.h>
#include <string.h>

static int cli_show_containers(void)
{
    icli_printf("Container: %d\n", 1);
    icli_printf("Container: %d\n", 2);
    icli_printf("Container: %d\n", 3);
    icli_printf("Container: %d\n", 4);
    return 0;
}

static enum icli_ret cli_containers_list(char *argv[], int argc, void *context)
{
    struct md_util *self = context;

    icli_printf("args:\n\n");
    for (int i = 0; i < argc; ++i)
        icli_printf("arg %d: %s\n", i, argv[i]);

    int ret = cli_show_containers();
    if (ret) {
        icli_err_printf("Error in cli_show_containers:%d\n", ret);
        return ICLI_ERR;
    }

    return ICLI_OK;
}

static enum icli_ret cli_show(char *argv[], int argc, void *context)
{
    struct my_context *self = context;

    int ret = cli_show_containers();
    if (ret) {
        icli_err_printf("Error in cli_show_containers:%d\n", ret);
        return ICLI_ERR;
    }

    return ICLI_OK;
}

struct my_context {
    int something;
};

int main(int argc, char *argv[])
{
    int res;
    int ret = EXIT_SUCCESS;
    struct my_context context = {};

    struct icli_params params = {.user_data = &context,
                                 .history_size = 10,
                                 .app_name = "example_cli",
                                 .prompt = "my_cli"};

    res = icli_init(&params);
    if (res)
        return EXIT_FAILURE;

    struct icli_command *containers;
    struct icli_command_params param = {.name = "containers", .help = "Containers"};

    struct icli_arg_val show_first_arg[] = {{.val = "containers"}, {.val = NULL}};
    struct icli_arg_val *show_args[] = {show_first_arg};

    res = icli_register_command(&param, &containers);
    if (res) {
        ret = EXIT_FAILURE;
        goto out;
    }

    memset(&param, 0, sizeof(param));
    param.parent = containers;
    param.help = "List containers";
    param.name = "list";
    param.command = cli_containers_list;

    res = icli_register_command(&param, NULL);
    if (res) {
        ret = EXIT_FAILURE;
        goto out;
    }

    memset(&param, 0, sizeof(param));
    param.help = "Print info";
    param.name = "show";
    param.command = cli_show;
    param.argc = 1;
    param.argv = show_args;

    res = icli_register_command(&param, NULL);
    if (res) {
        ret = EXIT_FAILURE;
        goto out;
    }

    icli_run();

out:
    icli_cleanup();

    return ret;
}
