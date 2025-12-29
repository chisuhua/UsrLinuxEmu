PLUGINS = sample_driver.so

all: $(PLUGINS)

sample_driver.so:
    g++ -fPIC -shared ../drivers/sample_driver.cpp -o $@ -I../include

clean:
    rm -f *.so
