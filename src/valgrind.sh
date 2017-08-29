#! /bin/sh

valgrind --tool=memcheck --leak-check=yes --track-origins=yes --log-file=valgrind.log ./rps
