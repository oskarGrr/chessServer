#ifndef CONNECTIONS_ACCEPTOR_H
#define CONNECTIONS_ACCEPTOR_H

//the size of the stack that the accept connections thread uses in bytes
#define ACCEPT_CONNECTIONS_STACKSIZE 64000

//this is the only function exposed in the .h meant to be the start of a
//new pthread. only 1 thread will work with this and 
//subsequent threads will immediately return
void __stdcall acceptConnectionsThreadStart(void*);

#endif //CONNECTIONS_ACCEPTOR_H