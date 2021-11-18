#include <string>

namespace otto {

// Start socket. Returns a file descriptor for an inactive socket
int create_socket(std::string);

// Performs the 'listen' call to an inactive socket, taking it to the 'passive'
// state, where connections can be made to it
int listen_to_socket(int);

} // namespace otto
