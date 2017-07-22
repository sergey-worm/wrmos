//##################################################################################################
//
//  psocket - POSIX Socket implementation.
//
//##################################################################################################

#ifndef PSOCKET_OPCODES_H
#define PSOCKET_OPCODES_H

enum Psocket_opcode_t
{
	Socket_create   = 1,
	Socket_close    = 2,
	Socket_bind     = 3,
	Socket_listen   = 4,
	Socket_accept   = 5,
	Socket_connect  = 6,
	Socket_sendto   = 7,
	Socket_recvfrom = 8
};

#endif // PSOCKET_OPCODES_H
