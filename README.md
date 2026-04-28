# Introduction

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
I have in this repository tried to see how close i can get to achiving the same things in c, I do use two compiler specific extensions though.
- Transparent union
  <img width="2543" height="740" alt="image" src="https://github.com/user-attachments/assets/290ff5ca-b7f7-4219-9c10-c83e9a0ea294" />
  This is the gcc documentation for transparent union, it also works in clang though.
- Attribute overloaded
  <img width="930" height="1152" alt="image" src="https://github.com/user-attachments/assets/a7a41f52-66e2-4aaa-9079-e18f47eb0ba6" />
  This is unfortunally clang only- but when used it makes alot possible you normally cant.
  I think the same can be done using a linker script for gcc though- its basicly just enabling name mangling like in c++.

I have created this repository as an example of whats possible and will try to explain my rationale and what im doing.
I do some things in different ways then what is traditionally done, I do not claim everything i do is nesseraly better then the traditional way and it come down to personal taste in the end, maybe some approaches can be usefull to the readers of this article though.
   
This project will have a main loop that will create a box every tick- another thread will iterate the collection and call each box draw method.
I choose this as an example of the kind of code I find c to be sub optimal for doing- and a good example to demonstrate some approaches I have not seen done before in c.
<img width="2558" height="1380" alt="image" src="https://github.com/user-attachments/assets/9b74ecac-9d3f-48fa-93d4-2c8edbbb60a0" />

If you need to catch up on current best practices for doing OOP in c I recommend this  [video](https://www.youtube.com/watch?v=1VPx7Tz_d6A&t=606s) - she explains everything in a really good way and also provide a repository with examples.

I will now go through each file and expand on what I am doing and my rationale for doing so, depending on current skill level alot may be irrelevant- but i will try to make it understandable for people at beginner level.

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
  
### main.c
The first logic to get executed.

<img width="203" height="63" alt="image" src="https://github.com/user-attachments/assets/ac1192af-3f29-41c7-a4a6-d4abfcb4eaac" />

- First thing you may notice is: I include c files, not h files as you usually see.  
  I will expand on this when we get to an included c file- for now ill just say I prefer having a single a single file per component when its not functionality that you need to link to as its referring to c files in the same project.

<img width="317" height="82" alt="image" src="https://github.com/user-attachments/assets/ebf69627-1f72-468f-8645-3867bc0ded93" />

- Main will create a new timer that will call drawtick function every 2 ms.
- Then exit after console.run finish.
  
<img width="577" height="627" alt="image" src="https://github.com/user-attachments/assets/7c8706b5-7a1b-4d1b-be18-63e594b95e6d" />

- There is not much to say about this, it will each time called update static variables controlling the properties of the box it create. The _Atomic is a leftover from doing it multithreaded I left out for simplicity when i introduce the basic concepts.

### array.c
A container for collections of specific types.  
Before we dive into the implementation, let me show an unittest of it:
<img width="740" height="355" alt="image" src="https://github.com/user-attachments/assets/a29f7ae7-c8df-46ed-a100-73fe30f59be1" />
We can create an array using array_of( <array of type> ) we get back a struct containing a pointer to the input on the heap, the count of elements, an enum representing the contained type and the size of the type.

<img width="282" height="25" alt="image" src="https://github.com/user-attachments/assets/3f2fb564-f6cb-4975-8238-891ce348e82e" />
<img width="821" height="166" alt="image" src="https://github.com/user-attachments/assets/b5654b87-aea4-445d-850c-6d95297d90c5" />
Notice that the append function is just 0x0, thats okay- we are not supposed to be able to call it directly anyway.  
As C do not have the thiscall calling convention enabling automatic passing of the object we call a method on we have to compesate.
We do this by calling the this function on the array we want to call a function on:
<img width="673" height="32" alt="image" src="https://github.com/user-attachments/assets/79024b52-d159-40a5-8a87-b6b880d83cae" />

The unittest will repeat the append operation with an array of ints also- to test that the correct append function is used as char is a special case- we want to overwrite the terminating null character when we append.

<img width="1458" height="246" alt="image" src="https://github.com/user-attachments/assets/543a90bf-371c-4b55-8802-44203d02f0e8" />
- First we make a typedef for the built in types containing a space for easier macro usage.
- We define the TYPES array to apply the passed in macro named $ to each of the possible types we want our array being able to contain. This enable us to reuse those types with in multiply macro expansions later.

  <img width="330" height="77" alt="image" src="https://github.com/user-attachments/assets/bfd81bb9-8cad-4bbe-95f8-8af5e0ecb7f4" />
- We make a macro called typeid that will return whatever we pass in appended the string _typeid
- We then give the typeid macro to the TYPES macro, causing the typeid to be called one time with each of the types define in TYPES- inside the of an enum.
- We then undefine the typeid macro as we dont want to pollute the global namespace with it and we only need to run it one time.
  <img width="1027" height="97" alt="image" src="https://github.com/user-attachments/assets/d6d233f3-920a-4bff-b5a9-ee8fe0498e40" />
  This is how we make an unique numeric value representation for each of the types the array can contain
<img width="592" height="105" alt="image" src="https://github.com/user-attachments/assets/0b330255-6d0b-46f3-901f-91d44a889f63" />
- Here we construct an transparent array that can contain a pointer to any of types the array can contain
  <img width="1428" height="147" alt="image" src="https://github.com/user-attachments/assets/3397403f-da57-4c08-b464-0816cb2aca59" />
<img width="1243" height="46" alt="image" src="https://github.com/user-attachments/assets/aefacfa6-72dc-497b-a064-bb6faab7696d" />
- we cannot use a void for type we need a way to special case a void* to not get dereferenced
<img width="732" height="22" alt="image" src="https://github.com/user-attachments/assets/dad46c8d-2826-4648-87f6-6580cb65da62" />
- This function will never get defined- we only need to forward declare it- because if we do a sizeof( typeid(TYPE) ) the compiler will only look at the function signature, see it return a struct with a char array having the size of the unique enum id for the specific type- because we declare it as overloaded we can have multiply functions of the same name that will then use type dispatch on the argument for selecting the correct one.
<img width="668" height="231" alt="image" src="https://github.com/user-attachments/assets/ee83adf7-75f6-48f6-9f81-56ba3ba86ef9" />
Here we make a this function for each of the possible contained type- it will return a per type structure, each having a vtable with an append function returning and exspecting argument of the given type.
<img width="632" height="210" alt="image" src="https://github.com/user-attachments/assets/6dde0be9-d116-4e99-807a-061a2664fe4c" />

- First we forward declare the append function we want the vtable to refer to.
  
- We then make a static variable for storing the pointer of the object passed in by the array argument.
  
- Given the array argument passed in is not 0 we:
  
  Set the vtables append to the the matching append function.
  
  Set the typeid to be the size of the returned struct from calling the typeid function with given type.
   
  Set the correct size of the contained type.
  
  Store the passed in pointer in the last variable.
  
  Return what is stored in the last variable- notice, this means when we pass in 0 we get back whatever was the last value given to the function for the array argument.
  
<img width="720" height="38" alt="image" src="https://github.com/user-attachments/assets/33d9af7a-5af2-4348-9341-5aa59b50d76b" />
- This function will also never be implemented as its only used to map types to their corresponding array_of containing type using typeof( array_of( TYPE ) )
