
OBJS += frontends/uhdm/UhdmAst.o
OBJS += frontends/uhdm/uhdmastreport.o
OBJS += frontends/uhdm/uhdmastfrontend.o

#*** UHDM ***
CPPFLAGS += -std=c++14 -I./../image/include/uhdm \
			-I./../image/include/uhdm/include \
			-I./../image/include/uhdm/headers
LDFLAGS += -L./../image/lib/uhdm -L./../image/lib
LDLIBS += -luhdm -lcapnp -lkj -ldl -lutil -lm -lrt -lpthread
