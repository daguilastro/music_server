CXX = g++
CXXFLAGS = -Wall
TARGET = main

SRCS = main.cpp \
       server/server.cpp \
       server/epoll_handler.cpp \
       server/client_handler.cpp \
       commands/command_handler.cpp \
       network/socket_utils.cpp \
       network/upnp.cpp \
       worker/worker.cpp \
       worker/worker_manager.cpp

$(TARGET): $(SRCS)
	$(CXX) $(CXXFLAGS) -o $(TARGET) $(SRCS)

clean:
	rm -f $(TARGET)