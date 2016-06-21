all: decoder

decoder: main.o bit_reader.o video.o video_init.o
	g++ --std=c++11 -lm $^ -o $@

%.o: %.cpp
	gcc --std=c++11 -lm -c $^

clean:
	rm -rf *.o decoder
