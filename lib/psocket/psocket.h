//##################################################################################################
//
//  psocket - POSIX Socket implementation;
//            used POSIX Socket API from client side and internal PSOCKET API
//            from net_server side.
//
//##################################################################################################

#ifndef PSOCKET_H
#define PSOCKET_H

#ifdef __cplusplus
extern "C" {
#endif

// initialise socket subsystem
int init_posix_sockets();

#ifdef __cplusplus
}
#endif

#endif // PSOCKET_H
