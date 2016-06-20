all: decoder

decoder: main.o bit_reader.o video.o video_init.o
	g++ --std=c++11 $^ -o $@

%.o: %.cpp
	gcc --std=c++11 -c $^

clean:
	rm -rf *.o decoder
