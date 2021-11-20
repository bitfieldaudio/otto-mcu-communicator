#include <string>
#include <optional>

namespace otto {

// Start socket. Returns a file descriptor for an inactive socket
int create_socket(std::string, bool nonblocking = false);

// Performs the 'listen' call to an inactive socket, taking it to the 'passive'
// state, where connections can be made to it
int listen_to_socket(int);

// Assumes a nonblocking socket
std::optional<int> accept_new_connection(int);

} // namespace otto
