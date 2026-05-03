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
