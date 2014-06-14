This is an implementation of one virtually complete database management system, as a course project for CS222 at University of California, Irvine. It includes Page File Management (pf), Record Management (rm), Index Management (ix), and Query Engine (qe). For more detailed information, please refer to project reports in Reports directory.

This is a cooperation workpiece with Changjian Zou. It is for personal reference only and should not be copied or referred by any others without notice.

#### Build Instruction

- Modify the "CODEROOT" variable in makefile.inc to point to the root of your code base.

- Go to folder "pf", "rm", "ix", and "qe", typing in:

    make clean
    make

#### Functor Application

Conceptual understanding:

The Functor class in essence wraps the context (template application) to the function that loads necessary children tree nodes for a given root node. The search function is a function over a root node. After applying the loading function as a Functor, it continues perform search on the loaded tree nodes, and in the end wraps the result again with the template application context.



![Creative Commons License] (http://i.creativecommons.org/l/by-nc-nd/4.0/88x31.png)

This work is licensed under a [Creative Commons Attribution-NonCommercial-NoDerivatives 4.0 International License] (http://creativecommons.org/licenses/by-nc-nd/4.0/).
