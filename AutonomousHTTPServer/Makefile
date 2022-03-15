bin=httpServer
bin_src=main.cc
cgi=test_cgi
cgi_location=$(shell pwd)/cgi
cc=g++
LD_FLAGS=-std=c++11 -lpthread


.PHONY:ALL
ALL:$(bin) $(cgi)

$(bin):$(bin_src)
	$(cc) -o $@ $^ $(LD_FLAGS)
$(cgi):$(cgi_location)/test_cgi.cc 
	$(cc) -o $@ $^ $(LD_FLAGS)
	cp $@ -rf ./wwwroot

.PHONY:output 
output:
	mkdir -p output
	cp -rf $(bin) output
	cp -rf wwwroot output 

.PHONY:clean 
clean:
	rm -rf $(bin) $(cgi)
	rm -rf output
