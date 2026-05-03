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
When you inside a struct write just the name of another structure it will be the same as if copy pasted the members directly  into the encapsulating structure.
When you declare a structure first and give it a member name, followed by a nameless declaration inside an anonymous union what will happen is:
The members of struct box_interface become accessible through boxinterface directly, but also grouped together in box_interface.box_interface, they will be the same though, as because of the union the memory align up at the same location.
This way we can still mass assign all the members of struct box_interface and have them directly acccessible on the outher encapsulating struct/union.
