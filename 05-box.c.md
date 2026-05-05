<img width="830" height="797" alt="image" src="https://github.com/user-attachments/assets/ae297cf2-1679-4406-88f4-a249551d6c13" />

Let me start with something I have not found much written about- designing component based software in c, a language that is inherently imperative.
This approach I have found to work best- I split each file into the public interface and the private implementation by seperating it with a 

<img width="221" height="20" alt="image" src="https://github.com/user-attachments/assets/17bc5e7d-bf26-4998-b049-78447444724d" />

This way- when you include the file from another c file you only get the public part at the beginning of the file, but when you compile the file directly the entirety of the file will get compiled.
You then put the needed data structures thats needed to use the interface in the public part.  

If a public function takes a structure with an enum you then need to put both the structure and the enums in the public part of the file, providing the needed data structures by a single include to use the given component.

The union, named struct, unnamed struct pattern:
<img width="196" height="100" alt="image" src="https://github.com/user-attachments/assets/4a302568-da86-4a25-bd03-287e5e06cac2" />
This pattern may seem weird, let me try to explain:  

When you inside a struct write just the name of another structure it will be the same as if copy pasted the members directly into the encapsulating structure.  
When you declare a structure first and give it a member name, followed by a nameless declaration inside an anonymous union what will happen is:  
The members of struct box_interface become accessible through boxinterface directly, but also grouped together in box_interface.box_interface, they will be the same though, as because of the union the memory will align up at the same location.  
This way we can still mass assign all the members of struct box_interface and have them directly acccessible on the outher encapsulating struct/union.  

<img width="485" height="52" alt="image" src="https://github.com/user-attachments/assets/039b602f-590f-4131-8322-1252bcc68a32" />
When I declare an structure in the public part of the file extern like this- I can then in the private part of it assign and at compile time fill the needed vtable ready for usage as I at the last line of the file:
<img width="972" height="862" alt="image" src="https://github.com/user-attachments/assets/b30c6467-6e8d-4645-836b-1f5c0f0f0df3" />
I use typeof(box) for the type as I dont want to repeat the type, but simple reuse whatever type i used in the interface part of the component.

Notice- my interface will handle the creation of the box- you can only call box.new and then you get returned a box allocated on the heap.
This enable me to do a trick with a transparent union, lets first look at the drawable interface in drawable.c:
<img width="807" height="272" alt="image" src="https://github.com/user-attachments/assets/20c1e7bf-90d8-49e0-945f-09be6b744332" />
We see the draw function takes a struct drawable* for its first argument-yet, the draw function we assign to the box that implements the drawable interface is:
<img width="631" height="32" alt="image" src="https://github.com/user-attachments/assets/00282921-a305-431e-8045-2a2415519df1" />
It takes struct box*, yet i never do any casting, and I get no compiler warnings- the magic is all in the box.new function:
<img width="647" height="171" alt="image" src="https://github.com/user-attachments/assets/bd3cf61c-4bbf-4eb2-adb8-0c3b0e0c88d2" />
First i declare a transparent union that for member have both a struct box and a struct drawable pointer- then i predeclare the draw function with that transparent union for the first argument.  
Then i assign the box draw function to the draw I just predeclared, this is valid because the transparent union will accept the struct drawable pointer and the struct box pointer.  
The assigningment is then delayed to linktime where it will choose the, later implemented, draw function.  
Since a struct box begins with draw function but additionally have the arguments a pointer to a struct drawable is valid but also a a pointer to a struct box and we can keep the struct box in the private part of the component.  
This will end up having the same effect as when we have private members in c++, from the outside we cannot edit or even access the private members as shown here:  
<img width="1283" height="537" alt="image" src="https://github.com/user-attachments/assets/da72d16f-8e00-4a04-a3a0-ce62c0f92ef0" />
We can only see the public part of the box with the draw method, yet when we the call draw and pass in the exact same object pointer inside the draw method we get this:
<img width="1275" height="520" alt="image" src="https://github.com/user-attachments/assets/dd641642-40d5-4e61-9dd1-5d8f0d15d896" />
This is a way we can make true typesafe encapsulation without any nasty casts or void* in c trough the magic of transparent unions.
