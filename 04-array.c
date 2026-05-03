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
  <img width="291" height="26" alt="image" src="https://github.com/user-attachments/assets/86f884fe-0082-473e-a84b-9c7fae7c691c" />
- This is why I include c files- the trick is that when you include this c file __INCLUDE_LEVEL__ will be 1, so the implementations will not get evaluated- but when we compile the c file directly, they will.
  <img width="1785" height="417" alt="image" src="https://github.com/user-attachments/assets/f7a5ad1a-f92e-4373-b364-09deded66080" />
- First we make an append for array_of_char- as we want it to overwrite the null terminators only when its a char.
- Then an append for handling every other datatype.
  <img width="1597" height="58" alt="image" src="https://github.com/user-attachments/assets/edf9f148-fc84-4fbf-b577-8106613d1488" />
  When we call the append function we loose information about the contained type- this wrapper will restore the original type.
  <img width="1306" height="251" alt="image" src="https://github.com/user-attachments/assets/ffd4664d-b7a8-40f2-ac15-fff572c5b8d4" />
- As this is in the else clause to the __INCLUDE_LEVEL__ it will only get evaluated when you include the file.
- These macros are used mostly to extract the information about arrays before they decay into pointers
