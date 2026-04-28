# coping with c
I have been programming c++ professionally for around 15 years, I do find c++ to be the best suited programming language for most purposes due to its flexibility and powerfull language constructs.  
C++ enable mixing of OOP, functional and generic programming paradigms as needed- enabling using the best suited paradigm for a given task with frictionless interoperability between them.  
Another big factor is the boilerplate free interop with the c language- which have become a universal interface language for everything from kernels to interop bindings for languages.  

There is no denying that c++ certanly also have its issues- mostly it suffers from having to provide backwards compability with all the previous design choices made in its entire lifetime.  
With time this has caused the language to grow to a size where it takes many years to learn all the needed concepts, constructs, patterns, libaries and language features involved in creating performant, elegant, safe and reliable applications.  

C++ do get alot things right though- it provides language features that support some of the most importnant principles for designing good software such as:  
- Constructors- providing a single function that can enforce invariant conditions on aquiring a given structure enables an established intuitive mechanism for grouping of functions and shared state into reuseable components(classes).  
- Encapsulation- hiding of private members/member functions supports invariant enforcement by denying access to manipulation of specific variables without going through a specific function.  
- Generics- templates enable typesafe containers, decoupling algorhitmns from any specific container- making truly reusable implementations of algorhitmns possible.  
- Iterators- by writing algorhitmns to use the containers iterator you avoid creating a dependence on the contained type.  
- Compile time code generation- constexpr and templates enables creating flexible components with dynamic types inferred on usage during compilation.  
- Scoping- like classes encapsulate private members, namespaces also enable enforcing of invariants by making functions uncallable except through the intended call sequence provided by the api creator.
  Additionally scoping enables avoding pulloting the global namespace unnesserary so multiply sub namespaces dont collide.  

C++ is just not always an option, for example for kernel development or embedded- and some people prefer avoiding the complexities of c++.
I have in this repository tried to see how close i can get to achiving the same things in c- I do though use two compiler specific extensions.
- Transparent union
  <img width="2543" height="740" alt="image" src="https://github.com/user-attachments/assets/290ff5ca-b7f7-4219-9c10-c83e9a0ea294" />
  This is the gcc documentation for transparent union, it also works in clang though.
- Attribute overloaded
  <img width="930" height="1152" alt="image" src="https://github.com/user-attachments/assets/a7a41f52-66e2-4aaa-9079-e18f47eb0ba6" />



  
