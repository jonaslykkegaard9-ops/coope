# coping with c
I have been programming c++ professionally for around 15 years, I do find c++ to be the best suited programming language for most purposes due to its flexibility and powerfull language constructs.  
C++ enable mixing of OOP, functional and generic programming paradigms as needed- enabling using the best suited paradigm for a given task and provides frictionless interoperability between them.  
Another big factor is the boilerplate free interop with the c language- which have become a universal interface language for everything from kernels to bindings for interop with any languages.  

There is no denying that c++ certanly also have its issues- mostly it suffers from having to provide backwards compability with all the previous choices made in its entire lifetime.
With time this has caused the language to grow to a size where it takes many years to learn all the needed concepts, constructs, patterns, libaries and language features involved in creating performant, elegant, safe and reliable applications.  

C++ do get alot things right though- it provides language features that support some of the most importnant principles for designing good software such as:  
-Encapsulation- enabling invariant enforcement for creating isolated components with type erasing interfaces used for interop.  
-Templates- enabling typesafe containers, decoupling algorhitmns from any specific container enabling truly reusable implementations of algorhitmns.  
-Iterators- by writing algorhitmns to use the containers iterator you avoid creating a dependence on the contained type in the algorhitmn implementation.  
-Constexpr/template meta programming with specialaisations on types- being able to write code that generate code in the same language enables creating flexible code with a default functionality that can diverge only in specific edge cases.  
-Scoping- Namespaces and classes enable compartmentalization and enforcing of invariants on usage of functionality in a way that enable users to create apis that cannot be used wrong.  

  
