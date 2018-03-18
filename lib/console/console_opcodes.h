//##################################################################################################
//
//  console - console protocol implementation.
//
//##################################################################################################

#ifndef CONSOLE_OPCODES_H
#define CONSOLE_OPCODES_H

enum Psocket_opcode_t
{
	Console_open  = 1,
	Console_close = 2,
	Console_read  = 3,
	Console_write = 4
};

#endif // CONSOLE_OPCODES_H
