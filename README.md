# coping with c
I have been programming c++ professionally for around 15 years, I do find c++ to be the best suited programming language for most purposes due to its flexibility and powerfull language constructs.  
C++ enable mixing of OOP, functional and generic programming paradigms as needed- enabling using the best suited paradigm for a given task with frictionless interoperability between them.  
Another big factor is the boilerplate free interop with the c language- which have become a universal interface language for everything from kernels to interop bindings for languages.  

There is no denying that c++ certanly also have its issues- mostly it suffers from having to provide backwards compability with all the previous design choices made in its entire lifetime.  
With time this has caused the language to grow to a size where it takes many years to learn all the needed concepts, constructs, patterns, libaries and language features involved in creating performant, elegant, safe and reliable applications.  

C++ do get alot things right though- it provides language features that support some of the most importnant principles for designing good software such as:  
- Encapsulation- hiding of private members/member functions supports invariant enforcement by denying access to manipulation of specific variables without going through a specific function.  
- Generics- templates enable typesafe containers, decoupling algorhitmns from any specific container- making truly reusable implementations of algorhitmns possible.  
- Iterators- by writing algorhitmns to use the containers iterator you avoid creating a dependence on the contained type.  
- Compile time code generation- constexpr and template meta programming enables creating flexible components with dynamic types inferred on usage during compilation.  
- Scoping- like classes encapsulate private members, namespaces also enable enforcing of invariants by making functions uncallable except through the intended call sequence provided by the api creator.  Additionally scoping enables avoding pulloting the global namespace unnesserary so multiply sub namespaces dont collide.  



  
