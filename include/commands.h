#ifndef COMMANDS_H
#define COMMANDS_H

extern struct cmd_handler
{
    const char *cmd;
    void (*execute)(char **cmd_line);
} command_handlers[];

#endif
