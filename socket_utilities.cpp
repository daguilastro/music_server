#include "socket_utilities.hpp"

int createSocket(int domain, int type){
    int fd = socket(domain, type, 0);

}