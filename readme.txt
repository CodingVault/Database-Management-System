
- Modify the "CODEROOT" variable in makefile.inc to point to the root of your code base

- Copy your own implementation of PF component to folder "pf", RM component to folder "rm", IX component to folder "ix",

- Implement the Query Engine:

   Go to folder "qe" and type in:

    make clean
    make
    ./qetest

   The program should work.  But it does nothing.  You are supposed to implement the API of the index manager defined in qe.h

- By default you should not change those functions of the classes defined in qe/qe.h. If you think some changes are really necessary, please contact us first.
