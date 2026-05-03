
### stdafx.h

<img width="572" height="555" alt="image" src="https://github.com/user-attachments/assets/5917aa5f-65df-439a-8f3e-16581e216912" />
This file is intended to be included in every c file.  

- I make alias for __auto_type- it works as in c++ except it only works inside functions, not for global variables nor function declarations.  
- I create aliases for the commonly used attributes.  
- Before i include windows.h I define STRICT, this will cause creation of typesafe per object type handles due to
  <img width="766" height="217" alt="image" src="https://github.com/user-attachments/assets/3beecc38-a9e5-4f2c-bc9d-dfee271c2e97" />
  <img width="301" height="275" alt="image" src="https://github.com/user-attachments/assets/380be824-2c14-46d6-8e20-4fad5d39d4fa" />
  A typedef alias is not an unique type, but an unique named struct is, so it enables us to use the per type handle in function signatures instead of just a typedef for void* that will not differentiate between the different types of handles during compilation.  
- I make an enum representing true and false, if you have not seen an a nameless enum before this is how it works:
  In c theres 4 namespaces: enum,struct,union and global. When you do a typedef alias to an existing type you actually create an alias in the global namespace, that is why you dont have to type struct in front anymore.
  Additionally a typedef alias can append the following type information: array of x dimensions, pointer of x count, const, volatile etc.
- I undef some macros windows.h defined to remove compiler warnings when I use them myself later.
 
