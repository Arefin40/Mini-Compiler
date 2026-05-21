all:
	bison -d -o parser.tab.cpp parser.y
	flex -o lexer.yy.c lexer.l
	g++ -o compiler parser.tab.cpp lexer.yy.c utils.cpp -std=c++17
	./compiler < input.txt

clean:
	rm -f compiler parser.tab.cpp parser.tab.hpp lexer.yy.c
