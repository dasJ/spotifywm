CXX = g++
CPPFLAGS = -Wall -Wextra
LDFLAGS = -O3 -shared -fPIC -static-libgcc -lX11

all: spotifywm

spotifywm: spotifywm.so

spotifywm.so: spotifywm.cpp
	$(CXX) $(CPPFLAGS) $(LDFLAGS) -DSONAME="$@" -o $@ $^

clean:
	rm spotifywm.so
