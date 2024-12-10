bison -d ../parser.y
flex ../lexer.l
gcc -o server lex.yy.c -ggdb3 parser.tab.c ../source/*.c ../main.c -lfl -pthread