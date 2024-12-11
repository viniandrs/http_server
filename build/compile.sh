bison -d ../parser.y
flex ../lexer.l
gcc -ggdb3 ../source/*.c ../main.c parser.tab.c lex.yy.c -lfl -pthread -lcrypt -o server