bison -d ../parser.y
flex ../lexer.l
gcc -o server -ggdb3 lex.yy.c parser.tab.c ../source/*.c ../main.c -lfl