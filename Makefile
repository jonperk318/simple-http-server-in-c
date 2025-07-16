cc=gcc
flags=-Wall
src=src
bin=bin

default: all

all: setup clean $(bin)/server run

setup:
	mkdir -p $(bin)

clean:
	rm -f $(bin)/*

$(bin)/server: $(src)/server.c
	$(cc) $(flags) -o $@ $^

run:
	$(bin)/server
