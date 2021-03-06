############################################################
#
# grpc-shared-channel Makefile
# Generated by amyznikov Aug 7, 2016
#   from 'linux-g++-executable' template
#
############################################################

SHELL = /bin/bash

CLIENT = client
SERVER = server


all: $(CLIENT) $(SERVER)


cross   =
sysroot =
DESTDIR =
prefix  = /usr/local
bindir  = $(prefix)/bin
incdir  = $(prefix)/include
libdir  = $(prefix)/lib

# DEFINES += -D__STDC_CONSTANT_MACROS -D__STDC_FORMAT_MACROS
INCLUDES+= -I.


# C preprocessor flags
CPPFLAGS=$(DEFINES) $(INCLUDES)

# C Compiler and flags
CC = $(cross)gcc -std=c11
CFLAGS= -Wall -Wextra -O3 -g0

# C++ Compiler and flags
CXX = $(cross)g++ -std=c++11
CXXFLAGS = $(CFLAGS)



# Loader Flags And Libraries
LD=$(CXX)
LDFLAGS = $(CXXFLAGS)

# STRIP = $(cross)strip --strip-all
STRIP = @echo "don't strip "


LDLIBS += -L/usr/local/lib `pkg-config --libs grpc++ grpc`       \
           -Wl,--no-as-needed -lgrpc++_reflection -Wl,--as-needed \
            -lstdc++ -lprotobuf -lpthread -ldl


#########################################


GRPC_PROTO = mutual-auth.proto
GRPC_PROTO_PATH = .
GRPC_SOURCES = mutual-auth.pb.cc mutual-auth.grpc.pb.cc
GRPC_HEADERS = mutual-auth.pb.h mutual-auth.grpc.pb.h
$(GRPC_SOURCES) $(GRPC_HEADERS) : $(GRPC_PROTO)


#########################################


CLIENT_SOURCES = $(GRPC_SOURCES) mutual-auth-client.cc
CLIENT_HEADERS = $(GRPC_HEADERS) 
CLIENT_MODULES = $(foreach s,$(CLIENT_SOURCES),$(addsuffix .o,$(basename $(s))))

$(CLIENT_MODULES): $(CLIENT_HEADERS) Makefile
$(CLIENT) : $(CLIENT_MODULES) Makefile
	$(LD) $(LDFLAGS)  $(CLIENT_MODULES) $(LDLIBS) -o $@

#########################################

SERVER_SOURCES = $(GRPC_SOURCES) mutual-auth-server.cc
SERVER_HEADERS = $(GRPC_HEADERS)
SERVER_MODULES = $(foreach s,$(SERVER_SOURCES),$(addsuffix .o,$(basename $(s))))

$(SERVER_MODULES): $(SERVER_HEADERS) Makefile
$(SERVER) : $(SERVER_MODULES) Makefile
	$(LD) $(LDFLAGS)  $(SERVER_MODULES) $(LDLIBS) -o $@


#########################################


PROTOC = protoc
GRPC_CPP_PLUGIN = grpc_cpp_plugin
GRPC_CPP_PLUGIN_PATH ?= `which $(GRPC_CPP_PLUGIN)`

vpath %.proto $(GRPC_PROTO_PATH)

.PRECIOUS: %.grpc.pb.cc
%.grpc.pb.cc: %.proto
	$(PROTOC) -I $(GRPC_PROTO_PATH) --grpc_out=. --plugin=protoc-gen-grpc=$(GRPC_CPP_PLUGIN_PATH) $<

.PRECIOUS: %.pb.cc
%.pb.cc: %.proto
	$(PROTOC) -I $(GRPC_PROTO_PATH) --cpp_out=. $<


#########################################




clean:
	$(RM) $(CLIENT_MODULES) $(SERVER_MODULES)

distclean: clean
	$(RM) $(CLIENT) $(SERVER) $(GRPC_SOURCES) $(GRPC_HEADERS)



install: $(CLIENT) $(SERVER) $(DESTDIR)/$(bindir)
	$(STRIP) --strip-all $(CLIENT) && cp $(CLIENT) $(DESTDIR)/$(bindir)
	$(STRIP) --strip-all $(SERVER) && cp $(SERVER) $(DESTDIR)/$(bindir)

uninstall:
	$(RM) $(DESTDIR)/$(bindir)/{$(CLIENT),$(SERVER)}


$(DESTDIR)/$(bindir):
	mkdir -p $@
