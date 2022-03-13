bin=HttpServer
cc=g++
LD_FLAGS=-std=c++11 -lpthread
src=main.cc

$(bin):$(src)
	$(cc) -o $@ $^ $(LD_FLAGS)

.PHONY:clean 
clean:
	rm -rf $(bin)