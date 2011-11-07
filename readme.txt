
- Modify the "CODEROOT" variable in makefile.inc to point to the root of your code base

- Copy your own implementation of PF component to folder "pf"

- Implement the Record Manager (RM):

   Go to folder "rm" and type in:

    make clean
    make
    ./rmtest

   The program should work.  But it does nothing.  You are supposed to implement the API of the record manager defined in rm.h

- By default you should not change those functions of the RM and RM_ScanIterator class defined in rm/rm.h. If you think some changes are really necessary, please contact us first.
